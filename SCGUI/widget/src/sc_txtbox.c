#include "sc_obj_widget.h"

// 绘制回调
void txtbox_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Txtbox *Txtbox = (Obj_Txtbox*)obj;
    SC_pfb_text_box(dest,&obj->abs,-Txtbox->box.xs,-Txtbox->box.ys,&Txtbox->text);
}

// 按键回调
bool txtbox_handle_key(Obj_t *obj, int cmd)
{
    // 框架函数，无具体实现
    return false;
}

// 触摸回调
bool txtbox_handle_touch(Obj_t *obj,ObjState state, int16_t x, int16_t y)
{
    Obj_Txtbox *Txtbox= ( Obj_Txtbox *)obj;
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
            if((move_x>0&&Txtbox->box.xs>0)||(move_x<0&&Txtbox->box.xs< -Txtbox->box.xe))
            {
                move_x/=8;       //左右移边界
            }
        }
        if(obj->State&OBJ_STATE_MOVY)
        {
            move_y=y- gui->touch_y;
            if((move_y>0&&Txtbox->box.ys>0)||(move_y<0&&Txtbox->box.ys<-Txtbox->box.ye))
            {
                move_y/=8;        //上下移边界
            }
        }
        if(sc_widget_move_pos(obj,move_x,move_y))
        {
            obj->touch_cb(obj,x,y,obj->State&OBJ_STATE_MOVXY);
        }
    }
    else
    {
//        if(g_dirty_area.xs||g_dirty_area.ys)          //有移动开启平移动画
//        {
//            int w=(obj->abs.xe-obj->abs.xs)+1;
//            int h=(obj->abs.ye-obj->abs.ys)+1;
//            if(SC_ABS(g_dirty_area.xs)>w/4)       //平移超过1/4
//            {
//                g_dirty_area.xe =g_dirty_area.xs>0? w:-w;  //前一页或后一页坐标
//            }
//            if(SC_ABS(g_dirty_area.ys)>h/4)
//            {
//                g_dirty_area.ye =g_dirty_area.ys>0? h:-h;  //上一页或下一页坐标
//            }
//            return true;
//        }
    }
    return false;
}

// 虚函数表
const ObjVtbl txtbox_vtbl =
{
    .handle_draw =  txtbox_handle_draw,
    .handle_touch = txtbox_handle_touch,
    .handle_key =   txtbox_handle_key,
};


///创建Txtbox控件
void sc_create_txtbox(void* parent, Obj_Txtbox* obj,int x, int y, int w, int h,int row,int column,SC_Align  align)
{
    if(sc_obj_create(parent,obj,TEXTBOX,&txtbox_vtbl))
    {
        obj->box.xs=0;
        obj->box.ys=0;
        obj->box.xe=row-w;
        obj->box.ye=column-h;
        sc_obj_set_geometry(obj, x,  y, w, h, align);
    }
}

///设置Txtbox字体
void sc_set_txtbox_text(Obj_Txtbox* txtbox,const char *str,uint16_t tc, lv_font_t* font,uint8_t align)
{
    SC_set_label_text(&txtbox->text,str,tc,font,align);
}
