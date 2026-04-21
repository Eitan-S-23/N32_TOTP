#ifndef _SC_GUI_DEMO_H_
#define	_SC_GUI_DEMO_H_

#include "sc_gui.h"
#include "sc_obj_widget.h"


extern lv_font_t lv_font_12;

void  demo_main_init(void *screen);
void  demo_main_task(void *arg);


void demo_menu_init(void* screen);
void demo_menu_task(void *arg);


void demo_keyboard_init(void* screen);
void demo_keyboard_task(void *arg);

#endif // _SC_GUI_DEMO_H_


