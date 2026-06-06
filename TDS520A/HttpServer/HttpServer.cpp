// HttpServer/HttpServer.cpp
#include "pch.h"
#include "HttpServer.h"
#include "HtmlGenerator.h"
#include "NetworkUtils.h"
#include "Logger.h"
#include "StringUtils.h"

HttpServer::HttpServer() = default;

HttpServer::~HttpServer()
{
    Stop();
}

bool HttpServer::Start()
{
    if (m_running) return true;

    m_listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSock == INVALID_SOCKET)
    {
        LOG_ERR("HTTP", L"socket() failed: %d", ::WSAGetLastError());
        return false;
    }

    NetworkUtils::SetReuseAddr(m_listenSock, true);

    std::string bindAddress = m_bindAddress.empty()
        ? NetworkUtils::GetPreferredLocalIPv4Address()
        : m_bindAddress;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = ::htons(m_port);

    if (::inet_pton(AF_INET, bindAddress.c_str(), &addr.sin_addr) != 1)
    {
        addr.sin_addr.s_addr = INADDR_ANY;
        LOG_WRN("HTTP", L"Preferred bind address %S is invalid, falling back to INADDR_ANY",
            StringUtils::Utf8ToWide(bindAddress).c_str());
    }

    if (::bind(m_listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        LOG_ERR("HTTP", L"bind() failed on port %u: %d", m_port, ::WSAGetLastError());
        ::closesocket(m_listenSock);
        m_listenSock = INVALID_SOCKET;
        return false;
    }

    if (::listen(m_listenSock, SOMAXCONN) != 0)
    {
        LOG_ERR("HTTP", L"listen() failed: %d", ::WSAGetLastError());
        ::closesocket(m_listenSock);
        m_listenSock = INVALID_SOCKET;
        return false;
    }

    m_running = true;
    m_acceptThread    = std::thread(&HttpServer::AcceptLoop,       this);
    m_cmdThread       = std::thread(&HttpServer::CommandDispatchLoop, this);
    m_broadcastThread = std::thread(&HttpServer::BroadcastLoop,    this);
    LOG_INF("HTTP", L"HTTP server started on port %u", m_port);
    return true;
}

void HttpServer::Stop()
{
    if (!m_running) return;
    m_running = false;

    if (m_listenSock != INVALID_SOCKET)
    {
        ::closesocket(m_listenSock);
        m_listenSock = INVALID_SOCKET;
    }

    // Close all WebSocket clients
    {
        std::lock_guard<std::mutex> lock(m_wsMutex);
        m_wsClients.clear();
    }

    // Wake command dispatch thread and broadcast thread so they can exit
    m_cmdQueueCv.notify_all();
    m_pendingCv.notify_all();

    if (m_acceptThread.joinable())    m_acceptThread.join();
    if (m_cmdThread.joinable())       m_cmdThread.join();
    if (m_broadcastThread.joinable()) m_broadcastThread.join();

    LOG_INF("HTTP", L"HTTP server stopped");
}

void HttpServer::AcceptLoop()
{
    // Use select() with a short timeout so we can check m_running
    while (m_running)
    {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(m_listenSock, &readSet);

        timeval tv{ 0, 200000 };  // 200 ms
        int sel = ::select(0, &readSet, nullptr, nullptr, &tv);
        if (sel <= 0) continue;

        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSock = ::accept(m_listenSock,
            reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
        if (clientSock == INVALID_SOCKET) continue;

        NetworkUtils::SetNoDelay(clientSock, true);
        char addrBuf[INET_ADDRSTRLEN]{};
        ::inet_ntop(AF_INET, &clientAddr.sin_addr, addrBuf, sizeof(addrBuf));
        LOG_DBG("HTTP", L"New connection from %S:%u",
            addrBuf, ::ntohs(clientAddr.sin_port));

        // Each client gets its own thread.
        // For a production system, a thread pool would be preferable.
        std::thread([this, clientSock]() { ClientThread(clientSock); }).detach();
    }
}

void HttpServer::ClientThread(SOCKET clientSock)
{
    // Set receive timeout
    DWORD timeout = 5000;
    ::setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    HttpRequestParser parser;
    char buf[4096];
    bool upgraded = false;

    while (m_running && !upgraded)
    {
        int r = ::recv(clientSock, buf, sizeof(buf), 0);
        if (r <= 0) break;

        if (parser.Feed(buf, r))
        {
            HttpRequest req = parser.TakeRequest();
            if (!req.IsValid())
            {
                ::closesocket(clientSock);
                return;
            }

            if (req.isWebSocketUpgrade)
            {
                HandleWebSocketClient(clientSock, req);
                upgraded = true;
            }
            else
            {
                HandleHttpRequest(clientSock, req);
            }
        }
    }

    if (!upgraded)
        ::closesocket(clientSock);
}

void HttpServer::HandleHttpRequest(SOCKET s, const HttpRequest& req)
{
    HttpResponse resp;
    std::string localIp = NetworkUtils::GetPrimaryLocalIP();

    if (req.path == "/" || req.path == "/index.html")
    {
        std::string html = HtmlGen::MainPage(localIp, m_port);
        resp.statusCode = 200;
        resp.statusText = "OK";
        resp.SetBody(html, "text/html; charset=utf-8");
    }
    else if (req.path == "/status")
    {
        std::string json;
        {
            std::lock_guard<std::mutex> sl(m_statusMutex);
            json = HtmlGen::StatusJson(m_cachedIdn, m_cachedChannel,
                m_cachedVoltsPerDiv, m_cachedSecPerDiv,
                m_cachedAcqRate, m_cachedConnected);
        }
        resp.SetBody(json, "application/json");
        resp.headers["Access-Control-Allow-Origin"] = "*";
    }
    else
    {
        resp.statusCode = 404;
        resp.statusText = "Not Found";
        resp.SetBody("404 Not Found");
    }

    resp.headers["Connection"] = "close";
    resp.headers["Server"]     = "TDS520A/1.0";

    std::string raw = resp.Serialize();
    NetworkUtils::SendAll(s, raw.c_str(), static_cast<int>(raw.size()));
    ::closesocket(s);
}

void HttpServer::HandleWebSocketClient(SOCKET s, const HttpRequest& upgradeReq)
{
    auto wsConn = std::make_shared<WebSocketConnection>(s);
    if (!wsConn->Upgrade(upgradeReq))
    {
        ::closesocket(s);
        return;
    }

    // Send latest waveform immediately so client gets data right away
    {
        std::lock_guard<std::mutex> lock(m_waveMutex);
        if (m_hasWave)
            wsConn->SendText(m_latestWaveJson);
    }

    // Send cached status (no GPIB calls in the HTTP thread)
    {
        std::lock_guard<std::mutex> sl(m_statusMutex);
        std::string sj = HtmlGen::StatusJson(m_cachedIdn, m_cachedChannel,
            m_cachedVoltsPerDiv, m_cachedSecPerDiv,
            m_cachedAcqRate, m_cachedConnected);
        wsConn->SendText(sj);
    }

    // Register client
    {
        std::lock_guard<std::mutex> lock(m_wsMutex);
        m_wsClients.push_back(wsConn);
    }

    // Use a short receive timeout so the loop stays responsive without spinning
    // (do NOT set non-blocking: BroadcastLoop sends on this same socket)
    DWORD rcvTimeout = 50;  // ms
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&rcvTimeout), sizeof(rcvTimeout));

    // Receive loop for commands
    while (m_running && wsConn->GetState() == WsState::Open)
    {
        std::string payload;
        if (!wsConn->Receive(payload))
            break;
        if (!payload.empty())
            ProcessWsCommand(payload);
    }

    // Unregister
    {
        std::lock_guard<std::mutex> lock(m_wsMutex);
        m_wsClients.erase(
            std::remove(m_wsClients.begin(), m_wsClients.end(), wsConn),
            m_wsClients.end());
    }
    LOG_DBG("HTTP", L"WebSocket client disconnected");
}

void HttpServer::ProcessWsCommand(const std::string& json)
{
    if (!m_cmdCallback) return;

    auto findVal = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":";
        auto pos = json.find(search);
        if (pos == std::string::npos) return {};
        pos += search.size();
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '"')) ++pos;
        size_t end = json.find_first_of(",}", pos);
        if (end == std::string::npos) end = json.size();
        std::string val = json.substr(pos, end - pos);
        while (!val.empty() && (val.back() == '"' || val.back() == ' ')) val.pop_back();
        return val;
    };

    std::string cmd = findVal("cmd");
    WebCommand wc;

    if (cmd == "channel")
    {
        wc.type = WebCommand::Type::Channel;
        std::string chStr = findVal("ch");
        int ch = 1;
        StringUtils::ParseInt(chStr, ch);
        if (ch < 1 || ch > 4) ch = 1;
        wc.channel = static_cast<ScopeChannel>(ch);
    }
    else if (cmd == "run")    wc.type = WebCommand::Type::Run;
    else if (cmd == "stop")   wc.type = WebCommand::Type::Stop;
    else if (cmd == "single") wc.type = WebCommand::Type::Single;
    else return;

    // Enqueue � never blocks the WS thread
    {
        std::lock_guard<std::mutex> lock(m_cmdQueueMutex);
        m_cmdQueue.push(wc);
    }
    m_cmdQueueCv.notify_one();
}

void HttpServer::CommandDispatchLoop()
{
    while (m_running)
    {
        WebCommand wc{};
        {
            std::unique_lock<std::mutex> lock(m_cmdQueueMutex);
            m_cmdQueueCv.wait_for(lock, std::chrono::milliseconds(200),
                [this]{ return !m_cmdQueue.empty() || !m_running; });
            if (m_cmdQueue.empty()) continue;
            wc = m_cmdQueue.front();
            m_cmdQueue.pop();
        }
        if (m_cmdCallback)
            m_cmdCallback(wc);
    }
}

// ?? Called from acquisition thread: store latest wave, wake broadcast loop ??
void HttpServer::PostWaveform(const DecodedWaveform& wave)
{
    // Update cached status fields from the waveform itself � no scope calls needed.
    {
        std::lock_guard<std::mutex> sl(m_statusMutex);
        m_cachedChannel     = wave.Channel;
        m_cachedVoltsPerDiv = wave.VoltsPerDiv;
        m_cachedSecPerDiv   = wave.SecPerDiv;
        m_cachedAcqRate     = 0;  // updated via SetCachedAcqRate() by AcquisitionThread
        m_cachedConnected   = true;
    }

    // Store waveform in pending slot
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingWave = wave;
        m_pendingSeq  = wave.SequenceNumber;
    }
    m_pendingCv.notify_one();  // wake BroadcastLoop
}

// ?? Dedicated thread: picks up pending waveforms, sends to all WS clients ??
void HttpServer::BroadcastLoop()
{
    uint32_t lastSent = 0;
    while (m_running)
    {
        DecodedWaveform wave;
        uint32_t seq = 0;
        {
            std::unique_lock<std::mutex> lock(m_pendingMutex);
            m_pendingCv.wait_for(lock, std::chrono::milliseconds(200),
                [&]{ return (m_pendingSeq != lastSent) || !m_running; });
            if (!m_running) break;
            if (m_pendingSeq == lastSent) continue;  // spurious wake
            wave = m_pendingWave;
            seq  = m_pendingSeq;
        }
        lastSent = seq;

        // Build JSON outside any lock
        std::string json = HtmlGen::WaveformJson(wave);

        // Update wave cache for new WS connections
        {
            std::lock_guard<std::mutex> lock(m_waveMutex);
            m_latestWaveJson = json;
            m_hasWave        = true;
        }

        SendToAllClients(json);
    }
}

// ?? Snapshot client list then send outside lock ??
void HttpServer::SendToAllClients(const std::string& json)
{
    std::vector<std::shared_ptr<WebSocketConnection>> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_wsMutex);
        snapshot = m_wsClients;
    }
    for (auto& ws : snapshot)
    {
        if (ws && ws->GetState() == WsState::Open)
            ws->SendText(json);
    }
}

void HttpServer::BroadcastStatus()
{
    std::string sj;
    {
        std::lock_guard<std::mutex> sl(m_statusMutex);
        sj = HtmlGen::StatusJson(m_cachedIdn, m_cachedChannel,
            m_cachedVoltsPerDiv, m_cachedSecPerDiv,
            m_cachedAcqRate, m_cachedConnected);
    }
    SendToAllClients(sj);
}

int HttpServer::GetClientCount() const
{
    std::lock_guard<std::mutex> lock(m_wsMutex);
    return static_cast<int>(m_wsClients.size());
}