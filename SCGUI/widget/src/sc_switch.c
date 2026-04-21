#include "sc_obj_widget.h"

// 绘制回调
 void swhtch_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Sw *sw = (Obj_Sw*)obj;
    SC_AREA *abs=&obj->abs;
    uint8_t Flag=obj->Flag&OBJ_FLAG_CLICK?1:0;
    gui->alpha=sw->alpha;
    SC_pfb_switch(dest,abs->xs, abs->ys,abs->xe,abs->ye,sw->ac,sw->bc, Flag);
}

// 按键回调
 bool swhtch_handle_key(Obj_t *obj, int cmd)
{
    // 框架函数，无具体实现
    return false;
}

// 触摸回调
// bool swhtch_handle_touch(Obj_t *obj, int16_t x, int16_t y)
//{
//    bool hit =  sc_obj_area_touch(&obj->abs, x, y);
//    return hit;
//}

// 虚函数表
const  ObjVtbl swhtch_vtbl =
{

    .handle_draw = swhtch_handle_draw,
    .handle_touch = NULL,
    .handle_key = swhtch_handle_key,
};

///创建开关
void sc_create_switch(void* parent, Obj_Sw* obj,int x, int y, int w, int h,uint16_t bc,uint16_t ac, SC_Align  align)
{
    if(sc_obj_create(parent,obj,SWITCH,&swhtch_vtbl))
    {
        obj->bc=bc;   //填充色
        obj->ac=ac;   //边界色
        obj->alpha=255;
        sc_obj_set_geometry(obj,  x,  y, w, h, align);
    }
}

///设置开关样式
void sc_set_switch_style(Obj_Sw *sw, uint16_t bc, uint16_t ac)
{
    sw->bc=bc;   //填充色
    sw->ac=ac;   //边界色
}
