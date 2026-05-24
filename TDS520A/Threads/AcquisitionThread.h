// Threads/AcquisitionThread.h - Background thread that fetches waveforms
// from the oscilloscope and places decoded results in the ring buffer.
#pragma once
#include "pch.h"
#include "TektronixScope.h"
#include "WaveformDecoder.h"
#include "RingBuffer.h"
#include <queue>
#include <mutex>

enum class AcqThreadState
{
    Idle,
    Running,
    Stopping,
    Stopped,
    Error
};

struct AcqThreadStats
{
    uint32_t acquisitionsTotal{ 0 };
    uint32_t acquisitionsPerSec{ 0 };
    uint32_t gpibErrors{ 0 };
    uint32_t reconnects{ 0 };
    std::wstring lastError;
};

// Commands posted from UI/HttpServer threads and executed inside the GPIB thread.
enum class ScopeCommand
{
    SetChannel1,
    SetChannel2,
    SetChannel3,
    SetChannel4,
    TimeDivUp,
    TimeDivDown,
    VoltDivUp,
    VoltDivDown,
    TrigLevelUp,
    TrigLevelDown,
    AcquireSingle,
    Run,
};

class AcquisitionThread
{
public:
    explicit AcquisitionThread(WaveformRingBuffer& ringBuf);
    ~AcquisitionThread();

    // Attach to an already-connected scope
    void SetScope(TektronixScope* scope) { m_scope = scope; }

    // Start/stop the acquisition loop
    bool Start(ScopeChannel channel = ScopeChannel::CH1);
    void Stop();                     // Request stop
    void WaitForStop(DWORD timeoutMs = 5000);

    AcqThreadState GetState() const { return m_state.load(); }
    AcqThreadStats GetStats() const;

    // Thread-safe: post a command to be executed inside the GPIB thread.
    // Never call scope methods directly from UI or HttpServer threads.
    void PostCommand(ScopeCommand cmd)
    {
        std::lock_guard<std::mutex> lk(m_cmdMutex);
        m_cmdQueue.push(cmd);
    }

    // Keep legacy helpers as thin wrappers over PostCommand.
    void RequestChannelChange(ScopeChannel ch)
    {
        switch (ch)
        {
        case ScopeChannel::CH1: PostCommand(ScopeCommand::SetChannel1); break;
        case ScopeChannel::CH2: PostCommand(ScopeCommand::SetChannel2); break;
        case ScopeChannel::CH3: PostCommand(ScopeCommand::SetChannel3); break;
        case ScopeChannel::CH4: PostCommand(ScopeCommand::SetChannel4); break;
        }
    }
    void RequestSingle() { PostCommand(ScopeCommand::AcquireSingle); }
    void RequestRun()    { PostCommand(ScopeCommand::Run); }

    // Limit acquisition rate (0 = as fast as possible)
    void SetTargetRate(uint32_t acqPerSec)
    {
        m_targetIntervalMs = (acqPerSec > 0)
            ? static_cast<uint32_t>(1000u / acqPerSec)
            : 0u;
    }
    uint32_t GetTargetRate() const
    {
        uint32_t ms = m_targetIntervalMs.load();
        return (ms > 0) ? 1000u / ms : 0u;
    }

    // Callback invoked on each successful acquisition (optional, called from acq thread)
    using AcqCallback = std::function<void(const DecodedWaveform&)>;
    void SetCallback(AcqCallback cb) { m_callback = std::move(cb); }

private:
    void ThreadProc();
    bool DoAcquisitionCycle();
    void DrainCommandQueue();   // execute all pending commands inside GPIB thread
    void HandleError(const GpibError& err);

    TektronixScope*           m_scope{ nullptr };
    WaveformRingBuffer&       m_ringBuf;
    WaveformDecoder           m_decoder;

    std::thread               m_thread;
    std::atomic<AcqThreadState> m_state{ AcqThreadState::Stopped };
    std::atomic<bool>         m_stopRequest{ false };

    // Command queue — written by any thread, drained exclusively inside GPIB thread
    std::mutex                m_cmdMutex;
    std::queue<ScopeCommand>  m_cmdQueue;

    std::atomic<uint32_t>     m_targetIntervalMs{ 500 };

    AcqCallback               m_callback;

    // Stats
    mutable std::mutex        m_statsMutex;
    AcqThreadStats            m_stats;
    uint32_t                  m_acqCountInWindow{ 0 };
    DWORD                     m_windowStart{ 0 };

    static constexpr int kMaxConsecutiveErrors = 15;
    int  m_consecutiveErrors{ 0 };
    void AttemptReconnect();

    uint32_t m_seqNum{ 0 };
};
