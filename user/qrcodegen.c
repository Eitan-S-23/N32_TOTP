/**
 * Minimal QR-code encoder — byte mode, EC Level L, versions 1..9.
 *
 * Implements the standard QR pipeline end-to-end:
 *   1. Pick smallest version that holds the payload.
 *   2. Build bitstream (mode indicator + 8-bit char count + data + terminator
 *      + 0xEC/0x11 padding).
 *   3. Split into blocks, compute Reed-Solomon ECC per block.
 *   4. Interleave data+ECC codewords.
 *   5. Draw function patterns (finders, timing, alignment, format/version
 *      reservation, dark module).
 *   6. Place codewords in zigzag columns.
 *   7. Try all 8 masks, pick lowest penalty, write format info.
 *
 * V10+ is intentionally out of scope: it needs two-group interleaving and
 * a 16-bit char count, and our otpauth URI payload fits V9 easily (232 B).
 *
 * Algorithm references:
 *   - ISO/IEC 18004:2015
 *   - Nayuki's qrcodegen (for numerical conventions; re-implemented here
 *     much smaller since we only need byte mode + EC-L).
 */
#include <string.h>
#include "qrcodegen.h"

/* ==== Error-correction table (EC Level L only) =========================== */
/*   {blocks, data_cw_per_block, ec_cw_per_block}
 * Total data capacity in codewords = blocks * data_cw_per_block.
 * All V1..V9 use a single group (every block same size).                   */
static const uint8_t EC_L[9][3] = {
    /* V1 */ { 1,  19,  7 },
    /* V2 */ { 1,  34, 10 },
    /* V3 */ { 1,  55, 15 },
    /* V4 */ { 1,  80, 20 },
    /* V5 */ { 1, 108, 26 },
    /* V6 */ { 2,  68, 18 },
    /* V7 */ { 2,  78, 20 },
    /* V8 */ { 2,  97, 24 },
    /* V9 */ { 2, 116, 30 },
};

/* Alignment-pattern center coordinates (−1 = sentinel). V1 has none.       */
static const int8_t ALIGN_POS[9][4] = {
    /* V1 */ { -1,-1,-1,-1 },
    /* V2 */ {  6,18,-1,-1 },
    /* V3 */ {  6,22,-1,-1 },
    /* V4 */ {  6,26,-1,-1 },
    /* V5 */ {  6,30,-1,-1 },
    /* V6 */ {  6,34,-1,-1 },
    /* V7 */ {  6,22,38,-1 },
    /* V8 */ {  6,24,42,-1 },
    /* V9 */ {  6,26,46,-1 },
};

/* ==== GF(256) arithmetic (primitive polynomial x^8+x^4+x^3+x^2+1 = 0x11D)  */
static uint8_t gf_exp[512];
static uint8_t gf_log[256];
static uint8_t gf_ready = 0;

static void gf_init(void)
{
    uint16_t x;
    int i;
    if (gf_ready) return;
    x = 1;
    for (i = 0; i < 255; i++) {
        gf_exp[i] = (uint8_t)x;
        gf_log[x] = (uint8_t)i;
        x <<= 1;
        if (x & 0x100) x ^= 0x11D;
    }
    for (i = 255; i < 512; i++)
        gf_exp[i] = gf_exp[i - 255];
    gf_ready = 1;
}

static uint8_t gf_mul(uint8_t a, uint8_t b)
{
    if (a == 0 || b == 0) return 0;
    return gf_exp[gf_log[a] + gf_log[b]];
}

/* ==== Reed-Solomon encoder =============================================== */
/* gen[0..degree-1] holds the generator polynomial coefficients with gen[0]
 * = x^(degree-1) and gen[degree-1] = constant term. Leading x^degree is
 * implicit 1. Iteratively multiplies by (x - α^i) for i = 0..degree-1.     */
static void rs_gen(int degree, uint8_t *gen)
{
    uint8_t root;
    int i, j;
    memset(gen, 0, degree);
    gen[degree - 1] = 1;
    root = 1;
    for (i = 0; i < degree; i++) {
        for (j = 0; j < degree; j++) {
            gen[j] = gf_mul(gen[j], root);
            if (j + 1 < degree)
                gen[j] ^= gen[j + 1];
        }
        root = gf_mul(root, 0x02);
    }
}

/* ecc[0..degree-1] = data * x^degree mod gen(x).                            */
static void rs_encode(const uint8_t *data, int data_len,
                      const uint8_t *gen, int degree,
                      uint8_t *ecc)
{
    uint8_t factor;
    int i, j;
    memset(ecc, 0, degree);
    for (i = 0; i < data_len; i++) {
        factor = data[i] ^ ecc[0];
        memmove(ecc, ecc + 1, degree - 1);
        ecc[degree - 1] = 0;
        if (factor) {
            for (j = 0; j < degree; j++)
                ecc[j] ^= gf_mul(gen[j], factor);
        }
    }
}

/* ==== Matrix bit-packed helpers ========================================== */

static int bit_get(const uint8_t *buf, int side, int x, int y)
{
    int idx = y * side + x;
    return (buf[idx >> 3] >> (idx & 7)) & 1;
}

static void bit_set(uint8_t *buf, int side, int x, int y, int val)
{
    int idx = y * side + x;
    uint8_t m = (uint8_t)(1u << (idx & 7));
    if (val) buf[idx >> 3] |=  m;
    else     buf[idx >> 3] &= ~m;
}

static int iabs_(int v) { return v < 0 ? -v : v; }

/* ==== Function patterns ================================================== */

static void draw_finder(uint8_t *qr, uint8_t *fm, int side, int cx, int cy)
{
    int dx, dy, xx, yy, dist, dark;
    for (dy = -4; dy <= 4; dy++) {
        for (dx = -4; dx <= 4; dx++) {
            xx = cx + dx; yy = cy + dy;
            if (xx < 0 || xx >= side || yy < 0 || yy >= side) continue;
            dist = iabs_(dx) > iabs_(dy) ? iabs_(dx) : iabs_(dy);
            dark = (dist != 2) && (dist != 4);
            bit_set(qr, side, xx, yy, dark);
            bit_set(fm, side, xx, yy, 1);
        }
    }
}

static void draw_alignment(uint8_t *qr, uint8_t *fm, int side, int cx, int cy)
{
    int dx, dy, dist, dark;
    for (dy = -2; dy <= 2; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            dist = iabs_(dx) > iabs_(dy) ? iabs_(dx) : iabs_(dy);
            dark = (dist != 1);
            bit_set(qr, side, cx + dx, cy + dy, dark);
            bit_set(fm, side, cx + dx, cy + dy, 1);
        }
    }
}

static void draw_function_patterns(uint8_t *qr, uint8_t *fm, int version)
{
    int side = QR_SIZE(version);
    int i, j, cx, cy;
    const int8_t *pos;

    /* Timing patterns on row 6 and column 6 (alternating dark/light). */
    for (i = 0; i < side; i++) {
        int dark = (i & 1) == 0;
        bit_set(qr, side, 6, i, dark);
        bit_set(qr, side, i, 6, dark);
        bit_set(fm, side, 6, i, 1);
        bit_set(fm, side, i, 6, 1);
    }

    /* Three finder patterns + separators (drawn via 9×9 envelope). */
    draw_finder(qr, fm, side,          3,          3);
    draw_finder(qr, fm, side, side - 4,          3);
    draw_finder(qr, fm, side,          3, side - 4);

    /* Alignment patterns — skip the three corners that collide with finders. */
    pos = ALIGN_POS[version - 1];
    for (i = 0; pos[i] >= 0; i++) {
        for (j = 0; pos[j] >= 0; j++) {
            cx = pos[i]; cy = pos[j];
            if ((cx == 6 && cy == 6) ||
                (cx == side - 7 && cy == 6) ||
                (cx == 6 && cy == side - 7))
                continue;
            draw_alignment(qr, fm, side, cx, cy);
        }
    }

    /* Format-info reservation (drawn for real after mask selection). */
    for (i = 0; i <= 8; i++) {
        bit_set(fm, side, i, 8, 1);
        bit_set(fm, side, 8, i, 1);
    }
    for (i = 0; i < 8; i++)
        bit_set(fm, side, side - 1 - i, 8, 1);
    for (i = 0; i < 7; i++)
        bit_set(fm, side, 8, side - 1 - i, 1);

    /* "Always dark" module at (8, 4·v+9). */
    bit_set(qr, side, 8, 4 * version + 9, 1);
    bit_set(fm, side, 8, 4 * version + 9, 1);

    /* Version-info reservation (V7+): two 6×3 blocks. */
    if (version >= 7) {
        for (i = 0; i < 6; i++) {
            for (j = 0; j < 3; j++) {
                bit_set(fm, side, side - 11 + j, i, 1);
                bit_set(fm, side, i, side - 11 + j, 1);
            }
        }
    }
}

/* ==== Bitstream assembly ================================================ */

static void bs_append(uint8_t *buf, int *bit_idx, int v, int nbits)
{
    int i, b, idx;
    for (i = nbits - 1; i >= 0; i--) {
        b = (v >> i) & 1;
        idx = *bit_idx;
        if (b) buf[idx >> 3] |=  (uint8_t)(1u << (7 - (idx & 7)));
        else   buf[idx >> 3] &= ~(uint8_t)(1u << (7 - (idx & 7)));
        (*bit_idx)++;
    }
}

/* ==== BCH encoders for format / version info =========================== */

/* 15-bit format info (5 data + 10 BCH parity, generator 0x537), XORed with
 * 0x5412 per spec. level=0b01 for EC-L. mask=0..7. Output is 15 bits. */
static int bch_format(int level, int mask)
{
    int data = (level << 3) | mask;      /* 5 bits */
    int rem = data;
    int i;
    for (i = 0; i < 10; i++)
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    return ((data << 10) | rem) ^ 0x5412;
}

/* 18-bit version info (6 data + 12 BCH parity, generator 0x1F25). V7+. */
static int bch_version(int version)
{
    int rem = version;
    int i;
    for (i = 0; i < 12; i++)
        rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    return (version << 12) | rem;
}

/* ==== Format / version info placement ================================== */

static void draw_format_info(uint8_t *qr, int side, int mask)
{
    int bits = bch_format(0x01, mask);    /* EC-L */
    int i;

    /* First copy — column 8 rows 0..5, (8,7), (8,8), (7,8), row 8 cols 5..0. */
    for (i = 0; i <= 5; i++)
        bit_set(qr, side, 8, i, (bits >> i) & 1);
    bit_set(qr, side, 8, 7, (bits >> 6) & 1);
    bit_set(qr, side, 8, 8, (bits >> 7) & 1);
    bit_set(qr, side, 7, 8, (bits >> 8) & 1);
    for (i = 9; i < 15; i++)
        bit_set(qr, side, 14 - i, 8, (bits >> i) & 1);

    /* Second copy — row 8 cols side-1..side-8, then col 8 rows size-7..size-1. */
    for (i = 0; i < 8; i++)
        bit_set(qr, side, side - 1 - i, 8, (bits >> i) & 1);
    for (i = 8; i < 15; i++)
        bit_set(qr, side, 8, side - 15 + i, (bits >> i) & 1);
}

static void draw_version_info(uint8_t *qr, int side, int version)
{
    int bits, i, b, r, c;
    if (version < 7) return;
    bits = bch_version(version);
    for (i = 0; i < 18; i++) {
        b = (bits >> i) & 1;
        r = i / 3;
        c = i % 3;
        bit_set(qr, side, side - 11 + c, r, b);
        bit_set(qr, side, r, side - 11 + c, b);
    }
}

/* ==== Codeword placement (zigzag) ====================================== */

static void place_codewords(uint8_t *qr, const uint8_t *fm, int side,
                            const uint8_t *cw, int total_bytes)
{
    int bit_idx = 0;
    int total_bits = total_bytes * 8;
    int col_pair = 0;
    int right, v, j, x, y, up, bit;

    for (right = side - 1; right >= 1; right -= 2) {
        if (right == 6) right = 5;                /* skip vertical timing */
        up = (col_pair & 1) == 0;
        for (v = 0; v < side; v++) {
            y = up ? side - 1 - v : v;
            for (j = 0; j < 2; j++) {
                x = right - j;
                if (bit_get(fm, side, x, y)) continue;
                if (bit_idx < total_bits) {
                    bit = (cw[bit_idx >> 3] >> (7 - (bit_idx & 7))) & 1;
                    bit_set(qr, side, x, y, bit);
                }
                bit_idx++;
            }
        }
        col_pair++;
    }
}

/* ==== Masks and penalty scoring ========================================= */

static int mask_bit(int mask, int x, int y)
{
    switch (mask) {
        case 0: return ((y + x) & 1) == 0;
        case 1: return (y & 1) == 0;
        case 2: return (x % 3) == 0;
        case 3: return ((x + y) % 3) == 0;
        case 4: return (((y / 2) + (x / 3)) & 1) == 0;
        case 5: return (((x * y) & 1) + ((x * y) % 3)) == 0;
        case 6: return ((((x * y) & 1) + ((x * y) % 3)) & 1) == 0;
        case 7: return ((((x + y) & 1) + ((x * y) % 3)) & 1) == 0;
        default: return 0;
    }
}

static void apply_mask(uint8_t *qr, const uint8_t *fm, int side, int mask)
{
    int x, y, cur;
    for (y = 0; y < side; y++) {
        for (x = 0; x < side; x++) {
            if (bit_get(fm, side, x, y)) continue;
            if (mask_bit(mask, x, y)) {
                cur = bit_get(qr, side, x, y);
                bit_set(qr, side, x, y, cur ^ 1);
            }
        }
    }
}

static int penalty_score(const uint8_t *qr, int side)
{
    int penalty = 0, dark = 0, total = side * side;
    int x, y, run, prev, v, a, b, c, d, k, m, pct, diff;

    /* N1 + dark count — rows. */
    for (y = 0; y < side; y++) {
        prev = bit_get(qr, side, 0, y);
        if (prev) dark++;
        run = 1;
        for (x = 1; x < side; x++) {
            v = bit_get(qr, side, x, y);
            if (v) dark++;
            if (v == prev) run++;
            else {
                if (run >= 5) penalty += 3 + (run - 5);
                prev = v; run = 1;
            }
        }
        if (run >= 5) penalty += 3 + (run - 5);
    }
    /* N1 — columns. */
    for (x = 0; x < side; x++) {
        prev = bit_get(qr, side, x, 0);
        run = 1;
        for (y = 1; y < side; y++) {
            v = bit_get(qr, side, x, y);
            if (v == prev) run++;
            else {
                if (run >= 5) penalty += 3 + (run - 5);
                prev = v; run = 1;
            }
        }
        if (run >= 5) penalty += 3 + (run - 5);
    }

    /* N2 — 2×2 blocks of same colour. */
    for (y = 0; y < side - 1; y++) {
        for (x = 0; x < side - 1; x++) {
            a = bit_get(qr, side, x,     y);
            b = bit_get(qr, side, x + 1, y);
            c = bit_get(qr, side, x,     y + 1);
            d = bit_get(qr, side, x + 1, y + 1);
            if (a == b && a == c && a == d) penalty += 3;
        }
    }

    /* N3 — finder-lookalike 11-bit patterns in rows / columns. */
    for (y = 0; y < side; y++) {
        for (x = 0; x <= side - 11; x++) {
            m = 0;
            for (k = 0; k < 11; k++)
                m = (m << 1) | bit_get(qr, side, x + k, y);
            if (m == 0x5D0 || m == 0x05D) penalty += 40;
        }
    }
    for (x = 0; x < side; x++) {
        for (y = 0; y <= side - 11; y++) {
            m = 0;
            for (k = 0; k < 11; k++)
                m = (m << 1) | bit_get(qr, side, x, y + k);
            if (m == 0x5D0 || m == 0x05D) penalty += 40;
        }
    }

    /* N4 — dark-module proportion. */
    pct = dark * 100 / total;
    diff = pct > 50 ? pct - 50 : 50 - pct;
    penalty += (diff / 5) * 10;

    return penalty;
}

/* ==== Static scratch buffers =========================================== */
/* Sized for the maximum supported version. Static to keep the stack small
 * (main call stack stays < 128 B); ~1.4 KB of ZI total across all four. */
static uint8_t s_fm      [QR_BUFSIZE_MAX];      /* function-pattern mask */
static uint8_t s_cand    [QR_BUFSIZE_MAX];      /* candidate buffer during mask trial */
static uint8_t s_data_cw [232];                 /* data codewords (pre-interleave) */
static uint8_t s_ecc_cw  [60];                  /* ECC codewords (pre-interleave) */
static uint8_t s_final_cw[292];                 /* interleaved data+ECC */
static uint8_t s_gen     [32];                  /* RS generator poly */

/* ==== Public API ======================================================== */

int qr_encode_bytes(const uint8_t *data, int len,
                    uint8_t *qrbuf, int bufsize)
{
    int version = 0, total_data = 0, side, buf_bytes;
    int blocks, data_per, ec_per, total_cw;
    int bit_idx, rem_bits, term, b, i, idx;
    int best_mask, best_pen, p, m;
    uint8_t pad;

    if (!data || !qrbuf || len < 0) return 0;

    gf_init();

    /* 1. Pick smallest version that fits (byte mode: 4+8 bit header). */
    for (i = QR_VER_MIN; i <= QR_VER_MAX; i++) {
        int cap = EC_L[i - 1][0] * EC_L[i - 1][1];
        if (12 + 8 * len <= 8 * cap) {
            version = i;
            total_data = cap;
            break;
        }
    }
    if (version == 0) return 0;

    side = QR_SIZE(version);
    buf_bytes = (side * side + 7) / 8;
    if (bufsize < buf_bytes) return 0;

    memset(qrbuf, 0, buf_bytes);
    memset(s_fm, 0, buf_bytes);

    /* 2. Function patterns (also reserves format+version+dark-module area). */
    draw_function_patterns(qrbuf, s_fm, version);

    /* 3. Build data bitstream into s_data_cw. */
    memset(s_data_cw, 0, total_data);
    bit_idx = 0;
    bs_append(s_data_cw, &bit_idx, 0x4, 4);              /* byte mode */
    bs_append(s_data_cw, &bit_idx, len,  8);             /* char count (V1-9) */
    for (i = 0; i < len; i++)
        bs_append(s_data_cw, &bit_idx, data[i], 8);

    /* Terminator (up to 4 zero bits). */
    rem_bits = total_data * 8 - bit_idx;
    term = rem_bits < 4 ? rem_bits : 4;
    bs_append(s_data_cw, &bit_idx, 0, term);
    /* Pad to next byte boundary. */
    while (bit_idx & 7) bs_append(s_data_cw, &bit_idx, 0, 1);
    /* Fill remaining codewords with 0xEC / 0x11. */
    pad = 0xEC;
    while (bit_idx < total_data * 8) {
        bs_append(s_data_cw, &bit_idx, pad, 8);
        pad = (pad == 0xEC) ? 0x11 : 0xEC;
    }

    /* 4. RS-encode each block into s_ecc_cw (contiguous by block). */
    blocks   = EC_L[version - 1][0];
    data_per = EC_L[version - 1][1];
    ec_per   = EC_L[version - 1][2];
    total_cw = blocks * (data_per + ec_per);
    rs_gen(ec_per, s_gen);
    for (b = 0; b < blocks; b++)
        rs_encode(s_data_cw + b * data_per, data_per, s_gen, ec_per,
                  s_ecc_cw + b * ec_per);

    /* 5. Interleave: data by index across blocks, then ECC by index across
     * blocks. For V1-5 (single block) this degenerates to a straight copy. */
    idx = 0;
    for (i = 0; i < data_per; i++)
        for (b = 0; b < blocks; b++)
            s_final_cw[idx++] = s_data_cw[b * data_per + i];
    for (i = 0; i < ec_per; i++)
        for (b = 0; b < blocks; b++)
            s_final_cw[idx++] = s_ecc_cw[b * ec_per + i];

    /* 6. Place codewords zigzag, then draw version info. */
    place_codewords(qrbuf, s_fm, side, s_final_cw, total_cw);
    draw_version_info(qrbuf, side, version);

    /* 7. Try all 8 masks, pick minimum-penalty one. */
    best_mask = 0;
    best_pen  = 0x7FFFFFFF;
    for (m = 0; m < 8; m++) {
        memcpy(s_cand, qrbuf, buf_bytes);
        apply_mask(s_cand, s_fm, side, m);
        draw_format_info(s_cand, side, m);
        p = penalty_score(s_cand, side);
        if (p < best_pen) { best_pen = p; best_mask = m; }
    }
    apply_mask(qrbuf, s_fm, side, best_mask);
    draw_format_info(qrbuf, side, best_mask);

    return side;
}

bool qr_get_module(const uint8_t *qrbuf, int side, int x, int y)
{
    if (x < 0 || x >= side || y < 0 || y >= side) return false;
    return bit_get(qrbuf, side, x, y) != 0;
}

/* ==== Base32 (RFC 4648 alphabet, no padding) ============================ */

int base32_encode(const uint8_t *in, int in_len, char *out, int out_buf)
{
    static const char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    int out_len = 0, i;
    uint32_t buffer = 0;
    int bits = 0;
    int need = (in_len * 8 + 4) / 5;           /* ceil(n*8/5) */
    if (out_buf < need + 1) return -1;
    for (i = 0; i < in_len; i++) {
        buffer = (buffer << 8) | in[i];
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            out[out_len++] = alpha[(buffer >> bits) & 0x1F];
        }
    }
    if (bits > 0)
        out[out_len++] = alpha[(buffer << (5 - bits)) & 0x1F];
    out[out_len] = 0;
    return out_len;
}
