#include "sc_obj_widget.h"

// 绘制回调
 void but_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_But *but = (Obj_But*)obj;
    SC_AREA *abs=&obj->abs;
    uint8_t Flag=obj->Flag&OBJ_FLAG_CLICK?1:0;
    gui->alpha=but->alpha;
    SC_pfb_button(dest,&but->text,abs->xs, abs->ys,abs->xe, abs->ye,but->ac,but->bc,but->r,but->ir,Flag);
}
// 按键回调
 bool but_handle_key(Obj_t *obj, int cmd)
{
    // 框架函数，无具体实现
    return false;
}

// 触摸回调
// bool but_handle_touch(Obj_t *obj, int16_t x, int16_t y)
//{
//    bool hit =  sc_obj_area_touch(&obj->abs, x, y);
//    return hit;
//}

// 虚函数表
const  ObjVtbl but_vtbl =
{

    .handle_draw = but_handle_draw,
    .handle_touch = NULL,
    .handle_key = but_handle_key,
};

///创建按键控件
void sc_create_button(void* parent, Obj_But* button,int x, int y, int w, int h,uint16_t bc,uint16_t ac,uint8_t r,uint8_t ir, SC_Align  align)
{
    if(sc_obj_create(parent,button,BUTTON,&but_vtbl))
    {
        button->r=r;
        button->ir=ir;   //倒角
        button->bc=bc;   //填充色
        button->ac=ac;   //边界色
        button->alpha=255;
        sc_obj_set_geometry(button,  x,  y,  w,  h, align);
    }
}
///设置按键字体
void sc_set_button_text(Obj_But* button, const char *str,uint16_t tc, lv_font_t* font,uint8_t align)
{
    SC_set_label_text(&button->text,str,tc,font,align);
}
