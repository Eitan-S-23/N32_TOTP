#include "sc_obj_widget.h"

// 绘制回调
void arc_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Arc *arc = (Obj_Arc*)obj;
    SC_AREA *abs= &obj->abs;
    SC_arc  temp;
    temp.cx=abs->xs+arc->r;
    temp.cy=abs->ys+arc->r;
    temp.r=arc->r;
    temp.ir=arc->ir;
    uint8_t Flag=obj->Flag&OBJ_FLAG_CLICK?1:0;
    gui->alpha=Flag? arc->alpha:128;
    SC_pfb_DrawArc(dest,&temp,arc->start_deg, arc->end_deg,arc->ac,arc->bc,arc->dot);
}

// 按键回调
bool arc_handle_key(Obj_t *obj, int cmd)
{
   // Obj_Arc* arc=(Obj_Arc*) obj;
    return false;
}

// 触摸回调
bool arc_handle_touch(Obj_t *obj, ObjState state,int16_t x, int16_t y)
{
    Obj_Arc* arc=(Obj_Arc*) obj;
    if(state==OBJ_STATE_CLICK)
    {
        int cx= x-(obj->abs.xe+obj->abs.xs)/2;
        int cy= y-(obj->abs.ye+obj->abs.xs)/2;
        int rmax=arc->r * arc->r;
        int rmin=arc->ir* arc->ir;
        int dxy= cx*cx+cy*cy;
        if(dxy>=rmin&&dxy<=rmax)
        {
           return true;
        }
    }
    return false;
}

// 虚函数表
const ObjVtbl arc_vtbl =
{
    .handle_draw = arc_handle_draw,
    .handle_touch = arc_handle_touch,
    .handle_key = arc_handle_key,
};

///创建圆弧
void sc_create_arc(void* parent,Obj_Arc *obj, int cx,int cy,int r, int ir, uint16_t ac,uint16_t bc, SC_Align  align)
{
    if(sc_obj_create(parent,obj,ARC,&arc_vtbl))
    {
        sc_obj_set_geometry(obj, cx,  cy, 2*r+1, 2*r+1, align);
        obj->r=r;
        obj->ir=ir;
        obj->ac=ac;
        obj->bc=bc;
        obj->start_deg=0;
        obj->end_deg=360;
        obj->alpha=255;
    }
}

///设置圆弧角度
void sc_set_arc_deg(Obj_Arc* obj,int start_deg,int end_deg,uint8_t dot)
{
    obj->start_deg=start_deg;
    obj->end_deg=end_deg;
    obj->dot = dot;
    obj->base.Flag|=  OBJ_FLAG_ACTIVE;
}




