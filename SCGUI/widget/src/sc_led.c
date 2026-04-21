#include "sc_obj_widget.h"

// 绘制回调
 void led_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Led *led = (Obj_Led*)obj;
    SC_AREA *abs=&obj->abs;
    uint16_t bc=obj->Flag&OBJ_FLAG_CLICK?led->bc:led->ac;
    gui->alpha=led->alpha;
    SC_pfb_led(dest,abs->xs, abs->ys,abs->xe,abs->ye, led->ac,bc);
}
// 按键回调
 bool led_handle_key(Obj_t *obj, int cmd)
{
    // 框架函数，无具体实现
    return false;
}

// 触摸回调
// bool led_handle_touch(Obj_t *obj, int16_t x, int16_t y)
//{
//    bool hit =  sc_obj_area_touch(&obj->abs, x, y);
//    return hit;
//}

// 虚函数表
const  ObjVtbl led_vtbl =
{

    .handle_draw = led_handle_draw,
    .handle_touch = NULL,
    .handle_key = led_handle_key,
};

///创建LED
void sc_create_led(void* parent, Obj_Led* led,int x, int y, int w, int h,uint16_t bc,uint16_t ac, SC_Align  align)
{
    if(sc_obj_create(parent,led,SC_LED,&led_vtbl))
    {
        led->bc=bc;   //填充色
        led->ac=ac;   //边界色
        led->alpha=255;
        sc_obj_set_geometry(led,  x,  y, w, h, align);
    }
}

///设置LED样式
void sc_set_led_style(Obj_Led *led, uint16_t bc, uint16_t ac)
{
    led->ac=gui->fc;   //边界色
    led->bc=gui->bc;
}
