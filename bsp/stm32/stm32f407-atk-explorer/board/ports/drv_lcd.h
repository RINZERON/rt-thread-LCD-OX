/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-12-28     unknow       copy by STemwin
 */
#ifndef __DRV_LCD_H
#define __DRV_LCD_H
#include <rtthread.h>
#include "rtdevice.h"
#include <drv_common.h>


#ifdef BSP_USING_TOUCH_CAP
#define LCD_W 800
#define LCD_H 480
#endif // BSP_USING_TOUCH_CAP

#ifdef BSP_USING_TOUCH_RES
#define LCD_W 320
#define LCD_H 480
#endif // BSP_USING_TOUCH_RES

//LCD重要参数集
typedef struct
{
    uint16_t width;         //LCD 宽度
    uint16_t height;        //LCD 高度
    uint16_t id;            //LCD ID
    uint8_t  dir;           //横屏还是竖屏控制：0，竖屏；1，横屏。
    uint16_t wramcmd;       //开始写gram指令
    uint16_t setxcmd;       //设置x坐标指令
    uint16_t setycmd;       //设置y坐标指令
}_lcd_dev;

//LCD参数
extern _lcd_dev lcddev; //管理LCD重要参数

extern rt_uint16_t POINT_COLOR;	//画笔颜色
extern rt_uint16_t BACK_COLOR;  //背景色 

#define LCD_BL GET_PIN(B, 15)

	/* 无需 */
//#define FSMC_NE4 GET_PIN(G,12)
//#define FSMC_NWE GET_PIN(D,5)
//#define	FSMC_NOE GET_PIN(D,4)
//#define FSMC_A6	 GET_PIN(F,12)

//#define FSMC_D0 GET_PIN(D, 14)
//#define FSMC_D1 GET_PIN(D, 15)
//#define FSMC_D2 GET_PIN(D, 0)
//#define FSMC_D3 GET_PIN(D, 1)
//#define FSMC_D4 GET_PIN(E, 7)
//#define FSMC_D5 GET_PIN(E, 8)
//#define FSMC_D6 GET_PIN(E, 5)
//#define FSMC_D7 GET_PIN(E, 6)
//#define FSMC_D8 GET_PIN(E, 11)
//#define FSMC_D9 GET_PIN(E, 12)
//#define FSMC_D10 GET_PIN(E, 13)
//#define FSMC_D11 GET_PIN(E, 14)
//#define FSMC_D12 GET_PIN(E, 15)
//#define FSMC_D13 GET_PIN(D, 8)
//#define FSMC_D14 GET_PIN(D, 9)
//#define FSMC_D15 GET_PIN(D, 10)


#define LCD_BASE ((uint32_t)(0x6C000000 | 0x0000007E))
#define LCD ((LCD_CONTROLLER_TypeDef *)LCD_BASE)

#define LCD_DEVICE(dev) (struct drv_lcd_device *)(dev)

typedef struct
{
  __IO uint16_t REG;
  __IO uint16_t RAM;
}LCD_CONTROLLER_TypeDef;

//扫描方向定义
#define L2R_U2D  0      //从左到右,从上到下
#define L2R_D2U  1      //从左到右,从下到上
#define R2L_U2D  2      //从右到左,从上到下
#define R2L_D2U  3      //从右到左,从下到上

#define U2D_L2R  4      //从上到下,从左到右
#define U2D_R2L  5      //从上到下,从右到左
#define D2U_L2R  6      //从下到上,从左到右
#define D2U_R2L  7      //从下到上,从右到左

#ifdef BSP_USING_TOUCH_CAP
#define DFT_SCAN_DIR  L2R_U2D  //电容触摸屏默认的扫描方向
#endif // BSP_USING_TOUCH_CAP

#ifdef BSP_USING_TOUCH_RES
#define DFT_SCAN_DIR  D2U_L2R  //电阻触摸屏默认的扫描方向
#endif // BSP_USING_TOUCH_RES

//画笔颜色
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
#define GGREEN           0X6400	//深绿色


//GUI颜色

#define DARKBLUE      	 0X01CF	//深蓝色
#define LIGHTBLUE      	 0X7D7C	//浅蓝色  
#define GRAYBLUE       	 0X5458 //灰蓝色
//以上三色为PANEL的颜色 
 
#define LIGHTGREEN     	 0X841F //浅绿色
//#define LIGHTGRAY        0XEF5B //浅灰色(PANNEL)
#define LGRAY 			 0XC618 //浅灰色(PANNEL),窗体背景色

#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           0X2B12 //浅棕蓝色(选择条目的反色)

//LCD分辨率设置
#define SSD_HOR_RESOLUTION      800     //LCD水平分辨率
#define SSD_VER_RESOLUTION      480     //LCD垂直分辨率
//LCD驱动参数设置
#define SSD_HOR_PULSE_WIDTH     1       //水平脉宽
#define SSD_HOR_BACK_PORCH      46      //水平前廊
#define SSD_HOR_FRONT_PORCH     210     //水平后廊

#define SSD_VER_PULSE_WIDTH     1       //垂直脉宽
#define SSD_VER_BACK_PORCH      23      //垂直前廊
#define SSD_VER_FRONT_PORCH     22      //垂直前廊
//如下几个参数，自动计算
#define SSD_HT  (SSD_HOR_RESOLUTION+SSD_HOR_BACK_PORCH+SSD_HOR_FRONT_PORCH)
#define SSD_HPS (SSD_HOR_BACK_PORCH)
#define SSD_VT  (SSD_VER_RESOLUTION+SSD_VER_BACK_PORCH+SSD_VER_FRONT_PORCH)
#define SSD_VPS (SSD_VER_BACK_PORCH)




void lcd_fill_array(rt_uint16_t x_start, rt_uint16_t y_start, rt_uint16_t x_end, rt_uint16_t y_end, void *pcolor);
void LCD_WR_REG(uint16_t regval);
void LCD_WR_DATA(uint16_t data);
uint16_t LCD_RD_DATA(void);
void LCD_WriteReg(uint16_t LCD_Reg, uint16_t LCD_RegValue);

void LCD_ShowChar(rt_uint16_t x,rt_uint16_t y,rt_uint8_t num,rt_uint8_t size,rt_uint8_t mode);						//显示一个字符
void LCD_ShowNum(rt_uint16_t x,rt_uint16_t y,rt_uint32_t num,rt_uint8_t len,rt_uint8_t size);  						//显示一个数字
void LCD_ShowxNum(rt_uint16_t x,rt_uint16_t y,rt_uint32_t num,rt_uint8_t len,rt_uint8_t size,rt_uint8_t mode);				//显示 数字
void LCD_ShowString(rt_uint16_t x,rt_uint16_t y,rt_uint16_t width,rt_uint16_t height,rt_uint8_t size,rt_uint8_t *p);		//显示一个字符串,12/16字体
void LCD_Clear(uint16_t color);
void LCD_DrawLine(const uint16_t *pixel, rt_uint16_t x1, rt_uint16_t y1, rt_uint16_t x2, rt_uint16_t y2);
void LCD_DrawCircle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);	// 画圆
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);	//空心矩形
void LCD_DrawThickLine(rt_uint16_t color,uint16_t x0, uint16_t y0,uint16_t x1, uint16_t y1,uint16_t thickness); // 打印粗线条

#endif
