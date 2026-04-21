#ifndef SC_KEY_TABLE_H
#define SC_KEY_TABLE_H

#include "sc_gui.h"

typedef struct
{
    const char     *kb_str;
    const uint8_t  *keys_per_row;
    uint8_t        rows;
} key_Info_t;

typedef struct
{
    const      key_Info_t *head;
    uint8_t    cursor;
    uint8_t    last_cursor;
    uint8_t    key_cnt;
    SC_AREA    abs[40];   //320×Ö½ÚÓÃÓÚ´¥ÆÁ
} SC_Keyboard;

void SC_key_layout_init(SC_Keyboard *p, int xs, int ys,int xe,int ye);
void SC_key_layout_draw(SC_tile *dest,SC_Keyboard *p,int indx,uint8_t state);
#endif
