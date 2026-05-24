// Gpib/GpibDevice.cpp
#include "pch.h"
#include "GpibDevice.h"
#include "Logger.h"
#include "StringUtils.h"

GpibDevice::GpibDevice() = default;

GpibDevice::~GpibDevice()
{
    Close();
}

bool GpibDevice::Open(const GpibDeviceInfo& info, GpibError& err)
{
    Close();
    m_info = info;

    // ibdev(boardIndex, primaryAddr, secondaryAddr, timeout, sendEOI, eosMode)
    m_ud = ::ibdev(
        info.boardIndex,
        info.primaryAddr,
        info.secondaryAddr == 0 ? NO_SAD : info.secondaryAddr,
        info.timeoutCode,
        info.sendEOI,
        (info.eotMode << 8) | info.eosByte  // EOS config
    );

    if (m_ud < 0 || (ibsta & ERR))
    {
        FillError(err, L"ibdev");
        LOG_ERR("GPIB", L"ibdev failed: ud=%d ibsta=%04X iberr=%d - %s",
            m_ud, ibsta, iberr, err.message.c_str());
        m_ud = -1;
        return false;
    }

    LOG_INF("GPIB", L"Opened device: board=%d addr=%d ud=%d",
        info.boardIndex, info.primaryAddr, m_ud);
    return true;
}

void GpibDevice::Close()
{
    if (m_ud >= 0)
    {
        ::ibonl(m_ud, 0);  // Take device offline
        LOG_INF("GPIB", L"Closed device ud=%d", m_ud);
        m_ud = -1;
    }
}

bool GpibDevice::Write(const std::string& cmd, GpibError& err)
{
    if (m_ud < 0) { err.message = L"Device not open"; return false; }

    // Append LF if not present
    std::string s = cmd;
    if (!s.empty() && s.back() != '\n')
        s += '\n';

    ::ibwrt(m_ud, const_cast<char*>(s.c_str()), static_cast<long>(s.size()));
    if (ibsta & ERR)
    {
        FillError(err, L"ibwrt");
        LOG_ERR("GPIB", L"ibwrt failed cmd='%S' iberr=%d - %s",
            cmd.c_str(), iberr, err.message.c_str());
        return false;
    }
    return true;
}

int GpibDevice::Read(char* buf, int bufSize, GpibError& err)
{
    if (m_ud < 0) { err.message = L"Device not open"; return -1; }

    ::ibrd(m_ud, buf, static_cast<long>(bufSize));
    if (ibsta & ERR)
    {
        FillError(err, L"ibrd");
        LOG_ERR("GPIB", L"ibrd failed iberr=%d - %s", iberr, err.message.c_str());
        return -1;
    }
    return static_cast<int>(ibcntl);
}

bool GpibDevice::Query(const std::string& cmd, std::string& response, GpibError& err)
{
    if (!Write(cmd, err)) return false;

    // 64KB should be sufficient for ASCII responses
    constexpr int kBufSize = 65536;
    std::string buf(kBufSize, '\0');
    int n = Read(&buf[0], kBufSize, err);
    if (n < 0) return false;

    response = StringUtils::TrimRight(buf.substr(0, static_cast<size_t>(n)));
    return true;
}

bool GpibDevice::QueryRaw(const std::string& cmd, std::vector<uint8_t>& response,
                           GpibError& err)
{
    if (!Write(cmd, err)) return false;

    // WAVFRM? for 500 points: ~200 bytes preamble + 1 byte '%' + ~7 bytes
    // block header + 500 bytes data = ~710 bytes.  Allocate generously.
    constexpr int kBufSize = 131072;  // 128 KB
    response.resize(static_cast<size_t>(kBufSize));
    ::ibrd(m_ud, response.data(), static_cast<long>(kBufSize));
    if (ibsta & ERR)
    {
        FillError(err, L"ibrd QueryRaw");
        response.clear();
        return false;
    }
    response.resize(static_cast<size_t>(ibcntl));
    return true;
}

bool GpibDevice::ReadBinaryBlock(std::vector<uint8_t>& data, GpibError& err)
{
    if (m_ud < 0) { err.message = L"Device not open"; return false; }

    // --- Single-ibrd fast path ---
    // Read the entire CURVE? response in one USB/GPIB transaction.
    // For 250 pts @1 byte: response = "#" + "3" + "250" + 250 bytes = 256 bytes.
    // 512 bytes covers up to ~500 points. NI USB driver transfers exactly
    // as many bytes as ibrd requests over DMA — smaller buffer = faster USB transfer.
    // Re-using m_readBuf avoids per-call heap allocation.
    constexpr long kMaxResponse = 512;
    m_readBuf.resize(static_cast<size_t>(kMaxResponse));

    ::ibrd(m_ud, m_readBuf.data(), kMaxResponse);
    if (ibsta & ERR)
    {
        FillError(err, L"ibrd ReadBinaryBlock");
        return false;
    }
    long total = ibcntl;

    // Parse IEEE 488.2 block header from the received bytes
    const uint8_t* p = m_readBuf.data();
    if (total < 2 || p[0] != '#')
    {
        err.message = L"Binary block does not start with '#'";
        LOG_ERR("GPIB", L"Binary block header[0]=0x%02X total=%ld", p[0], total);
        return false;
    }

    int nDigits = p[1] - '0';
    if (nDigits <= 0 || nDigits > 9 || total < 2 + nDigits)
    {
        err.message = L"Invalid binary block digit count";
        return false;
    }

    char lenBuf[11]{};
    std::memcpy(lenBuf, p + 2, static_cast<size_t>(nDigits));
    long dataLen = std::atol(lenBuf);
    if (dataLen <= 0)
    {
        err.message = L"Binary block data length is zero";
        return false;
    }

    const uint8_t* dataStart = p + 2 + nDigits;
    long available = total - 2 - nDigits;
    long toCopy = (available >= dataLen) ? dataLen : available;

    data.assign(dataStart, dataStart + toCopy);

    LOG_DBG("GPIB", L"Binary block read: %ld bytes (1 ibrd, total=%ld)", toCopy, total);
    return true;
}

bool GpibDevice::Clear(GpibError& err)
{
    if (m_ud < 0) { err.message = L"Device not open"; return false; }
    ::ibclr(m_ud);
    if (ibsta & ERR) { FillError(err, L"ibclr"); return false; }
    return true;
}

void GpibDevice::DrainInput()
{
    if (m_ud < 0) return;
    // Temporarily set a very short timeout (T30ms=8) to drain quickly without long blocking
    // IbcTMO = 3 (ibconfig option for timeout)
    ::ibconfig(m_ud, IbcTMO, T30ms);
    char buf[4096];
    for (int i = 0; i < 8; ++i)
    {
        ::ibrd(m_ud, buf, sizeof(buf));
        if (ibsta & ERR) break;
        if (ibcnt < static_cast<long>(sizeof(buf))) break;
    }
    // Restore original timeout
    ::ibconfig(m_ud, IbcTMO, m_info.timeoutCode);
}

bool GpibDevice::SerialPoll(uint8_t& statusByte, GpibError& err)
{
    if (m_ud < 0) { err.message = L"Device not open"; return false; }
    char spb = 0;
    ::ibrsp(m_ud, &spb);
    if (ibsta & ERR) { FillError(err, L"ibrsp"); return false; }
    statusByte = static_cast<uint8_t>(spb);
    return true;
}

bool GpibDevice::SetTimeout(int tCode, GpibError& err)
{
    if (m_ud < 0) { err.message = L"Device not open"; return false; }
    ::ibconfig(m_ud, IbcTMO, tCode);
    if (ibsta & ERR) { FillError(err, L"ibconfig(IbcTMO)"); return false; }
    return true;
}

void GpibDevice::FillError(GpibError& err, const wchar_t* context) const
{
    err.ibsta  = ibsta;
    err.iberr  = iberr;
    err.ibcntl = ibcntl;
    err.message = std::wstring(context ? context : L"") + L": " +
                  GpibError::ErrorCodeToString(iberr);
}

/*static*/ std::wstring GpibError::ErrorCodeToString(int code)
{
    switch (code)
    {
    case GPIB_EDVR: return L"System error (EDVR)";
    case GPIB_ECIC: return L"Not CIC (ECIC)";
    case GPIB_ENOL: return L"No Listeners (ENOL)";
    case GPIB_EADR: return L"Addressing error (EADR)";
    case GPIB_EARG: return L"Invalid argument (EARG)";
    case GPIB_ESAC: return L"Not System Controller (ESAC)";
    case GPIB_EABO: return L"I/O timeout (EABO)";
    case GPIB_ENEB: return L"Non-existent board (ENEB)";
    case GPIB_EDMA: return L"DMA error (EDMA)";
    case GPIB_EOIP: return L"I/O in progress (EOIP)";
    case GPIB_ECAP: return L"No capability (ECAP)";
    case GPIB_EFSO: return L"File system error (EFSO)";
    case GPIB_EBUS: return L"Bus error (EBUS)";
    case GPIB_ESTB: return L"Status byte lost (ESTB)";
    case GPIB_ESRQ: return L"SRQ stuck ON (ESRQ)";
    case GPIB_ETAB: return L"Table problem (ETAB)";
    default:        return L"Unknown GPIB error (" + std::to_wstring(code) + L")";
    }
}
