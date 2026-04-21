#include "range_switch.h"
#include "INA226/bsp_ina226.h"
#include "n32wb03x_gpio.h"
#include "AT24C02/i2c_eeprom.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ns_delay.h"

// GPIO定义（从main.c复制）
#define INA226_40m_PIN    GPIO_PIN_3   // 修正：40mΩ对应5A量程
#define INA226_40m_GPIO   GPIOB
#define INA226_780m_PIN   GPIO_PIN_2
#define INA226_780m_GPIO  GPIOB
#define INA226_100_PIN    GPIO_PIN_1
#define INA226_100_GPIO   GPIOB

// 全局量程配置
RangeConfig current_range_config;
uint8_t switch_flag = 0;

volatile extern double L_CURRENT_LSB; 
volatile extern float L_R_SHUNT;  
volatile extern float L_R_STEP; 

// 量程参数配置表
RangeConfig range_configs[3] = {
    // 0-0.8mA量程，使用100Ω采样电阻，精度要求0.1uA
    {
        .range = RANGE_2MA,
        .r_shunt = 99.0f,           // 100Ω
        .current_lsb = 0.0000001f,   // 0.1uA = 0.0000001A
        .max_current = 0.79f,         // 最大0.8mA
        .switch_up_threshold = 0.8f, // 2.0.8mA时切换到上一档
        .switch_down_threshold = 0,  // 无下切换
        .hysteresis_count = 0,
        .current_unit = "uA",
        .voltage_unit = "V",
        .power_unit = "mW"
    },
    // 0-90mA量程，使用780mΩ采样电阻，精度要求1uA
    {
        .range = RANGE_250MA,
        .r_shunt = 0.85f,            // 780mΩ
        .current_lsb = 0.00001f,    // 10uA = 0.00001A
        .max_current = 90.0f,       // 最大90mA
        .switch_up_threshold = 92.0f, // 90mA时切换到上一档
        .switch_down_threshold = 0.7f, // 0.8mA时切换到下一档（留0.0.8mA缓冲）
        .hysteresis_count = 0,
        .current_unit = "uA",
        .voltage_unit = "V",
        .power_unit = "mW"
    },
    // 0-5A量程，使用40mΩ采样电阻，精度要求1mA
    {
        .range = RANGE_5A,
        .r_shunt = 0.04f,            // 40mΩ
        .current_lsb = 0.001f,       // 1mA = 0.001A
        .max_current = 5000.0f,      // 最大5000mA (5A)
        .switch_up_threshold = 5500.0f, // 无上切换
        .switch_down_threshold = 40.0f, // 40mA时切换到下一档（留10mA缓冲）
        .hysteresis_count = 0,
        .current_unit = "A",         // 5A量程显示单位为A
        .voltage_unit = "V",
        .power_unit = "W"            // 5A量程功率显示单位为W
    }
};

// 滞后计数器，防止频繁切换
static uint8_t switch_hysteresis_counter = 0;
#define HYSTERESIS_THRESHOLD 1  // 需要连续3次超过阈值才切换

/**
 * @brief 初始化量程切换功能
 */
void range_switch_init(void)
{
    // 首次使用时，将所有量程的默认值写入EEPROM
    init_eeprom_default_configs();
		
	  for(uint8_t i = 0; i <= RANGE_5A; i ++)
		{
			load_range_config_from_eeprom(i, &range_configs[i].r_shunt, &range_configs[i].current_lsb);
		}
			
    // 默认使用5A量程
    set_current_range(RANGE_5A);
}

/**
 * @brief 设置采样电阻切换IO
 * @param range 目标量程
 */
static void switch_shunt_resistor(CurrentRange range)
{
    // 先关闭所有采样电阻
    GPIO_ResetBits(INA226_40m_GPIO, INA226_40m_PIN);
    GPIO_ResetBits(INA226_780m_GPIO, INA226_780m_PIN);
    GPIO_ResetBits(INA226_100_GPIO, INA226_100_PIN);
		
    // 打开对应的采样电阻
    switch (range) {
        case RANGE_2MA:    // 100Ω
            GPIO_SetBits(INA226_100_GPIO, INA226_100_PIN);
            break;
        case RANGE_250MA:  // 780mΩ
            GPIO_SetBits(INA226_780m_GPIO, INA226_780m_PIN);
            break;
        case RANGE_5A:     // 40mΩ
            GPIO_SetBits(INA226_40m_GPIO, INA226_40m_PIN);
            break;
    }
}

extern double x_flag;
/**
 * @brief 设置当前量程
 * @param range 目标量程
 */
void set_current_range(CurrentRange range)
{
    if (range > RANGE_5A) return;

    // 首先从默认配置表获取配置
    current_range_config = range_configs[range];

    // 切换采样电阻
    switch_shunt_resistor(range);

    // 更新INA226全局变量
    R_SHUNT = current_range_config.r_shunt;
    CURRENT_LSB = current_range_config.current_lsb;

    // 重新校准INA226
    update_ina226_calibration();

    // 重置滞后计数器
    switch_hysteresis_counter = 0;
}

/**
 * @brief 更新INA226校准值
 */
void update_ina226_calibration(void)
{
    // 重新计算并写入校准值
    uint16_t cal_value = (uint16_t)(0.00512f / (CURRENT_LSB * R_SHUNT));
    INA226_SendData(INA226_ADDR1, CAL_REG, cal_value);		
}

/**
 * @brief 自动量程切换
 * @param current_ma 当前电流值（mA）
 * @return 新的量程
 */
CurrentRange auto_range_switch(float current_ma)
{
    CurrentRange new_range = current_range_config.range;
    float abs_current = (current_ma < 0) ? -current_ma : current_ma;

    // 判断是否需要切换量程
    switch (current_range_config.range) {
        case RANGE_2MA:
            // 0-0.8mA量程，电流超过0.8mA时切换到0-90mA
            if (abs_current > current_range_config.switch_up_threshold) {
                switch_hysteresis_counter++;
                if (switch_hysteresis_counter >= HYSTERESIS_THRESHOLD) {
                    new_range = RANGE_250MA;
                    switch_hysteresis_counter = 0;
                }
            } else {
                switch_hysteresis_counter = 0;
            }
            break;

        case RANGE_250MA:
            // 0-90mA量程，电流超过90mA时切换到0-5A
            if (abs_current > current_range_config.switch_up_threshold) {
                switch_hysteresis_counter++;
                if (switch_hysteresis_counter >= HYSTERESIS_THRESHOLD) {
                    new_range = RANGE_5A;
                    switch_hysteresis_counter = 0;
                }
            }
            // 电流小于0.8mA时切换到0-0.8mA
            else if (abs_current < current_range_config.switch_down_threshold) {
                switch_hysteresis_counter++;
                if (switch_hysteresis_counter >= HYSTERESIS_THRESHOLD) {
                    new_range = RANGE_2MA;
                    switch_hysteresis_counter = 0;
                }
            } else {
                switch_hysteresis_counter = 0;
            }
            break;

        case RANGE_5A:
            // 0-5A量程，电流小于90mA时切换到0-90mA
            if (abs_current < current_range_config.switch_down_threshold) {
                switch_hysteresis_counter++;
                if (switch_hysteresis_counter >= HYSTERESIS_THRESHOLD) {
                    new_range = RANGE_250MA;
                    switch_hysteresis_counter = 0;
                }
            } else {
                switch_hysteresis_counter = 0;
            }
            break;
    }

    // 如果需要切换量程
    if (new_range != current_range_config.range) {
				switch_flag = 1;
        set_current_range(new_range);
    }

    return new_range;
}

/**
 * @brief 获取当前量程的显示单位
 */
void get_display_units(const char** i_unit, const char** v_unit, const char** p_unit)
{
    if (i_unit) *i_unit = current_range_config.current_unit;
    if (v_unit) *v_unit = current_range_config.voltage_unit;
    if (p_unit) *p_unit = current_range_config.power_unit;
}

/**
 * @brief 格式化电流显示
 */
void format_current_display(float current_ma, char* buffer, size_t size)
{
    if (current_range_config.range == RANGE_5A) {
        // 5A量程显示为A
			  snprintf(buffer, size, "I:%.3fA", current_ma/1000);
    } else if (current_range_config.range == RANGE_250MA) {
        // 90mA量程显示为mA
			  snprintf(buffer, size, "I:%.2fmA", current_ma);
    }
		else if (current_range_config.range == RANGE_2MA) {
        // 0.8mA量程显示为uA
        snprintf(buffer, size, "I:%.1fuA", current_ma*1000);
    }
}

/**
 * @brief 格式化电压显示
 */
void format_voltage_display(float voltage_mv, char* buffer, size_t size)
{
    // 始终显示为V
    snprintf(buffer, size, "U:%.2fV", voltage_mv / 1000.0f);
}

/**
 * @brief 格式化功率显示
 */
void format_power_display(float power_mw, char* buffer, size_t size)
{
    if (current_range_config.range == RANGE_5A) {
        // 5A量程显示为W
        snprintf(buffer, size, "P:%.3fW", power_mw/1000);
    } else {
        // 其他量程显示为mW
        snprintf(buffer, size, "P:%.4fmW", power_mw);
    }
}

/**
 * @brief 将R_SHUNT转换为有效数字和指数形式
 * @param r_shunt R_SHUNT值
 * @param a 输出：有效数字
 * @param b 输出：指数
 * @note 编码公式：r_shunt = a × 10^b，其中1 <= a <= 255
 */
static void encode_r_shunt(float r_shunt, int *a, int *b)
{
    if (r_shunt == 0)
    {
        *a = 0;
        *b = 0;
        return;
    }

    float scaled = fabs(r_shunt);
    int exp = 0;

    // 第一步：将scaled调整到1-255范围
    while (scaled > 255 && exp < 10)
    {
        scaled /= 10;
        exp++;
    }
    while (scaled < 1 && exp > -10)
    {
        scaled *= 10;
        exp--;
    }

    // 第二步：尽量让scaled接近整数（保留精度）
    // 继续放大直到scaled接近整数或达到255上限
    while (scaled < 255 && exp > -10)
    {
        float next_scaled = scaled * 10;
        if (next_scaled > 255)
            break;

        // 检查当前scaled是否已经足够接近整数
        if (fabs(scaled - round(scaled)) < 0.01)
            break;

        scaled = next_scaled;
        exp--;
    }

    *a = (int)round(scaled);
    *b = exp;
}

/**
 * @brief 从有效数字和指数形式还原R_SHUNT值
 * @param a 有效数字
 * @param b 指数
 * @return R_SHUNT值
 * @note 解码公式：r_shunt = a × 10^b
 */
static float decode_r_shunt(int a, int b)
{
    return (float)a * pow(10, b);
}

/**
 * @brief 保存某个量程的配置到EEPROM（不设置标志位）
 * @param range 量程
 * @param r_shunt R_SHUNT值
 * @param current_lsb CURRENT_LSB值
 */
void save_range_config_to_eeprom(CurrentRange range, float r_shunt, double current_lsb)
{
    if (range > RANGE_5A) return;

    // 计算EEPROM基址
    uint16_t base_addr = EEPROM_RANGE_BASE_ADDR + range * EEPROM_RANGE_SIZE;

    // 编码R_SHUNT
    int a, b;
    encode_r_shunt(r_shunt, &a, &b);

    // 保存R_SHUNT
    I2C_EE_WriteByte(base_addr + EEPROM_R_SHUNT_A_OFFSET, (uint8_t)a);
    I2C_EE_WriteByte(base_addr + EEPROM_R_SHUNT_B_OFFSET, (uint8_t)abs(b));
    I2C_EE_WriteByte(base_addr + EEPROM_R_SHUNT_SIGN_OFFSET, b >= 0 ? 0 : 1);

    // 保存CURRENT_LSB（使用对数编码）
    uint8_t lsb_exp = (uint8_t)round(log10(current_lsb / 0.0000001));
    I2C_EE_WriteByte(base_addr + EEPROM_CURRENT_LSB_OFFSET, lsb_exp);
}

/**
 * @brief 从EEPROM加载某个量程的配置
 * @param range 量程
 * @param r_shunt 输出：R_SHUNT值指针
 * @param current_lsb 输出：CURRENT_LSB值指针
 * @return 1=成功加载, 0=无自定义配置
 */
void load_range_config_from_eeprom(CurrentRange range, float* r_shunt, double* current_lsb)
{
    if (range > RANGE_5A) return;

    // 检查是否有自定义配置
    if (I2C_EE_ReadByte(EEPROM_CONFIG_FLAG_ADDR) != 1)
    {
        return; // 无自定义配置
    }

    // 计算EEPROM基址
    uint16_t base_addr = EEPROM_RANGE_BASE_ADDR + range * EEPROM_RANGE_SIZE;

    // 读取R_SHUNT
    uint8_t a = I2C_EE_ReadByte(base_addr + EEPROM_R_SHUNT_A_OFFSET);
    uint8_t b_abs = I2C_EE_ReadByte(base_addr + EEPROM_R_SHUNT_B_OFFSET);
    uint8_t b_sign = I2C_EE_ReadByte(base_addr + EEPROM_R_SHUNT_SIGN_OFFSET);

    // 如果a为0，说明没有保存过，使用默认值
    if (a != 0)
    {
        int b = b_sign == 0 ? b_abs : -b_abs;
        *r_shunt = decode_r_shunt(a, b);
    }

    // 读取CURRENT_LSB
    uint8_t lsb_exp = I2C_EE_ReadByte(base_addr + EEPROM_CURRENT_LSB_OFFSET);
    if (lsb_exp != 0xFF) // 0xFF表示未初始化
    {
        *current_lsb = 0.0000001 * pow(10, lsb_exp);
    }
		
		L_CURRENT_LSB = CURRENT_LSB; 
		L_R_SHUNT = R_SHUNT;  
		L_R_STEP = R_STEP;  
}

/**
 * @brief 检查EEPROM中是否有自定义配置
 * @return 1=有自定义配置, 0=无
 */
uint8_t check_eeprom_config_exists(void)
{
    return I2C_EE_ReadByte(EEPROM_CONFIG_FLAG_ADDR) == 1 ? 1 : 0;
}

/**
 * @brief 初始化EEPROM配置 - 首次使用时写入所有量程的默认值
 */
void init_eeprom_default_configs(void)
{
    // 检查是否已经初始化
    if (check_eeprom_config_exists())
    {
        return; // 已经初始化过
    }

    // 首次使用，将所有量程的默认配置写入EEPROM
    for (int i = 0; i <= RANGE_5A; i++)
    {
        save_range_config_to_eeprom((CurrentRange)i,
                                    range_configs[i].r_shunt,
                                    range_configs[i].current_lsb);
    }

    // 所有量程的默认值都写入后，设置标志位
    I2C_EE_WriteByte(EEPROM_CONFIG_FLAG_ADDR, 1);
}
