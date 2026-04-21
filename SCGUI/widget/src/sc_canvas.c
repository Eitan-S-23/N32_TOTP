#include "sc_obj_widget.h"
// 绘制回调
void canvas_handle_draw(Obj_t *obj, SC_tile *dest)
{
    SC_AREA *abs=&obj->abs;
    SC_pfb_RoundFrame(dest, abs->xs, abs->ys,abs->xe, abs->ye,1,1,C_GREEN,gui->bkc);
}
// 按键回调
bool canvas_handle_key(Obj_t *obj, int cmd)
{
    Obj_Canvas *Canvas= ( Obj_Canvas *)obj;
    int move_x=0;
    int move_y=0;
    int w=(obj->abs.xe-obj->abs.xs)+1;
    int h=(obj->abs.ye-obj->abs.ys)+1;
    switch(cmd)
    {
    case   CMD_UP:
        move_y=h;
        break;
    case   CMD_DOWN:
        move_y=-h;
        break;
    case  CMD_LEFT:
        move_x=-w;
        break;
    case CMD_RIGHT:
        move_x=w;
        break;
    }
    if((move_x>0&&Canvas->box.xs>=0)||(move_x<0&&Canvas->box.xs<= -Canvas->box.xe))
    {
        move_x=0;         //左右移边界
    }
    if((move_y>0&&Canvas->box.ys>=0)||(move_y<0&&Canvas->box.ys<= -Canvas->box.ye))
    {
        move_y=0;         //左右移边界
    }
    if(move_x!=0||move_y!=0)
    {
        g_dirty_area.xe=g_dirty_area.xs+move_x;
        g_dirty_area.ye=g_dirty_area.ys+move_y;
        return true;
    }
    return false;
}

// 触摸回调
bool canvas_handle_touch(Obj_t *obj,ObjState state, int16_t x, int16_t y)
{
    Obj_Canvas *Canvas= ( Obj_Canvas *)obj;
    int move_x=0,move_y=0;
    if(state==OBJ_STATE_CLICK)
    {
        return sc_obj_area_touch(&obj->abs, x, y);
    }
    else   if(state==OBJ_STATE_MOVXY)
    {
        if(obj->State&OBJ_STATE_MOVX)
        {
            move_x= x- gui->touch_x;
            if((move_x>0&&Canvas->box.xs>0)||(move_x<0&&Canvas->box.xs< -Canvas->box.xe))
            {
                move_x/=8;       //左右移边界
            }
        }
        if(obj->State&OBJ_STATE_MOVY)
        {
            move_y=y- gui->touch_y;
            if((move_y>0&&Canvas->box.ys>0)||(move_y<0&&Canvas->box.ys<-Canvas->box.ye))
            {
                move_y/=8;        //上下移边界
            }
        }
        if(sc_widget_move_pos(obj,move_x,move_y))
        {
            obj->touch_cb(obj,x,y,obj->State&OBJ_STATE_MOVXY);
        }
    }
    else   if(state==OBJ_STATE_REL)
    {
        if(g_dirty_area.xs||g_dirty_area.ys)          //有移动开启平移动画
        {
            int w=(obj->abs.xe-obj->abs.xs)+1;
            int h=(obj->abs.ye-obj->abs.ys)+1;
            if(SC_ABS(g_dirty_area.xs)>w/4)       //平移超过1/4
            {
                g_dirty_area.xe =g_dirty_area.xs>0? w:-w;  //前一页或后一页坐标
            }
            if(SC_ABS(g_dirty_area.ys)>h/4)
            {
                g_dirty_area.ye =g_dirty_area.ys>0? h:-h;  //上一页或下一页坐标
            }
            return true;
        }
    }
    return false;
}

// 虚函数表
const ObjVtbl canvas_vtbl =
{
    .handle_draw  = canvas_handle_draw,
    .handle_touch = canvas_handle_touch,
    .handle_key   = canvas_handle_key,
};

///创建画布
void sc_create_canvas(void* parent, Obj_Canvas* obj,int x, int y, int w, int h, int row,int column,SC_Align  align)
{
    if (sc_obj_create(parent,obj,CANVAS,&canvas_vtbl))
    {
        obj->box.xs=0;
        obj->box.ys=0;
        obj->box.xe=row-w;
        obj->box.ye=column-h;
        sc_obj_set_geometry(obj, x,  y, w, h, align);
    }
}
