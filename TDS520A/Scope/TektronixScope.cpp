// Scope/TektronixScope.cpp
#include "pch.h"
#include "TektronixScope.h"
#include "Logger.h"
#include "StringUtils.h"

TektronixScope::TektronixScope() = default;
TektronixScope::~TektronixScope() { Disconnect(); }

bool TektronixScope::AutoConnect(int gpibBoard, GpibError& err)
{
    int addr = 0;
    if (!m_controller.FindTektronixScope(gpibBoard, addr))
    {
        err.message = L"No Tektronix oscilloscope found on GPIB board " +
                      std::to_wstring(gpibBoard);
        return false;
    }
    GpibDeviceInfo info;
    info.boardIndex  = gpibBoard;
    info.primaryAddr = addr;
    info.timeoutCode = T3s;  // 3 s is plenty for any TDS 520A response
    return Connect(info, err);
}

bool TektronixScope::Connect(const GpibDeviceInfo& info, GpibError& err)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_controller.OpenDevice(info, err))
        return false;
    m_deviceInfo = info;

    // Clear any pending errors and drain leftover bytes from previous sessions
    GpibDevice* dev = m_controller.GetDevice();
    dev->Clear(err);
    ::Sleep(100);
    dev->DrainInput();
    ::Sleep(100);

    // Identify — retry once in case first response is stale buffer data
    std::string idn;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        if (!dev->Query(Scpi::IDN(), idn, err))
        {
            LOG_ERR("Scope", L"*IDN? failed after connect");
            return false;
        }
        // Valid IDN must contain a comma (manufacturer,model,serial,fw)
        if (idn.find(',') != std::string::npos)
            break;
        LOG_WRN("Scope", L"*IDN? returned stale data '%S', retrying...", idn.c_str());
        dev->DrainInput();
        ::Sleep(100);
        idn.clear();
    }
    if (idn.find(',') == std::string::npos)
    {
        err.message = L"*IDN? did not return a valid response";
        LOG_ERR("Scope", L"*IDN? invalid: %S", idn.c_str());
        return false;
    }
    m_idn = StringUtils::AnsiToWide(idn);
    LOG_INF("Scope", L"Connected: %s", m_idn.c_str());

    // Setup encoding for waveform transfers
    if (!SetupDataEncoding(err)) return false;

    m_acqCount = 0;
    m_acqCountResetTick = ::GetTickCount();
    return true;
}

void TektronixScope::Disconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_controller.CloseDevice();
    LOG_INF("Scope", L"Disconnected");
}

bool TektronixScope::IsConnected() const
{
    return m_controller.IsDeviceOpen();
}

bool TektronixScope::Identify(std::string& idn, GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }
    return dev->Query(Scpi::IDN(), idn, err);
}

bool TektronixScope::SetupDataEncoding(GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    if (!dev) return false;

    // Use signed binary (RIBinary), 1 byte per sample (TDS 520A has 8-bit ADC)
    if (!dev->Write(Scpi::DataEncdg(), err))     return false;
    if (!dev->Write(Scpi::DataWidth(1), err))    return false;
    if (!dev->Write(Scpi::DataStartStop(), err)) return false;

    LOG_DBG("Scope", L"Data encoding configured: RIBinary, width=1");
    return true;
}

bool TektronixScope::StartContinuous(GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }

    if (!dev->Write(Scpi::AcqMode("SAMple"), err))       return false;
    if (!dev->Write("ACQuire:STOPAfter RUNSTop", err)) return false;
    if (!dev->Write(Scpi::AcqState(true), err))          return false;
    LOG_INF("Scope", L"Acquisition started (continuous)");
    return true;
}

bool TektronixScope::Stop(GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }
    if (!dev->Write(Scpi::AcqState(false), err)) return false;
    LOG_INF("Scope", L"Acquisition stopped");
    return true;
}

bool TektronixScope::AcquireSingle(GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }

    // Set single-sequence mode and arm the trigger.
    // Do NOT wait for OPC – that blocks the command thread while the GPIB bus
    // is occupied.  The acquisition thread will fetch the waveform on its next cycle.
    if (!dev->Write(Scpi::AcqSingle(), err))    return false;
    if (!dev->Write(Scpi::AcqState(true), err)) return false;
    LOG_INF("Scope", L"Single acquisition triggered");
    return true;
}

bool TektronixScope::SetChannel(ScopeChannel ch, GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }

    if (!dev->Write(Scpi::DataSource(ch), err)) return false;
    m_channel = ch;
    InvalidatePreambleCache();
    LOG_INF("Scope", L"Active channel: %S", ChannelName(ch));
    return true;
}

bool TektronixScope::FetchWaveform(WaveformBuffer& outBuf, GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }

    // --- Fast continuous path ---
    // TDS 520A in SAMple+RUNStop mode continuously updates its waveform buffer.
    // We do NOT stop/restart the scope on every cycle — that costs 2 extra GPIB
    // round-trips (~100-200 ms) and limits us to ~2 Hz.
    //
    // Instead we read CURVE? while the scope is running.  The worst case is a
    // single glitch frame if a trigger fires mid-transfer, which is acceptable
    // for a live display.  If a clean single-shot capture is needed, use
    // AcquireSingle() which arms proper single-sequence mode.
    //
    // Sequence per cycle (scope already running):
    //   CURVE? ? ibrd binary block          ? only 1 round-trip!

    // 1. Refresh preamble only when settings changed (channel / scale / etc.)
    if (!m_preambleValid)
    {
        // Brief stop while reading preamble so the format fields are stable
        if (!dev->Write(Scpi::AcqState(false), err))
        {
            LOG_ERR("Scope", L"STOP for preamble refresh failed");
            return false;
        }
        std::string preambleStr;
        bool ok = dev->Query(Scpi::WfmPre(), preambleStr, err);
        // Always re-arm regardless of query result
        GpibError restartErr;
        dev->Write(Scpi::AcqState(true), restartErr);
        if (!ok)
        {
            LOG_ERR("Scope", L"WFMPRE? failed");
            return false;
        }
        if (!m_preambleCache.Parse(preambleStr))
        {
            err.message = L"Failed to parse waveform preamble";
            LOG_ERR("Scope", L"Preamble parse failed: %S", preambleStr.c_str());
            return false;
        }
        m_preambleValid = true;
        LOG_DBG("Scope", L"Preamble refreshed from scope");
    }

    // 2. Copy cached preamble into output buffer
    outBuf.Preamble = m_preambleCache;

    // 3. Request curve data — scope keeps running, no STOP needed
    if (!dev->Write(Scpi::Curve(), err))
    {
        LOG_ERR("Scope", L"CURVE? write failed");
        return false;
    }

    if (!dev->ReadBinaryBlock(outBuf.RawData, err))
    {
        LOG_ERR("Scope", L"Binary block read failed");
        // Device Clear flushes the GPIB bus after a timeout
        GpibError clrErr;
        dev->Clear(clrErr);
        ::Sleep(50);
        return false;
    }

    outBuf.Channel   = m_channel;
    outBuf.Timestamp = std::chrono::steady_clock::now();

    // Update cached display parameters from the preamble — UI reads these
    // without touching the GPIB bus, avoiding race conditions with the timer.
    AtomicStoreDouble(m_cachedSecPerDiv,   outBuf.Preamble.secPerDiv);
    AtomicStoreDouble(m_cachedVoltsPerDiv, outBuf.Preamble.voltsPerDiv);

    // Update acquisition rate counter
    m_acqCount++;
    DWORD now = ::GetTickCount();
    DWORD elapsed = now - m_acqCountResetTick;
    if (elapsed >= 1000)
    {
        m_acqRate = static_cast<uint32_t>((m_acqCount * 1000u) / elapsed);
        m_acqCount = 0;
        m_acqCountResetTick = now;
    }

    LOG_DBG("Scope", L"Waveform fetched: %zu samples, channel %S",
        outBuf.RawData.size(), ChannelName(m_channel));
    return true;
}

bool TektronixScope::QueryHorizontalScale(double& secPerDiv, GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }
    std::string resp;
    if (!dev->Query(Scpi::HorScaleQuery(), resp, err)) return false;
    return StringUtils::ParseDouble(resp, secPerDiv);
}

bool TektronixScope::QueryChannelScale(ScopeChannel ch, double& voltsPerDiv, GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }
    std::string resp;
    if (!dev->Query(Scpi::ChScaleQuery(ch), resp, err)) return false;
    return StringUtils::ParseDouble(resp, voltsPerDiv);
}

bool TektronixScope::QueryTriggerState(std::wstring& state, GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }
    std::string resp;
    if (!dev->Query(Scpi::TrigStateQuery(), resp, err)) return false;
    state = StringUtils::AnsiToWide(resp);
    return true;
}

bool TektronixScope::QueryTriggerLevel(double& level, GpibError& err)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }
    std::string resp;
    if (!dev->Query(Scpi::TrigLevelQuery(), resp, err)) return false;
    return StringUtils::ParseDouble(resp, level);
}

bool TektronixScope::SetHorizontalScale(double secPerDiv, GpibError& err)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }
    bool ok = dev->Write(Scpi::HorScale(secPerDiv), err);
    if (ok) InvalidatePreambleCache();
    return ok;
}

bool TektronixScope::SetChannelScale(ScopeChannel ch, double voltsPerDiv, GpibError& err)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }
    bool ok = dev->Write(Scpi::ChScale(ch, voltsPerDiv), err);
    if (ok) InvalidatePreambleCache();
    return ok;
}

bool TektronixScope::AdjustTriggerLevel(double deltaVolts, GpibError& err)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }
    std::string resp;
    double current = 0.0;
    if (dev->Query(Scpi::TrigLevelQuery(), resp, err))
        StringUtils::ParseDouble(resp, current);
    return dev->Write(Scpi::TrigLevel(current + deltaVolts), err);
}

bool TektronixScope::Reset(GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }
    if (!dev->Write(Scpi::RST(), err)) return false;
    ::Sleep(3000);  // TDS 520A needs ~3s after RST
    return SetupDataEncoding(err);
}

bool TektronixScope::ClearStatus(GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    if (!dev) { err.message = L"Not connected"; return false; }
    return dev->Write(Scpi::CLS(), err);
}

bool TektronixScope::WaitForOPC(int timeoutMs, GpibError& err)
{
    auto* dev = m_controller.GetDevice();
    auto start = std::chrono::steady_clock::now();
    while (true)
    {
        std::string resp;
        if (dev->Query(Scpi::OPCQuery(), resp, err) && StringUtils::Trim(resp) == "1")
            return true;

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs)
        {
            err.message = L"WaitForOPC timeout";
            LOG_ERR("Scope", L"*OPC? timeout after %d ms", timeoutMs);
            return false;
        }
        ::Sleep(100);
    }
}

ScopeStatus TektronixScope::GetStatus() const
{
    ScopeStatus s;
    s.idn           = m_idn;
    s.activeChannel = m_channel;
    s.acquisitionsPerSec = m_acqRate.load();
    s.state = m_controller.IsDeviceOpen() ? ScopeState::Connected : ScopeState::Disconnected;
    return s;
}
