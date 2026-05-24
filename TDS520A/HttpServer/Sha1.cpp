// HttpServer/Sha1.cpp - Public domain SHA-1 implementation (RFC 3174)
#include "pch.h"
#include "Sha1.h"

#define SHA1_ROL(bits,word) \
    (((word) << (bits)) | ((word) >> (32-(bits))))

void Sha1Init(Sha1Context* ctx)
{
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu;
    ctx->state[3] = 0x10325476u;
    ctx->state[4] = 0xC3D2E1F0u;
    ctx->count[0] = ctx->count[1] = 0;
}

static void Sha1Transform(uint32_t state[5], const uint8_t buffer[64])
{
    uint32_t W[80];
    for (int i = 0; i < 16; i++)
        W[i] = ((uint32_t)buffer[i*4]     << 24) |
               ((uint32_t)buffer[i*4 + 1] << 16) |
               ((uint32_t)buffer[i*4 + 2] <<  8) |
               ((uint32_t)buffer[i*4 + 3]);
    for (int i = 16; i < 80; i++)
        W[i] = SHA1_ROL(1, W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16]);

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

#define SHA1_R0(v,w,x,y,z,i) \
    z += ((w&(x^y))^y) + W[i] + 0x5A827999u + SHA1_ROL(5,v); w=SHA1_ROL(30,w)
#define SHA1_R1(v,w,x,y,z,i) \
    z += ((w&(x^y))^y) + W[i] + 0x5A827999u + SHA1_ROL(5,v); w=SHA1_ROL(30,w)
#define SHA1_R2(v,w,x,y,z,i) \
    z += (w^x^y) + W[i] + 0x6ED9EBA1u + SHA1_ROL(5,v); w=SHA1_ROL(30,w)
#define SHA1_R3(v,w,x,y,z,i) \
    z += (((w|x)&y)|(w&x)) + W[i] + 0x8F1BBCDCu + SHA1_ROL(5,v); w=SHA1_ROL(30,w)
#define SHA1_R4(v,w,x,y,z,i) \
    z += (w^x^y) + W[i] + 0xCA62C1D6u + SHA1_ROL(5,v); w=SHA1_ROL(30,w)

    SHA1_R0(a,b,c,d,e, 0); SHA1_R0(e,a,b,c,d, 1); SHA1_R0(d,e,a,b,c, 2); SHA1_R0(c,d,e,a,b, 3);
    SHA1_R0(b,c,d,e,a, 4); SHA1_R0(a,b,c,d,e, 5); SHA1_R0(e,a,b,c,d, 6); SHA1_R0(d,e,a,b,c, 7);
    SHA1_R0(c,d,e,a,b, 8); SHA1_R0(b,c,d,e,a, 9); SHA1_R0(a,b,c,d,e,10); SHA1_R0(e,a,b,c,d,11);
    SHA1_R0(d,e,a,b,c,12); SHA1_R0(c,d,e,a,b,13); SHA1_R0(b,c,d,e,a,14); SHA1_R0(a,b,c,d,e,15);
    SHA1_R1(e,a,b,c,d,16); SHA1_R1(d,e,a,b,c,17); SHA1_R1(c,d,e,a,b,18); SHA1_R1(b,c,d,e,a,19);
    SHA1_R2(a,b,c,d,e,20); SHA1_R2(e,a,b,c,d,21); SHA1_R2(d,e,a,b,c,22); SHA1_R2(c,d,e,a,b,23);
    SHA1_R2(b,c,d,e,a,24); SHA1_R2(a,b,c,d,e,25); SHA1_R2(e,a,b,c,d,26); SHA1_R2(d,e,a,b,c,27);
    SHA1_R2(c,d,e,a,b,28); SHA1_R2(b,c,d,e,a,29); SHA1_R2(a,b,c,d,e,30); SHA1_R2(e,a,b,c,d,31);
    SHA1_R2(d,e,a,b,c,32); SHA1_R2(c,d,e,a,b,33); SHA1_R2(b,c,d,e,a,34); SHA1_R2(a,b,c,d,e,35);
    SHA1_R2(e,a,b,c,d,36); SHA1_R2(d,e,a,b,c,37); SHA1_R2(c,d,e,a,b,38); SHA1_R2(b,c,d,e,a,39);
    SHA1_R3(a,b,c,d,e,40); SHA1_R3(e,a,b,c,d,41); SHA1_R3(d,e,a,b,c,42); SHA1_R3(c,d,e,a,b,43);
    SHA1_R3(b,c,d,e,a,44); SHA1_R3(a,b,c,d,e,45); SHA1_R3(e,a,b,c,d,46); SHA1_R3(d,e,a,b,c,47);
    SHA1_R3(c,d,e,a,b,48); SHA1_R3(b,c,d,e,a,49); SHA1_R3(a,b,c,d,e,50); SHA1_R3(e,a,b,c,d,51);
    SHA1_R3(d,e,a,b,c,52); SHA1_R3(c,d,e,a,b,53); SHA1_R3(b,c,d,e,a,54); SHA1_R3(a,b,c,d,e,55);
    SHA1_R3(e,a,b,c,d,56); SHA1_R3(d,e,a,b,c,57); SHA1_R3(c,d,e,a,b,58); SHA1_R3(b,c,d,e,a,59);
    SHA1_R4(a,b,c,d,e,60); SHA1_R4(e,a,b,c,d,61); SHA1_R4(d,e,a,b,c,62); SHA1_R4(c,d,e,a,b,63);
    SHA1_R4(b,c,d,e,a,64); SHA1_R4(a,b,c,d,e,65); SHA1_R4(e,a,b,c,d,66); SHA1_R4(d,e,a,b,c,67);
    SHA1_R4(c,d,e,a,b,68); SHA1_R4(b,c,d,e,a,69); SHA1_R4(a,b,c,d,e,70); SHA1_R4(e,a,b,c,d,71);
    SHA1_R4(d,e,a,b,c,72); SHA1_R4(c,d,e,a,b,73); SHA1_R4(b,c,d,e,a,74); SHA1_R4(a,b,c,d,e,75);
    SHA1_R4(e,a,b,c,d,76); SHA1_R4(d,e,a,b,c,77); SHA1_R4(c,d,e,a,b,78); SHA1_R4(b,c,d,e,a,79);

    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

void Sha1Update(Sha1Context* ctx, const uint8_t* data, size_t len)
{
    uint32_t i = (ctx->count[0] >> 3) & 63u;
    ctx->count[0] += static_cast<uint32_t>(len << 3);
    if (ctx->count[0] < static_cast<uint32_t>(len << 3)) ctx->count[1]++;
    ctx->count[1] += static_cast<uint32_t>(len >> 29);

    uint32_t j = 64u - i;
    size_t pos = 0;

    if (len >= j)
    {
        memcpy(ctx->buffer + i, data, j);
        Sha1Transform(ctx->state, ctx->buffer);
        for (pos = j; pos + 63 < len; pos += 64)
            Sha1Transform(ctx->state, data + pos);
        i = 0;
    }
    memcpy(ctx->buffer + i, data + pos, len - pos);
}

void Sha1Final(Sha1Context* ctx, uint8_t digest[20])
{
    uint8_t finalcount[8];
    for (int i = 0; i < 8; i++)
        finalcount[i] = static_cast<uint8_t>(
            (ctx->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) & 0xFF);

    uint8_t c = 0x80;
    Sha1Update(ctx, &c, 1);
    while ((ctx->count[0] & 504) != 448)
    {
        c = 0x00;
        Sha1Update(ctx, &c, 1);
    }
    Sha1Update(ctx, finalcount, 8);

    for (int i = 0; i < 20; i++)
        digest[i] = static_cast<uint8_t>(
            (ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 0xFF);
}
