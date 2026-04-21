#include "bsp_ina226.h"
#include "stdio.h"

// 全局数据结构体
INA226 ina226_data;

#define INA226_I2Cx    I2C1
#define INA226_I2C_CLK RCC_APB1_PERIPH_I2C1
#define INA226_GPIO_CLK RCC_APB2_PERIPH_GPIOB
#define INA226_SCL_PIN GPIO_PIN_9
#define INA226_SDA_PIN GPIO_PIN_8
#define INA226_GPIOx   GPIOB
#define INA226_AF      GPIO_AF2_I2C

volatile double CURRENT_LSB = 0.001; // 电流LSB（单位：A）= 1mA
volatile float R_SHUNT = 0.04;   // 实际分流电阻值（单位：Ω），需与硬件一致
volatile float R_STEP = 0.01;   // 实际分流电阻值步进调整值

extern void system_delay_n_10us(uint32_t value);

/**
 * @brief  初始化INA226（含I2C外设和GPIO配置）
 */
void mx_ina226_init(void)
{
//    GPIO_InitType GPIO_InitStruct;
//    I2C_InitType I2C_InitStruct;

//    // 1. 使能I2C和GPIO时钟
//    RCC_EnableAPB1PeriphClk(INA226_I2C_CLK, ENABLE);
//    RCC_EnableAPB2PeriphClk(INA226_GPIO_CLK, ENABLE);

//    // 2. 配置SCL/SDA引脚（复用开漏输出）
//    GPIO_InitStruct.Pin        = INA226_SCL_PIN;
//    GPIO_InitStruct.GPIO_Mode  = GPIO_MODE_AF_OD;  // I2C要求开漏输出
//    GPIO_InitStruct.GPIO_Speed = GPIO_SPEED_HIGH;
//    GPIO_InitPeripheral(INA226_GPIOx, &GPIO_InitStruct);

//    GPIO_InitStruct.Pin = INA226_SDA_PIN;
//    GPIO_InitPeripheral(INA226_GPIOx, &GPIO_InitStruct);

//    // 3. 配置I2C参数（100kHz标准模式，匹配INA226通信要求）
//    I2C_InitStruct.ClkSpeed      = 100000;
//    I2C_InitStruct.BusMode       = I2C_BUSMODE_I2C;
//    I2C_InitStruct.FmDutyCycle   = I2C_FMDUTYCYCLE_2;
//    I2C_InitStruct.OwnAddr1      = 0x00;            // 主机模式无需自身地址
//    I2C_InitStruct.AckEnable     = I2C_ACKEN;       // 使能应答
//    I2C_InitStruct.AddrMode      = I2C_ADDR_MODE_7BIT;
//    I2C_Init(INA226_I2Cx, &I2C_InitStruct);

//    // 4. 使能I2C外设
//    I2C_Enable(INA226_I2Cx, ENABLE);

    // 5. 初始化INA226寄存器
    INA226_SendData(INA226_ADDR1, CFG_REG, 0x8000);  // 软件复位

    system_delay_n_10us(100);                                     // 等待复位完成
    INA226_SendData(INA226_ADDR1, CFG_REG, MODE_INA226); // 配置测量模式
    INA226_SendData(INA226_ADDR1, CAL_REG, CAL);     // 写入校准值
    INA226_Get_ID(INA226_ADDR1);                     // 读取芯片ID，验证通信
}

/**
 * @brief  设置INA226的寄存器指针（指定要操作的寄存器）
 * @param  addr：INA226设备地址（写模式）
 * @param  reg：目标寄存器地址
 */
void INA226_SetRegPointer(uint8_t addr, uint8_t reg)
{
    // 等待I2C总线空闲
    while (I2C_GetFlag(INA226_I2Cx, I2C_FLAG_BUSY));

    // 1. 发送START条件
    I2C_GenerateStart(INA226_I2Cx, ENABLE);
    while (!I2C_GetFlag(INA226_I2Cx, I2C_FLAG_STARTBF));

    // 2. 发送设备地址（写模式）
    I2C_SendData(INA226_I2Cx, addr);
    while (!I2C_GetFlag(INA226_I2Cx, I2C_FLAG_ADDRF));
    // 清除ADDRF标志（必须先读STS1，再读STS2）
    uint16_t tmp = INA226_I2Cx->STS1;
    tmp = INA226_I2Cx->STS2;
    (void)tmp;

    // 3. 发送寄存器地址
    I2C_SendData(INA226_I2Cx, reg);
    while (!I2C_GetFlag(INA226_I2Cx, I2C_FLAG_TXDATE));

    // 4. 发送STOP条件
    I2C_GenerateStop(INA226_I2Cx, ENABLE);
    while (I2C_GetFlag(INA226_I2Cx, I2C_FLAG_BUSY));
}

/**
 * @brief  向INA226指定寄存器写入16位数据
 * @param  addr：设备地址（写模式）
 * @param  reg：目标寄存器
 * @param  data：要写入的16位数据
 */
void INA226_SendData(uint8_t addr, uint8_t reg, uint16_t data)
{
    // 等待I2C总线空闲
    while (I2C_GetFlag(INA226_I2Cx, I2C_FLAG_BUSY));

    // 1. 发送START条件
    I2C_GenerateStart(INA226_I2Cx, ENABLE);
    while (!I2C_GetFlag(INA226_I2Cx, I2C_FLAG_STARTBF));

    // 2. 发送设备地址（写模式）
    I2C_SendData(INA226_I2Cx, addr);
    while (!I2C_GetFlag(INA226_I2Cx, I2C_FLAG_ADDRF));

    // 清除ADDRF标志
    uint16_t tmp = INA226_I2Cx->STS1;
    tmp = INA226_I2Cx->STS2;
    (void)tmp;

    // 3. 发送寄存器地址
    I2C_SendData(INA226_I2Cx, reg);
    while (!I2C_GetFlag(INA226_I2Cx, I2C_FLAG_TXDATE));

    // 4. 发送16位数据（高8位+低8位）
    I2C_SendData(INA226_I2Cx, (uint8_t)(data >> 8));  // 高8位
    while (!I2C_GetFlag(INA226_I2Cx, I2C_FLAG_TXDATE));
    I2C_SendData(INA226_I2Cx, (uint8_t)(data & 0xFF));// 低8位
    while (!I2C_GetFlag(INA226_I2Cx, I2C_FLAG_TXDATE));

    // 5. 发送STOP条件
    I2C_GenerateStop(INA226_I2Cx, ENABLE);
    while (I2C_GetFlag(INA226_I2Cx, I2C_FLAG_BUSY));
}

/**
 * @brief  从INA226读取16位数据（需先调用INA226_SetRegPointer指定寄存器）
 * @param  addr：设备地址（写模式，仅用于匹配之前的寄存器指针）
 * @return 读取到的16位数据
 */
uint16_t INA226_ReadData(uint8_t addr)
{
    uint16_t data = 0;

    // 等待I2C总线空闲
    while (I2C_GetFlag(INA226_I2Cx, I2C_FLAG_BUSY));

    // 1. 发送START条件（重复START，切换为读模式）
    I2C_GenerateStart(INA226_I2Cx, ENABLE);
    while (!I2C_GetFlag(INA226_I2Cx, I2C_FLAG_STARTBF));

    // 2. 发送设备地址（读模式：addr + 1）
    I2C_SendData(INA226_I2Cx, addr + 1);
    while (!I2C_GetFlag(INA226_I2Cx, I2C_FLAG_ADDRF));
    // 清除ADDRF标志
    uint16_t tmp = INA226_I2Cx->STS1;
    tmp = INA226_I2Cx->STS2;
    (void)tmp;

    // 3. 读取16位数据（高8位+低8位，低8位后禁止应答）
    I2C_ConfigAck(INA226_I2Cx, ENABLE);              // 高8位后应答
    while (!I2C_GetFlag(INA226_I2Cx, I2C_FLAG_RXDATNE));
    data = (uint16_t)(I2C_RecvData(INA226_I2Cx) << 8);

    I2C_ConfigAck(INA226_I2Cx, DISABLE);             // 低8位后不应答
    while (!I2C_GetFlag(INA226_I2Cx, I2C_FLAG_RXDATNE));
    data |= (uint16_t)I2C_RecvData(INA226_I2Cx);

    // 4. 发送STOP条件
    I2C_GenerateStop(INA226_I2Cx, ENABLE);
    while (I2C_GetFlag(INA226_I2Cx, I2C_FLAG_BUSY));
    I2C_ConfigAck(INA226_I2Cx, ENABLE);              // 恢复应答使能

    return data;
}

/**
 * @brief  读取INA226芯片ID（验证通信是否正常）
 * @param  addr：设备地址（写模式）
 */
void INA226_Get_ID(uint8_t addr)
{
    INA226_SetRegPointer(addr, INA226_GET_ADDR);  // 指定ID寄存器
    ina226_data.ina226_id = INA226_ReadData(addr); // 读取ID（应为0x2260）
}

/**
 * @brief  读取校准寄存器值
 * @param  addr：设备地址（写模式）
 * @return 校准寄存器的16位值
 */
uint16_t INA226_GET_CAL_REG(uint8_t addr)
{
    INA226_SetRegPointer(addr, CAL_REG);
    return (uint16_t)INA226_ReadData(addr);
}

/**
 * @brief  读取分流电压（原始16位值）
 * @param  addr：设备地址（写模式）
 * @return 分流电压原始值（LSB=2.5uV）
 */
int16_t INA226_GetShuntVoltage(uint8_t addr)
{
    INA226_SetRegPointer(addr, SV_REG);
    return (int16_t)INA226_ReadData(addr);
}

/**
 * @brief  读取总线电压（原始16位值）
 * @param  addr：设备地址（写模式）
 * @return 总线电压原始值（LSB=1.25mV）
 */
uint16_t INA226_GetVoltage(uint8_t addr)
{
    INA226_SetRegPointer(addr, BV_REG);
    return (uint16_t)INA226_ReadData(addr);
}

/**
 * @brief  读取功率（原始16位值）
 * @param  addr：设备地址（写模式）
 * @return 功率原始值（LSB=25*mW，由CAL寄存器决定）
 */
uint16_t INA226_Get_Power(uint8_t addr)
{
    INA226_SetRegPointer(addr, PWR_REG);
    return (uint16_t)INA226_ReadData(addr);
}

/**
 * @brief  读取电流（原始16位值）
 * @param  addr：设备地址（写模式）
 * @return 电流原始值（LSB=1mA，由CAL寄存器决定）
 */
int16_t INA226_GetShunt_Current(uint8_t addr)
{
    INA226_SetRegPointer(addr, CUR_REG);
    return (int16_t)INA226_ReadData(addr);
}

/**
 * @brief  计算分流电压（单位：uV）
 * @param  Voltage：存储分流电压的指针
 */
void Get_Shunt_voltage(float *Voltage)
{
    *Voltage = (float)INA226_GetShuntVoltage(INA226_ADDR1) * INA226_VAL_LSB;
}

/**
 * @brief  计算总线电压（单位：mV）
 * @param  Voltage：存储总线电压的指针
 */
void GetVoltage(float *Voltage)
{
    *Voltage = (float)INA226_GetVoltage(INA226_ADDR1) * Voltage_LSB;
}

/**
 * @brief  计算功率（单位：mW，芯片直接读取）
 * @param  Power：存储功率的指针
 */
void Get_Power(float *Power)
{
    *Power = (float)INA226_Get_Power(INA226_ADDR1) * POWER_LSB;
}

extern double x_flag;

/**
 * @brief  计算电流（单位：mA）
 * @param  Current：存储电流的指针
 */
void Get_Shunt_Current(double *Current)
{
//		if(fabs(CURRENT_LSB * 1000 - 1) < 1e-6){//0-5A量程 单位A
//			*Current = (double)INA226_GetShunt_Current(INA226_ADDR1) * (CURRENT_LSB * 1000);
//			
//		}else if(fabs(CURRENT_LSB - 0.00001) < 1e-6){//0-90mA量程 单位mA
//			*Current = (double)INA226_GetShunt_Current(INA226_ADDR1) * (CURRENT_LSB * 1000);
//			
//		}else if(fabs(CURRENT_LSB - 0.0000001) < 1e-6){//0-0.8mA量程 单位uA
//			*Current = (double)INA226_GetShunt_Current(INA226_ADDR1) * (CURRENT_LSB * 1000);
//			
//		}
		*Current = (double)INA226_GetShunt_Current(INA226_ADDR1) * (CURRENT_LSB * 1000);
		//x_flag = INA226_GetShunt_Current(INA226_ADDR1);
}

/**
 * @brief  计算功率（单位：W，总线电压×电流）
 */
void get_power(void)
{
    GetVoltage(&ina226_data.voltageVal);         // 读取总线电压（mV）
    Get_Shunt_Current(&ina226_data.Shunt_Current); // 读取电流（mA）
    Get_Power(&ina226_data.Power);               // 读取芯片功率（mW）
    // 计算实际功率（W）：mV × mA = uW → 转换为W
    ina226_data.Power_Val = (ina226_data.voltageVal * 0.001f) * (ina226_data.Shunt_Current * 0.001f);
}
