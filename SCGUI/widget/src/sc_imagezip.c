#include "sc_obj_widget.h"

// 绘制回调
 void imagezip_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Imagezip *zip = (Obj_Imagezip*)obj;
    SC_pfb_Image_zip(dest,obj->abs.xs, obj->abs.ys,zip->src,&zip->dec);
}

// 按键回调
 bool imagezip_handle_key(Obj_t *obj, int cmd)
{
    // 框架函数，无具体实现
    return false;
}

// 触摸回调
// bool imagezip_handle_touch(Obj_t *obj, int16_t x, int16_t y)
//{
//    bool hit =  sc_obj_area_touch(&obj->abs, x, y);
//    return hit;
//}

// 虚函数表
const  ObjVtbl imagezip_vtbl =
{

    .handle_draw = imagezip_handle_draw,
    .handle_touch = NULL,
    .handle_key = imagezip_handle_key,
};


///创建ZIP图片
void sc_create_imagezip(void* parent, Obj_Imagezip* zip,const SC_img_zip *src,int x, int y, SC_Align  align)
{
    if(sc_obj_create(parent,zip,IMAGEZIP,&imagezip_vtbl))
    {
        zip->src=src;
        sc_obj_set_geometry(zip,  x,  y, src->w,  src->h, align);
    }
}
