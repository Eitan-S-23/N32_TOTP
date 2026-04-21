#ifndef APP_GUI_H
#define APP_GUI_H

#include "sc_gui.h"
#include "sc_obj_widget.h"
#include "sc_event_task.h"
#include "sc_menu_table.h"

extern lv_font_t lv_font_12;
extern lv_font_t lv_font_16;
extern lv_font_t lv_font_20;
extern lv_font_t lv_font_34;

void app_gui_init(void *screen);
void app_gui_task(void *arg);

#endif /* APP_GUI_H */
