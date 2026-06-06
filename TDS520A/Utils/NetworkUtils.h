// Utils/NetworkUtils.h
#pragma once
#include "pch.h"

namespace NetworkUtils
{
    // Returns all local IPv4 addresses (excluding loopback)
    std::vector<std::string> GetLocalIPv4Addresses();

    // Returns the preferred local IPv4 address for the web server.
    // Prefers 192.168.0.* / 192.168.1.* and falls back to any non-loopback address.
    std::string GetPreferredLocalIPv4Address();

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
