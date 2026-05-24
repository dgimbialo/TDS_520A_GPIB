// HttpServer/WebSocketServer.cpp
#include "pch.h"
#include "WebSocketServer.h"
#include "Sha1.h"
#include "Base64.h"
#include "NetworkUtils.h"
#include "Logger.h"

static const char* kWsGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

WebSocketConnection::WebSocketConnection(SOCKET s) : m_socket(s) {}

WebSocketConnection::~WebSocketConnection()
{
    Close();
}

bool WebSocketConnection::Upgrade(const HttpRequest& req)
{
    if (!req.isWebSocketUpgrade || req.wsKey.empty())
        return false;

    // Compute accept key: Base64(SHA1(key + GUID))
    std::string combined = req.wsKey + kWsGuid;
    auto digest = Sha1Digest(combined.data(), combined.size());
    std::string acceptKey = Base64::Encode(digest.data(), digest.size());

    // Build 101 response
    std::string resp =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + acceptKey + "\r\n"
        "\r\n";

    if (!NetworkUtils::SendAll(m_socket, resp.c_str(), static_cast<int>(resp.size())))
    {
        LOG_ERR("WS", L"Failed to send upgrade response");
        return false;
    }

    m_state = WsState::Open;
    LOG_DBG("WS", L"WebSocket upgraded on socket %llu", (uint64_t)m_socket);
    return true;
}

bool WebSocketConnection::SendText(const std::string& payload)
{
    if (m_state != WsState::Open) return false;
    return SendFrame(0x01, payload.data(), payload.size());  // opcode 1 = text
}

bool WebSocketConnection::SendBinary(const void* data, size_t len)
{
    if (m_state != WsState::Open) return false;
    return SendFrame(0x02, data, len);  // opcode 2 = binary
}

bool WebSocketConnection::SendFrame(uint8_t opcode, const void* data, size_t len)
{
    std::lock_guard<std::mutex> lock(m_sendMutex);
    if (m_state != WsState::Open) return false;
    // RFC 6455 frame: FIN=1, RSV=0, opcode, no mask (server->client unmasked)
    uint8_t header[10];
    size_t headerLen = 0;

    header[headerLen++] = 0x80 | (opcode & 0x0F);  // FIN + opcode

    if (len < 126)
    {
        header[headerLen++] = static_cast<uint8_t>(len);
    }
    else if (len < 65536)
    {
        header[headerLen++] = 126;
        header[headerLen++] = static_cast<uint8_t>((len >> 8) & 0xFF);
        header[headerLen++] = static_cast<uint8_t>(len & 0xFF);
    }
    else
    {
        header[headerLen++] = 127;
        for (int i = 7; i >= 0; --i)
            header[headerLen++] = static_cast<uint8_t>((len >> (i * 8)) & 0xFF);
    }

    if (!NetworkUtils::SendAll(m_socket, reinterpret_cast<const char*>(header),
                               static_cast<int>(headerLen)))
        return false;

    if (len > 0 && !NetworkUtils::SendAll(m_socket,
                                           static_cast<const char*>(data),
                                           static_cast<int>(len)))
        return false;

    return true;
}

bool WebSocketConnection::Receive(std::string& outPayload)
{
    if (m_state != WsState::Open) return false;

    // Non-blocking read
    char tmp[4096];
    int r = ::recv(m_socket, tmp, sizeof(tmp), 0);
    if (r == 0) { m_state = WsState::Closed; return false; }
    if (r == SOCKET_ERROR)
    {
        int err = ::WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT) return true;  // No data yet
        m_state = WsState::Closed;
        return false;
    }

    m_recvBuf.append(tmp, static_cast<size_t>(r));

    // Parse frames
    while (m_recvBuf.size() >= 2)
    {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(m_recvBuf.data());
        uint8_t opcode = b[0] & 0x0F;
        bool masked    = (b[1] & 0x80) != 0;
        size_t payLen  = b[1] & 0x7F;
        size_t offset  = 2;

        if (payLen == 126)
        {
            if (m_recvBuf.size() < 4) break;
            payLen = (static_cast<size_t>(b[2]) << 8) | b[3];
            offset = 4;
        }
        else if (payLen == 127)
        {
            if (m_recvBuf.size() < 10) break;
            payLen = 0;
            for (int i = 0; i < 8; ++i)
                payLen = (payLen << 8) | b[2 + i];
            offset = 10;
        }

        size_t maskOffset = masked ? offset : offset;
        if (masked) offset += 4;

        if (m_recvBuf.size() < offset + payLen) break;  // Partial frame

        if (opcode == 0x08)  // Close frame
        {
            m_state = WsState::Closed;
            return false;
        }

        if (opcode == 0x01 || opcode == 0x02)  // Text or binary
        {
            outPayload.resize(payLen);
            const uint8_t* maskKey = masked ?
                reinterpret_cast<const uint8_t*>(m_recvBuf.data()) + maskOffset : nullptr;
            const uint8_t* payload = b + offset;
            for (size_t i = 0; i < payLen; ++i)
                outPayload[i] = static_cast<char>(
                    masked ? (payload[i] ^ maskKey[i % 4]) : payload[i]);
        }
        // Consume frame
        m_recvBuf.erase(0, offset + payLen);
    }
    return true;
}

void WebSocketConnection::Close()
{
    if (m_state == WsState::Open)
    {
        // Send close frame
        uint8_t closeFrame[2] = { 0x88, 0x00 };
        ::send(m_socket, reinterpret_cast<const char*>(closeFrame), 2, 0);
        m_state = WsState::Closed;
    }
    if (m_socket != INVALID_SOCKET)
    {
        ::closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}
