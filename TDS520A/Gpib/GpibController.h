// Gpib/GpibController.h - Board-level GPIB controller; auto-discovers devices
#pragma once
#include "pch.h"
#include "GpibDevice.h"

struct FoundDevice
{
    int boardIndex;
    int primaryAddr;
    std::string idn;   // *IDN? response
};

// Progress callback: (currentAddr, totalAddrs, foundDevice or nullptr)
using ScanProgressCallback = std::function<void(int addr, int total, const FoundDevice*)>;

class GpibController
{
public:
    GpibController();
    ~GpibController();

    // Find all responding devices on a GPIB board (primary addr 1-30)
    // boardIndex: 0 = first GPIB-USB adapter
    std::vector<FoundDevice> FindDevices(int boardIndex = 0);

    // Same scan with per-address progress callback. Pass stopFlag ptr to cancel early.
    // startAddr lets the user skip addresses below a known device to speed up scanning.
    std::vector<FoundDevice> FindDevices(int boardIndex,
                                         const ScanProgressCallback& progress,
                                         const std::atomic<bool>* stopFlag = nullptr,
                                         int startAddr = 1);

    // Find device matching a specific IDN substring (e.g. "TDS 520")
    bool FindTektronixScope(int boardIndex, int& primaryAddrOut);

    // Open a specific device
    bool OpenDevice(const GpibDeviceInfo& info, GpibError& err);
    void CloseDevice();

    GpibDevice* GetDevice() { return m_device.get(); }
    bool IsDeviceOpen() const { return m_device && m_device->IsOpen(); }

private:
    std::unique_ptr<GpibDevice> m_device;
};
