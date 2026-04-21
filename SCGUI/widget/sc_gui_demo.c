
#include "sc_gui_demo.h"

const char* test_txt = "一个小内存(1-3k)的开源GUI,支持LVGL抗锯齿字体,动态文字,实用波形图,支持双DMA,简易图层叠加"
                      "感谢网友开源工具Lvgl Font Tool V0.4 生成 阿里(qq:617622104)"
                      "(WX:13018616872)"
                      "(QQ群:799501887)";

extern const SC_img_t tempC_img_48;     //温度图标
extern const SC_img_t jpgkm_img;        //仪表图
uint16_t km_prt_buf[200];  //旋转指针图片
const SC_img_t km_prt=
{
    .map=(const uint8_t*)&km_prt_buf,
    .mask=NULL,
    .w=3,
    .h=60,
};

static Obj_Image screen;
static Obj_Canvas Canvas;
static Obj_Frame  Frame1;
static Obj_But    But1,But2,But3;
static Obj_Label  Label1,ver,bat,time;
static Obj_Image  image,image2;
static Obj_Led    Led1,Led2,Led3;
static Obj_Sw     SW1,SW2;
static Obj_Slider  slider,slider2;
static Obj_Txtbox  txtbox;
static Obj_Chart   chart;
static Obj_Rotate  Rotate;
static Obj_Arc     arc1,arc2;

void touch_printf(Obj_t* obj, int x, int y,uint8_t state)
{
    sc_printf("touch (%d %d) state=%d\n",x,y,state);
}

void demo_main_init(void* screen)
{
    for(int j=0; j<200; j++)
    {
        km_prt_buf[j]=C_WHITE;  //白色指针
    }
    //------------容器层--------------------
    sc_create_canvas(screen,&Canvas,0, 0, 320, 240,320*4,240,ALIGN_CENTER);   //容器640*240

    sc_create_frame(&Canvas,&Frame1,0, 0,200,100,gui->fc,gui->bc,20,12,ALIGN_CENTER);

    sc_create_image(&Frame1,&image,&tempC_img_48, 20, 0,ALIGN_VER);

    sc_create_label(&Frame1,&Label1,0,-10,50,30,ALIGN_CENTER);

    sc_set_label_text(&Label1,"SCGUI",C_WHITE,gui->font,ALIGN_CENTER);
    sc_create_label(&Frame1,&ver,0,10, 50,30,ALIGN_CENTER);
    sc_set_label_text(&ver,"V2.2",C_BLUE,  gui->font,ALIGN_CENTER);

    sc_create_slider(&Canvas,&slider, 20,0,12,120, C_RED,C_BLUE,5,4,50,ALIGN_LEFT|ALIGN_VER);
		
    sc_create_slider(&Canvas,&slider2,0,-40,120,12,C_GREEN,C_BLUE,5,4,50,ALIGN_BOTTOM|ALIGN_HOR);

    sc_create_txtbox(&Canvas,&txtbox,320, 0,200,150,200,150*3, ALIGN_CENTER);    //容器200*450

    sc_create_chart(&Canvas, &chart,640, 0, 200, 160,gui->fc,gui->bc,10,10, ALIGN_CENTER);

    sc_create_button(&Canvas,&But1,650, 40, 50, 30,C_ROYAL_BLUE,C_BLUE,8,7,  0);
    sc_create_button(&Canvas,&But2,650, 100, 50, 30,C_ROYAL_BLUE,C_BLUE,8,7, 0);

    sc_create_switch(&Canvas,&SW1,650, 150, 45,30,gui->bkc,C_BLUE,0);
    sc_create_switch(&Canvas,&SW2,650, 200, 45,30,gui->bkc,C_BLUE,0);

    //-----------------仪表-------------------------
    sc_create_image(&Canvas,&image2,&jpgkm_img, 320*3, 0,0);
    sc_create_rotate(&image2,&Rotate,&km_prt,0,40,0,-120,ALIGN_CENTER);
    sc_create_arc(&image2,&arc1, 0,40,75, 60, gui->fc,gui->bc,ALIGN_CENTER);
    sc_create_arc(&image2,&arc2, 0,40,55, 40, C_RED,gui->bc,ALIGN_CENTER);

    //------------屏幕层放到最后不会触摸到容器--------------------
    sc_create_label(screen,&time,0,10, 50,20,ALIGN_HOR);
    sc_create_label(screen,&bat,0,10,  50,20, ALIGN_RIGHT);

    sc_create_led(screen,&Led1,-40,-10, 30,20,gui->bkc, C_RED,  ALIGN_BOTTOM|ALIGN_HOR);
    sc_create_led(screen,&Led2,0,  -10, 20,20,gui->bkc, C_GREEN,ALIGN_BOTTOM|ALIGN_HOR);
    sc_create_led(screen,&Led3,40, -10, 20,20,gui->bkc, C_BLUE, ALIGN_BOTTOM|ALIGN_HOR);


    //------------初始化参数--------------------
    sc_set_txtbox_text(&txtbox,test_txt,C_RED,gui->font,ALIGN_CENTER);
    sc_set_label_text(&time,"20:06",C_BLACK,  gui->font,ALIGN_CENTER);
    sc_set_label_text(&bat,"bat",   C_BLACK,  gui->font,ALIGN_CENTER);
    sc_set_button_text(&But1,"but1",C_WHITE,  gui->font,ALIGN_CENTER);
    sc_set_button_text(&But2,"but2",C_WHITE,  gui->font,ALIGN_CENTER);
    sc_set_button_text(&But3,"but3",C_WHITE,  gui->font,ALIGN_CENTER);

    Led1.alpha=128;
    Led2.alpha=128;
    Led3.alpha=128;
    But1.alpha=128;
    But2.alpha=128;
    SW1.alpha=128;
    SW2.alpha=128;
    Frame1.alpha=200;
    arc1.alpha=128;
    arc2.alpha=128;

    //------------触摸回调函数--------------------
    sc_set_touch_cb(&txtbox,touch_printf,OBJ_STATE_MOVY);
    sc_set_touch_cb(&Led1, touch_printf,OBJ_STATE_CLICK);
    sc_set_touch_cb(&Led2, touch_printf,OBJ_STATE_CLICK);
    sc_set_touch_cb(&Led3, touch_printf,OBJ_STATE_CLICK);
    sc_set_touch_cb(&SW1,  touch_printf,OBJ_STATE_BOOL);  //无释放响应
    sc_set_touch_cb(&SW2,  touch_printf,OBJ_STATE_BOOL);  //无释放响应
    sc_set_touch_cb(&slider, touch_printf,OBJ_STATE_BOOL);
    sc_set_touch_cb(&slider2, touch_printf,OBJ_STATE_BOOL);
    sc_set_touch_cb(&But1, touch_printf,OBJ_STATE_CLICK);
    sc_set_touch_cb(&But2, touch_printf,OBJ_STATE_CLICK);
    sc_set_touch_cb(&But3, touch_printf,OBJ_STATE_CLICK);
    sc_set_touch_cb(&time,  touch_printf,OBJ_STATE_CLICK);
    sc_set_touch_cb(&bat,  touch_printf,OBJ_STATE_CLICK);
    sc_set_touch_cb(&image,  touch_printf,OBJ_STATE_BOOL);
    sc_set_touch_cb(&Canvas,touch_printf,OBJ_STATE_MOVX);

}

void demo_main_task(void *arg)
{
    static  int adc_vol;
    static  int xi,stup;
    Event *e= (Event*)arg;
    //接收所有事件
    switch (e->type)
    {
    case EVENT_TYPE_INIT:
				sc_create_screen(NULL, (Obj_t*)&screen, 0, 0, LCD_SCREEN_WIDTH, LCD_SCREEN_HEIGHT,ALIGN_NONE);    //空屏幕
				demo_main_init((void*)&screen);
        adc_vol=0;
        xi=60;
        stup=1;
        break;
    case EVENT_TYPE_KEY:
        //这里处理按键事件
        sc_printf("EVENT_TYPE_KEY: %d\n", e->dat.key);
        break;
    case EVENT_TYPE_CMD:
        //这里处理消息事件，比如低电报警消息
        sc_printf("EVENT_TYPE_MSG: %d\n", e->dat.cmd);
        sc_widget_cmd_event(&Canvas,e->dat.cmd);
        if(e->dat.cmd==CMD_UP)
        {
             //sc_obj_del(&Frame1);
              sc_obj_set_Visible(&Frame1,0);
        }
        if(e->dat.cmd==CMD_DOWN)
        {
            sc_obj_set_Visible(&Frame1,1);
        }
        break;
    case EVENT_TYPE_TIMER:
        sc_set_rotate_deg(&Rotate,xi);   //指针旋转
        sc_set_arc_deg(&arc1,60,xi,0);   //弧旋转
        sc_set_arc_deg(&arc2,60,xi,1);   //弧旋转
        xi+=stup;
        if(xi>=300||xi<=60) stup=-stup;
        sc_set_chart_vol(0,&chart, adc_vol%30-50,256);  //通道1
        sc_set_chart_vol(1,&chart, adc_vol%70,128);     //通道2
        adc_vol++;
        break;
    default:
        break;
    }
}



