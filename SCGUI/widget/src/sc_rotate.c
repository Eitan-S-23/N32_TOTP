#include "sc_obj_widget.h"

// 绘制回调
 void rotate_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Rotate *Rotate = (Obj_Rotate*)obj;
    SC_pfb_transform(dest,Rotate->src,&Rotate->params);
}

// 按键回调
 bool rotate_handle_key(Obj_t *obj, int cmd)
{
    // 框架函数，无具体实现
    return false;
}

// 触摸回调
// bool rotate_handle_touch(Obj_t *obj, int16_t x, int16_t y)
//{
//    bool hit =  sc_obj_area_touch(&obj->abs, x, y);
//    return hit;
//}

// 虚函数表
const  ObjVtbl rotate_vtbl =
{

    .handle_draw = rotate_handle_draw,
    .handle_touch = NULL,
    .handle_key = rotate_handle_key,
};


///图片变换,设置底图和指针src的中心坐标
void sc_create_rotate(void* parent, Obj_Rotate* obj,const SC_img_t *src,int dx, int dy,int src_x,int src_y, SC_Align  align)
{
    Obj_t *p=(Obj_t *)parent;
    if(sc_obj_create(parent,obj,ROTATE,&rotate_vtbl))
    {
        obj->src=src;
        obj->params.scaleX   = 256;                 //缩放1.0
        obj->params.scaleY   = 256;
        obj->params.center_x = obj->src->w/2+src_x;  //源图中心
        obj->params.center_y = obj->src->h/2+src_y;
        SC_set_align(&p->abs,&dx,&dy, 0, 0, align);
        obj->move_x=dx-p->abs.xs;      //转为偏移量
        obj->move_y=dy-p->abs.ys;      //转为偏移量
        obj->base.abs=p->abs;        //首次坐标
        obj->params.last= &obj->base.abs; //下次计算脏矩阵
    }
}

///设置图片旋转
void sc_set_rotate_deg(Obj_Rotate* obj,int angle)
{
    Obj_t *parent=obj->base.parent;
    if(parent==NULL) return;
    if(sc_obj_area_intersect(&parent->abs,&gui->lcd_area))  //相交于屏幕
    {
        obj->params.move_x=parent->abs.xs+obj->move_x;   //跟随parent
        obj->params.move_y=parent->abs.ys+obj->move_y;
        SC_set_transform(&obj->params,obj->src,angle);
        obj->base.Flag |=  OBJ_FLAG_ACTIVE;
    }
}
