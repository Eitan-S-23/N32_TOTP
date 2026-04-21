
#include "sc_obj_widget.h"

SC_AREA g_dirty_area= {0};                  //当前脏矩阵区域
static int16_t dirty_last_x,dirty_last_y=0; //上次脏矩阵
static Obj_t*  g_focus=NULL;                //聚焦控件
static uint8_t touch_state=0;               //触控状态

///移动控件平移画布，按键可以直接调用
int sc_widget_move_pos(Obj_t* obj,int move_x,int move_y)
{
    if(move_x==0&&move_y==0) return 0;
    Obj_t* current = obj;
    Obj_Canvas *Canvas=NULL;
    if(obj->type==CANVAS||obj->type==TEXTBOX)
    {
        Canvas= (Obj_Canvas *)obj;
    }
    while(IS_CHILD_OR_SELF(obj, current))
    {
        if(Canvas)
        {
            Canvas->box.xs+=move_x;
            Canvas->box.ys+=move_y;
            Canvas=NULL;
        }
        else
        {
            current->abs.xs+=move_x;
            current->abs.xe+=move_x;
            current->abs.ys+=move_y;
            current->abs.ye+=move_y;
        }
        current->Flag|=OBJ_FLAG_ACTIVE;
        current = current->next;
    }
    g_dirty_area.xs+=move_x;    //记录位置
    g_dirty_area.ys+=move_y;    //记录位置
    return 1;
}

///部件按键事件
void sc_widget_cmd_event(void *vobj,int cmd)
{
    if(vobj==NULL) return;
    Obj_t *obj=(Obj_t *)vobj;
    if(touch_state==0)
    {
        if(obj->vtbl&&obj->vtbl->handle_key)
        {
            if (obj->vtbl->handle_key(obj,cmd)) //如果有虚表回调
            {
                g_focus=obj;
                touch_state |= obj->State&OBJ_STATE_MOVXY;
                return;
            }
        }
        if(cmd==CMD_TAB_NEXT)
        {
            Obj_t *current=g_focus;
            while(current!=NULL)
            {
                if(current->State>OBJ_STATE_VISIBLE)
                {
                    g_focus=current;  //聚焦下一个
                }
                current=current->next;
            }
        }
    }
}

///部件触控事件
void sc_widget_touch_event(void* screen, int x, int y,uint8_t state)
{
    if(screen==NULL)    return;
    Obj_t* obj=(Obj_t*) screen;
    switch(state)
    {
    case OBJ_STATE_CLICK:  //按下
        if(touch_state) break;
        g_focus= obj;
        for (Obj_t* current = obj->next; current != NULL; current = current->next)
        {
            if(current->State<=OBJ_STATE_VISIBLE)             continue; //不可见或不可点击
            if(!sc_obj_area_touch(&current->parent->abs,x,y)) continue; //超出父级外
            if(current->vtbl&&current->vtbl->handle_touch)
            {
                if (current->vtbl->handle_touch(current,OBJ_STATE_CLICK,x,y))  //执行虚表菜单
                {
                    g_focus=current;
                }
            }
            else if(sc_obj_area_touch(&current->abs,x,y))            //常规判断
            {
                g_focus=current;
            }
        }
        if(g_focus)
        {
            touch_state|=OBJ_STATE_CLICK;          //按下标志
            if(g_focus->State&OBJ_STATE_CLICK)     //单击事件回调
            {
                g_focus->Flag|=OBJ_FLAG_ACTIVE|OBJ_FLAG_CLICK;
                sc_touch_cb(g_focus,x,y,OBJ_STATE_CLICK);
            }
            else  if(g_focus->State&OBJ_STATE_BOOL) //开关事件回调
            {
                g_focus->Flag|=OBJ_FLAG_ACTIVE;
                g_focus->Flag^=OBJ_FLAG_CLICK;
                sc_touch_cb(g_focus,x,y,OBJ_STATE_BOOL);
            }
            g_dirty_area.xs=0;
            g_dirty_area.ys=0;
            g_dirty_area.xe=0;
            g_dirty_area.ye=0;
            gui->touch_x=x;
            gui->touch_y=y;
        }
        break;
    case OBJ_STATE_MOVXY:     //移动事件
        if(g_focus&&(touch_state&OBJ_STATE_CLICK))
        {
            if(g_focus->vtbl&&g_focus->vtbl->handle_touch)  //执行虚表菜单
            {
                g_focus->vtbl->handle_touch(g_focus,OBJ_STATE_MOVXY,x,y);
            }
            gui->touch_x=x;
            gui->touch_y=y;
        }
        break;
    case OBJ_STATE_REL://释放
        if(g_focus)
        {
            touch_state=0;                   //清除标志
            if(g_focus->State&OBJ_STATE_CLICK) //释放事件回调
            {
                g_focus->Flag=OBJ_FLAG_ACTIVE;
                sc_touch_cb(g_focus,x,y,OBJ_STATE_REL);
            }
            if(g_focus->vtbl&&g_focus->vtbl->handle_touch)      //执行虚表菜单
            {
                if(g_focus->vtbl->handle_touch(g_focus,OBJ_STATE_REL,x,y))
                {
                    touch_state|= g_focus->State&OBJ_STATE_MOVXY;  //释放后复位
                }
            }
        }
        break;
        //----------------复位--------------------
    }
}

///控件触控释放,防止中断重入
static inline void sc_widget_touch_rel(int x, int y)
{
    int move_x=0,move_y=0;
    if(g_focus&&(touch_state&OBJ_STATE_MOVXY))
    {
        int errx=g_dirty_area.xe-g_dirty_area.xs;
        int erry=g_dirty_area.ye-g_dirty_area.ys;
        if(erry!=0||errx!=0)
        {
            if(SC_ABS(errx)<x)
            {
                move_x=errx%x;
            }
            else
            {
                move_x=errx<0 ?-x:x;
            }
            if(SC_ABS(erry)<y)
            {
                move_y=erry%y;
            }
            else
            {
                move_y=erry<0 ?-y:y;
            }
            sc_widget_move_pos(g_focus,move_x,move_y);
        }
        else
        {
            touch_state=0;
        }
    }
}

///绘制虚表函数.实测指针调用会慢一些
static void sc_widget_draw_cb(Obj_t *obj,SC_tile *dest)
{
   // if(obj->vtbl==NULL||obj->vtbl->handle_draw==NULL) return;//绘制回调
    uint8_t alpha=gui->alpha;  //备份alpha
    gui->parent= obj->parent? &obj->parent->abs:NULL;       //启用父级cilp
    obj->vtbl->handle_draw(obj,dest);
    gui->alpha=alpha;          //还原alpha
}


///刷新单个活动控件
void sc_widget_draw_actice(void *screen,Obj_t *active)
{
    int intersect_cnt=0;                //相交控件
    Obj_t* intersect_buf[20];
    SC_AREA arer = active->abs;                                        //刷新区
    sc_obj_area_merge_xy(&arer,g_dirty_area.xs-dirty_last_x,g_dirty_area.ys-dirty_last_y);  //如果移动合并刷新区
    SC_tile pfb;
    Obj_t* current=(Obj_t *)screen;
    pfb.xs= current->abs.xs;
    pfb.ys= current->abs.ys;
    pfb.w=  current->abs.xe-current->abs.xs+1;
    pfb.h=  current->abs.ye-current->abs.ys+1;
    pfb.stup= 0;
    gui->parent= active->parent? &active->parent->abs:NULL;                       //启用父级clip
    if(!SC_pfb_intersection(&pfb,&arer,arer.xs, arer.ys,arer.xe, arer.ye))return; //无效控件
    while (current != NULL)
    {
        if(current->type<=CANVAS)        //占位区不用更新
        {
            current = current->next;
            continue;
        }
        if(sc_obj_area_intersect(&arer,&current->abs))
        {
            //完全在脏矩形内或是子控件
            if(current->parent==active||sc_obj_area_contained(&active->abs,&current->abs))
            {
                current->Flag&=~OBJ_FLAG_ACTIVE;     //清标志
            }
            if(current->State>=OBJ_STATE_VISIBLE)     //过滤不可见
            {
                intersect_buf[intersect_cnt++]=current;
            }
        }
        current = current->next;
    }
    //sc_printf("pfb_arer (%d, %d,%d, %d) %d/%d \n",arer.xs, arer.ys,arer.xe, arer.ye,intersect_cnt,(active->type));
    //-----------------刷新控件--------------------------------
    do
    {
        SC_pfb_clip(&pfb,arer.xs,arer.ys,arer.xe,arer.ye,gui->bkc);
        for(int i=0; i<intersect_cnt; i++)
        {
            sc_widget_draw_cb(intersect_buf[i],&pfb);
        }
    }
    while(SC_pfb_Refresh(&pfb,0));            //分帧刷新
}

///遍历刷新
void sc_widget_draw_screen(void *screen)
{
    uint8_t destroyed=0;
    Obj_t* current=(Obj_t*)screen;
    sc_widget_touch_rel(15,15); //复位画布
    while (current != NULL)
    {
        while(current->Flag&OBJ_FLAG_ACTIVE)     //控件活动事件
        {
            current->Flag&=~OBJ_FLAG_ACTIVE;      //清标志
            if(current->type>CANVAS)        //占位区不用更新
            {
                sc_widget_draw_actice(screen,current);
            }
        }
        if(current->Flag&OBJ_FLAG_DEL)
        {
            destroyed=1;
        }
        current = current->next;
    }
    dirty_last_x=g_dirty_area.xs;
    dirty_last_y=g_dirty_area.ys;
    //--------------自动内存回收------------
    if(destroyed)
    {
        Obj_t* prev = (Obj_t*)screen;
        current = prev->next;
        while (current != NULL)
        {
            if(current->Flag&OBJ_FLAG_DEL)
            {
                current->Flag=0;            //清标志
                //Obj_t* to_free = current;
                prev->next = current->next;  // 跳过当前节点
                current = current->next;
                //free(to_free);  // 释放节点
            }
            else
            {
                prev = current;
                current = current->next;
            }
        }
    }
}



