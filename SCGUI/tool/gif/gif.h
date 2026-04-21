#ifndef __GIF_H__
#define __GIF_H__

#include "lvgl.h"

//////////////////////////////////////////用户配置区//////////////////////////////////

#define PIC_FORMAT_ERR		0x27	//格式错误
#define PIC_SIZE_ERR		0x28	//图片尺寸错误
#define PIC_WINDOW_ERR		0x29	//窗口设定错误
#define PIC_MEM_ERR			0x11	//内存错误

//图片显示物理层接口
//在移植的时候,必须由用户自己实现这几个函数
typedef struct
{
	u16(*read_point)(u16,u16);				//u16 read_point(u16 x,u16 y)						读点函数
	void(*draw_point)(u16,u16,u16);			//void draw_point(u16 x,u16 y,u16 color)		    画点函数
 	void(*fill)(u16,u16,u16,u16,u16);		///void fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 color) 单色填充函数
 	void(*draw_hline)(u16,u16,u16,u16);		//void draw_hline(u16 x0,u16 y0,u16 len,u16 color)  画水平线函数
 	void(*fillcolor)(u16,u16,u16,u16,u16*);	//void piclib_fill_color(u16 x,u16 y,u16 width,u16 height,u16 *color) 颜色填充
}_pic_phy;

//////////////////////////////////////////////END/////////////////////////////////////

#define LCD_MAX_LOG_COLORS  256
#define MAX_NUM_LWZ_BITS 	(1<<12)


#define GIF_INTRO_TERMINATOR ';'	//0X3B   GIF文件结束符
#define GIF_INTRO_EXTENSION  '!'    //0X21
#define GIF_INTRO_IMAGE      ','	//0X2C

#define GIF_COMMENT     	0xFE
#define GIF_APPLICATION 	0xFF
#define GIF_PLAINTEXT   	0x01
#define GIF_GRAPHICCTL  	0xF9

typedef struct
{
	u8    aBuffer[258];                     // Input buffer for data block
	short aCode  [MAX_NUM_LWZ_BITS];        // This array stores the LZW codes for the compressed strings
	u8    aPrefix[MAX_NUM_LWZ_BITS];        // Prefix character of the LZW code.
	u8    aDecompBuffer[1000];              // Decompression buffer. The higher the compression, the more bytes are needed in the buffer.
	u8 *  sp;                               // Pointer into the decompression buffer
	int   CurBit;
	int   LastBit;
	int   GetDone;
	int   LastByte;
	int   ReturnClear;
	int   CodeSize;
	int   SetCodeSize;
	int   MaxCode;
	int   MaxCodeSize;
	int   ClearCode;
	int   EndCode;
	int   FirstCode;
	int   OldCode;
}LZW_INFO;

//逻辑屏幕描述块
 typedef struct
{
	u16 width;		//GIF宽度
	u16 height;		//GIF高度
	u8 flag;		//标识符  1:3:1:3=全局颜色表标志(1):颜色深度(3):分类标志(1):全局颜色表大小(3)
	u8 bkcindex;	//背景色在全局颜色表中的索引(仅当存在全局颜色表时有效)
	u8 pixratio;	//像素宽高比
}LogicalScreenDescriptor;


//图像描述块
 typedef struct
{
	u16 xoff;		//x方向偏移
	u16 yoff;		//y方向偏移
	u16 width;		//宽度
	u16 height;		//高度
	u8 flag;		//标识符  1:1:1:2:3=局部颜色表标志(1):交织标志(1):保留(2):局部颜色表大小(3)
}ImageScreenDescriptor;

//图像描述
typedef struct
{
	LogicalScreenDescriptor gifLSD;	//逻辑屏幕描述块
	ImageScreenDescriptor gifISD;	//图像描述快
	u16 colortbl[256];				//当前使用颜色表
	u16 bkpcolortbl[256];			//备份颜色表.当存在局部颜色表时使用
	u16 numcolors;					//颜色表大小
	u16 delay;					    //延迟时间
	LZW_INFO *lzw;					//LZW信息
}gif89a;


void piclib_init(void);

#endif
