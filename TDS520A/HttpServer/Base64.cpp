// HttpServer/Base64.cpp
#include "pch.h"
#include "Base64.h"

namespace Base64
{

static const char kTable[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Encode(const uint8_t* data, size_t len)
{
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3)
    {
        uint32_t b = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) b |= static_cast<uint32_t>(data[i+1]) << 8;
        if (i + 2 < len) b |= static_cast<uint32_t>(data[i+2]);

        out += kTable[(b >> 18) & 0x3F];
        out += kTable[(b >> 12) & 0x3F];
        out += (i + 1 < len) ? kTable[(b >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? kTable[ b       & 0x3F] : '=';
    }
    return out;
}

std::string Encode(const std::string& s)
{
    return Encode(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

std::string Decode(const std::string& b64)
{
    // Build reverse lookup table
    static uint8_t rev[256];
    static bool revInit = false;
    if (!revInit)
    {
        memset(rev, 0xFF, sizeof(rev));
        for (uint8_t i = 0; i < 64; ++i)
            rev[static_cast<uint8_t>(kTable[i])] = i;
        revInit = true;
    }

    std::string out;
    out.reserve(b64.size() * 3 / 4);
    uint32_t b = 0; int bits = 0;
    for (char c : b64)
    {
        if (c == '=') break;
        uint8_t v = rev[static_cast<uint8_t>(c)];
        if (v == 0xFF) continue;
        b = (b << 6) | v;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out += static_cast<char>((b >> bits) & 0xFF);
        }
    }
    return out;
}

} // namespace Base64
