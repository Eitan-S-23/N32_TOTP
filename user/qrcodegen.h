/**
 * Minimal QR-code generator for N32 TOTP.
 *
 *   Modes  : byte mode only
 *   EC lvl : L (≈7% recovery) — chosen for max data capacity
 *   Versions: 1..9 (21×21..53×53 modules) — enough for otpauth:// URIs
 *             with secrets up to ~32 Base32 chars (20 raw bytes)
 *
 * Implements the ISO/IEC 18004 algorithm (finder/timing/alignment patterns,
 * Reed-Solomon ECC, zigzag codeword placement, mask scoring). Intentionally
 * limited to single-group blocks so the interleaving logic stays simple.
 */
#ifndef __QRCODEGEN_H__
#define __QRCODEGEN_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QR_VER_MIN      1
#define QR_VER_MAX      9
#define QR_SIZE(v)      (17 + 4*(v))
#define QR_SIZE_MAX     QR_SIZE(QR_VER_MAX)                              /* 53 */
#define QR_BUFSIZE_MAX  ((QR_SIZE_MAX * QR_SIZE_MAX + 7) / 8)            /* 351 */

/**
 * Encode raw bytes into a QR code at EC Level L, byte mode.
 *   data, len  : input payload (len ≤ 232 for V9-L)
 *   qrbuf      : output buffer of at least QR_BUFSIZE_MAX bytes; modules
 *                packed 8/byte in row-major order (bit = 1 → dark)
 *   bufsize    : size of qrbuf in bytes
 * Returns the QR side length in modules on success, or 0 if the data is too
 * long for V9 or the buffer is too small.
 */
int qr_encode_bytes(const uint8_t *data, int len,
                    uint8_t *qrbuf, int bufsize);

/** Read a module from an encoded QR buffer. */
bool qr_get_module(const uint8_t *qrbuf, int side, int x, int y);

/**
 * RFC-4648 Base32 encode (no padding).
 *   in, in_len : raw bytes
 *   out        : NUL-terminated ASCII output
 *   out_buf    : size of out (needs at least 8*ceil(in_len/5) + 1)
 * Returns number of characters written (excluding NUL), or -1 if out_buf
 * is too small.
 */
int base32_encode(const uint8_t *in, int in_len, char *out, int out_buf);

#ifdef __cplusplus
}
#endif

#endif /* __QRCODEGEN_H__ */
