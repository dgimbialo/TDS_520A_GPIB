// Threads/AcquisitionThread.cpp
#include "pch.h"
#include "AcquisitionThread.h"
#include "Logger.h"

AcquisitionThread::AcquisitionThread(WaveformRingBuffer& ringBuf)
    : m_ringBuf(ringBuf)
{
}

AcquisitionThread::~AcquisitionThread()
{
    Stop();
    WaitForStop(3000);
}

bool AcquisitionThread::Start(ScopeChannel channel)
{
    if (m_state != AcqThreadState::Stopped)
        return false;
    if (!m_scope || !m_scope->IsConnected())
    {
        LOG_ERR("AcqThread", L"Cannot start: scope not connected");
        return false;
    }

    m_stopRequest      = false;
    m_consecutiveErrors = 0;
    m_seqNum           = 0;
    m_windowStart      = ::GetTickCount();
    m_acqCountInWindow = 0;

    // Seed the queue with the initial channel command so SetChannel()
    // is called inside the GPIB thread, not from the UI thread.
    PostCommand(channel == ScopeChannel::CH1 ? ScopeCommand::SetChannel1 :
                channel == ScopeChannel::CH2 ? ScopeCommand::SetChannel2 :
                channel == ScopeChannel::CH3 ? ScopeCommand::SetChannel3 :
                                               ScopeCommand::SetChannel4);

    m_state  = AcqThreadState::Running;
    m_thread = std::thread(&AcquisitionThread::ThreadProc, this);
    LOG_INF("AcqThread", L"Started");
    return true;
}

void AcquisitionThread::Stop()
{
    m_stopRequest = true;
    m_state = AcqThreadState::Stopping;
    LOG_INF("AcqThread", L"Stop requested");
}

void AcquisitionThread::WaitForStop(DWORD timeoutMs)
{
    if (m_thread.joinable())
    {
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeoutMs);
        while (m_state != AcqThreadState::Stopped &&
               std::chrono::steady_clock::now() < deadline)
        {
            ::Sleep(50);
        }
        if (m_thread.joinable())
            m_thread.join();
    }
}

AcqThreadStats AcquisitionThread::GetStats() const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

// ---------------------------------------------------------------------------
// Main GPIB thread loop
// ---------------------------------------------------------------------------

void AcquisitionThread::ThreadProc()
{
    LOG_INF("AcqThread", L"Thread started");

    while (!m_stopRequest)
    {
        DWORD cycleStart = ::GetTickCount();

        // Execute all queued commands first.
        // This is the ONLY place any GPIB function is allowed to be called.
        DrainCommandQueue();

        if (!DoAcquisitionCycle())
        {
            if (m_stopRequest) break;

            if (m_consecutiveErrors >= kMaxConsecutiveErrors)
            {
                LOG_WRN("AcqThread", L"%d consecutive errors - attempting reconnect",
                    m_consecutiveErrors);
                AttemptReconnect();
            }
            else
            {
                ::Sleep(200);
            }
            continue;
        }

        // Rate throttle: sleep the remainder of the target interval.
        uint32_t interval = m_targetIntervalMs.load();
        if (interval > 0)
        {
            DWORD elapsed = ::GetTickCount() - cycleStart;
            if (elapsed < interval)
                ::Sleep(interval - elapsed);
        }
    }

    m_state = AcqThreadState::Stopped;
    LOG_INF("AcqThread", L"Thread exited");
}

void AcquisitionThread::DrainCommandQueue()
{
    // Snapshot under lock, execute outside — lets other threads keep posting
    // while we perform (potentially slow) GPIB write/read operations.
    std::queue<ScopeCommand> local;
    {
        std::lock_guard<std::mutex> lk(m_cmdMutex);
        std::swap(local, m_cmdQueue);
    }

    GpibError err;
    while (!local.empty())
    {
        ScopeCommand cmd = local.front();
        local.pop();

        switch (cmd)
        {
        case ScopeCommand::SetChannel1:
        case ScopeCommand::SetChannel2:
        case ScopeCommand::SetChannel3:
        case ScopeCommand::SetChannel4:
        {
            ScopeChannel ch = static_cast<ScopeChannel>(
                static_cast<int>(cmd) -
                static_cast<int>(ScopeCommand::SetChannel1) + 1);
            if (!m_scope->SetChannel(ch, err))
                HandleError(err);
            break;
        }
        case ScopeCommand::TimeDivUp:
        {
            double cur = 0.0;
            if (m_scope->QueryHorizontalScale(cur, err) && cur > 0.0)
                m_scope->SetHorizontalScale(cur * 2.0, err);
            break;
        }
        case ScopeCommand::TimeDivDown:
        {
            double cur = 0.0;
            if (m_scope->QueryHorizontalScale(cur, err) && cur > 0.0)
                m_scope->SetHorizontalScale(cur * 0.5, err);
            break;
        }
        case ScopeCommand::VoltDivUp:
        {
            double cur = 0.0;
            ScopeChannel ch = m_scope->GetChannel();
            if (m_scope->QueryChannelScale(ch, cur, err) && cur > 0.0)
                m_scope->SetChannelScale(ch, cur * 2.0, err);
            break;
        }
        case ScopeCommand::VoltDivDown:
        {
            double cur = 0.0;
            ScopeChannel ch = m_scope->GetChannel();
            if (m_scope->QueryChannelScale(ch, cur, err) && cur > 0.0)
                m_scope->SetChannelScale(ch, cur * 0.5, err);
            break;
        }
        case ScopeCommand::TrigLevelUp:
            m_scope->AdjustTriggerLevel(+0.1, err);
            break;
        case ScopeCommand::TrigLevelDown:
            m_scope->AdjustTriggerLevel(-0.1, err);
            break;
        case ScopeCommand::AcquireSingle:
            if (!m_scope->AcquireSingle(err))
                HandleError(err);
            else
                LOG_INF("AcqThread", L"Single acquisition triggered");
            break;
        case ScopeCommand::Run:
            m_scope->StartContinuous(err);
            LOG_INF("AcqThread", L"Continuous acquisition resumed");
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Per-cycle waveform fetch + decode
// ---------------------------------------------------------------------------

bool AcquisitionThread::DoAcquisitionCycle()
{
    WaveformBuffer raw;
    GpibError err;
    if (!m_scope->FetchWaveform(raw, err))
    {
        HandleError(err);
        ::Sleep(300);
        return false;
    }
    m_consecutiveErrors = 0;
    raw.SequenceNumber  = ++m_seqNum;

    DecodedWaveform decoded;
    WaveformDecoder dec;
    if (!dec.Decode(raw, decoded))
    {
        LOG_WRN("AcqThread", L"Decode failed for seq %u", m_seqNum);
        return true;  // not a GPIB error — keep running
    }

    if (m_callback)
        m_callback(decoded);

    m_ringBuf.Push(std::move(decoded));

    // Update acquisitions/sec counter.
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.acquisitionsTotal++;
        m_acqCountInWindow++;
        DWORD now     = ::GetTickCount();
        DWORD elapsed = now - m_windowStart;
        if (elapsed >= 1000)
        {
            m_stats.acquisitionsPerSec =
                static_cast<uint32_t>((m_acqCountInWindow * 1000u) / elapsed);
            m_acqCountInWindow = 0;
            m_windowStart      = now;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Error handling / reconnect
// ---------------------------------------------------------------------------

void AcquisitionThread::HandleError(const GpibError& err)
{
    m_consecutiveErrors++;
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats.gpibErrors++;
    m_stats.lastError = err.message;
    LOG_ERR("AcqThread", L"GPIB error #%d: ibsta=%04X iberr=%d - %s",
        m_consecutiveErrors, err.ibsta, err.iberr, err.message.c_str());
}

void AcquisitionThread::AttemptReconnect()
{
    m_consecutiveErrors = 0;
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.reconnects++;
    }

    GpibError err;
    m_scope->Disconnect();
    ::Sleep(2000);

    GpibDeviceInfo info = m_scope->GetDeviceInfo();
    LOG_INF("AcqThread", L"Reconnecting to board=%d addr=%d",
        info.boardIndex, info.primaryAddr);

    if (!m_scope->Connect(info, err))
    {
        LOG_ERR("AcqThread", L"Reconnect failed: %s", err.message.c_str());
        if (info.primaryAddr == 0)
        {
            if (!m_scope->AutoConnect(info.boardIndex, err))
            {
                LOG_ERR("AcqThread", L"AutoConnect also failed");
                m_state       = AcqThreadState::Error;
                m_stopRequest = true;
                return;
            }
        }
        else
        {
            m_state       = AcqThreadState::Error;
            m_stopRequest = true;
            return;
        }
    }

    m_scope->StartContinuous(err);
    PostCommand(ScopeCommand::SetChannel1);  // re-apply channel after reconnect
    LOG_INF("AcqThread", L"Reconnected successfully");
}
