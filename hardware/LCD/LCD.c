#include "lcd.h"
#include "stdlib.h"
#include "font.h"
#include "string.h"
#include "main.h"

// LCD前景色和背景色
u16 POINT_COLOR = 0x0000; // 画笔颜色
u16 BACK_COLOR = 0xFFFF;  // 背景色

// 管理LCD重要参数
// 默认为竖屏
_lcd_dev lcddev;

// SPI DMA发送完成标志
volatile uint8_t spi_dma_tx_complete = 0;
volatile uint8_t spi_dma_rx_complete = 0;

// SPI初始化函数
void LCD_SPI_Init(void)
{
	SPI_InitType SPI_InitStructure;
	GPIO_InitType GPIO_InitStructure;

	// 使能SPI和GPIO时钟
	RCC_EnableAPB2PeriphClk(LCD_SPI_CLK | LCD_CS_CLK | LCD_DC_CLK | LCD_RST_CLK | LCD_BLK_CLK, ENABLE);

	// 配置SPI引脚 (只使用MOSI不使用MISO)
	GPIO_InitStruct(&GPIO_InitStructure);
	GPIO_InitStructure.Pin = SPI_MASTER_PIN_SCK | SPI_MASTER_PIN_MOSI;
	GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_AF_PP;
	GPIO_InitStructure.GPIO_Pull = GPIO_NO_PULL;
	GPIO_InitStructure.GPIO_Alternate = GPIO_AF1_SPI1;
	GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure);

	// 初始化SPI
	SPI_I2S_DeInit(LCD_SPI_PORT);
	SPI_InitStructure.DataDirection = SPI_DIR_DOUBLELINE_FULLDUPLEX;
	SPI_InitStructure.SpiMode = SPI_MODE_MASTER;
	SPI_InitStructure.DataLen = SPI_DATA_SIZE_8BITS;
	SPI_InitStructure.CLKPOL = SPI_CLKPOL_LOW;
	SPI_InitStructure.CLKPHA = SPI_CLKPHA_FIRST_EDGE;
	SPI_InitStructure.NSS = SPI_NSS_SOFT;
	SPI_InitStructure.BaudRatePres = SPI_BR_PRESCALER_2; // 优化SPI时钟速度
	SPI_InitStructure.FirstBit = SPI_FB_MSB;
	SPI_InitStructure.CRCPoly = 7;
	SPI_Init(LCD_SPI_PORT, &SPI_InitStructure);

	// 使能SPI
	SPI_Enable(LCD_SPI_PORT, ENABLE);
	
	// 配置LCD控制引脚
	// CS - PA0 片选引脚
	GPIO_InitStructure.Pin = LCD_CS_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.GPIO_Pull = GPIO_NO_PULL;
	GPIO_InitPeripheral(LCD_CS_PORT, &GPIO_InitStructure);

	// DC - PB0 数据/命令选择引脚
	GPIO_InitStructure.Pin = LCD_DC_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.GPIO_Pull = GPIO_NO_PULL;
	GPIO_InitPeripheral(LCD_DC_PORT, &GPIO_InitStructure);

	// RST - PA3 复位引脚
	GPIO_InitStructure.Pin = LCD_RST_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.GPIO_Pull = GPIO_NO_PULL;
	GPIO_InitPeripheral(LCD_RST_PORT, &GPIO_InitStructure);

	// BLK - PA6 背光控制引脚
	GPIO_InitStructure.Pin = LCD_BLK_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.GPIO_Pull = GPIO_NO_PULL;
	GPIO_InitPeripheral(LCD_BLK_PORT, &GPIO_InitStructure);
	
	// 初始化控制引脚默认状态
	LCD_CS_SET;
	LCD_DC_SET;
	LCD_RST_SET;
}

// DMA初始化函数
void LCD_DMA_Init(void)
{
	DMA_InitType DMA_InitStructure;

	// 使能DMA时钟
	RCC_EnableAHBPeriphClk(SPI_MASTER_DMA_CLK, ENABLE);

	// 配置TX DMA通道
	DMA_DeInit(SPI_MASTER_TX_DMA_CHANNEL);
	DMA_InitStructure.PeriphAddr = (uint32_t)&LCD_SPI_PORT->DAT;
	DMA_InitStructure.MemAddr = 0; // 内存地址稍后设置
	DMA_InitStructure.Direction = DMA_DIR_PERIPH_DST;
	DMA_InitStructure.BufSize = 0; // 缓冲区大小稍后设置
	DMA_InitStructure.PeriphInc = DMA_PERIPH_INC_DISABLE;
	DMA_InitStructure.DMA_MemoryInc = DMA_MEM_INC_ENABLE;
	DMA_InitStructure.PeriphDataSize = DMA_PERIPH_DATA_SIZE_BYTE;
	DMA_InitStructure.MemDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.CircularMode = DMA_MODE_NORMAL;
	DMA_InitStructure.Priority = DMA_PRIORITY_HIGH;
	DMA_InitStructure.Mem2Mem = DMA_M2M_DISABLE;
	DMA_Init(SPI_MASTER_TX_DMA_CHANNEL, &DMA_InitStructure);
	DMA_RequestRemap(SPI_MASTER_TX_DMA_REMAP, SPI_MASTER_DMA, SPI_MASTER_TX_DMA_CHANNEL, ENABLE);

	// 使能SPI的DMA传输
	SPI_I2S_EnableDma(LCD_SPI_PORT, SPI_I2S_DMA_TX, ENABLE);
}

// SPI发送单字节函数
void LCD_SPI_SendByte(uint8_t data)
{
	// 等待发送缓冲区为空
	while (SPI_I2S_GetStatus(LCD_SPI_PORT, SPI_I2S_TE_FLAG) == RESET)
		;
	// 发送数据
	SPI_I2S_TransmitData(LCD_SPI_PORT, data);
	// 等待发送完成
	while (SPI_I2S_GetStatus(LCD_SPI_PORT, SPI_I2S_BUSY_FLAG) == SET)
		;
}

// SPI DMA传输函数
void LCD_SPI_DMA_Send(uint8_t *data, uint16_t size)
{
	uint32_t timeout = 0;

	// 复位发送完成标志为0
	spi_dma_tx_complete = 0;

	// 清除DMA传输完成标志
	DMA_ClearFlag(SPI_MASTER_TX_DMA_FLAG, SPI_MASTER_DMA);

	// 设置DMA传输参数
	DMA_InitType DMA_InitStructure;

	// 配置TX DMA通道
	DMA_DeInit(SPI_MASTER_TX_DMA_CHANNEL);
	DMA_InitStructure.PeriphAddr = (uint32_t)&LCD_SPI_PORT->DAT;
	DMA_InitStructure.MemAddr = (uint32_t)data;
	DMA_InitStructure.Direction = DMA_DIR_PERIPH_DST;
	DMA_InitStructure.BufSize = size;
	DMA_InitStructure.PeriphInc = DMA_PERIPH_INC_DISABLE;
	DMA_InitStructure.DMA_MemoryInc = DMA_MEM_INC_ENABLE;
	DMA_InitStructure.PeriphDataSize = DMA_PERIPH_DATA_SIZE_BYTE;
	DMA_InitStructure.MemDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.CircularMode = DMA_MODE_NORMAL;
	DMA_InitStructure.Priority = DMA_PRIORITY_HIGH;
	DMA_InitStructure.Mem2Mem = DMA_M2M_DISABLE;
	DMA_Init(SPI_MASTER_TX_DMA_CHANNEL, &DMA_InitStructure);
	DMA_RequestRemap(SPI_MASTER_TX_DMA_REMAP, SPI_MASTER_DMA, SPI_MASTER_TX_DMA_CHANNEL, ENABLE);

	// 启用DMA中断
	DMA_ConfigInt(SPI_MASTER_TX_DMA_CHANNEL, DMA_INT_TXC, ENABLE);

	// 使能DMA通道，开始传输
	DMA_EnableChannel(SPI_MASTER_TX_DMA_CHANNEL, ENABLE);

	// 等待传输完成，带超时机制
	while (!spi_dma_tx_complete && timeout < 100000)
	{
		timeout++;
	}

	// 如果超时，说明传输失败
	if (timeout >= 100000)
	{

	}
}

// SPI DMA快速批量传输函数 - 不使用中断，轮询方式提高效率
void LCD_SPI_DMA_Send_Fast(uint8_t *data, uint16_t size)
{
	// 清除DMA传输完成标志
	DMA_ClearFlag(SPI_MASTER_TX_DMA_FLAG, SPI_MASTER_DMA);

	// 设置DMA传输参数
	DMA_InitType DMA_InitStructure;

	// 配置TX DMA通道
	DMA_DeInit(SPI_MASTER_TX_DMA_CHANNEL);
	DMA_InitStructure.PeriphAddr = (uint32_t)&LCD_SPI_PORT->DAT;
	DMA_InitStructure.MemAddr = (uint32_t)data;
	DMA_InitStructure.Direction = DMA_DIR_PERIPH_DST;
	DMA_InitStructure.BufSize = size;
	DMA_InitStructure.PeriphInc = DMA_PERIPH_INC_DISABLE;
	DMA_InitStructure.DMA_MemoryInc = DMA_MEM_INC_ENABLE;
	DMA_InitStructure.PeriphDataSize = DMA_PERIPH_DATA_SIZE_BYTE;
	DMA_InitStructure.MemDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.CircularMode = DMA_MODE_NORMAL;
	DMA_InitStructure.Priority = DMA_PRIORITY_HIGH;
	DMA_InitStructure.Mem2Mem = DMA_M2M_DISABLE;
	DMA_Init(SPI_MASTER_TX_DMA_CHANNEL, &DMA_InitStructure);
	DMA_RequestRemap(SPI_MASTER_TX_DMA_REMAP, SPI_MASTER_DMA, SPI_MASTER_TX_DMA_CHANNEL, ENABLE);

	// 使能DMA通道，开始传输
	DMA_EnableChannel(SPI_MASTER_TX_DMA_CHANNEL, ENABLE);

	// 等待传输完成，使用轮询方式提高效率
	while (DMA_GetFlagStatus(SPI_MASTER_TX_DMA_FLAG, SPI_MASTER_DMA) == RESET);

	// 禁用DMA通道
	DMA_EnableChannel(SPI_MASTER_TX_DMA_CHANNEL, DISABLE);
}

// 快速写寄存器函数 - 使用轮询方式提高效率
// data: 寄存器值
void LCD_WR_REG_Fast(uint16_t data)
{
	LCD_CS_CLR; // 片选LCD
	LCD_DC_CLR; // DC=0 命令模式

	uint8_t value = (uint8_t)(data & 0xFF);
	// 等待发送缓冲区为空
	while (SPI_I2S_GetStatus(LCD_SPI_PORT, SPI_I2S_TE_FLAG) == RESET);
	// 发送数据
	SPI_I2S_TransmitData(LCD_SPI_PORT, value);
	// 等待发送完成
	while (SPI_I2S_GetStatus(LCD_SPI_PORT, SPI_I2S_BUSY_FLAG) == SET);

	LCD_CS_SET; // 取消片选
}

// 快速写数据函数 - 使用轮询方式提高效率
// data: 数据值
void LCD_WR_DATA_Fast(uint16_t data)
{
	LCD_CS_CLR; // 片选LCD
	LCD_DC_SET; // DC=1 数据模式

	uint8_t value = (uint8_t)(data & 0xFF);
	// 等待发送缓冲区为空
	while (SPI_I2S_GetStatus(LCD_SPI_PORT, SPI_I2S_TE_FLAG) == RESET);
	// 发送数据
	SPI_I2S_TransmitData(LCD_SPI_PORT, value);
	// 等待发送完成
	while (SPI_I2S_GetStatus(LCD_SPI_PORT, SPI_I2S_BUSY_FLAG) == SET);

	LCD_CS_SET; // 取消片选
}

// 快速写16位数据函数 - 使用轮询方式提高效率
void LCD_WR_DATA_16_Fast(uint16_t data)
{
	LCD_CS_CLR; // 片选LCD
	LCD_DC_SET; // DC=1 数据模式

	uint8_t value = (uint8_t)(data >> 8 & 0xFF);
	// 等待发送缓冲区为空
	while (SPI_I2S_GetStatus(LCD_SPI_PORT, SPI_I2S_TE_FLAG) == RESET);
	// 发送高字节
	SPI_I2S_TransmitData(LCD_SPI_PORT, value);
	// 等待发送完成
	while (SPI_I2S_GetStatus(LCD_SPI_PORT, SPI_I2S_BUSY_FLAG) == SET);

	value = (uint8_t)(data & 0xFF);
	// 等待发送缓冲区为空
	while (SPI_I2S_GetStatus(LCD_SPI_PORT, SPI_I2S_TE_FLAG) == RESET);
	// 发送低字节
	SPI_I2S_TransmitData(LCD_SPI_PORT, value);
	// 等待发送完成
	while (SPI_I2S_GetStatus(LCD_SPI_PORT, SPI_I2S_BUSY_FLAG) == SET);

	LCD_CS_SET; // 取消片选
}

// 写寄存器函数
// data: 寄存器值
void LCD_WR_REG(uint16_t data)
{
	LCD_WR_REG_Fast(data);
}

// 写数据函数
// data: 数据值
void LCD_WR_DATA(uint16_t data)
{
	LCD_WR_DATA_Fast(data);
}

// 写16位数据函数
void LCD_WR_DATA_16(uint16_t data)
{
	LCD_WR_DATA_16_Fast(data);
}

// 写寄存器
// LCD_Reg: 寄存器地址
// LCD_RegValue: 要写入的值
void LCD_WriteReg(uint16_t LCD_Reg, uint16_t LCD_RegValue)
{
	LCD_WR_REG(LCD_Reg);
	LCD_WR_DATA_16(LCD_RegValue);
}

// 开始写GRAM
void LCD_WriteRAM_Prepare(void)
{
	LCD_WR_REG(lcddev.wramcmd);
}

// LCD写GRAM
// RGB_Code: 颜色值
void LCD_WriteRAM(uint16_t RGB_Code)
{
	LCD_WR_DATA_16(RGB_Code);
	
	//以下是SPI+DMA中断传输数据
//	LCD_CS_CLR; // 片选LCD
//	LCD_DC_SET; // DC=1 数据模式
//	uint8_t value = (uint8_t)(RGB_Code >> 8 & 0xFF);
//	LCD_SPI_DMA_Send(&value,1);

//	value = (uint8_t)(RGB_Code & 0xFF);
//	LCD_SPI_DMA_Send(&value,1);
//	LCD_CS_SET; // 取消片选
}

// LCD开始显示
void LCD_DisplayOn(void)
{
	LCD_WR_REG(0X29); // 开始显示
}

// LCD关闭显示
void LCD_DisplayOff(void)
{
	LCD_WR_REG(0X28); // 关闭显示
}

// 设置光标位置
// Xpos: 横坐标
// Ypos: 纵坐标
void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos)
{
	LCD_WR_REG(lcddev.setxcmd);
	LCD_WR_DATA_16(Xpos);
	LCD_WR_REG(lcddev.setycmd);
	LCD_WR_DATA_16(Ypos);
}

// 画点
// x,y: 坐标
// POINT_COLOR: 点的颜色
void LCD_DrawPoint(uint16_t x, uint16_t y)
{
	LCD_SetCursor(x, y);	// 设置光标位置
	LCD_WriteRAM_Prepare(); // ��ʼдGRAM
	LCD_WriteRAM(POINT_COLOR);
}

// 快速画点
// x,y: 坐标
// color: 颜色
void LCD_Fast_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
	// 设置光标位置
	LCD_SetCursor(x, y);
	// 写入颜色
	LCD_WriteReg(lcddev.wramcmd, color);
}

// 屏幕选择 	0-0度旋转 1-180度旋转 2-270度旋转 3-90度旋转
void LCD_Display_Dir(uint8_t dir)
{
	if (dir == 0 || dir == 1) // 竖屏
	{
		lcddev.dir = 0; // 竖屏
		lcddev.width = 128;
		lcddev.height = 160;

		lcddev.wramcmd = 0X2C;
		lcddev.setxcmd = 0X2A;
		lcddev.setycmd = 0X2B;

		if (dir == 0) // 0-0度旋转
		{
			LCD_WR_REG(0x36);
			LCD_WR_DATA(0x00); // 正确的MADCTL配置
		}
		else // 1-180度旋转
		{
			LCD_WR_REG(0x36);
			LCD_WR_DATA(0xC0); // 正确的MADCTL配置
		}
	}
	else if (dir == 2 || dir == 3)
	{
		lcddev.dir = 1; // 横屏
		lcddev.width = 160;
		lcddev.height = 128;

		lcddev.wramcmd = 0X2C;
		lcddev.setxcmd = 0X2A;
		lcddev.setycmd = 0X2B;

		if (dir == 2) // 2-270度旋转
		{
			LCD_WR_REG(0x36);
			LCD_WR_DATA(0x60); // 正确的MADCTL配置
		}
		else // 3-90度旋转
		{
			LCD_WR_REG(0x36);
			LCD_WR_DATA(0x20); // 正确的MADCTL配置
		}
	}

	// 设置显示范围
	LCD_WR_REG(lcddev.setxcmd);
	LCD_WR_DATA_16(0);
	LCD_WR_DATA_16(lcddev.width - 1);
	LCD_WR_REG(lcddev.setycmd);
	LCD_WR_DATA_16(0);
	LCD_WR_DATA_16(lcddev.height - 1);
}

// 设置窗口,所有坐标都以原点为基准(sx,sy).
// sx,sy: 窗口起始坐标(左上角)
// width,height: 窗口宽度和高度,必须大于0!!
// 窗口大小: width*height.
void LCD_Set_Window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
	uint16_t twidth, theight;
	twidth = sx + width - 1;
	theight = sy + height - 1;

	LCD_WR_REG(lcddev.setxcmd);
	LCD_WR_DATA_16(sx);
	LCD_WR_DATA_16(twidth);
	LCD_WR_REG(lcddev.setycmd);
	LCD_WR_DATA_16(sy);
	LCD_WR_DATA_16(theight);
}

// 初始化lcd
void LCD_Init(void)
{
	// 初始化SPI和DMA
	LCD_SPI_Init();
	LCD_DMA_Init();
	LCD_BLK_CLR;
	// LCD复位
	LCD_RST_CLR;
	for (int i = 0; i < 0xFFFFF; i++)
		; // 延时
	LCD_RST_SET;
	for (int i = 0; i < 0xFFFFF; i++)
		; // 延时

	//************* Start Initial Sequence **********//
	LCD_WR_REG(0x11); // Sleep out
	for (int i = 0; i < 0xFFFFF; i++)
		; // 延时

	LCD_WR_REG(0xB1);  // In normal mode
	LCD_WR_DATA(0x00); // frame rate=85.3Hz
	LCD_WR_DATA(0x2C);
	LCD_WR_DATA(0x2B);

	LCD_WR_REG(0xB2); // In Idle mode
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x01);
	LCD_WR_DATA(0x01);

	LCD_WR_REG(0xB3); // In partial mode
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x01);
	LCD_WR_DATA(0x01);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x01);
	LCD_WR_DATA(0x01);

	LCD_WR_REG(0xB4); // DOT inversion Display Inversion Control
	LCD_WR_DATA(0x03);

	LCD_WR_REG(0xB9); // In normal mode
	LCD_WR_DATA(0xFF);
	LCD_WR_DATA(0x83);
	LCD_WR_DATA(0x47);

	LCD_WR_REG(0xC0); // VRH: Set the GVDD
	LCD_WR_DATA(0xA2);
	LCD_WR_DATA(0x02);
	LCD_WR_DATA(0x84);

	LCD_WR_REG(0xC1); // set VGH/ VGL
	LCD_WR_DATA(0x02);

	LCD_WR_REG(0xC2); // APA: adjust the operational amplifier DCA: adjust the booster Voltage
	LCD_WR_DATA(0x0A);
	LCD_WR_DATA(0x00);

	LCD_WR_REG(0xC3); // In Idle mode (8-colors)
	LCD_WR_DATA(0x8A);
	LCD_WR_DATA(0x2A);

	LCD_WR_REG(0xC4); // In partial mode + Full color
	LCD_WR_DATA(0x8A);
	LCD_WR_DATA(0xEE);

	LCD_WR_REG(0xC5); // VCOM
	LCD_WR_DATA(0x09);

	LCD_WR_REG(0x20); // Display inversion

	LCD_WR_REG(0xC7);
	LCD_WR_DATA(0x10);

	LCD_WR_REG(0x36); // MX, MY, RGB mode
	LCD_WR_DATA(0xC0);

	LCD_WR_REG(0xE0);
	LCD_WR_DATA(0x0C);
	LCD_WR_DATA(0x1C);
	LCD_WR_DATA(0x1B);
	LCD_WR_DATA(0x1A);
	LCD_WR_DATA(0x2F);
	LCD_WR_DATA(0x28);
	LCD_WR_DATA(0x20);
	LCD_WR_DATA(0x24);
	LCD_WR_DATA(0x23);
	LCD_WR_DATA(0x22);
	LCD_WR_DATA(0x2A);
	LCD_WR_DATA(0x36);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x05);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x10);

	LCD_WR_REG(0xE1);
	LCD_WR_DATA(0x0C);
	LCD_WR_DATA(0x1A);
	LCD_WR_DATA(0x1A);
	LCD_WR_DATA(0x1A);
	LCD_WR_DATA(0x2E);
	LCD_WR_DATA(0x27);
	LCD_WR_DATA(0x21);
	LCD_WR_DATA(0x24);
	LCD_WR_DATA(0x24);
	LCD_WR_DATA(0x22);
	LCD_WR_DATA(0x2A);
	LCD_WR_DATA(0x35);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x05);
	LCD_WR_DATA(0x00);
	LCD_WR_DATA(0x10);

	LCD_WR_REG(0x35); // 65k mode
	LCD_WR_DATA(0x00);

	LCD_WR_REG(0x3A); // 65k mode
	LCD_WR_DATA(0x05);

	LCD_WR_REG(0x29); // Display on

	// 设置默认显示方向
	LCD_Display_Dir(0);

	// 开启背光
	LCD_BLK_SET;
}

// 读数据
// 返回值: 读到的值
uint16_t LCD_RD_DATA(void)
{
	uint16_t data = 0;

	// 读取虚假数据以防时序信号
	while (SPI_I2S_GetStatus(LCD_SPI_PORT, SPI_I2S_TE_FLAG) == RESET)
		;
	SPI_I2S_TransmitData(LCD_SPI_PORT, 0xFF);

	// 读取返回的数据
	while (SPI_I2S_GetStatus(LCD_SPI_PORT, SPI_I2S_RNE_FLAG) == RESET)
		;
	data = SPI_I2S_ReceiveData(LCD_SPI_PORT);

	return data;
}

// 读取显示屏 ID
uint16_t LCD_Read_ID(uint8_t reg)
{
	LCD_CS_CLR; // 片选LCD
	LCD_DC_CLR; // DC=0 命令模式

	LCD_SPI_DMA_Send(&reg,1);

	LCD_DC_SET; // DC=1 数据模式

	// 读取数据
	lcddev.id = LCD_RD_DATA();
	lcddev.id <<= 8;
	lcddev.id |= LCD_RD_DATA();

	LCD_CS_SET; // 取消片选

	return lcddev.id;
}

// 读取某个点的颜色值
// x,y: 坐标
// ����ֵ: �˵����ɫ
uint16_t LCD_ReadPoint(uint16_t x, uint16_t y)
{
	uint16_t color = 0;
	uint8_t r, g, b;
	uint8_t reg = 0x2E; // 读取颜色值命令

	if (x >= lcddev.width || y >= lcddev.height)
		return 0; // 超出范围,返回

	LCD_SetCursor(x, y);

	LCD_CS_CLR; // 片选LCD
	LCD_DC_CLR; // DC=0 命令模式

	LCD_SPI_DMA_Send(&reg,1);

	LCD_DC_SET; // DC=1 数据模式

	// 读取数据 (第一个是dummy读)
	LCD_RD_DATA(); // dummy read
	r = LCD_RD_DATA();
	g = LCD_RD_DATA();
	b = LCD_RD_DATA();

	LCD_CS_SET; // 取消片选

	// 转换为16位颜色值
	color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);

	return color;
}

// 清屏
// color: 要填充的颜色
void LCD_Clear(uint16_t color)
{
	uint32_t totalpoint = lcddev.width;
	totalpoint *= lcddev.height; // 得到总点数

	// 设置显示窗口为全屏
	LCD_SetCursor(0, 0);
	LCD_WriteRAM_Prepare();

	// 使用DMA批量传输提高填充效率
	// 将16位颜色转换为字节数组进行传输
	uint8_t *fill_data = (uint8_t *)malloc(totalpoint * 2);
	if (fill_data != NULL)
	{
		// 填充颜色数据 - 高字节在前，低字节在后
		for (uint32_t i = 0; i < totalpoint; i++)
		{
			fill_data[i * 2] = (uint8_t)(color >> 8);     // 高字节
			fill_data[i * 2 + 1] = (uint8_t)(color & 0xFF); // 低字节
		}

		// 使用快速DMA传输
		LCD_SPI_DMA_Send(fill_data, totalpoint * 2);

		free(fill_data);
	}
	else
	{
		// 内存分配失败，回退到普通方式
		for (uint32_t index = 0; index < totalpoint; index++)
			LCD_WriteRAM(color);
	}
}

// 在指定区域填充单一颜色
// �����С: (xend-xsta+1)*(yend-ysta+1)
// xsta
// color: Ҫ������ɫ
void LCD_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
	uint16_t i, j;
	uint16_t xlen = 0;

	xlen = ex - sx + 1;
	for (i = sy; i <= ey; i++)
	{
		LCD_SetCursor(sx, i);	// 设置光标位置
		LCD_WriteRAM_Prepare(); // 开始写GRAM
		for (j = 0; j < xlen; j++)
			LCD_WriteRAM(color); // 设置光标位置
	}
}

// 在指定区域填充指定颜色数组
// (sx,sy),(ex,ey): ����Խ�����,�����СΪ: (ex-sx+1)*(ey-sy+1)
// color: Ҫ������ɫ
void LCD_Color_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
	uint16_t height, width;
	uint16_t i, j;
	width = ex - sx + 1;  // �õ����εĿ���
	height = ey - sy + 1; // �߶�
//	for (i = 0; i < height; i++)
//	{
//		LCD_SetCursor(sx, sy + i); // ���ù��λ��
//		LCD_WriteRAM_Prepare();	   // ��ʼдGRAM
//		for (j = 0; j < width; j++)
//			LCD_WriteRAM(color[i * width + j]); // д������		
//	}
	
	LCD_Set_Window(sx, sy, width, height); 
	LCD_WriteRAM_Prepare();	 
	LCD_CS_CLR; // 片选LCD
	LCD_DC_SET; // DC=0 命令模式
	LCD_SPI_DMA_Send_Fast((uint8_t*)color,width*height*2);
	LCD_CS_SET; // 取消片选
}

// ����
// x1,y1: �������
// x2,y2: �յ�����
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	uint16_t t;
	int xerr = 0, yerr = 0, delta_x, delta_y, distance;
	int incx, incy, uRow, uCol;
	delta_x = x2 - x1; // ������������
	delta_y = y2 - y1;
	uRow = x1;
	uCol = y1;
	if (delta_x > 0)
		incx = 1; // ���õ�������
	else if (delta_x == 0)
		incx = 0; // ��ֱ��
	else
	{
		incx = -1;
		delta_x = -delta_x;
	}
	if (delta_y > 0)
		incy = 1;
	else if (delta_y == 0)
		incy = 0; // ˮƽ��
	else
	{
		incy = -1;
		delta_y = -delta_y;
	}
	if (delta_x > delta_y)
		distance = delta_x; // ѡȡ��������������
	else
		distance = delta_y;
	for (t = 0; t <= distance + 1; t++) // �������
	{
		LCD_DrawPoint(uRow, uCol); // ����
		xerr += delta_x;
		yerr += delta_y;
		if (xerr > distance)
		{
			xerr -= distance;
			uRow += incx;
		}
		if (yerr > distance)
		{
			yerr -= distance;
			uCol += incy;
		}
	}
}

// ������
// (x1,y1),(x2,y2): ���εĶԽ�����
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	LCD_DrawLine(x1, y1, x2, y1);
	LCD_DrawLine(x1, y1, x1, y2);
	LCD_DrawLine(x1, y2, x2, y2);
	LCD_DrawLine(x2, y1, x2, y2);
}

// ��ָ��λ�û�һ��ָ����С��Բ
// (x,y): ���ĵ�
// r    : �뾶
void LCD_Draw_Circle(uint16_t x0, uint16_t y0, uint8_t r)
{
	int a, b;
	int di;
	a = 0;
	b = r;
	di = 3 - (r << 1); // �ж��¸���λ�õı�־
	while (a <= b)
	{
		LCD_DrawPoint(x0 + a, y0 - b); // 5
		LCD_DrawPoint(x0 + b, y0 - a); // 0
		LCD_DrawPoint(x0 + b, y0 + a); // 4
		LCD_DrawPoint(x0 + a, y0 + b); // 6
		LCD_DrawPoint(x0 - a, y0 + b); // 1
		LCD_DrawPoint(x0 - b, y0 + a);
		LCD_DrawPoint(x0 - a, y0 - b); // 2
		LCD_DrawPoint(x0 - b, y0 - a); // 7
		a++;
		// ʹ��Bresenham�㷨��Բ
		if (di < 0)
			di += 4 * a + 6;
		else
		{
			di += 10 + 4 * (a - b);
			b--;
		}
	}
}

// ��ָ��λ����ʾһ���ַ�
// x,y: ��ʼ����
// num: Ҫ��ʾ���ַ�:" "--->"~"
// size: �����С 12/16/24
// mode: ���ӷ�ʽ(1)�Ƿ�ǵ��ӷ�ʽ(0)
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint8_t mode)
{
	uint8_t temp, t1, t;
	uint16_t y0 = y;
	uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2); // �õ�һ���ַ���ռ���ֽ���
	num = num - ' ';												// �õ�ƫ�ƺ��ֵ(ASCII���Ǵӿո�ʼȡģ,����-' '���Ƕ�Ӧ�ַ����ֿ�)
	for (t = 0; t < csize; t++)
	{
		if (size == 12)
			temp = asc2_1206[num][t]; // ����1206����
		else if (size == 16)
			temp = asc2_1608[num][t]; // ����1608����
		else if (size == 24)
			temp = asc2_2412[num][t]; // ����2412����
		else
			return; // û�е��ֿ�
		for (t1 = 0; t1 < 8; t1++)
		{
			if (temp & 0x80)
				LCD_Fast_DrawPoint(x, y, POINT_COLOR);
			else if (mode == 0)
				LCD_Fast_DrawPoint(x, y, BACK_COLOR);
			temp <<= 1;
			y++;
			if (y >= lcddev.height)
				return; // ������Χ
			if ((y - y0) == size)
			{
				y = y0;
				x++;
				if (x >= lcddev.width)
					return; // ������Χ
				break;
			}
		}
	}
}

// m^n�η�
// ����ֵ: m^n�η�.
uint32_t LCD_Pow(uint8_t m, uint8_t n)
{
	uint32_t result = 1;
	while (n--)
		result *= m;
	return result;
}

// ��ʾ����,��λΪ0,����ʾ
// x,y : �������
// len : ���ֵ�λ��
// size: �����С
// color: ��ɫ
// num: ��ֵ(0~4294967295);
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size)
{
	uint8_t t, temp;
	uint8_t enshow = 0;
	for (t = 0; t < len; t++)
	{
		temp = (num / LCD_Pow(10, len - t - 1)) % 10;
		if (enshow == 0 && t < (len - 1))
		{
			if (temp == 0)
			{
				LCD_ShowChar(x + (size / 2) * t, y, ' ', size, 0);
				continue;
			}
			else
				enshow = 1;
		}
		LCD_ShowChar(x + (size / 2) * t, y, temp + '0', size, 0);
	}
}

// ��ʾ�ַ���
// x,y: �������
// width,height: �����С
// size: �����С
// *p: �ַ�����ʼ��ַ
void LCD_ShowString(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, uint8_t *p)
{
	uint8_t x0 = x;
	width += x;
	height += y;
	while ((*p <= '~') && (*p >= ' ')) // �ж��ǲ��ǷǷ��ַ�!
	{
		if (x >= width)
		{
			x = x0;
			y += size;
		}
		if (y >= height)
			break; // �˳�
		LCD_ShowChar(x, y, *p, size, 0);
		x += size / 2;
		p++;
	}
}

// ��ʾ���ֻ��ַ���
void Show_Str(uint16_t x, uint16_t y, uint8_t *str, uint8_t size, uint8_t mode)
{
	uint16_t x0 = x;
	uint8_t bHz = 0;  // �ַ��Ƿ�Ϊ����
	while (*str != 0) // ����δ����
	{
		if (!bHz)
		{
			if (x > (lcddev.width - size / 2) || y > (lcddev.height - size))
				return;
			if (*str > 0x80)
				bHz = 1; // ����
			else		 // �ַ�
			{
				if (*str == 0x0D) // ���з�
				{
					y += size;
					x = x0;
					str++;
				}
				else
				{
					if (size >= 24) // �ֿ���û��12X24 16X32��Ӣ������,��8X16����
					{
						LCD_ShowChar(x, y, *str, 24, mode);
						x += 12; // �ַ�,Ϊȫ�ǵ�һ��
					}
					else
					{
						LCD_ShowChar(x, y, *str, size, mode);
						x += size / 2; // �ַ�,Ϊȫ�ǵ�һ��
					}
				}
				str++;
			}
		}
		else // ����
		{
			if (x > (lcddev.width - size) || y > (lcddev.height - size))
				return;
			bHz = 0; // �к��ֿ�
			// ����Ӧ�����Ӻ�����ʾ���룬��Ϊ�˼�ʾ��������ֻ����ASCII�ַ�
			str += 2;
			x += size; // һ������ƫ��
		}
	}
}

// ��ʾ40*40ͼƬ
void Gui_Drawbmp16(uint16_t x, uint16_t y, const unsigned char *p) // ��ʾ40*40ͼƬ
{
	int i;
	unsigned char picH, picL;
	LCD_Set_Window(x, y, 40, 40);
	LCD_WriteRAM_Prepare();

	for (i = 0; i < 40 * 40; i++)
	{
		picL = *(p + i * 2); // ���ݵ�λ��ǰ
		picH = *(p + i * 2 + 1);
		LCD_WriteRAM(picH << 8 | picL);
	}
	LCD_Set_Window(0, 0, lcddev.width, lcddev.height); // �ָ���ʾ��ΧΪȫ��
}

// ������ʾ
void Gui_StrCenter(uint16_t x, uint16_t y, uint8_t *str, uint8_t size, uint8_t mode)
{
	uint16_t x1;
	uint16_t len = strlen((const char *)str);
	if (size > 16)
	{
		x1 = (lcddev.width - len * (size / 2)) / 2;
	}
	else
	{
		x1 = (lcddev.width - len * 8) / 2;
	}

	Show_Str(x + x1, y, str, size, mode);
}

// ��ˮƽ��
// x0,y0: ���
// len: �߳�
// color: ��ɫ
void gui_draw_hline(uint16_t x0, uint16_t y0, uint16_t len, uint16_t color)
{
	if (len == 0)
		return;
	LCD_Fill(x0, y0, x0 + len - 1, y0, color);
}

// ��ʵ��Բ
// x0,y0: ���ĵ�
// r: �뾶
// color: ��ɫ
void gui_fill_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color)
{
	uint32_t i;
	uint32_t imax = ((uint32_t)r * 707) / 1000 + 1;
	uint32_t sqmax = (uint32_t)r * (uint32_t)r + (uint32_t)r / 2;
	uint32_t x = r;
	gui_draw_hline(x0 - r, y0, 2 * r, color);
	for (i = 1; i <= imax; i++)
	{
		if ((i * i + x * x) > sqmax) // draw lines from outside
		{
			if (x > imax)
			{
				gui_draw_hline(x0 - i + 1, y0 + x, 2 * (i - 1), color);
				gui_draw_hline(x0 - i + 1, y0 - x, 2 * (i - 1), color);
			}
			x--;
		}
		// draw lines from inside (center)
		gui_draw_hline(x0 - x, y0 + i, 2 * x, color);
		gui_draw_hline(x0 - x, y0 - i, 2 * x, color);
	}
}

// ��һ������
// (x1,y1),(x2,y2): ֱ�ߵ���ʼ����
// size��������ϸ�³̶�
// color����������ɫ
void lcd_draw_bline(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t size, uint16_t color)
{
	uint16_t t;
	int xerr = 0, yerr = 0, delta_x, delta_y, distance;
	int incx, incy, uRow, uCol;
	if (x1 < size || x2 < size || y1 < size || y2 < size)
		return;
	delta_x = x2 - x1; // ������������
	delta_y = y2 - y1;
	uRow = x1;
	uCol = y1;
	if (delta_x > 0)
		incx = 1; // ���õ�������
	else if (delta_x == 0)
		incx = 0; // ��ֱ��
	else
	{
		incx = -1;
		delta_x = -delta_x;
	}
	if (delta_y > 0)
		incy = 1;
	else if (delta_y == 0)
		incy = 0; // ˮƽ��
	else
	{
		incy = -1;
		delta_y = -delta_y;
	}
	if (delta_x > delta_y)
		distance = delta_x; // ѡȡ��������������
	else
		distance = delta_y;
	for (t = 0; t <= distance + 1; t++) // �������
	{
		gui_fill_circle(uRow, uCol, size, color); // ����
		xerr += delta_x;
		yerr += delta_y;
		if (xerr > distance)
		{
			xerr -= distance;
			uRow += incx;
		}
		if (yerr > distance)
		{
			yerr -= distance;
			uCol += incy;
		}
	}
}

// �����Ի���
void Load_Drow_Dialog(void)
{
	LCD_Clear(WHITE);	// ����
	POINT_COLOR = BLUE; // ��������Ϊ��ɫ
	BACK_COLOR = WHITE;
	LCD_ShowString(lcddev.width - 24, 0, 200, 16, 16, "RST"); // ��ʾ������ť
	POINT_COLOR = RED;										  // ���û�����ɫ
}
