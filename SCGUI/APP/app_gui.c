/**
 * TOTP demo screen layout (160 × 128):
 *   y=0..14   : title bar + BLE status
 *   y=18..34  : previous code (font_16)
 *   y=38..58  : current code  (font_20, bold)
 *   y=62..78  : next code     (font_16)
 *   y=82..88  : progress bar  (6 px)
 *   y=92..104 : status text   (font_12)
 *   y=108..120: BLE RX debug  (font_12) — last write's count/len/hex
 *
 * Key map (xpt2046.c EXTI4_12_IRQHandler sets the global `key`):
 *   CODE screen  — KEY3 → switch to QR screen (requires key+time set)
 *   QR screen    — KEY1 → back to CODE screen
 * We poll `key` edge-triggered at the top of app_gui_task; the ISR never
 * clears it so we must zero it after handling to avoid repeat fires.
 */
#include "app_gui.h"
#include "app_totp.h"
#include "app_rtc.h"
#include "app_profile/app_totp_ble.h"
#include "app_user_config.h"
#include "qrcodegen.h"
#include "xpt2046.h"
#include "lcd.h"

#include <stdio.h>
#include <string.h>

static Obj_Canvas   canvas;
static Obj_Frame    title_frame;
static Obj_Label    title_label;
static Obj_Label    ble_label;

static Obj_Label    prev_label;
static Obj_Label    curr_label;
static Obj_Label    next_label;

static Obj_Slider   progress;
static Obj_Label    status_label;
static Obj_Label    debug_label;

/* Redraw cache — avoid pushing the same string every tick. */
static uint32_t s_cached_counter  = 0xFFFFFFFFu;
static uint8_t  s_cached_key_ok   = 0xFF;
static uint8_t  s_cached_time_ok  = 0xFF;
static uint8_t  s_cached_ble      = 0xFF;
static uint32_t s_cached_rx_count = 0xFFFFFFFFu;
static uint16_t s_cached_rx_len   = 0xFFFF;
static uint32_t s_cached_add      = 0xFFFFFFFFu;

static char buf_prev[12];
static char buf_curr[12];
static char buf_next[12];
static char buf_status[24];
static char buf_ble[8];
static char buf_debug[48];
static char buf_title[16];

static uint32_t s_cached_time_sod = 0xFFFFFFFFu;  /* seconds-of-day for HH:MM:SS label */

/* ---- QR screen state ---------------------------------------------------- */

enum { GUI_MODE_CODE = 0, GUI_MODE_QR = 1 };
static uint8_t s_mode = GUI_MODE_CODE;
static uint8_t s_qrbuf[QR_BUFSIZE_MAX];

static void format_code(char *out, uint32_t code)
{
    snprintf(out, 12, "%06u", (unsigned)code);
}

/* SCGUI quirk: sc_set_slider_value only updates the numeric value, it does
 * NOT mark the widget dirty — so without this helper the bar never repaints
 * after the initial draw. */
static void set_progress(uint8_t pct)
{
    sc_set_slider_value(&progress, (int)pct);
    progress.base.Flag |= OBJ_FLAG_ACTIVE;
}

static void render_title_clock(uint64_t now)
{
    int64_t  local = (int64_t)now + (int64_t)app_totp_ble_get_tz_offset();
    uint32_t sod;
    uint8_t  hh, mm, ss;

    /* Positive modulo so a TZ that puts us before UTC epoch still wraps. */
    local = ((local % 86400) + 86400) % 86400;
    sod   = (uint32_t)local;
    hh    = (uint8_t)(sod / 3600u);
    mm    = (uint8_t)((sod / 60u) % 60u);
    ss    = (uint8_t)(sod % 60u);

    if (sod == s_cached_time_sod)
    {
        return;
    }
    s_cached_time_sod = sod;

    snprintf(buf_title, sizeof(buf_title), "%02u:%02u:%02u", hh, mm, ss);
    sc_set_label_text(&title_label, buf_title, C_WHITE, &lv_font_12, ALIGN_CENTER);
}

static void render_waiting(void)
{
    uint8_t key_ok  = (app_totp_ble_get_keylen() > 0) ? 1u : 0u;
    uint8_t time_ok = app_rtc_is_time_set();

    if (s_cached_key_ok == key_ok && s_cached_time_ok == time_ok)
    {
        return;
    }
    s_cached_key_ok  = key_ok;
    s_cached_time_ok = time_ok;

    sc_set_label_text(&prev_label, "------", C_DIM_GRAY, &lv_font_16, ALIGN_CENTER);
    sc_set_label_text(&curr_label, "------", C_WHITE,    &lv_font_20, ALIGN_CENTER);
    sc_set_label_text(&next_label, "------", C_DIM_GRAY, &lv_font_16, ALIGN_CENTER);

    if (!key_ok && !time_ok)
    {
        snprintf(buf_status, sizeof(buf_status), "Send key + time");
    }
    else if (!key_ok)
    {
        snprintf(buf_status, sizeof(buf_status), "Send key");
    }
    else
    {
        snprintf(buf_status, sizeof(buf_status), "Send time");
    }
    sc_set_label_text(&status_label, buf_status, C_ORANGE, &lv_font_12, ALIGN_CENTER);
    set_progress(0);
    s_cached_counter  = 0xFFFFFFFFu;
    s_cached_time_sod = 0xFFFFFFFFu;
}

static void render_running(uint64_t now)
{
    const uint8_t *key    = app_totp_ble_get_key();
    uint32_t       keylen = app_totp_ble_get_keylen();
    uint64_t       counter = totp_counter_of(now);
    uint32_t       counter_lo;
    uint8_t        remaining;
    uint32_t       code_prev, code_curr, code_next;

    counter_lo = (uint32_t)counter;

    if (counter_lo != s_cached_counter)
    {
        s_cached_counter = counter_lo;
        s_cached_key_ok  = 0xFF;
        s_cached_time_ok = 0xFF;

        code_prev = totp_compute(key, keylen, now - TOTP_STEP);
        code_curr = totp_compute(key, keylen, now);
        code_next = totp_compute(key, keylen, now + TOTP_STEP);

        format_code(buf_prev, code_prev);
        format_code(buf_curr, code_curr);
        format_code(buf_next, code_next);

        sc_set_label_text(&prev_label, buf_prev, C_DIM_GRAY,   &lv_font_16, ALIGN_CENTER);
        sc_set_label_text(&curr_label, buf_curr, C_LIME_GREEN, &lv_font_20, ALIGN_CENTER);
        sc_set_label_text(&next_label, buf_next, C_DIM_GRAY,   &lv_font_16, ALIGN_CENTER);
    }

    remaining = (uint8_t)(TOTP_STEP - (uint8_t)(now % TOTP_STEP));
    snprintf(buf_status, sizeof(buf_status), "Next in %us", (unsigned)remaining);
    sc_set_label_text(&status_label, buf_status, C_WHITE, &lv_font_12, ALIGN_CENTER);

    /* Bar is a remaining-time gauge: full at the start of each 30 s window,
     * empties to ~0 just before the code rolls over. */
    set_progress((uint8_t)(remaining * 100u / TOTP_STEP));
}

static void render_ble_status(void)
{
    uint8_t connected = app_totp_ble_is_connected();
    if (connected == s_cached_ble)
    {
        return;
    }
    s_cached_ble = connected;

    snprintf(buf_ble, sizeof(buf_ble), connected ? "BLE+" : "BLE-");
    sc_set_label_text(&ble_label, buf_ble, connected ? C_LIME_GREEN : C_DIM_GRAY,
                      &lv_font_12, ALIGN_CENTER);
}

/* Bottom debug line — lets us see whether the phone's write reached the
 * handler at all.
 *   "A<n>" — how many times the stack invoked our add-service func.
 *            If A0, the RDTSS service was never registered (something in
 *            the BLE init sequence failed — check CFG_PRF_RDTSS).
 *   "R<n>" — how many RDTSS_VAL_WRITE_IND messages arrived.
 *            If A1 but R0 while writing, the phone is writing to a
 *            different service/characteristic (UUID mismatch).
 *   "H<hex>" — attribute index of the last write (2 = our value char).
 *   Tail — first few hex bytes of the last payload. */
static void render_debug(void)
{
    uint32_t        add_count = app_totp_ble_add_count();
    uint32_t        rx_count  = app_totp_ble_rx_count();
    uint16_t        rx_len    = app_totp_ble_last_rx_len();
    uint16_t        rx_handle = app_totp_ble_last_rx_handle();
    const uint8_t  *data      = app_totp_ble_last_rx_data();

    if (rx_count == s_cached_rx_count && rx_len == s_cached_rx_len
        && add_count == s_cached_add)
    {
        return;
    }
    s_cached_rx_count = rx_count;
    s_cached_rx_len   = rx_len;
    s_cached_add      = add_count;

    if (rx_count == 0)
    {
        snprintf(buf_debug, sizeof(buf_debug), "A%lu R0",
                 (unsigned long)add_count);
        sc_set_label_text(&debug_label, buf_debug, C_DIM_GRAY, &lv_font_12,
                          ALIGN_CENTER);
        return;
    }

    uint16_t shown = rx_len;
    if (shown > 4) shown = 4;

    int pos = snprintf(buf_debug, sizeof(buf_debug), "A%lu R%lu H%02X L%u:",
                       (unsigned long)add_count,
                       (unsigned long)rx_count,
                       (unsigned)rx_handle,
                       (unsigned)rx_len);
    for (uint16_t i = 0; i < shown && pos + 3 < (int)sizeof(buf_debug); i++)
    {
        pos += snprintf(&buf_debug[pos], sizeof(buf_debug) - pos, "%02X", data[i]);
    }
    sc_set_label_text(&debug_label, buf_debug, C_CYAN, &lv_font_12, ALIGN_CENTER);
}

/* ---- QR screen ---------------------------------------------------------- */

static void invalidate_all_caches(void)
{
    s_cached_counter  = 0xFFFFFFFFu;
    s_cached_time_sod = 0xFFFFFFFFu;
    s_cached_ble      = 0xFF;
    s_cached_rx_count = 0xFFFFFFFFu;
    s_cached_rx_len   = 0xFFFF;
    s_cached_add      = 0xFFFFFFFFu;
    s_cached_key_ok   = 0xFF;
    s_cached_time_ok  = 0xFF;
}

/* Re-mark every widget dirty so sc_widget_draw_screen repaints them on the
 * next main-loop iteration. sc_set_label_text already sets OBJ_FLAG_ACTIVE
 * when text changes, but on QR→CODE return the cached text may be identical
 * and render_* would skip the update — so we kick every widget here. */
static void force_redraw_widgets(void)
{
    title_frame.base.Flag  |= OBJ_FLAG_ACTIVE;
    title_label.base.Flag  |= OBJ_FLAG_ACTIVE;
    ble_label.base.Flag    |= OBJ_FLAG_ACTIVE;
    prev_label.base.Flag   |= OBJ_FLAG_ACTIVE;
    curr_label.base.Flag   |= OBJ_FLAG_ACTIVE;
    next_label.base.Flag   |= OBJ_FLAG_ACTIVE;
    progress.base.Flag     |= OBJ_FLAG_ACTIVE;
    status_label.base.Flag |= OBJ_FLAG_ACTIVE;
    debug_label.base.Flag  |= OBJ_FLAG_ACTIVE;
}

/* Build an otpauth:// URI that Google / Microsoft Authenticator can import:
 *   otpauth://totp/N32-TOTP?secret=BASE32&issuer=N32-TOTP&algorithm=SHA1&digits=6&period=30
 * Secret is unpadded Base32 (both apps accept it). */
static int build_otpauth_uri(char *out, int out_sz)
{
    const uint8_t *key = app_totp_ble_get_key();
    uint32_t keylen    = app_totp_ble_get_keylen();
    char b32[128];
    int n;

    if (keylen == 0) return -1;
    n = base32_encode(key, (int)keylen, b32, sizeof(b32));
    if (n < 0) return -1;

    n = snprintf(out, out_sz,
                 "otpauth://totp/%s?secret=%s&issuer=%s"
                 "&algorithm=SHA1&digits=6&period=30",
                 CUSTOM_DEVICE_NAME, b32, CUSTOM_DEVICE_NAME);
    if (n < 0 || n >= out_sz) return -1;
    return n;
}

static void render_qr(void)
{
    char uri[220];
    int uri_len, side, scale, qr_px, ox, oy, x, y;

    /* Full-white canvas doubles as the QR quiet zone (≥ 4 modules on every
     * side) since the QR is centered on a screen much larger than the code. */
    LCD_Clear(WHITE);

    uri_len = build_otpauth_uri(uri, sizeof(uri));
    if (uri_len < 0) return;

    side = qr_encode_bytes((const uint8_t *)uri, uri_len,
                           s_qrbuf, sizeof(s_qrbuf));
    if (side <= 0) return;

    /* Largest integer scale that fits (side + 8-module margin) in the LCD
     * height. Cap at 4 px/module — small QRs look silly pushed to 6+ px. */
    scale = 128 / (side + 8);
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;

    qr_px = side * scale;
    ox    = (LCD_SCREEN_WIDTH  - qr_px) / 2;
    oy    = (LCD_SCREEN_HEIGHT - qr_px) / 2;

    for (y = 0; y < side; y++)
    {
        for (x = 0; x < side; x++)
        {
            if (qr_get_module(s_qrbuf, side, x, y))
            {
                LCD_Fill((u16)(ox + x * scale),
                         (u16)(oy + y * scale),
                         (u16)(ox + x * scale + scale - 1),
                         (u16)(oy + y * scale + scale - 1),
                         BLACK);
            }
        }
    }
}

static void enter_qr_mode(void)
{
    s_mode = GUI_MODE_QR;
    render_qr();
}

static void enter_code_mode(void)
{
    s_mode = GUI_MODE_CODE;
    LCD_Clear(C_WHITE);
    invalidate_all_caches();
    force_redraw_widgets();
}

static void poll_keys(void)
{
    uint8_t pressed;

    /* Atomic-ish read/clear — `key` is a single byte written by an ISR.
     * Even if a second press races in between, missing one of the same
     * code in a 250 ms window is benign. */
    pressed = key;
    if (pressed == 0) return;
    key = 0;

    if (s_mode == GUI_MODE_CODE)
    {
        /* KEY3 → QR, gated on having a key AND a time. `pressed == 5` covers
         * the xpt2046 alt-mode mapping after a prior KEY1 press toggled
         * key_flag — we accept either so the UX isn't surprising. */
        if ((pressed == 3 || pressed == 5) &&
            app_totp_ble_get_keylen() > 0 && app_rtc_is_time_set())
        {
            enter_qr_mode();
        }
    }
    else /* GUI_MODE_QR */
    {
        if (pressed == 2 || pressed == 4)
        {
            enter_code_mode();
        }
    }
}

void app_gui_init(void *screen)
{
    sc_create_canvas(screen, &canvas, 0, 0,
                     LCD_SCREEN_WIDTH, LCD_SCREEN_HEIGHT,
                     LCD_SCREEN_WIDTH, LCD_SCREEN_HEIGHT,
                     ALIGN_NONE);

    /* Title bar */
    sc_create_frame(&canvas, &title_frame, 0, 0, LCD_SCREEN_WIDTH, 16,
                    C_NAVY, C_NAVY, 0, 0, ALIGN_NONE);
    sc_create_label(&canvas, &title_label, 0, 1, 120, 14, ALIGN_NONE);
    sc_set_label_text(&title_label, "N32 TOTP", C_WHITE, &lv_font_12, ALIGN_CENTER);
    sc_create_label(&canvas, &ble_label, 120, 1, 40, 14, ALIGN_NONE);
    sc_set_label_text(&ble_label, "BLE-", C_DIM_GRAY, &lv_font_12, ALIGN_CENTER);

    /* Three codes */
    sc_create_label(&canvas, &prev_label, 0, 18, LCD_SCREEN_WIDTH, 16, ALIGN_NONE);
    sc_set_label_text(&prev_label, "------", C_DIM_GRAY, &lv_font_16, ALIGN_CENTER);

    sc_create_label(&canvas, &curr_label, 0, 38, LCD_SCREEN_WIDTH, 20, ALIGN_NONE);
    sc_set_label_text(&curr_label, "------", C_WHITE, &lv_font_20, ALIGN_CENTER);

    sc_create_label(&canvas, &next_label, 0, 62, LCD_SCREEN_WIDTH, 16, ALIGN_NONE);
    sc_set_label_text(&next_label, "------", C_DIM_GRAY, &lv_font_16, ALIGN_CENTER);

    /* Progress bar */
    sc_create_slider(&canvas, &progress,
                     8, 82, LCD_SCREEN_WIDTH - 16, 6,
                     C_LIME_GREEN, C_DIM_GRAY, 2, 2, 0, ALIGN_NONE);

    /* Status row (countdown / prompt) */
    sc_create_label(&canvas, &status_label, 0, 92, LCD_SCREEN_WIDTH, 12, ALIGN_NONE);
    sc_set_label_text(&status_label, "Send key + time", C_ORANGE, &lv_font_12, ALIGN_CENTER);

    /* Debug row — last BLE RX summary */
    sc_create_label(&canvas, &debug_label, 0, 108, LCD_SCREEN_WIDTH, 12, ALIGN_NONE);
    sc_set_label_text(&debug_label, "A0 R0", C_DIM_GRAY, &lv_font_12, ALIGN_CENTER);
}

void app_gui_task(void *arg)
{
    Event *e = (Event *)arg;

    if (e->type != EVENT_TYPE_TIMER)
    {
        return;
    }

    /* Edge-detect KEY2/KEY3 first — this is also how we return from QR. */
    poll_keys();

    /* QR screen bypasses SCGUI — widgets stay un-dirty so
     * sc_widget_draw_screen won't overpaint the QR. */
    if (s_mode == GUI_MODE_QR)
    {
        return;
    }

    render_ble_status();
    render_debug();

    if (app_totp_ble_get_keylen() == 0 || !app_rtc_is_time_set())
    {
        /* Reset the title back to the product name until time is seeded. */
        if (s_cached_time_sod != 0xFFFFFFFFu)
        {
            s_cached_time_sod = 0xFFFFFFFFu;
            sc_set_label_text(&title_label, "N32 TOTP", C_WHITE, &lv_font_12, ALIGN_CENTER);
        }
        render_waiting();
        return;
    }

    uint64_t now = app_rtc_get_unix();
    render_title_clock(now);
    render_running(now);
}
