#ifndef _SC_ENEVT_TASK_H_
#define	_SC_ENEVT_TASK_H_

//#include "typedef.h"
#include  <stdint.h>

#ifndef NULL
#define NULL  0
#endif

//=======================新增多类型队列===================================
#define MAX_THREADS 		4     	//任务数量
#define EVENT_QUEUE_SIZE    4		//队列数量
#define MAX_EVENT_DATA_SIZE 16		//队列数据长度，用于传字符串

#define    CMD_NONE        0
#define    CMD_UP         82
#define    CMD_DOWN       81
#define    CMD_LEFT       80
#define    CMD_RIGHT      79
#define    CMD_ENTER      40
#define    CMD_BACK       42
#define    CMD_TAB_NEXT   43

//队列数据类型
enum {
    EVENT_TYPE_TIMER,  // 定时器事件类型
    EVENT_TYPE_INIT,   // 初始化事件类型
    EVENT_TYPE_CMD,    // 消息事件类型
    EVENT_TYPE_KEY,    // 按键事件类型
    //----------以上顺序不要修改---------------
    EVENT_TYPE_CUSTOM,  //自定义事件
    EVENT_TYPE_STR,    // 字符事件类型
};
//队列数据结构
typedef struct {
#if (MAX_THREADS>16)
	uint32_t thread_mask;
#else
	uint16_t thread_mask;   //线程掩码
#endif
    uint8_t type;          //事件类型
    union {
        int key;
        int cmd;
        void *arg;
        char str[MAX_EVENT_DATA_SIZE];
    }dat;
} Event;

//环形队列
typedef struct {

    uint8_t size;
    uint8_t head;
    uint8_t tail;
    Event events[EVENT_QUEUE_SIZE];
} EventQueue;

//事件线程
typedef struct sc_task_t {
    uint32_t time_dly;
    uint32_t time_out;
    uint32_t ms;
    void (*thread_cb) (void*);
    void (*delay_cb) (void);
} sc_task_t;

extern volatile uint32_t system_tick;

// 创建事件线程
uint8_t sc_create_task(uint8_t id, void (*thread_cb)(void*), uint32_t ms);
// 线程存在否
uint8_t sc_task_true(uint8_t id);
// 删除线程
void sc_delete_task(uint8_t id);
// 遍历线程
void sc_task_loop(void);

//单次延时，可以递归使用
void sc_add_delay(uint8_t id,void (*delay_cb)(void),uint32_t ms);

//停止延时
void sc_stop_delay(uint8_t id);

void sc_send_key_event(int key);

void sc_send_cmd_event(int cmd);


#endif

