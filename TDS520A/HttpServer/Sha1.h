// HttpServer/Sha1.h - Minimal SHA-1 implementation for WebSocket handshake
// Based on public domain RFC 3174 reference implementation.
#pragma once
#include "pch.h"

struct Sha1Context
{
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
};

void Sha1Init(Sha1Context* ctx);
void Sha1Update(Sha1Context* ctx, const uint8_t* data, size_t len);
void Sha1Final(Sha1Context* ctx, uint8_t digest[20]);

// Convenience: hash data and return 20 bytes
inline std::array<uint8_t, 20> Sha1Digest(const void* data, size_t len)
{
    Sha1Context ctx;
    Sha1Init(&ctx);
    Sha1Update(&ctx, static_cast<const uint8_t*>(data), len);
    std::array<uint8_t, 20> digest{};
    Sha1Final(&ctx, digest.data());
    return digest;
}
