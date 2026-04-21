/**
 * TOTP demo screen layout (160 × 128):
 *   y=0..14   : title bar + BLE status
 *   y=18..34  : previous code (font_16)
 *   y=38..58  : current code  (font_20, bold)
 *   y=62..78  : next code     (font_16)
 *   y=82..88  : progress bar  (6 px)
 *   y=92..104 : status text   (font_12)
 *   y=108..120: BLE RX debug  (font_12) — last write's count/len/hex
 */
#include "app_gui.h"
#include "app_totp.h"
#include "app_rtc.h"
#include "app_profile/app_totp_ble.h"

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
