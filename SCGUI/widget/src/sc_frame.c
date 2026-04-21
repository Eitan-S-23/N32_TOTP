#include "sc_obj_widget.h"
// 绘制回调
 void frame_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Frame* Frame = (Obj_Frame*)obj;
    SC_AREA *abs=&obj->abs;
    gui->alpha=Frame->alpha;

    SC_pfb_RoundFrame(dest, abs->xs, abs->ys,abs->xe, abs->ye,Frame->r,Frame->ir,Frame->ac,Frame->bc);
}
// 按键回调
 bool frame_handle_key(Obj_t *obj, int cmd)
{
    // 框架函数，无具体实现
    return false;
}

// 触摸回调
// bool frame_handle_touch(Obj_t *obj, int16_t x, int16_t y)
//{
//    bool hit =  sc_obj_area_touch(&obj->abs, x, y);
//    return hit;
//}

// 虚函数表
const ObjVtbl frame_vtbl =
{
    .handle_draw = frame_handle_draw,
    .handle_touch = NULL,
    .handle_key = frame_handle_key,
};

///创建矩形控件
void sc_create_frame(void* parent, Obj_Frame* Frame,int x, int y, int w, int h,uint16_t ac,uint16_t bc,uint8_t r,uint8_t ir, SC_Align  align)
{
    if(sc_obj_create(parent,Frame,FRAME,&frame_vtbl))
    {
        Frame->r=r;
        Frame->ir=ir;   //倒角
        Frame->bc=bc;   //填充色
        Frame->ac=ac;   //边界色
        Frame->alpha=255;
        sc_obj_set_geometry(Frame,  x,  y,  w,  h, align);
    }
}
///设置矩形样式
void sc_set_frame_style(Obj_Frame *frame, uint16_t bc, uint16_t ac, uint8_t r, uint8_t ir)
{
    frame->r=r;
    frame->ir=ir;   //倒角
    frame->ac=ac;   //边界色
    frame->bc=bc;   //填充色
}

