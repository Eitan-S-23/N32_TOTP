/**
 * TOTP (RFC 6238) generator for N32WB031.
 *   HMAC-SHA1 with 30-second step, 6 decimal digits.
 *   Dependency-free: no OpenSSL, no mbedTLS.
 */
#include "app_totp.h"
#include <string.h>

/* ----------------------- SHA-1 ----------------------- */

typedef struct
{
    uint32_t state[5];
    uint32_t count_hi;
    uint32_t count_lo;
    uint8_t  buffer[64];
} sha1_ctx_t;

static uint32_t sha1_rol(uint32_t x, unsigned n)
{
    return (x << n) | (x >> (32u - n));
}

static void sha1_transform(uint32_t state[5], const uint8_t block[64])
{
    uint32_t w[80];
    uint32_t a, b, c, d, e, t;
    unsigned i;

    for (i = 0; i < 16; i++)
    {
        w[i] = ((uint32_t)block[i * 4 + 0] << 24)
             | ((uint32_t)block[i * 4 + 1] << 16)
             | ((uint32_t)block[i * 4 + 2] <<  8)
             | ((uint32_t)block[i * 4 + 3]);
    }
    for (i = 16; i < 80; i++)
    {
        w[i] = sha1_rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];

    for (i = 0; i < 80; i++)
    {
        uint32_t f, k;
        if (i < 20)       { f = (b & c) | ((~b) & d);                  k = 0x5A827999; }
        else if (i < 40)  { f = b ^ c ^ d;                              k = 0x6ED9EBA1; }
        else if (i < 60)  { f = (b & c) | (b & d) | (c & d);            k = 0x8F1BBCDC; }
        else              { f = b ^ c ^ d;                              k = 0xCA62C1D6; }
        t = sha1_rol(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = sha1_rol(b, 30);
        b = a;
        a = t;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1_init(sha1_ctx_t *ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count_hi = 0;
    ctx->count_lo = 0;
}

static void sha1_update(sha1_ctx_t *ctx, const uint8_t *data, uint32_t len)
{
    uint32_t i, j;

    j = (ctx->count_lo >> 3) & 0x3F;
    ctx->count_lo += ((uint32_t)len) << 3;
    if (ctx->count_lo < (((uint32_t)len) << 3))
    {
        ctx->count_hi++;
    }
    ctx->count_hi += ((uint32_t)len) >> 29;

    if (j + len >= 64)
    {
        i = 64 - j;
        memcpy(&ctx->buffer[j], data, i);
        sha1_transform(ctx->state, ctx->buffer);
        for (; i + 63 < len; i += 64)
        {
            sha1_transform(ctx->state, &data[i]);
        }
        j = 0;
    }
    else
    {
        i = 0;
    }
    memcpy(&ctx->buffer[j], &data[i], len - i);
}

static void sha1_final(sha1_ctx_t *ctx, uint8_t out[20])
{
    uint8_t  finalcount[8];
    uint8_t  c;
    unsigned i;

    for (i = 0; i < 4; i++)
    {
        finalcount[i]     = (uint8_t)(ctx->count_hi >> ((3 - i) * 8));
        finalcount[i + 4] = (uint8_t)(ctx->count_lo >> ((3 - i) * 8));
    }
    c = 0x80;
    sha1_update(ctx, &c, 1);
    c = 0x00;
    while ((ctx->count_lo & 0x1F8) != 0x1C0)
    {
        sha1_update(ctx, &c, 1);
    }
    sha1_update(ctx, finalcount, 8);
    for (i = 0; i < 20; i++)
    {
        out[i] = (uint8_t)(ctx->state[i >> 2] >> ((3 - (i & 3)) * 8));
    }
}

/* ----------------------- HMAC-SHA1 ----------------------- */

void hmac_sha1(const uint8_t *key, uint32_t keylen,
               const uint8_t *msg, uint32_t msglen,
               uint8_t out[20])
{
    uint8_t     k_ipad[64];
    uint8_t     k_opad[64];
    uint8_t     key_block[64];
    uint8_t     inner[20];
    sha1_ctx_t  ctx;
    unsigned    i;

    if (keylen > 64)
    {
        sha1_init(&ctx);
        sha1_update(&ctx, key, keylen);
        sha1_final(&ctx, key_block);
        memset(&key_block[20], 0, 44);
    }
    else
    {
        memcpy(key_block, key, keylen);
        memset(&key_block[keylen], 0, 64 - keylen);
    }

    for (i = 0; i < 64; i++)
    {
        k_ipad[i] = key_block[i] ^ 0x36;
        k_opad[i] = key_block[i] ^ 0x5C;
    }

    sha1_init(&ctx);
    sha1_update(&ctx, k_ipad, 64);
    sha1_update(&ctx, msg, msglen);
    sha1_final(&ctx, inner);

    sha1_init(&ctx);
    sha1_update(&ctx, k_opad, 64);
    sha1_update(&ctx, inner, 20);
    sha1_final(&ctx, out);
}

/* ----------------------- TOTP ----------------------- */

uint64_t totp_counter_of(uint64_t unix_time)
{
    return unix_time / TOTP_STEP;
}

uint32_t totp_compute(const uint8_t *key, uint32_t keylen, uint64_t unix_time)
{
    uint8_t  counter_be[8];
    uint8_t  mac[20];
    uint64_t counter;
    uint32_t offset;
    uint32_t bin;
    uint32_t mod;
    unsigned i;

    if (key == 0 || keylen == 0)
    {
        return 0;
    }

    counter = totp_counter_of(unix_time);
    for (i = 0; i < 8; i++)
    {
        counter_be[7 - i] = (uint8_t)(counter & 0xFF);
        counter >>= 8;
    }

    hmac_sha1(key, keylen, counter_be, 8, mac);

    offset = mac[19] & 0x0F;
    bin = ((uint32_t)(mac[offset]     & 0x7F) << 24)
        | ((uint32_t)(mac[offset + 1] & 0xFF) << 16)
        | ((uint32_t)(mac[offset + 2] & 0xFF) <<  8)
        | ((uint32_t)(mac[offset + 3] & 0xFF));

    mod = 1u;
    for (i = 0; i < TOTP_DIGITS; i++)
    {
        mod *= 10u;
    }
    return bin % mod;
}
