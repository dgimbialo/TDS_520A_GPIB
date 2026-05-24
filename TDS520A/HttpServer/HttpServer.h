// HttpServer/HttpServer.h - Embedded HTTP + WebSocket server
// Runs in its own thread pool. Non-blocking accept loop.
// Serves the Web UI and pushes waveform data to WebSocket clients.
#pragma once
#include "pch.h"
#include "WebSocketServer.h"
#include "WaveformSample.h"
#include "ScopeChannel.h"
#include "TektronixScope.h"

// Commands received from web clients
struct WebCommand
{
    enum class Type { Channel, Run, Stop, Single };
    Type         type;
    ScopeChannel channel{ ScopeChannel::CH1 };
};

// Callback for web commands -> main app (called from a dedicated command thread)
using WebCommandCallback = std::function<void(const WebCommand&)>;

class HttpServer
{
public:
    HttpServer();
    ~HttpServer();

    // Configure before starting
    void SetPort(uint16_t port) { m_port = port; }
    void SetScope(TektronixScope* scope) { m_scope = scope; }
    void SetCommandCallback(WebCommandCallback cb) { m_cmdCallback = std::move(cb); }

    // Start listening (creates threads)
    bool Start();
    void Stop();
    bool IsRunning() const { return m_running.load(); }

    uint16_t GetPort() const { return m_port; }

    // Called from acquisition thread: stores latest waveform in shared slot.
    // Returns immediately – no socket I/O on the caller's thread.
    void PostWaveform(const DecodedWaveform& wave);

    // Called once after scope connects to cache the IDN string for status JSON.
    void SetCachedIdn(const std::string& idn)
    {
        std::lock_guard<std::mutex> sl(m_statusMutex);
        m_cachedIdn = idn;
    }

    // Called from AcquisitionThread stats update to keep acq/s current.
    void SetCachedAcqRate(uint32_t rate)
    {
        std::lock_guard<std::mutex> sl(m_statusMutex);
        m_cachedAcqRate = rate;
    }

    void BroadcastStatus();

    int GetClientCount() const;

private:
    void AcceptLoop();
    void ClientThread(SOCKET clientSock);
    void HandleHttpRequest(SOCKET s, const HttpRequest& req);
    void HandleWebSocketClient(SOCKET s, const HttpRequest& upgradeReq);
    void ProcessWsCommand(const std::string& json);   // enqueues to command thread
    void CommandDispatchLoop();                        // dedicated command thread

    uint16_t             m_port{ 8080 };
    SOCKET               m_listenSock{ INVALID_SOCKET };
    std::atomic<bool>    m_running{ false };
    std::thread          m_acceptThread;

    // Active WebSocket connections – only held briefly to snapshot the list
    mutable std::mutex   m_wsMutex;
    std::vector<std::shared_ptr<WebSocketConnection>> m_wsClients;

    TektronixScope*      m_scope{ nullptr };
    WebCommandCallback   m_cmdCallback;

    // Latest waveform JSON for new connections (updated atomically)
    std::mutex           m_waveMutex;
    std::string          m_latestWaveJson;
    bool                 m_hasWave{ false };

    // Cached status – updated from BroadcastWaveform (no GPIB calls in HTTP threads)
    std::mutex           m_statusMutex;
    std::string          m_cachedIdn;
    ScopeChannel         m_cachedChannel{ ScopeChannel::CH1 };
    double               m_cachedVoltsPerDiv{ 0.0 };
    double               m_cachedSecPerDiv{ 0.0 };
    uint32_t             m_cachedAcqRate{ 0 };
    bool                 m_cachedConnected{ false };

    // Command queue – WS threads enqueue; CommandDispatchLoop dequeues and executes
    std::mutex              m_cmdQueueMutex;
    std::condition_variable m_cmdQueueCv;
    std::queue<WebCommand>  m_cmdQueue;
    std::thread             m_cmdThread;

    // Pending waveform slot – written by PostWaveform, read by BroadcastLoop
    std::mutex              m_pendingMutex;
    std::condition_variable m_pendingCv;
    DecodedWaveform         m_pendingWave;
    uint32_t                m_pendingSeq{ 0 };
    uint32_t                m_broadcastedSeq{ 0 };
    std::thread             m_broadcastThread;

    void BroadcastLoop();   // dedicated thread: reads pending slot, sends to WS clients
    void SendToAllClients(const std::string& json);  // snapshot + send outside lock
};
