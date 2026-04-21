#include "sc_obj_widget.h"

// 绘制回调
void line_edit_handle_draw(Obj_t *obj, SC_tile *dest)
{
    // 框架函数，无具体实现
}

// 按键回调
 bool line_edit_handle_key(Obj_t *obj, int cmd)
{
    // 框架函数，无具体实现
    return false;
}

// 触摸回调
// bool line_edit_handle_touch(Obj_t *obj, int16_t x, int16_t y)
//{
//    bool hit =  sc_obj_area_touch(&obj->abs, x, y);
//    return hit;
//}

// 虚函数表
const  ObjVtbl line_edit_vtbl =
{
    .handle_draw = line_edit_handle_draw,
    .handle_touch = NULL,
    .handle_key = line_edit_handle_key,
};

///创建line_edit控件
void sc_create_line_edit(void* parent, Obj_Line_edit* line_edit,int x, int y, int w, int h, SC_Align  align)
{
    if(sc_obj_create(parent,line_edit,LABEL,&line_edit_vtbl))
    {
        sc_obj_set_geometry(line_edit,  x,  y,  w,  h, align);
    }
}

///设置line_edit字体
void sc_set_line_edit_style(Obj_Line_edit* line_edit,const char *str,uint16_t tc, lv_font_t* font,uint8_t align)
{
  //  SC_set_label_text(&line_edit->text,str,tc,font,align);
}
