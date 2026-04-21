#ifndef SC_MENU_TABLE_H
#define SC_MENU_TABLE_H

#include "sc_gui.h"

typedef struct LangItem
{
    const char *en;  // 英文
    const char *ch;  // 中文
} LangItem;

//菜单项结构体
typedef struct
{
    const char  *title;         //标题
    const char  *parent;
    const LangItem  *items;     //菜单项数组
    uint8_t      items_cnt;     //菜单数量
    void  (*items_cb)(void *arg);
} Menu_t;

typedef struct
{
    const Menu_t  *head;
    uint8_t  cursor[5];
    uint8_t  level;
    uint8_t  lange;
    SC_AREA  abs[5];   //最大5条
    uint16_t fc;
    uint16_t bc;
} SC_Menu;

///字符串匹配函数
static inline int sc_strcmp(const char * s1, const char * s2)
{
    while(*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

const char* sc_menu_items(SC_Menu *p,int indx);

void sc_menu_parent(SC_Menu *p);

void sc_menu_child(SC_Menu *p);

void sc_menu_items_draw(SC_tile *dest,SC_Menu *p,int indx);

void sc_menu_init(SC_Menu *p,int xs,int ys,int w,int h);

void test_menu_task(void *arg);

#endif  // MENU_H
