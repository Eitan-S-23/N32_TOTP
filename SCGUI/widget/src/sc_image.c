#include "sc_obj_widget.h"

// 绘制回调
 void image_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Image *Image = (Obj_Image*)obj;
    SC_pfb_Image(dest,obj->abs.xs, obj->abs.ys,Image->src,255);
}

// 按键回调
 bool image_handle_key(Obj_t *obj, int cmd)
{
    // 框架函数，无具体实现
    return false;
}

// 触摸回调
// bool image_handle_touch(Obj_t *obj, int16_t x, int16_t y)
//{
//    bool hit =  sc_obj_area_touch(&obj->abs, x, y);
//    return hit;
//}

// 虚函数表
 const ObjVtbl image_vtbl =
{
    .handle_draw = image_handle_draw,
    .handle_touch = NULL,
    .handle_key = image_handle_key,
};


///创建图片
void sc_create_image(void* parent, Obj_Image* image,const SC_img_t *src,int x, int y, SC_Align  align)
{
    if(sc_obj_create(parent,image,IMAGE,&image_vtbl))
    {
        image->src=src;
        sc_obj_set_geometry(image,  x,  y, src->w,  src->h, align);
    }
}

