#ifndef __RANGE_SWITCH_H
#define __RANGE_SWITCH_H

#include "n32wb03x.h"
#include "string.h"

// 量程定义
typedef enum {
    RANGE_2MA = 0,      // 0-0.8mA量程，使用100Ω采样电阻
    RANGE_250MA = 1,    // 0-90mA量程，使用780mΩ采样电阻
    RANGE_5A = 2        // 0-5A量程，使用40mΩ采样电阻
} CurrentRange;

// 量程参数结构体
typedef struct {
    CurrentRange range;         // 当前量程
    float r_shunt;             // 当前采样电阻值（Ω）
    double current_lsb;         // 当前电流LSB（A）
    float max_current;         // 最大测量电流（mA）
    float switch_up_threshold; // 向上切换阈值（mA）
    float switch_down_threshold; // 向下切换阈值（mA）
    uint8_t hysteresis_count;  // 滞后计数，防止频繁切换
    const char* current_unit;  // 电流单位
    const char* voltage_unit;  // 电压单位
    const char* power_unit;    // 功率单位
} RangeConfig;

// 全局量程配置
extern RangeConfig current_range_config;

// EEPROM地址定义
#define EEPROM_R_STEP_ADDR        0    // R_STEP存储地址（全局共用）
#define EEPROM_RANGE_BASE_ADDR    1    // 量程配置基址
#define EEPROM_RANGE_SIZE         10   // 每个量程占用10字节
#define EEPROM_CONFIG_FLAG_ADDR   255  // 配置标志位地址

// 每个量程在EEPROM中的偏移
#define EEPROM_R_SHUNT_A_OFFSET   0    // R_SHUNT有效数字a
#define EEPROM_R_SHUNT_B_OFFSET   1    // R_SHUNT指数绝对值|b|
#define EEPROM_R_SHUNT_SIGN_OFFSET 2   // R_SHUNT指数符号 (0=正, 1=负)
#define EEPROM_CURRENT_LSB_OFFSET 3    // CURRENT_LSB指数

// 函数声明
void range_switch_init(void);
void set_current_range(CurrentRange range);
CurrentRange auto_range_switch(float current_ma);
void update_ina226_calibration(void);
void get_display_units(const char** i_unit, const char** v_unit, const char** p_unit);
void format_current_display(float current_ma, char* buffer, size_t size);
void format_voltage_display(float voltage_mv, char* buffer, size_t size);
void format_power_display(float power_mw, char* buffer, size_t size);

// EEPROM相关函数
void save_range_config_to_eeprom(CurrentRange range, float r_shunt, double current_lsb);
void load_range_config_from_eeprom(CurrentRange range, float* r_shunt, double* current_lsb);
uint8_t check_eeprom_config_exists(void);
void init_eeprom_default_configs(void);

#endif /* __RANGE_SWITCH_H */
