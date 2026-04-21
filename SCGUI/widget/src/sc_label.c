#include "sc_obj_widget.h"

// 绘制回调
 void label_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Label* label = (Obj_Label*)obj;
    uint16_t bc= obj->Flag&OBJ_FLAG_CLICK?C_RED:gui->bkc;

    SC_pfb_label(dest,0,0,&obj->abs,&label->text,bc);
}
// 按键回调
 bool label_handle_key(Obj_t *obj, int cmd)
{
    // 框架函数，无具体实现
    return false;
}

// 触摸回调
// bool label_handle_touch(Obj_t *obj, int16_t x, int16_t y)
//{
//    bool hit =  sc_obj_area_touch(&obj->abs, x, y);
//    return hit;
//}

// 虚函数表
const  ObjVtbl label_vtbl =
{

    .handle_draw = label_handle_draw,
    .handle_touch = NULL,
    .handle_key = label_handle_key,
};



///创建Label控件
void sc_create_label(void* parent, Obj_Label* Label,int x, int y, int w, int h, SC_Align  align)
{
    if(sc_obj_create(parent,Label,LABEL,&label_vtbl))
    {
        sc_obj_set_geometry(Label,  x,  y,  w,  h, align);
    }
}

///设置Label字体
void sc_set_label_text(Obj_Label* Label,const char *str,uint16_t tc, lv_font_t* font,uint8_t align)
{
    SC_set_label_text(&Label->text,str,tc,font,align);
		Label->base.Flag|=  OBJ_FLAG_ACTIVE;
}
