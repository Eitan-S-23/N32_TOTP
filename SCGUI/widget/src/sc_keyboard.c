
#include "sc_obj_widget.h"

// 绘制回调
void keyboard_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Keyboard* m=(Obj_Keyboard*) obj;
    SC_Keyboard  *p=&m->Keyboard;
    SC_pfb_RoundFrame(dest,m->key_rect.xs, m->key_rect.ys,m->key_rect.xe, m->key_rect.ye,4,4,gui->fc,gui->bc);
    if(p->last_cursor==0xff)
    {
        for(int i=0; i<p->key_cnt; i++)
        {
            SC_key_layout_draw(dest,p,i,obj->Flag&OBJ_FLAG_CLICK);
        }
    }
    else
    {
        SC_key_layout_draw(dest,p, p->last_cursor,obj->Flag&OBJ_FLAG_CLICK);
        if(dest->stup==dest->num-1)       ///绘完最后一帧
        {
            if(p->last_cursor!=p->cursor)
            {
                obj->abs = p->abs[p->cursor];
                sc_obj_set_Active(obj,1);
                p->last_cursor=p->cursor;
            }
            else
            {
                obj->abs = m->key_rect;
                p->last_cursor=0xff;
            }
        }
    }
}
// 按键回调
bool keyboard_handle_key(Obj_t *obj, int cmd)
{
    Obj_Keyboard* m=(Obj_Keyboard*)obj;
    SC_Keyboard  *p= &m->Keyboard;
    switch(cmd)
    {
    case   CMD_UP:
        p->last_cursor=  p->cursor;
        p->cursor=(p->cursor-9 +p->key_cnt)%p->key_cnt;
        break;
    case   CMD_DOWN:
        p->last_cursor=  p->cursor;
        p->cursor=(p->cursor+9)%p->key_cnt;
        break;
    case  CMD_LEFT:
        p->last_cursor=  p->cursor;
        p->cursor=(p->cursor-1+p->key_cnt)%p->key_cnt;
        break;
    case CMD_RIGHT:
        p->last_cursor=  p->cursor;
        p->cursor=(p->cursor+1)%p->key_cnt;
        break;
    case CMD_ENTER:
        p->last_cursor=  p->cursor;
        obj->Flag|= OBJ_FLAG_CLICK;
        break;
    case 0xff:
        p->last_cursor=  p->cursor;
        obj->Flag&= ~OBJ_FLAG_CLICK;
        break;
    }
    if(p->last_cursor!=0xff)
    {
        obj->abs = p->abs[p->last_cursor];
        obj->Flag|= OBJ_FLAG_ACTIVE;
    }
    return false;
}

// 触摸回调
bool keyboard_handle_touch(Obj_t *obj,ObjState state, int16_t x, int16_t y)
{
    Obj_Keyboard* m=(Obj_Keyboard*) obj;
    SC_Keyboard  *p=&m->Keyboard;
    char key_str[2]= {0};
    if(state==OBJ_STATE_CLICK)
    {
        int key_cnt= p->key_cnt;       //当前列表条目
        for(int i=0; i<key_cnt; i++)
        {
            if(sc_obj_area_touch(&p->abs[i], x, y))
            {
                p->last_cursor=p->cursor;
                obj->abs = p->abs[p->last_cursor];
                p->cursor = i;
                key_str[0]=p->head->kb_str[i];
                printf("key click %s\n",key_str);
                return true;
            }
        }
    }
    else   if(state==OBJ_STATE_REL)
    {
        p->last_cursor=p->cursor;
        obj->abs = p->abs[p->last_cursor];
    }
    return false;
}

// 虚函数表
const ObjVtbl keyboard_vtbl =
{
    .handle_draw =  keyboard_handle_draw,
    .handle_touch = keyboard_handle_touch,
    .handle_key =   keyboard_handle_key,
};

///创建菜单控件
void sc_create_keyboard(void* parent, Obj_Keyboard* obj,int x, int y, int w, int h,SC_Align  align)
{
    if(sc_obj_create(parent,obj,KEYBOARD,&keyboard_vtbl))
    {
        sc_obj_set_geometry(obj,  x,  y, w,  h,ALIGN_HOR|ALIGN_BOTTOM);
        obj->key_rect= obj->base.abs;
        SC_key_layout_init(&obj->Keyboard,obj->key_rect.xs, obj->key_rect.ys,obj->key_rect.xe, obj->key_rect.ye);
    }
}

#if 1
//带触摸框架使用例程
Obj_Keyboard keyboard;
void demo_keyboard_init(void *screen)
{
    sc_create_keyboard(screen,&keyboard,0, 0,320,120,ALIGN_HOR);
    sc_set_touch_cb(&keyboard, NULL,OBJ_STATE_CLICK);
}
void demo_keyboard_task(void *arg)
{
    Event *e= (Event*)arg;
    //接收所有事件
    switch (e->type)
    {
    case EVENT_TYPE_INIT:

        break;
    case EVENT_TYPE_CMD:
        sc_widget_cmd_event(&keyboard,e->dat.cmd);
        break;
    default:
        break;
    }
}

#endif // 1


