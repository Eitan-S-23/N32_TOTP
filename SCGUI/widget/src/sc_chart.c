#include "sc_obj_widget.h"

// 绘制回调
 void chart_handle_draw(Obj_t *obj, SC_tile *dest)
{
    Obj_Chart *chart = (Obj_Chart *)obj;
    SC_AREA *abs=&obj->abs;
    int w=abs->xe-abs->xs+1;
    int h=abs->ye-abs->ys+1;
    SC_pfb_chart(dest,abs->xs,abs->ys,w,h, chart->ac,chart->bc,chart->xd,chart->yd,chart->buf,sizeof(chart->buf)/sizeof(chart->buf[0]));
}
// 按键回调
 bool chart_handle_key(Obj_t *obj, int cmd)
{
    // 框架函数，无具体实现
    return false;
}

// 触摸回调
// bool chart_handle_touch(Obj_t *obj, int16_t x, int16_t y)
//{
//    bool hit =  sc_obj_area_touch(&obj->abs, x, y);
//    return hit;
//}

// 虚函数表
 const ObjVtbl chart_vtbl =
{

    .handle_draw =  chart_handle_draw,
    .handle_touch = NULL,
    .handle_key =   chart_handle_key,
};


///创建波形图
void sc_create_chart(void* parent, Obj_Chart* chart,int x, int y, int w, int h,uint16_t ac,uint16_t bc,uint8_t xd,uint8_t yd, SC_Align  align)
{
    if(sc_obj_create(parent,chart,CHART,&chart_vtbl))
    {
        int src_width=sizeof(chart->buf[0].src_buf)/2;   //SC_chart 数组默认为200
        chart->ac=ac;
        chart->bc=bc;
        chart->xd=xd;
        chart->yd=yd;
        if(w>src_width) w=src_width;
        sc_obj_set_geometry(chart,  x,  y, w, h, align);
    }
}

///示波器通道压入数据vol,scaleX缩放Q8格式
void sc_set_chart_vol(int ch,Obj_Chart *chart,uint16_t vol,int scaleX)
{
    if(sizeof(chart->buf)/sizeof(chart->buf[0])>ch)  //通道选择
    {
        SC_chart_put(&chart->buf[ch], vol,scaleX);
        chart->base.Flag|=OBJ_FLAG_ACTIVE;
    }
}
