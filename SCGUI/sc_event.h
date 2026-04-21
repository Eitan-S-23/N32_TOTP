
#ifndef SC_EVENT_H
#define SC_EVENT_H



// 事件总线：单生产者单消费者，环形 8 级队列（可改宏）
#define EVENT_QUEUE_SIZE 8


#define    CMD_NONE        0
#define    CMD_UP         82
#define    CMD_DOWN       81
#define    CMD_LEFT       80
#define    CMD_RIGHT      79
#define    CMD_ENTER      40
#define    CMD_BACK       42
#define    CMD_TAB_NEXT   43

// 事件类型
typedef enum
{
    EV_KEY,
    EV_TOUCH_DOWN,
    EV_TOUCH_MOVE,
    EV_TOUCH_UP
} EvType;

// 事件结构体
typedef struct
{
    EvType type;
    union
    {
        uint32_t key;   // EV_KEY
        struct
        {
            int16_t x;
            int16_t y;
        };
    };
} Event;


//环形队列
typedef struct {

    uint8_t size;
    uint8_t head;
    uint8_t tail;
    Event  events[EVENT_QUEUE_SIZE];
} EventQueue;


uint8_t ui_event_fetch(Event *out);
void    ui_event_key(Cmd c);
void    ui_event_touch(EvType tpye, int16_t x, int16_t y);


#endif // UI_EVENT_H
