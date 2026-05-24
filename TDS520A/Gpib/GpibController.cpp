// Gpib/GpibController.cpp
#include "pch.h"
#include "GpibController.h"
#include "Logger.h"
#include "StringUtils.h"
#include <chrono>
#include <thread>

GpibController::GpibController() = default;
GpibController::~GpibController() { CloseDevice(); }

std::vector<FoundDevice> GpibController::FindDevices(int boardIndex)
{
    return FindDevices(boardIndex, nullptr, nullptr);
}

std::vector<FoundDevice> GpibController::FindDevices(int boardIndex,
                                                      const ScanProgressCallback& progress,
                                                      const std::atomic<bool>* stopFlag,
                                                      int startAddr)
{
    std::vector<FoundDevice> found;
    LOG_INF("GPIB", L"Scanning GPIB board %d for devices...", boardIndex);

    // Ensure the board controller is online before scanning.
    // NI numbers board interfaces by adapter index (e.g. the adapter may appear
    // as GPIB17, not GPIB0).  Try the requested index first, then fall back to
    // scanning GPIB0-GPIB31 so we always find the right one.
    {
        auto tryBoard = [&](int idx) -> bool
        {
            wchar_t name[16];
            ::swprintf_s(name, L"GPIB%d", idx);
            int bd = ::ibfind(name);
            if (bd < 0 || (ibsta & ERR)) return false;
            ::ibonl(bd, 1);
            ::ibsic(bd);
            ::Sleep(200);
            LOG_INF("GPIB", L"Board GPIB%d online (ibfind=%d, ibsic sent)", idx, bd);
            return true;
        };

        if (!tryBoard(boardIndex))
        {
            LOG_WRN("GPIB", L"ibfind(GPIB%d) failed -- scanning GPIB0-GPIB31 for any available board", boardIndex);
            bool anyFound = false;
            for (int bi = 0; bi <= 31; ++bi)
            {
                if (bi == boardIndex) continue;
                if (tryBoard(bi))
                {
                    LOG_WRN("GPIB", L"NOTE: board found at GPIB%d, not GPIB%d. "
                        L"Update the board index in the Connect dialog.", bi, boardIndex);
                    anyFound = true;
                    break;
                }
            }
            if (!anyFound)
                LOG_ERR("GPIB", L"No GPIB board found (GPIB0-GPIB31). Check NI MAX / Device Manager.");
        }
    }

    constexpr int kLastAddr  = 30;
    const int kFirstAddr = (startAddr >= 1 && startAddr <= kLastAddr) ? startAddr : 1;
    const int kTotal     = kLastAddr - kFirstAddr + 1;

    for (int addr = kFirstAddr; addr <= kLastAddr; ++addr)
    {
        if (stopFlag && stopFlag->load())
            break;

        if (progress)
            progress(addr, kTotal, nullptr);

        GpibDeviceInfo info;
        info.boardIndex   = boardIndex;
        info.primaryAddr  = addr;
        info.timeoutCode  = T1s;

        GpibDevice dev;
        GpibError  err;
        if (!dev.Open(info, err))
            continue;

        std::string response;
        if (dev.Query("*IDN?", response, err) && !response.empty())
        {
            LOG_INF("GPIB", L"Found device at addr %d: %S", addr, response.c_str());
            FoundDevice fd;
            fd.boardIndex  = boardIndex;
            fd.primaryAddr = addr;
            fd.idn         = response;
            found.push_back(fd);

            if (progress)
                progress(addr, kTotal, &fd);
        }
        dev.Close();

        ::Sleep(50);
    }

    LOG_INF("GPIB", L"Scan complete: %zu device(s) found", found.size());
    return found;
}

bool GpibController::FindTektronixScope(int boardIndex, int& primaryAddrOut)
{
    auto devices = FindDevices(boardIndex);
    for (const auto& d : devices)
    {
        // Tektronix IDN contains "TEKTRONIX" or instrument model
        std::string idnUpper = d.idn;
        for (auto& c : idnUpper) c = static_cast<char>(::toupper(c));
        if (idnUpper.find("TEKTRONIX") != std::string::npos ||
            idnUpper.find("TDS") != std::string::npos)
        {
            primaryAddrOut = d.primaryAddr;
            LOG_INF("GPIB", L"Tektronix scope found at addr %d: %S",
                d.primaryAddr, d.idn.c_str());
            return true;
        }
    }
    LOG_WRN("GPIB", L"No Tektronix scope found on board %d", boardIndex);
    return false;
}

bool GpibController::OpenDevice(const GpibDeviceInfo& info, GpibError& err)
{
    CloseDevice();
    m_device = std::make_unique<GpibDevice>();
    if (!m_device->Open(info, err))
    {
        m_device.reset();
        return false;
    }
    return true;
}

void GpibController::CloseDevice()
{
    if (m_device)
    {
        m_device->Close();
        m_device.reset();
    }
}
