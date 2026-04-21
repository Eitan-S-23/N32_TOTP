
#include "sc_obj_widget.h"

// 绘制回调
void menu_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Menu* m=(Obj_Menu*) obj;
    SC_Menu  *p=&m->menu;

    // 一次性绘制所有菜单项
    for(int i = 0; i < m->draw_cnt && i < p->head->items_cnt; i++)
    {
        sc_menu_items_draw(dest, p, i);
    }

    // 重置绘制状态，为下次绘制做准备
    m->draw_st = 0;
}
// 按键回调
bool menu_handle_key(Obj_t *obj, int cmd)
{
    Obj_Menu* m=(Obj_Menu*)obj;
    SC_Menu  *p= &m->menu;

    // 检查菜单是否正确初始化
    if (p->head == NULL || p->head->items_cnt == 0)
    {
        return false;  // 菜单未初始化或无项目，不处理按键
    }

    int  cursor_cnt=-1;
    uint8_t *cursor= &p->cursor[p->level];  //记录每一级的光标
    if (cmd==CMD_DOWN)
    {
        cursor_cnt = p->head->items_cnt;
        if(cursor_cnt>0)
        {
            *cursor = (*cursor + 1)%cursor_cnt;
           // printf("[Menu] 下移，当前选中: %s\n", sc_menu_items(p,*cursor));
        }
    }
    else if(cmd==CMD_UP)
    {
        cursor_cnt = p->head->items_cnt;
        if(cursor_cnt>0)
        {
            *cursor = (*cursor - 1 + cursor_cnt) %cursor_cnt;
           // printf("[Menu] 上移，当前选中: %s\n", sc_menu_items(p,*cursor));
        }
    }
    else if(cmd==CMD_BACK)
    {
        cursor_cnt = p->head->items_cnt;
        sc_menu_parent(p);
    }
    else  if(cmd==CMD_ENTER)
    {
        cursor_cnt = p->head->items_cnt;
        sc_menu_child(p);
    }
    if(cursor_cnt!=-1)    //有事件
    {
        // 更新绘制计数为当前菜单的项目数（如果菜单项数发生了变化）
        if(p->head->items_cnt != m->draw_cnt)
        {
            m->draw_cnt = p->head->items_cnt;
        }
        if(m->draw_cnt > 0)
        {
            m->draw_st=0;
            obj->abs = p->abs[0];
            sc_obj_set_Active(obj,1);                      //刷新
        }
        return true;
    }
    return false;
}

// 触摸回调
bool menu_handle_touch(Obj_t *obj,ObjState state, int16_t x, int16_t y)
{
    Obj_Menu* m=(Obj_Menu*) obj;
    SC_Menu  *p=&m->menu;

    // 检查菜单是否正确初始化
    if (p->head == NULL || p->head->items_cnt == 0)
    {
        return false;  // 菜单未初始化或无项目，不处理触摸
    }

    if(state==OBJ_STATE_CLICK)
    {
        int items_cnt=p->head->items_cnt;       //当前列表条目
        for(int i=0;i<items_cnt;i++)
        {
           if(sc_obj_area_touch(&p->abs[i], x, y))
           {
              if( p->cursor[p->level]!=i)
              {
                 p->cursor[p->level]=i;
                 sc_obj_set_Active(obj,1);  // 光标位置改变时触发重绘
              }
              else
              {
                 sc_menu_child(p);
                 // 更新绘制计数为当前菜单的项目数（如果菜单项数发生了变化）
                 if(p->head->items_cnt != m->draw_cnt)
                 {
                     m->draw_cnt = p->head->items_cnt;
                 }
                 m->draw_st=0;
                 sc_obj_set_Active(obj,1);  // 触发重绘
              }
              return true;
           }
        }
    }
    return false;
}

// 虚函数表
const ObjVtbl menu_vtbl =
{
    .handle_draw =  menu_handle_draw,
    .handle_touch = menu_handle_touch,
    .handle_key =   menu_handle_key,
};

///创建菜单控件
void sc_create_menu(void* parent, Obj_Menu* obj,int x, int y, int w, int h,SC_Align  align)
{
    if(sc_obj_create(parent,obj,MENU,&menu_vtbl))
    {
        // 设置菜单控件的位置和大小
        sc_obj_set_geometry(obj, x, y, w, h, align);
        // 初始化菜单绘制状态
        obj->draw_st = 0;
        sc_menu_init(&obj->menu,x,y,w,25);  // 设置菜单项高度为25像素
        // 设置绘制计数为实际菜单项数量
        obj->draw_cnt = obj->menu.head->items_cnt;
    }
}

#if 1

//带触摸框架使用例程
Obj_Menu ui_menu;
Obj_Arc  arc1;
void demo_menu_init(void *screen)
{
    sc_create_arc(screen,&arc1, 0,40,75, 60, C_SANDY_BROWN,gui->bc,ALIGN_CENTER);
    sc_create_menu(screen,&ui_menu,0, 50,120,25,ALIGN_HOR);
    sc_set_touch_cb(&arc1, NULL,OBJ_STATE_BOOL);
    sc_set_touch_cb(&ui_menu, NULL,OBJ_STATE_BOOL);
}
void demo_menu_task(void *arg)
{
    Event *e= (Event*)arg;
    //接收所有事件
    switch (e->type)
    {
    case EVENT_TYPE_INIT:

        break;
    case EVENT_TYPE_CMD:
        sc_widget_cmd_event(&ui_menu,e->dat.cmd);
        break;
    default:
        break;
    }
}

#endif // 1

