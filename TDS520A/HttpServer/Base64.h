// HttpServer/Base64.h - Minimal Base64 encode/decode for WebSocket
#pragma once
#include "pch.h"

namespace Base64
{
    std::string Encode(const uint8_t* data, size_t len);
    std::string Encode(const std::string& s);
    std::string Decode(const std::string& b64);
}
