#ifndef __LCD_H
#define __LCD_H

#include "n32wb03x.h"
#include "main.h"

//支持横竖屏和不同角度切换
#define USE_LCM_DIR  	  2   	//默认液晶屏顺时针旋转角度 	0-0度旋转 1-180度旋转 2-270度旋转 3-90度旋转

// LCD重要参数结构体
typedef struct
{
	u16 width;	 // LCD 宽度
	u16 height;	 // LCD 高度
	u16 id;		 // LCD ID
	u8 dir;		 // 横竖屏控制标志,0竖屏 1横屏
	u16 wramcmd; // 开始写gram指令
	u16 setxcmd; // 设置x坐标指令
	u16 setycmd; // 设置y坐标指令
} _lcd_dev;

// LCD参数
extern _lcd_dev lcddev; // 管理LCD重要参数
// LCD前景色和背景色
extern u16 POINT_COLOR; // 默认红笔颜色
extern u16 BACK_COLOR;	// 背景颜色.默认白色

//常用颜色
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE         	 0x001F  
#define BRED             0XF81F
#define GRED 			 0XFFE0
#define GBLUE			 0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			 0XBC40 //棕色
#define BRRED 			 0XFC07 //棕红色
#define GRAY  			 0X8430 //灰色
//GUI颜色

#define DARKBLUE      	 0X01CF	//深蓝色
#define LIGHTBLUE      	 0X7D7C	//浅蓝色
#define GRAYBLUE       	 0X5458 //灰蓝色
//以上颜色为PANEL背景颜色

#define LIGHTGREEN     	 0X841F //浅绿色
#define LGRAY 					 0XC618 //浅灰色(PANNEL),窗体背景色

#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           0X2B12 //浅蓝蓝色(选中项目的反色)

//////////////////////////////////////////////////////////////////////////////////
//-----------------LCD端口定义----------------
// LCD驱动定义
#define LCD_SPI_PORT SPI1
#define LCD_SPI_CLK RCC_APB2_PERIPH_SPI1

// LCD控制引脚定义
#define LCD_CS_PORT GPIOA
#define LCD_CS_PIN GPIO_PIN_0
#define LCD_CS_CLK RCC_APB2_PERIPH_GPIOA

#define LCD_DC_PORT GPIOB
#define LCD_DC_PIN GPIO_PIN_0
#define LCD_DC_CLK RCC_APB2_PERIPH_GPIOB

#define LCD_RST_PORT GPIOA
#define LCD_RST_PIN GPIO_PIN_3
#define LCD_RST_CLK RCC_APB2_PERIPH_GPIOA

#define LCD_BLK_PORT GPIOA
#define LCD_BLK_PIN GPIO_PIN_6
#define LCD_BLK_CLK RCC_APB2_PERIPH_GPIOA

// LCD�������Ų����궨��
#define LCD_CS_SET GPIO_SetBits(LCD_CS_PORT, LCD_CS_PIN)
#define LCD_CS_CLR GPIO_ResetBits(LCD_CS_PORT, LCD_CS_PIN)

#define LCD_DC_SET GPIO_SetBits(LCD_DC_PORT, LCD_DC_PIN)
#define LCD_DC_CLR GPIO_ResetBits(LCD_DC_PORT, LCD_DC_PIN)

#define LCD_RST_SET GPIO_SetBits(LCD_RST_PORT, LCD_RST_PIN)
#define LCD_RST_CLR GPIO_ResetBits(LCD_RST_PORT, LCD_RST_PIN)

#define LCD_BLK_SET GPIO_SetBits(LCD_BLK_PORT, LCD_BLK_PIN)
#define LCD_BLK_CLR GPIO_ResetBits(LCD_BLK_PORT, LCD_BLK_PIN)

// LCD数据写函数
void LCD_WR_DATA(u16 data);
// LCD寄存器写函数
void LCD_WR_REG(u16 reg);
// LCD写寄存器
void LCD_WriteReg(u16 LCD_Reg, u16 LCD_RegValue);
// 写LCD GRAM
void LCD_WriteRAM_Prepare(void);
void LCD_WriteRAM(u16 RGB_Code);
// 读LCD数据
u16 LCD_RD_DATA(void);
// 读LCD ID
u16 LCD_Read_ID(u8 reg);
// 读取点颜色
u16 LCD_ReadPoint(u16 x, u16 y);
// LCD初始化
void LCD_Init(void);
// LCD显示开/关
void LCD_DisplayOn(void);
void LCD_DisplayOff(void);
// 设置光标位置
void LCD_SetCursor(u16 Xpos, u16 Ypos);
// 画点
void LCD_DrawPoint(u16 x, u16 y);
// 快速画点
void LCD_Fast_DrawPoint(u16 x, u16 y, u16 color);
// 设置显示方向
void LCD_Display_Dir(u8 dir);
// 设置窗口
void LCD_Set_Window(u16 sx, u16 sy, u16 width, u16 height);
// 清屏
void LCD_Clear(u16 Color);
// 在指定区域填充单一颜色
void LCD_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color);
// 在指定区域填充指定颜色数组
void LCD_Color_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 *color);
// 画线
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2);
// 画矩形
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2);
// 画圆
void LCD_Draw_Circle(u16 x0, u16 y0, u8 r);
// 显示字符
void LCD_ShowChar(u16 x, u16 y, u8 num, u8 size, u8 mode);
// 显示数字
void LCD_ShowNum(u16 x, u16 y, u32 num, u8 len, u8 size);
// 显示字符串
void LCD_ShowString(u16 x, u16 y, u16 width, u16 height, u8 size, u8 *p);
// 显示汉字
void Show_Str(u16 x, u16 y, u8 *str, u8 size, u8 mode);
// 显示图片
void Gui_Drawbmp16(u16 x, u16 y, const unsigned char *p);
#endif
