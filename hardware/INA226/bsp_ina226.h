#ifndef __INA226_H
#define __INA226_H

#include "n32wb03x_i2c.h"  
#include "n32wb03x_gpio.h"
#include "n32wb03x_rcc.h"

#define MODE_INA226 0X4327  // 配置寄存器值：连续测量分流电压、总线电压

// INA226寄存器地址
#define CFG_REG        0X00  // 配置寄存器
#define SV_REG         0X01  // 分流电压寄存器
#define BV_REG         0X02  // 总线电压寄存器
#define PWR_REG        0X03  // 功率寄存器
#define CUR_REG        0X04  // 电流寄存器
#define CAL_REG        0X05  // 校准寄存器
#define ONFF_REG       0X06  // 掩码/使能寄存器
#define AL_REG         0X07  // 警报限值寄存器
#define INA226_GET_ADDR 0XFF // 芯片ID寄存器
#define INA226_GETALADDR 0X14// 警报地址寄存器

// INA226设备地址（根据硬件A0/A1引脚配置，此处默认A0/A1接地）
#define INA226_ADDR1   0x80  // 7位地址：0x40 << 1，写模式；读模式为0x81

// 校准参数（需根据实际分流电阻R_SHUNT调整）
#define INA226_VAL_LSB 2.5f   // 分流电压LSB（单位：uV）
#define Voltage_LSB    1.25f  // 总线电压LSB（单位：mV）
#define POWER_LSB      (25 * CURRENT_LSB * 1000) // 功率LSB（单位：mW）
#define CAL            (uint16_t)(0.00512f / (CURRENT_LSB * R_SHUNT)) // 校准值计算

// INA226数据结构体
typedef struct
{
    float voltageVal;     // 总线电压（单位：mV）
    float Shunt_voltage;  // 分流电压（单位：uV）
    double Shunt_Current;  // 电流（单位：mA）
    float Power_Val;      // 功率（单位：W）
    float Power;          // 功率（单位：mW，芯片直接读取）
    uint32_t ina226_id;   // 芯片ID
} INA226;

// 函数声明（适配N32 I2C API）
void INA226_SetRegPointer(uint8_t addr, uint8_t reg);
void INA226_SendData(uint8_t addr, uint8_t reg, uint16_t data);
uint16_t INA226_ReadData(uint8_t addr);

void mx_ina226_init(void);
void INA226_Get_ID(uint8_t addr);
uint16_t INA226_GET_CAL_REG(uint8_t addr);
uint16_t INA226_GetVoltage(uint8_t addr);
int16_t INA226_GetShunt_Current(uint8_t addr);
int16_t INA226_GetShuntVoltage(uint8_t addr);
uint16_t INA226_Get_Power(uint8_t addr);

void GetVoltage(float *Voltage);
void Get_Shunt_voltage(float *Voltage);
void Get_Shunt_Current(double *Current);
void Get_Power(float *Power);
void get_power(void);

extern INA226 ina226_data;
extern volatile double CURRENT_LSB;
extern volatile float R_SHUNT;
extern volatile float R_STEP;   // 实际分流电阻值步进调整值

#endif /* __INA226_H */
