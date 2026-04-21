#ifndef SC_OBJ_WIDGET_H
#define SC_OBJ_WIDGET_H

#include "sc_obj_list.h"
#include "sc_event_task.h"
#include "sc_menu_table.h"
#include "sc_key_table.h"

///画布
typedef struct
{
    Obj_t  base;
    SC_AREA  box;
} Obj_Canvas;

///矩形
typedef struct
{
    Obj_t base;
    uint16_t bc;            //填充色
    uint16_t ac;            //边界色
    uint8_t r;
    uint8_t ir;
    uint8_t  alpha;
} Obj_Frame;

///标签控件
typedef struct
{
    Obj_t base;
    SC_label text;
} Obj_Label;

///输入框
typedef struct
{
    Obj_t base;
    lv_font_t*    font;
    const char    str[64];
    uint16_t      tc;//字体色
} Obj_Line_edit;

///文本框控件
typedef struct
{
    Obj_t base;
    SC_AREA box;
    SC_label text;
} Obj_Txtbox;

/// 按钮控件
typedef struct
{
    Obj_t    base;
    SC_label text;
    uint16_t bc;            //填充色
    uint16_t ac;            //边界色
    uint8_t r;
    uint8_t ir;
    uint8_t  alpha;
} Obj_But;

///LED控件
typedef struct
{
    Obj_t base;
    uint16_t bc;            //填充色
    uint16_t ac;            //边界色
    uint8_t  alpha;
} Obj_Led;

///开关控件
typedef struct
{
    Obj_t base;
    uint16_t bc;            //填充色
    uint16_t ac;            //边界色
    uint8_t  alpha;
} Obj_Sw;

/// 进度条控件
typedef struct
{
    Obj_t base;             // 基础控件
    int value;              // 滑块当前值
    uint16_t bc;            //填充色
    uint16_t ac;            //边界色
    uint8_t r;
    uint8_t ir;
    uint8_t  alpha;
} Obj_Slider;

///图片控件
typedef struct
{
    Obj_t base;
    const SC_img_t *src;
} Obj_Image;

///图片zip
typedef struct
{
    Obj_t base;
    const SC_img_zip *src;
    SC_dec_zip        dec;
} Obj_Imagezip;

///圆弧
typedef struct
{
    Obj_t base;
    uint16_t bc;            //填充色
    uint16_t ac;            //边界色
    uint8_t r;
    uint8_t ir;
    uint8_t alpha;
    uint8_t dot;        //端点
    int16_t start_deg;    //角度
    int16_t end_deg;      //角度
} Obj_Arc;

///图片变换
typedef struct
{
    Obj_t base;
    const SC_img_t *src;
    Transform    params;
    int16_t     move_x;
    int16_t     move_y;
} Obj_Rotate;

///示波器控件
typedef struct
{
    Obj_t base;             //基础控件
    SC_chart  buf[2];       //波形数量
    uint16_t bc;            //填充色
    uint16_t ac;            //边界色
    uint8_t xd;
    uint8_t yd;
} Obj_Chart;

//菜单项结构体
typedef struct
{
    Obj_t    base;                 //基础控件
    SC_Menu  menu;
    uint8_t  draw_st;
    uint8_t  draw_cnt;
} Obj_Menu;

typedef struct
{
    Obj_t        base;                 //基础控件
    SC_Keyboard  Keyboard;
    SC_AREA      key_rect;
} Obj_Keyboard;


extern SC_AREA g_dirty_area;

///部件移动坐标
int sc_widget_move_pos(Obj_t* obj,int move_x,int move_y);
///部件按键事件
void sc_widget_cmd_event(void *vobj,int cmd);

///部件触控事件
void sc_widget_touch_event(void* screen, int x, int y,uint8_t state);

///部件遍历刷新
void sc_widget_draw_screen(void *screen);


///设置触摸回调函数
static inline void sc_set_touch_cb(void* vobj,void (*touch_cb)(Obj_t*, int, int,uint8_t),uint8_t State)
{
    Obj_t*obj=(Obj_t*)vobj;
    obj->touch_cb=touch_cb;
    obj->State|=State;
}
static inline void sc_touch_cb( Obj_t* obj, int x, int y,uint8_t state)
{
    if(obj->touch_cb)
    {
        obj->touch_cb(obj,x,y,state);
    }
}

///创建屏幕
static inline void sc_create_screen(Obj_t* parent, Obj_t* obj,int x, int y, int w, int h, SC_Align  align)
{
    if(sc_obj_create(parent,obj,ARER,NULL))
    {
        sc_obj_set_geometry(obj,  x,  y,  w,  h, align);
    }
}

///创建画布
void sc_create_canvas(void* parent, Obj_Canvas* obj,int x, int y, int w, int h, int row,int column,SC_Align  align);

///创建矩形控件
void sc_create_frame(void* parent, Obj_Frame* Frame,int x, int y, int w, int h,uint16_t ac,uint16_t bc,uint8_t r,uint8_t ir, SC_Align  align);

///设置矩形样式
void sc_set_frame_style(Obj_Frame *frame, uint16_t bc, uint16_t ac, uint8_t r, uint8_t ir);

///创建进度条
void sc_create_slider(void* parent, Obj_Slider* slider,int x, int y, int w, int h,uint16_t bc,uint16_t ac,uint8_t r,uint8_t ir,int value, SC_Align  align);

///设置进度条参数
void sc_set_slider_value(Obj_Slider *slider, int value);

///创建Label控件
void sc_create_label(void* parent, Obj_Label* Label,int x, int y, int w, int h, SC_Align  align);

///设置Label字体
void sc_set_label_text(Obj_Label* Label,const char *str,uint16_t tc, lv_font_t* font,uint8_t align);

///创建Txtbox控件
void sc_create_txtbox(void* parent, Obj_Txtbox* obj,int x, int y, int w, int h,int row,int column,SC_Align  align);

///设置Txtbox字体
void sc_set_txtbox_text(Obj_Txtbox* txtbox,const char *str,uint16_t tc, lv_font_t* font,uint8_t align);

///创建圆弧
void sc_create_arc(void* parent,Obj_Arc *obj, int cx,int cy,int r, int ir, uint16_t ac,uint16_t bc, SC_Align  align);

///设置圆弧角度
void sc_set_arc_deg(Obj_Arc* obj,int start_deg,int end_deg,uint8_t dot);


///创建按键控件
void sc_create_button(void* parent, Obj_But* button,int x, int y, int w, int h,uint16_t bc,uint16_t ac,uint8_t r,uint8_t ir, SC_Align  align);

///设置按键字体
void sc_set_button_text(Obj_But* button, const char *str,uint16_t tc, lv_font_t* font,uint8_t align);

///创建图片
void sc_create_image(void* parent, Obj_Image* image,const SC_img_t *src,int x, int y, SC_Align  align);

///创建ZIP图片
void sc_create_imagezip(void* parent, Obj_Imagezip* zip,const SC_img_zip *src,int x, int y, SC_Align  align);

///图片变换,设置底图和指针src的中心坐标
void sc_create_rotate(void* parent, Obj_Rotate* obj,const SC_img_t *src,int dx, int dy,int src_x,int src_y, SC_Align  align);

///设置图片旋转
void sc_set_rotate_deg(Obj_Rotate* obj,int angle);

///创建开关
void sc_create_switch(void* parent, Obj_Sw* obj,int x, int y, int w, int h,uint16_t bc,uint16_t ac, SC_Align  align);
///设置开关样式
void sc_set_switch_style(Obj_Sw *sw, uint16_t bc, uint16_t ac);

///创建LED
void sc_create_led(void* parent, Obj_Led* led,int x, int y, int w, int h,uint16_t bc,uint16_t ac, SC_Align  align);

///设置LED样式
void sc_set_led_style(Obj_Led *led, uint16_t bc, uint16_t ac);

///创建波形图
void sc_create_chart(void* parent, Obj_Chart* chart,int x, int y, int w, int h,uint16_t ac,uint16_t bc,uint8_t xd,uint8_t yd, SC_Align  align);

///示波器通道压入数据vol,scaleX缩放Q8格式
void sc_set_chart_vol(int ch,Obj_Chart *chart,uint16_t vol,int scaleX);

///创建菜单控件
void sc_create_menu(void* parent, Obj_Menu* obj,int x, int y, int w, int h,SC_Align  align);
#endif
