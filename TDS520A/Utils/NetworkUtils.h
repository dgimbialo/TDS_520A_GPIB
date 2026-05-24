// Utils/NetworkUtils.h
#pragma once
#include "pch.h"

namespace NetworkUtils
{
    // Returns all local IPv4 addresses (excluding loopback)
    std::vector<std::string> GetLocalIPv4Addresses();

    // Returns first non-loopback IPv4 as string, or "127.0.0.1"
    std::string GetPrimaryLocalIP();

    // Initialize / cleanup Winsock (call once at app startup/shutdown)
    bool InitWinsock();
    void CleanupWinsock();

    // Set socket non-blocking
    bool SetNonBlocking(SOCKET s, bool nonBlocking);

    // Set TCP_NODELAY
    bool SetNoDelay(SOCKET s, bool noDelay);

    // Set SO_REUSEADDR
    bool SetReuseAddr(SOCKET s, bool reuse);

    // Safe send (handles partial sends)
    bool SendAll(SOCKET s, const char* data, int len);
}
