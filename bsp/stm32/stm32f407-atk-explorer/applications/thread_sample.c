
/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-06     SummerGift   first version
 * 2018-11-19     flybreak     add stm32f407-atk-explorer bsp
 * 2023-12-03     Meco Man     support nano version
 * 2025-03-31	  RINZERON	   add thread_sample.c
 */



#include <board.h>
#include <rtthread.h>
#include <drv_gpio.h>
#ifndef RT_USING_NANO
#include <rtdevice.h>
#endif /* RT_USING_NANO */

#define LED0_PIN    GET_PIN(F, 9)
#define LED1_PIN	GET_PIN(F,10)

#define THREAD_PRIORITY         25
#define THREAD_STACK_SIZE		512
#define THREAD_TIMESLICE        5


static rt_thread_t tid1 = RT_NULL;
static rt_thread_t tid2 = RT_NULL;

static rt_uint32_t count = 0;


static void thread1_entry(void *parameter)
{
	
	while(1)
	{
		rt_kprintf("tid1 count = %d\n", count ++);
		if(count % 5 == 0)
		{
			rt_pin_write(LED0_PIN, PIN_HIGH);
			rt_kprintf("LED0 open\n");
			rt_thread_mdelay(50);
			rt_pin_write(LED0_PIN, PIN_LOW);
			rt_kprintf("LED0 close\n");
		}
	}
}

static void thread2_entry(void *parameter)
{
	
	while(1)
	{
		rt_kprintf("tid2 count = %d\n", count ++);
		if(count % 20 == 0)
		{
			rt_pin_write(LED1_PIN, PIN_HIGH);
			rt_thread_mdelay(50);
			rt_pin_write(LED1_PIN, PIN_LOW);
		}
	}
}


int thread_sample(void)
{
	
	/* 设定LED的工作模式 */
	rt_pin_mode(LED0_PIN, PIN_MODE_OUTPUT);
	rt_pin_mode(LED1_PIN, PIN_MODE_OUTPUT);
	
	
    /* 创建动态线程1，名称是thread1，入口是thread1_entry*/
    tid1 = rt_thread_create("thread1",
                            thread1_entry, RT_NULL,
                            THREAD_STACK_SIZE,
                            THREAD_PRIORITY, THREAD_TIMESLICE);
    
    /* 如果获得线程控制块，启动这个线程 */
    if (tid1 != RT_NULL)
        rt_thread_startup(tid1);

	/*创建动态线程3*/
	tid2 = rt_thread_create("thread2",
             thread2_entry, RT_NULL,
             THREAD_STACK_SIZE,
             THREAD_PRIORITY - 2, THREAD_TIMESLICE);
	 /* 如果获得线程控制块，启动这个线程 */
    if (tid2 != RT_NULL)
        rt_thread_startup(tid2);
	

    return 0;
}

/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(thread_sample, thread sample);
