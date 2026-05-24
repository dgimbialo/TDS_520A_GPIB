// Utils/NetworkUtils.cpp
#include "pch.h"
#include "NetworkUtils.h"

namespace NetworkUtils
{

bool InitWinsock()
{
    WSADATA wsaData{};
    int err = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    return (err == 0);
}

void CleanupWinsock()
{
    ::WSACleanup();
}

std::vector<std::string> GetLocalIPv4Addresses()
{
    std::vector<std::string> result;

    ULONG bufLen = 15000;
    std::vector<BYTE> buf(bufLen);
    PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG ret = ::GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &bufLen);
    if (ret == ERROR_BUFFER_OVERFLOW)
    {
        buf.resize(bufLen);
        pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
        ret = ::GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &bufLen);
    }
    if (ret != NO_ERROR) return result;

    for (auto* adapter = pAddresses; adapter != nullptr; adapter = adapter->Next)
    {
        if (adapter->OperStatus != IfOperStatusUp) continue;
        for (auto* ua = adapter->FirstUnicastAddress; ua != nullptr; ua = ua->Next)
        {
            auto* sa = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            char ipBuf[INET_ADDRSTRLEN]{};
            if (::inet_ntop(AF_INET, &sa->sin_addr, ipBuf, sizeof(ipBuf)))
            {
                std::string ip(ipBuf);
                if (ip != "127.0.0.1")
                    result.push_back(ip);
            }
        }
    }
    return result;
}

std::string GetPrimaryLocalIP()
{
    auto ips = GetLocalIPv4Addresses();
    if (!ips.empty()) return ips[0];
    return "127.0.0.1";
}

bool SetNonBlocking(SOCKET s, bool nonBlocking)
{
    u_long mode = nonBlocking ? 1u : 0u;
    return (::ioctlsocket(s, FIONBIO, &mode) == 0);
}

bool SetNoDelay(SOCKET s, bool noDelay)
{
    int val = noDelay ? 1 : 0;
    return (::setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
        reinterpret_cast<const char*>(&val), sizeof(val)) == 0);
}

bool SetReuseAddr(SOCKET s, bool reuse)
{
    int val = reuse ? 1 : 0;
    return (::setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&val), sizeof(val)) == 0);
}

bool SendAll(SOCKET s, const char* data, int len)
{
    int sent = 0;
    while (sent < len)
    {
        int r = ::send(s, data + sent, len - sent, 0);
        if (r == SOCKET_ERROR)
        {
            int err = ::WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
            {
                // Socket temporarily full – back off briefly and retry
                ::Sleep(1);
                continue;
            }
            return false;
        }
        sent += r;
    }
    return true;
}

} // namespace NetworkUtils
