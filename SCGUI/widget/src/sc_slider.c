#include "sc_obj_widget.h"

// 绘制回调
void slider_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Slider *slider = (Obj_Slider*)obj;
    SC_AREA *abs=&obj->abs;
    SC_pfb_RoundBar(dest,abs->xs,abs->ys,abs->xe,abs->ye,slider->r,slider->ir,slider->ac,slider->bc,slider->value,100);
}

// 按键回调
bool slider_handle_key(Obj_t *obj, int cmd)
{
    // 框架函数，无具体实现
    return false;
}

// 触摸回调
bool slider_handle_touch(Obj_t *obj,ObjState state, int16_t x, int16_t y)
{
    Obj_Slider  *slider=(Obj_Slider*) obj;
    bool hit = false;
    if(state==OBJ_STATE_CLICK)
    {
        hit= sc_obj_area_touch(&obj->abs, x, y);
    }
    if(state==OBJ_STATE_MOVXY||hit)
    {
        int w=(obj->abs.xe-obj->abs.xs);
        int h=(obj->abs.ye-obj->abs.ys);
        if(w>h)
            slider->value= (x-obj->abs.xs)*100/w;
        else
            slider->value= (obj->abs.ye-y)*100/h;
        obj->Flag=OBJ_FLAG_ACTIVE;
    }
    return hit;
}
// 虚函数表
const ObjVtbl slider_vtbl =
{
    .handle_draw = slider_handle_draw,
    .handle_touch = slider_handle_touch,
    .handle_key = slider_handle_key,
};

///创建进度条
void sc_create_slider(void* parent, Obj_Slider* slider,int x, int y, int w, int h,uint16_t bc,uint16_t ac,uint8_t r,uint8_t ir,int value, SC_Align  align)
{
    if(sc_obj_create(parent,slider,SLIDER,&slider_vtbl))
    {
        slider->r=r;
        slider->ir=ir;   //倒角
        slider->bc=bc;   //填充色
        slider->ac=ac;   //边界色
        slider->value=value;
        sc_obj_set_geometry(slider,  x,  y, w, h, align);
    }
}
///设置进度条参数
void sc_set_slider_value(Obj_Slider *slider, int value)
{
    slider->value=value;
}
