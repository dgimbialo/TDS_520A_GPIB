// Scope/TektronixScope.h - High-level TDS 520A instrument abstraction
#pragma once
#include "pch.h"
#include "GpibController.h"
#include "ScpiCommand.h"
#include "WaveformPreamble.h"
#include "WaveformBuffer.h"
#include "ScopeChannel.h"
#include <cstring>

// Helper to store/load double via atomic int64 using memcpy (no UB, C++17 safe).
inline void AtomicStoreDouble(std::atomic<int64_t>& a, double v)
{
    int64_t bits;
    static_assert(sizeof(bits) == sizeof(v), "");
    std::memcpy(&bits, &v, sizeof(v));
    a.store(bits, std::memory_order_relaxed);
}
inline double AtomicLoadDouble(const std::atomic<int64_t>& a)
{
    int64_t bits = a.load(std::memory_order_relaxed);
    double v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

enum class ScopeState
{
    Disconnected,
    Connecting,
    Connected,
    Acquiring,
    Error
};

struct ScopeStatus
{
    ScopeState  state{ ScopeState::Disconnected };
    std::wstring idn;
    std::wstring lastError;
    ScopeChannel activeChannel{ ScopeChannel::CH1 };
    double       timeDiv{ 1e-3 };   // seconds/div
    double       voltDiv[4]{};      // volts/div per channel
    std::wstring triggerState;
    uint32_t     acquisitionsPerSec{ 0 };
};

class TektronixScope
{
public:
    TektronixScope();
    ~TektronixScope();

    // ---- Connection ----
    // Auto-find and open: scans board for Tektronix IDN
    bool AutoConnect(int gpibBoard, GpibError& err);
    bool Connect(const GpibDeviceInfo& info, GpibError& err);
    void Disconnect();
    bool IsConnected() const;

    // ---- Identification ----
    bool Identify(std::string& idn, GpibError& err);

    // ---- Acquisition control ----
    bool StartContinuous(GpibError& err);
    bool Stop(GpibError& err);
    bool AcquireSingle(GpibError& err);

    // ---- Channel ----
    bool SetChannel(ScopeChannel ch, GpibError& err);
    ScopeChannel GetChannel() const { return m_channel; }

    // ---- Waveform fetch ----
    // Fetches one waveform from active channel.
    // Fills preamble + raw binary data into WaveformBuffer.
    bool FetchWaveform(WaveformBuffer& outBuf, GpibError& err);

    // ---- Settings queries ----
    bool QueryHorizontalScale(double& secPerDiv, GpibError& err);
    bool QueryChannelScale(ScopeChannel ch, double& voltsPerDiv, GpibError& err);
    bool QueryTriggerState(std::wstring& state, GpibError& err);
    bool QueryTriggerLevel(double& level, GpibError& err);

    // ---- Settings writes ----
    bool SetHorizontalScale(double secPerDiv, GpibError& err);
    bool SetChannelScale(ScopeChannel ch, double voltsPerDiv, GpibError& err);
    bool AdjustTriggerLevel(double deltaVolts, GpibError& err);

    // ---- Instrument recovery ----
    bool Reset(GpibError& err);
    bool ClearStatus(GpibError& err);

    ScopeStatus GetStatus() const;
    const std::wstring& GetIdn() const { return m_idn; }
    GpibDevice* GetDevice() { return m_controller.GetDevice(); }
    const GpibDeviceInfo& GetDeviceInfo() const { return m_deviceInfo; }

    // Cached display parameters updated from the waveform preamble after each
    // successful FetchWaveform().  Safe to read from any thread without GPIB I/O.
    struct DisplayParams
    {
        double secPerDiv   { 0.0 };
        double voltsPerDiv { 0.0 };
    };
    DisplayParams GetCachedDisplayParams() const
    {
        DisplayParams p;
        p.secPerDiv   = AtomicLoadDouble(m_cachedSecPerDiv);
        p.voltsPerDiv = AtomicLoadDouble(m_cachedVoltsPerDiv);
        return p;
    }
    // Seed cached display params immediately after connect (called from App).
    void SetCachedDisplayParams(double secPerDiv, double voltsPerDiv)
    {
        AtomicStoreDouble(m_cachedSecPerDiv,   secPerDiv);
        AtomicStoreDouble(m_cachedVoltsPerDiv, voltsPerDiv);
    }
    // Force preamble re-read on next FetchWaveform (call after changing settings).
    void InvalidatePreambleCache() { m_preambleValid = false; }

private:
    bool SetupDataEncoding(GpibError& err);
    bool WaitForOPC(int timeoutMs, GpibError& err);

    GpibController  m_controller;
    GpibDeviceInfo  m_deviceInfo;
    ScopeChannel    m_channel{ ScopeChannel::CH1 };
    std::wstring    m_idn;
    mutable std::mutex m_mutex;

    // For acquisitions/sec counter
    std::atomic<uint32_t> m_acqCount{ 0 };
    DWORD                 m_acqCountResetTick{ 0 };
    std::atomic<uint32_t> m_acqRate{ 0 };

    // Cached display values — written only by AcquisitionThread, read by UI.
    // Stored as integer (scaled by 1e9) to allow std::atomic.
    std::atomic<int64_t> m_cachedSecPerDiv  { 0 };
    std::atomic<int64_t> m_cachedVoltsPerDiv{ 0 };

    // Preamble cache: WFMPRE? is only re-fetched when settings change.
    // Invalidated by SetChannel / SetHorizontalScale / SetChannelScale.
    WaveformPreamble   m_preambleCache;
    bool               m_preambleValid{ false };
};
