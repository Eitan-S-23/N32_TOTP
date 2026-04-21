
#ifndef SC_OBJ_H
#define SC_OBJ_H

#include "sc_gui.h"

// 宏定义：判断是否为当前节点或其子节点
#define IS_CHILD_OR_SELF(parent, child) \
    (child != NULL && (child->level > parent->level || child == parent))

typedef enum
{
    ARER,     //占位区
    CANVAS,   //画布
    FRAME,    //圆角矩形
    BUTTON,   //按键
    LABEL,    //标签
    TEXTBOX,  //文本框
    SLIDER,   //进度条
    IMAGE,    //图片
    IMAGEZIP, //图片压缩
    ARC,      //图片压缩
    SC_LED,    //LED
    SWITCH,   //开关
    CHART,    //波形图
    ROTATE,   //旋转表针
    MENU,     //菜单
    KEYBOARD,
} ObjType;

typedef enum
{
    OBJ_STATE_REL =          0x00,   //释放
    OBJ_STATE_CLICK =        0x01,   //可以点击
    OBJ_STATE_BOOL =         0x02,   //开关
    OBJ_STATE_MOVX =         0x04,   //可以X移动
    OBJ_STATE_MOVY =         0x08,   //可以Y移动
    OBJ_STATE_MOVXY =        (OBJ_STATE_MOVX|OBJ_STATE_MOVY),   //可以XY移动
    OBJ_STATE_VISIBLE =       0x10,   //可见
} ObjState ;

typedef enum
{
    OBJ_FLAG_ACTIVE   =       0x01,   //活动标志
    OBJ_FLAG_CLICK    =       0x02,   //点击标志
    OBJ_FLAG_DEL      =       0x04,   //删除标志
} ObjFlag;

typedef struct Obj_t  Obj_t;

typedef struct
{
    void (*handle_draw) (Obj_t *self, SC_tile *dest);                           //
    bool (*handle_key)  (Obj_t *self,  int cmd);                // 处理按键
    bool (*handle_touch)(Obj_t *self,ObjState state, int16_t x, int16_t y);    // 处理触摸
} ObjVtbl;

struct Obj_t
{
    //4个指针16字节
    struct Obj_t* parent;                  // 父控件
    struct Obj_t* next;                    // 指向下一个控件的指针
    const ObjVtbl *vtbl;                  // 虚表
    void (*touch_cb)(struct Obj_t* obj, int x, int y,uint8_t state);   //触摸回调
    //绝对位置 8字节(xs,ys,xe,ye)
    SC_AREA abs;
    //位域2字节
    uint8_t type:5;                        //控件类型
    uint8_t level:3;                       //层次最大7级
    uint8_t State:5;                       //状态
    uint8_t Flag:3;                        //标志

};

///添加节点到树链表
int sc_obj_list_add(Obj_t *parent,Obj_t *obj);

/// 创建新节点并加入
Obj_t* sc_obj_create(void* parent,void* p,uint8_t type,const ObjVtbl *vtbl);

/// 打印树结构
void sc_obj_list_print(void* tree);

///上一个节点
Obj_t* sc_obj_list_prev(Obj_t* obj);

///尾节点
Obj_t*  sc_obj_list_tail(Obj_t *obj);

///释放节点,子节点也会一起释放
void sc_obj_list_free(void *vobj);

///控件设置活动
void sc_obj_set_Active(void* vobj,uint8_t active);

///控件可见否
void sc_obj_set_Visible(void* vobj,uint8_t Visible);
///控件删除
void sc_obj_del(void* vobj);

///设置位置
void sc_obj_set_geometry(void* vobj, int x, int y, int w, int h, SC_Align  align);

///b完全在a区域内
static inline int sc_obj_area_contained(SC_AREA* a, SC_AREA* b)
{
    return (b->xs>=a->xs && b->ys>=a->ys  && b->xe <= a->xe && b->ye <= a->ye);
}
///判断两个区域是否相交
static inline int sc_obj_area_intersect(SC_AREA* a, SC_AREA* b)
{
    return (a->xe >= b->xs && a->xs <= b->xe && a->ye >= b->ys && a->ys <= b->ye);
}
///判断xy是否在区域内
static inline int sc_obj_area_touch(SC_AREA *a,int x, int y)
{
    return (x >= a->xs && x <= a->xe &&y >= a->ys && y <= a->ye);
}

///合并控件平移后的脏矩形
static inline  void sc_obj_area_merge_xy(SC_AREA* a,int move_x,int move_y)
{
    if(move_x>0)
    {
        a->xs-=move_x;
    }
    else
    {
        a->xe-=move_x;
    }
    if(move_y>0)
    {
        a->ys-=move_y;
    }
    else
    {
        a->ye-=move_y;
    }
}

#endif
