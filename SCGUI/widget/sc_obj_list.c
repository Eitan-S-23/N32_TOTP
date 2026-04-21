
#include "sc_obj_list.h"
#include "stdio.h"
// 宏定义：判断是否为当前节点或其子节点
#define IS_CHILD_OR_SELF(parent, child) \
    (child != NULL && (child->level > parent->level || child == parent))

static const char* type_name[32]=
{
    "ARER",
    "CANVAS",
    "FRAME",
    "BUTTON",
    "LABEL",
    "TEXTBOX",
    "SLIDER",
    "IMAGE",
    "IMAGEZIP",
    "ARC",
    "LED",
    "SWITCH",
    "CHART",
    "ROTATE",
    "MENU",
    "KEYBOARD",
};

///添加节点到树链表
int sc_obj_list_add(Obj_t *parent,Obj_t *obj)
{
    if (parent == NULL || obj == NULL)        return 0;
    Obj_t* prev  = parent;
    Obj_t* current = parent;
    while(IS_CHILD_OR_SELF(parent, current))
    {
        if (current == obj)   return 0;      // 检查是否已存在相同的节点，
        prev  =    current;                  //保存前驱
        current =  current->next;
    }
    //---------------------
    obj->parent=parent;
    int level_bak=obj->level;    //批量加入
    obj->level=parent->level+1;  //子控件
    Obj_t* tail= obj;
    current=obj->next;
    while(current!=NULL&&current->level>level_bak)
    {
        current->level = obj->level+1;
        tail=current;
        current=current->next;
    }
    tail ->next = prev ->next;
    prev ->next = obj;

    return 1;
}

/// 创建新节点并加入
Obj_t* sc_obj_create(void* parent,void* p,uint8_t type, const ObjVtbl *vtbl)
{
    Obj_t* obj=(Obj_t*)p;
    if (obj)
    {
        if(parent==NULL)
        {
            obj->next = NULL;
        }
        obj->level= 0;
        obj->touch_cb=NULL;
        obj->type = type;
        obj->Flag = 0;
        obj->State = OBJ_STATE_VISIBLE;  //可见
        obj->vtbl = vtbl;
        sc_obj_list_add((Obj_t*)parent,obj);
        return obj;
    }
    return NULL;
}

/// 打印树结构
void sc_obj_list_print(void* tree)
{
    Obj_t* obj=(Obj_t*) tree;
    if (obj == NULL) return ;
    int obj_cnt=0;
    Obj_t* current = obj;
    while (current != NULL)
    {
        if(current->level<=obj->level&&current!=obj)
        {
            break;
        }
        // 打印缩进和节点数据
        for (int i = 0; i < current->level; i++)
        {
            printf("  ");
        }
        printf("|__%s\n", type_name[current->type]);
        obj_cnt++;
        current = current->next;
    }
    printf("obj_cnt=%d\n", obj_cnt);
}

///上一个节点
Obj_t* sc_obj_list_prev(Obj_t* obj)
{
    Obj_t* current = obj->parent;
    while (current != NULL && (current->level > obj->level-1||current == obj->parent))
    {
        if(current->next==obj)
        {
            break;
        }
        current = current->next;
    }
    return current;
}

///尾节点
Obj_t*  sc_obj_list_tail(Obj_t *obj)
{
    Obj_t* tail =    obj;
    Obj_t* current = obj;
    while(IS_CHILD_OR_SELF(obj, current))
    {
        tail =  current;              //记录尾节点
        current = current->next;
    }
    return tail;
}

///控件可见否
void sc_obj_set_Visible(void* vobj,uint8_t Visible)
{
    Obj_t* obj=(Obj_t*)vobj;
    Obj_t* current = obj;
    while(IS_CHILD_OR_SELF(obj, current))
    {
        if(Visible)
        {
            current->State|=OBJ_STATE_VISIBLE;
        }
        else
        {
            current->State&=~OBJ_STATE_VISIBLE;
        }
        current=current->next;
    }
    obj->Flag|=OBJ_FLAG_ACTIVE;
}

///删除控件
void sc_obj_del(void* vobj)
{
    Obj_t* obj=(Obj_t*)vobj;
    Obj_t* current = obj;
    while(IS_CHILD_OR_SELF(obj, current))
    {
        current->Flag= OBJ_FLAG_ACTIVE|OBJ_FLAG_DEL;
        current->State&=~OBJ_STATE_VISIBLE;
        current=current->next;
    }
}
///设置活动
void sc_obj_set_Active(void* vobj,uint8_t active)
{
    Obj_t* obj=(Obj_t*) vobj;
    if(active)
    {
        obj->Flag|=OBJ_FLAG_ACTIVE;
    }
    else
    {
        obj->Flag&=~OBJ_FLAG_ACTIVE;
    }
}

///设置位置
void sc_obj_set_geometry(void* vobj, int x, int y, int w, int h, SC_Align  align)
{
    Obj_t* obj=(Obj_t*) vobj;
    if(obj==NULL) return;
    if(obj->parent)
    {
        SC_set_align(&obj->parent->abs,&x,&y, w, h, align);
    }
    obj->abs.xs = x;      //相对坐标
    obj->abs.ys = y;
    obj->abs.xe = obj->abs.xs+w-1;
    obj->abs.ye = obj->abs.ys+h-1;
    obj->Flag |=  OBJ_FLAG_ACTIVE;      // 设置位置后，控件需要重新绘制
}

