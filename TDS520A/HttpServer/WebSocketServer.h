// HttpServer/WebSocketServer.h - WebSocket upgrade and framing
// Implements RFC 6455 server-side handshake and framing (text frames only).
#pragma once
#include "pch.h"
#include "HttpRequestParser.h"

// State of a WebSocket connection
enum class WsState { Handshake, Open, Closed };

class WebSocketConnection
{
public:
    explicit WebSocketConnection(SOCKET s);
    ~WebSocketConnection();

    // Perform the upgrade handshake from an HTTP request
    bool Upgrade(const HttpRequest& req);

    // Send a text frame
    bool SendText(const std::string& payload);

    // Send a binary frame
    bool SendBinary(const void* data, size_t len);

    // Receive frames (call in a loop; returns payload or empty on no data)
    // Returns false if connection closed.
    bool Receive(std::string& outPayload);

    // Send close frame and close socket
    void Close();

    WsState GetState() const { return m_state; }
    SOCKET  GetSocket() const { return m_socket; }

private:
    bool SendFrame(uint8_t opcode, const void* data, size_t len);

    SOCKET   m_socket{ INVALID_SOCKET };
    WsState  m_state{ WsState::Handshake };
    std::string m_recvBuf;
    mutable std::mutex m_sendMutex;  // serialises concurrent sends from multiple threads
};
