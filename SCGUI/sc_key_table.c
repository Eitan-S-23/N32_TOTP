

#include "sc_key_table.h"

const key_Info_t   key_table[3]=
{
    {
        .kb_str="qwertyuiopasdfghjkl#zxcvbnm,0123456789",
        .keys_per_row=(const uint8_t[])
        {
            10, 9, 9,10
        },
        .rows = 4,
    },
    {
        .kb_str="7894561230.",
        .keys_per_row=(const uint8_t[])
        {
            3, 3, 3, 3
        },
        .rows = 4,
    },
};

//组合选框与文字
void SC_key_layout_draw(SC_tile *dest,SC_Keyboard *p,int indx,uint8_t state)
{
    SC_tile pfb;
    SC_AREA intersection;
    SC_AREA *abs=&p->abs[indx];
    if(dest==NULL)
    {
        pfb.stup=0;
        dest=&pfb;
_items_draw:
        SC_pfb_clip(dest,abs->xs,abs->ys,abs->xe,abs->ye,gui->bkc);//创建局部pfb
    }
    if(!SC_pfb_intersection(dest,&intersection,abs->xs,abs->ys,abs->xe,abs->ye))
    {
        return;
    }

    char key_str[2]= {0};
    if(indx<p->key_cnt)
    {
        uint16_t fc=(p->cursor==indx) ? C_RED:gui->fc;
        uint16_t bc=(p->cursor==indx&&state) ? C_RED:gui->bkc;
        SC_pfb_RoundFrame(dest,abs->xs,abs->ys,abs->xe,abs->ye,4,4,fc,bc);
        key_str[0]=p->head->kb_str[indx];
        SC_pfb_str(dest,abs->xs,abs->ys,key_str,gui->fc,gui->bkc,gui->font,NULL,ALIGN_CENTER);
    }
    if((dest==&pfb)&&SC_pfb_Refresh(&pfb,0))
    {
        goto _items_draw;
    }
}

void SC_key_layout_init(SC_Keyboard *p, int xs, int ys,int xe,int ye)
{
    uint16_t lcd_w=xe-xs;
    uint16_t lcd_h=ye-ys;
    uint16_t key_gap  = lcd_w/80;
    uint16_t top_gap  = lcd_h/40;
    p->head=key_table;
    p->key_cnt = 0;
    p->cursor =0;
    p->last_cursor=0xff;
    uint16_t key_w    = (lcd_w-key_gap)*0.90/10;        // 固定 9 % 屏宽
    uint16_t row_h    = (lcd_h-top_gap)/(p->head->rows);
    SC_AREA *abs=p->abs;
	  const char *str = p->head->kb_str;
    for (uint8_t r = 0; r < p->head->rows; ++r)
    {
        uint8_t n = p->head->keys_per_row[r];
        /* 居中留白 */
        uint16_t total = n * key_w + (n - 1) * key_gap;
        uint16_t left  = (lcd_w - total) / 2;
        for (uint8_t c = 0; c < n; c++)
        {
            abs->xs = left + c * (key_w + key_gap)+xs;
            abs->ys = ys+top_gap+(r*row_h);
            abs->xe = abs->xs + key_w;
            abs->ye = abs->ys + row_h * 0.9f;
            if(str[p->key_cnt]=='#')
            {
                abs->xs=p->abs[0].xs;
            }
            else  if(str[p->key_cnt]==',')
            {
                abs->xe=p->abs[9].xe;
            }
            p->key_cnt++;
						abs++;
        }
    }
}



