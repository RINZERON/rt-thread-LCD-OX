

/*
 * Copyright (c) 2006-2025, 
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-04-06	  RINZERON	   add LCD_sample.c
 */

/* 点亮LCD屏幕
 * 问题在于 LCD的ID无法正确读取
 * 
 */

#include <board.h>
#include <rtdef.h>
#include <rtthread.h>
#include <drv_gpio.h>
#include "drv_lcd.h"
#include <stdio.h>


#define THREAD_PRIORITY         25
#define THREAD_STACK_SIZE		512
#define THREAD_TIMESLICE        10

static rt_thread_t tid1 = RT_NULL;




static void thread_lcd_entry(void *parameter)
{
	rt_kprintf("LCD open\n");
	
	static rt_uint8_t lcd_init = 0;

	rt_device_t lcd = RT_NULL;

    if(lcd_init == 0)
    {
        lcd_init = 1;

        lcd = rt_device_find("lcd");
        rt_device_init(lcd);
    }
	LCD_Clear(RED);
	rt_uint8_t x=0;
	rt_kprintf("LCD open\n");
	
	
	uint8_t lcd_id[12];				//存放LCD ID字符串
	sprintf((char*)lcd_id,"LCD ID:%04X",lcddev.id);//将LCD ID打印到lcd_id数组。			
	
	static rt_uint16_t *colory = (rt_uint16_t *) YELLOW;
	
    while (1)
    {
		switch(x)
		{
			case 0:LCD_Clear(WHITE);break;
			case 1:LCD_Clear(BLACK);break;
			case 2:LCD_Clear(BLUE);break;
			case 3:LCD_Clear(RED);break;
			case 4:LCD_Clear(MAGENTA);break;
			case 5:LCD_Clear(GREEN);break;
			case 6:LCD_Clear(CYAN);break; 
			case 7:LCD_Clear(YELLOW);break;
			case 8:LCD_Clear(BRRED);break;
			case 9:LCD_Clear(GRAY);break;
			case 10:LCD_Clear(LGRAY);break;
			case 11:LCD_Clear(BROWN);break;
		}
		POINT_COLOR=BLACK;		//画笔颜色：黑色
		LCD_ShowString(30,40,210,24,24,"Explorer STM32F4");	
		LCD_ShowString(30,80,200,16,16,"TFTLCD TEST");
		LCD_ShowString(30,100,200,16,16,"ATOM@ALIENTEK");
		LCD_ShowxNum(30,100,200,5,16,0x41);
 		LCD_ShowString(30,150,200,16,16,lcd_id);		//显示LCD ID	      					 
		LCD_ShowString(30,180,200,24,24,"2022/5/4");	     

		LCD_ShowNum(30,40,0,24,36);
		LCD_DrawLine(colory,80,80,300,300);
		
		x++;
		if(x==12)x=0;
		
		rt_thread_mdelay(100);
		
		
		rt_thread_mdelay(100);
		rt_kprintf("youarehere\n");
		rt_thread_mdelay(100);	
	
	}
	
	
	
}



int lcd_sample(void)
{
	
	
    /* 创建动态线程1，名称是thread1，入口是thread1_entry*/
    tid1 = rt_thread_create("thread_lcd",
                            thread_lcd_entry, RT_NULL,
                            THREAD_STACK_SIZE,
                            THREAD_PRIORITY-2, THREAD_TIMESLICE);
    
    /* 如果获得线程控制块，启动这个线程 */
    if (tid1 != RT_NULL)
        rt_thread_startup(tid1);

    return 0;
}

/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(lcd_sample, lcd sample);

