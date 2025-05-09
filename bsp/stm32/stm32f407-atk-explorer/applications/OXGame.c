/* OXGame.c */

/*

	date		version
	2025-05-06	1.0
	2025-05-06	2.0
  


*/

/* OXGame.c */
#include "oxgame.h"
#include <rtdevice.h>
#include <rtthread.h>
#include <drv_gpio.h>

#define CELL_SIZE      80	//格子大小
#define BOARD_OFFSET_X 40	//棋盘xy启示点
#define BOARD_OFFSET_Y 200

// 按键定义
#define BUTTON_RIGHT GET_PIN(E, 4)  // 要有宏定义 drv_gpio.h
#define BUTTON_DOWN  GET_PIN(E, 3)
#define BUTTON_LEFT  GET_PIN(E, 2)

#define BUTTON_OK    GET_PIN(A, 0)


// 串口配置
#define UART_NAME       "uart1"    // 根据实际使用的串口修改
#define CMD_BUF_SIZE    256



// 同步对象
static rt_sem_t key_sem;
static rt_mutex_t lcd_mutex;
static  rt_sem_t uart_rx_sem;
static struct rt_ringbuffer *cmd_rb;


static OXGame game;
static rt_device_t lcd_dev;






// 颜色定义
static const rt_uint16_t color_table[] = {
    [0] = WHITE,    // 空
    [1] = BRRED,    // X
    [2] = BLUE      // O
};


/* 串口接收回调 ：读字节→放环形缓冲区→发信号 */
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size)
{
	char ch;
	while (rt_device_read(dev, 0, &ch, 1) == 1)
    {
        rt_ringbuffer_putchar(cmd_rb, (rt_uint8_t)ch);
    }
	
    /* 释放接收信号量 */
    rt_sem_release(uart_rx_sem);
    
	return RT_EOK;
}



/* 按键中断服务函数 */
static void button_isr(void *args)
{
    rt_base_t pin = (rt_base_t)args;
    rt_pin_irq_enable(pin, RT_FALSE); // 临时关闭中断
    rt_sem_release(key_sem);          // 发送信号量
//	rt_kprintf("release key_sem! key_sem value is: %d \n", key_sem->value);
}


/* 按键初始化 */
static void button_init(void)
{
    // 配置中断模式
    rt_pin_mode(BUTTON_LEFT, PIN_MODE_INPUT_PULLUP);
    rt_pin_attach_irq(BUTTON_LEFT, PIN_IRQ_MODE_FALLING, button_isr, (void*)BUTTON_LEFT);
    rt_pin_irq_enable(BUTTON_LEFT, RT_TRUE);

    rt_pin_mode(BUTTON_RIGHT, PIN_MODE_INPUT_PULLUP);
    rt_pin_attach_irq(BUTTON_RIGHT, PIN_IRQ_MODE_FALLING, button_isr, (void*)BUTTON_RIGHT);
    rt_pin_irq_enable(BUTTON_RIGHT, RT_TRUE);

    rt_pin_mode(BUTTON_DOWN, PIN_MODE_INPUT_PULLUP);
    rt_pin_attach_irq(BUTTON_DOWN, PIN_IRQ_MODE_FALLING, button_isr, (void*)BUTTON_DOWN);
    rt_pin_irq_enable(BUTTON_DOWN, RT_TRUE);

    rt_pin_mode(BUTTON_OK, PIN_MODE_INPUT_PULLDOWN);
    rt_pin_attach_irq(BUTTON_OK, PIN_IRQ_MODE_RISING, button_isr, (void*)BUTTON_OK);
    rt_pin_irq_enable(BUTTON_OK, RT_TRUE);
}




static void reset_game(ResetType type)
{
    rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER);
    

    // 公共重置部分
    game.cursor_x = 1;
    game.cursor_y = 1;
    game.total_steps = 0;
	game.player_x.symbol = 'x';
	game.player_o.symbol = 'o';
    game.state = GAME_PLAYING;
    game.current_player = 0;
    rt_memset(game.board, 0, sizeof(game.board));
    game.state_changed = 1;
    
	
	 if (type == RESET_FULL) {
        // 完全重置时清除玩家信息
        rt_memset(game.player_x.name, 0, sizeof(game.player_x.name));
        rt_memset(game.player_o.name, 0, sizeof(game.player_o.name));
		//	玩家名称
		input_player_name(&game.player_x);
		input_player_name(&game.player_o);
    }
	
    rt_mutex_release(lcd_mutex);
}



/*  从环形缓冲区组装玩家名 */
static void input_player_name(Player *player)
{
    char buf[16] = {0};
    rt_uint8_t len = 0;
    char ch;
	
	
	rt_kprintf("Please input %c's name (max 15 chars):\n", player->symbol);
	
	
	    // 清空旧数据
    rt_ringbuffer_reset(cmd_rb);
	
	while (len < sizeof(buf) - 1)
    {
        if (rt_ringbuffer_getchar(cmd_rb, (rt_uint8_t *)&ch) <= 0)
        {
            rt_sem_take(uart_rx_sem, RT_WAITING_FOREVER);
            continue;
        }
		// len = 0 默认是\n
        if ( len > 0 && (ch == '\r' || ch == '\n'))
		{
			break;
		}
		if (len == 0 && (ch == '\r' || ch == '\n'))
		{
			continue;
		}
        buf[len++] = ch;
        rt_kprintf("%c", ch);
    }
    buf[len] = '\0';
    rt_strncpy(player->name, buf, sizeof(player->name));
    rt_kprintf("\nName set: %s\n", player->name);
    
}






/* 游戏初始化 */
void ox_game_init(void)
{
    // 初始化同步对象
    key_sem = rt_sem_create("key_sem", 0, RT_IPC_FLAG_FIFO);
    lcd_mutex = rt_mutex_create("lcd_mutex", RT_IPC_FLAG_FIFO);
	uart_rx_sem = rt_sem_create("uart_rx_sem", 0, RT_IPC_FLAG_FIFO);
	
	cmd_rb = rt_ringbuffer_create(CMD_BUF_SIZE);
    
    // 初始化串口
    rt_device_t uart_device = rt_device_find(UART_NAME);
    RT_ASSERT(uart_device != RT_NULL);
	// 以中断接收方式打开串口设备
    rt_device_open(uart_device, RT_DEVICE_FLAG_INT_RX);
    // 设置接受回调函数 只能是rt_err_t uart_rx_ind
	rt_device_set_rx_indicate(uart_device, uart_rx_ind);
	
	
	// 初始化硬件
    button_init();
    lcd_dev = rt_device_find("lcd");
    RT_ASSERT(lcd_dev != RT_NULL);
    rt_device_init(lcd_dev);
	
	
	LCD_Clear(WHITE);
	


	// 初始化游戏状态
	reset_game(RESET_FULL);
   
    
}








// 绘制棋盘网格
static void draw_grid(void)
{
    // 竖线
    LCD_DrawLine((rt_uint16_t[]){BLACK}, BOARD_OFFSET_X + CELL_SIZE, BOARD_OFFSET_Y,
                BOARD_OFFSET_X + CELL_SIZE, BOARD_OFFSET_Y + CELL_SIZE*3);
    LCD_DrawLine((rt_uint16_t[]){BLACK}, BOARD_OFFSET_X + CELL_SIZE*2, BOARD_OFFSET_Y,
                BOARD_OFFSET_X + CELL_SIZE*2, BOARD_OFFSET_Y + CELL_SIZE*3);

    // 横线
    LCD_DrawLine((rt_uint16_t[]){BLACK}, BOARD_OFFSET_X, BOARD_OFFSET_Y + CELL_SIZE,
                BOARD_OFFSET_X + CELL_SIZE*3, BOARD_OFFSET_Y + CELL_SIZE);
    LCD_DrawLine((rt_uint16_t[]){BLACK}, BOARD_OFFSET_X, BOARD_OFFSET_Y + CELL_SIZE*2,
                BOARD_OFFSET_X + CELL_SIZE*3, BOARD_OFFSET_Y + CELL_SIZE*2);
}

// 绘制棋子
static void draw_piece(rt_uint8_t x, rt_uint8_t y, rt_uint16_t color)
{
    rt_uint16_t px = BOARD_OFFSET_X + x*CELL_SIZE + CELL_SIZE/2;
    rt_uint16_t py = BOARD_OFFSET_Y + y*CELL_SIZE + CELL_SIZE/2;

    if(color == color_table[1]) { // 画X
        LCD_DrawLine((rt_uint16_t[]){color}, px-20, py-20, px+20, py+20);
        LCD_DrawLine((rt_uint16_t[]){color}, px+20, py-20, px-20, py+20);
    } else if (color == color_table[2]) { // 画O
        LCD_DrawCircle(px, py, 20, color);
    }
}

// 绘制整个棋盘
void ox_game_draw_board(void)
{
    LCD_Clear(WHITE);
    draw_grid();
	
    // 绘制所有棋子
    for(int y=0; y<3; y++) {
        for(int x=0; x<3; x++) {
            if(game.board[x][y] != 0) {
                draw_piece(x, y, color_table[game.board[x][y]]);
            }
        }
    }
	
	
	 // 新增玩家信息显示
    char info_buf[40];
    
	// 显示玩家X信息
    rt_snprintf(info_buf, sizeof(info_buf), "X: %s", game.player_x.name);
    LCD_ShowString(300, 50, 200, 24, 24, (rt_uint8_t*)info_buf);
	
	// 显示玩家O信息
    rt_snprintf(info_buf, sizeof(info_buf), "O: %s", game.player_o.name);
    LCD_ShowString(300, 100, 200, 24, 24, (rt_uint8_t*)info_buf);
    // 显示当前玩家
    rt_snprintf(info_buf, sizeof(info_buf), "Current: %c", 
			game.current_player ? game.player_o.symbol : game.player_x.symbol);
    LCD_ShowString(300, 150, 200, 24, 24, (rt_uint8_t*)info_buf);

	char *msg = "";
	msg = "move:wasd";
	LCD_ShowString(300, 200, 600, 24, 24, (rt_uint8_t*)msg);
	msg = "place piece:e";
	LCD_ShowString(300, 250, 600, 24, 24, (rt_uint8_t*)msg);
	msg = "reset game:r";
	LCD_ShowString(300, 300, 600, 24, 24, (rt_uint8_t*)msg);
	msg = "reset curr:c";
	LCD_ShowString(300, 350, 600, 24, 24, (rt_uint8_t*)msg);
	
	
    // 绘制光标
    rt_uint16_t cursor_x = BOARD_OFFSET_X + game.cursor_x*CELL_SIZE;
    rt_uint16_t cursor_y = BOARD_OFFSET_Y + game.cursor_y*CELL_SIZE;
    LCD_DrawRect(cursor_x, cursor_y, CELL_SIZE, CELL_SIZE, YELLOW);
}

// 检查胜利条件
static GameState check_win(void)
{
    // 检查行
    for(int y=0; y<3; y++) {
        if(game.board[0][y] && 
           game.board[0][y] == game.board[1][y] &&
           game.board[1][y] == game.board[2][y]) {
            return (game.board[0][y] == 1) ? GAME_X_WIN : GAME_O_WIN;
        }
    }

    // 检查列
    for(int x=0; x<3; x++) {
        if(game.board[x][0] && 
           game.board[x][0] == game.board[x][1] &&
           game.board[x][1] == game.board[x][2]) {
            return (game.board[x][0] == 1) ? GAME_X_WIN : GAME_O_WIN;
        }
    }

    // 检查对角线
    if(game.board[0][0] && 
       game.board[0][0] == game.board[1][1] &&
       game.board[1][1] == game.board[2][2]) {
        return (game.board[0][0] == 1) ? GAME_X_WIN : GAME_O_WIN;
    }

    if(game.board[2][0] && 
       game.board[2][0] == game.board[1][1] &&
       game.board[1][1] == game.board[0][2]) {
        return (game.board[2][0] == 1) ? GAME_X_WIN : GAME_O_WIN;
    }

    // 检查平局
    if(game.total_steps >= 9) return GAME_DRAW;

    return GAME_PLAYING;
}


/* 按键处理线程 */
static void key_thread_entry(void *parameter)
{
	rt_err_t result_key;
	rt_err_t result_lcd;
	
    while (1) {
		
		result_key = rt_sem_take(key_sem, RT_WAITING_FOREVER); 
        if (result_key == RT_EOK) {
            
			rt_thread_mdelay(20); // 硬件消抖
            
			result_lcd = rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER);
            if( result_lcd == RT_EOK)// 等待LCD屏幕mutex
			{	
				// 获取触发引脚
				rt_base_t active_pin = RT_NULL;
				if (rt_pin_read(BUTTON_LEFT) == PIN_LOW) active_pin = BUTTON_LEFT;
				else if (rt_pin_read(BUTTON_RIGHT) == PIN_LOW) active_pin = BUTTON_RIGHT;
				else if (rt_pin_read(BUTTON_DOWN) == PIN_LOW) active_pin = BUTTON_DOWN;
				else if (rt_pin_read(BUTTON_OK) == PIN_HIGH) active_pin = BUTTON_OK;

				// 处理按键
				switch (active_pin) {
				case BUTTON_LEFT:
					game.cursor_x = (game.cursor_x == 0) ? 2 : (game.cursor_x - 1);
					game.state_changed = 1;
					break;
					
				case BUTTON_RIGHT:
					game.cursor_x = (game.cursor_x + 1) % 3;
					game.state_changed = 1;
					break;
					
				case BUTTON_DOWN:
					game.cursor_y = (game.cursor_y < 2) ? (game.cursor_y + 1) : 0;
					game.state_changed = 1;
					break;
					
				case BUTTON_OK:
					if (game.board[game.cursor_x][game.cursor_y] == 0 && game.state == GAME_PLAYING) 
					{
						game.board[game.cursor_x][game.cursor_y] = game.current_player + 1;
						game.total_steps++;
						game.state = check_win();
						game.current_player = !game.current_player;
						game.state_changed = 1;
						rt_kprintf("button ok!\n");
						rt_kprintf("确认落子在[x,y]=[%d,%d]\n",game.cursor_x,game.cursor_y);
					}
					break;
				}
			
				rt_mutex_release(lcd_mutex);
//				rt_kprintf("realse lcd_mutex!\n");
				rt_pin_irq_enable(active_pin, RT_TRUE); // 重新使能中断
			}
			else{
				rt_kprintf("Take LCD mutex error!\n");
			}
        }
		else{
			rt_kprintf("Take Key sem error!\n");
		}
    }
}



/* LCD刷新线程 */
static void lcd_thread_entry(void *parameter)
{
	rt_err_t result;
	
    while (1) {
        result = rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER);
        
		if (result == RT_EOK){
		
			if (game.state_changed) {
				ox_game_draw_board();	//绘制棋盘 包括光标和棋子
				
				game.state_changed = 0;
			}
			
			// 处理胜利状态
			if (game.state != GAME_PLAYING) 
			{
				// 显示胜利信息
				char *msg = "";
				if(game.state == GAME_X_WIN) msg = "X Wins!";
				else if(game.state == GAME_O_WIN) msg = "O Wins!";
				else msg = "Draw!";
				
				LCD_ShowString(100, 200, 200, 24, 24, (rt_uint8_t*)msg);
				
				rt_mutex_release(lcd_mutex); // 释放锁允许其他操作
				rt_thread_mdelay(2000);     // 显示消息2秒
				reset_game(RESET_CURRENT);                // 重置游戏
				rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER);
			}
			
			rt_mutex_release(lcd_mutex);
			rt_thread_mdelay(500); // 控制刷新率
		
		
		}
		
        
    }
}


/* 指令处理线程 */
static void cmd_thread_entry(void *parameter)
{
    rt_err_t result;

	rt_device_t uart_device = rt_device_find(UART_NAME);
	
    while (1) {
        
		
		/* 等待串口接收信号量 */
        rt_sem_take(uart_rx_sem, RT_WAITING_FOREVER);

		
		
        /* 处理缓冲区中的所有可用指令 */
        while (1) {
            char cmd;
            int count = rt_ringbuffer_getchar(cmd_rb, (rt_uint8_t*)&cmd);
            
            if (count <= 0) break;  // 缓冲区无数据时退出循环

            /* 获取LCD互斥锁 */
            result = rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER);
            if (result != RT_EOK) {
                rt_kprintf("Take LCD mutex failed!\n");
                continue;
            }

            /* 执行指令处理 */
            switch (cmd) {
            case 'a': // 左移
                game.cursor_x = (game.cursor_x == 0) ? 2 : (game.cursor_x - 1);
                game.state_changed = 1;
                rt_kprintf("[CMD] Left move to [%d,%d]\n", game.cursor_x, game.cursor_y);
                break;

            case 'd': // 右移
                game.cursor_x = (game.cursor_x + 1) % 3;
                game.state_changed = 1;
                rt_kprintf("[CMD] Right move to [%d,%d]\n", game.cursor_x, game.cursor_y);
                break;

            case 's': // 下移
                game.cursor_y = (game.cursor_y < 2) ? (game.cursor_y + 1) : 0;
                game.state_changed = 1;
                rt_kprintf("[CMD] Down move to [%d,%d]\n", game.cursor_x, game.cursor_y);
                break;

            case 'w': // 上移
                game.cursor_y = (game.cursor_y == 0) ? 2 : (game.cursor_y - 1);
                game.state_changed = 1;
                rt_kprintf("[CMD] Up move to [%d,%d]\n", game.cursor_x, game.cursor_y);
                break;

            case 'e': // 确认落子
                if (game.board[game.cursor_x][game.cursor_y] == 0 && 
                    game.state == GAME_PLAYING) 
                {
                    game.board[game.cursor_x][game.cursor_y] = game.current_player + 1;
                    game.total_steps++;
                    game.state = check_win();
                    game.current_player = !game.current_player;
                    game.state_changed = 1;
                    rt_kprintf("[CMD] Place piece at [%d,%d]\n", game.cursor_x, game.cursor_y);
                }
                break;

            case 'r': // 重置游戏（完全重置）
                reset_game(RESET_FULL);
                rt_kprintf("[CMD] Full reset executed\n");
                break;

            case 'c': // 新增：重置当前游戏（保留玩家信息）
                reset_game(RESET_CURRENT);
                rt_kprintf("[CMD] Current game reset\n");
                break;

			case '\n':	// 处理换行
				break;
			case '\r':	// 处理回车
				break;
			
            default:  // 无效指令处理
                rt_kprintf("[CMD] Invalid command: 0x%02X\n", cmd);
                break;
            }

            /* 释放互斥锁 */
            rt_mutex_release(lcd_mutex);
        }
		
		rt_sem_release(uart_rx_sem);

        /* 短暂释放CPU */
        rt_thread_mdelay(5);
    }
}




/* 启动游戏 */
void ox_game_start(void)
{
    ox_game_init();
	

    
    rt_thread_t key_tid = rt_thread_create("key_th", key_thread_entry, 
                                        NULL, 1024, 10, 10);
    rt_thread_t lcd_tid = rt_thread_create("lcd_th", lcd_thread_entry,
                                        NULL, 2048, 11, 10);
                                        
	rt_thread_t cmd_tid = rt_thread_create("cmd_th", cmd_thread_entry, 
                                         NULL, 1024, 7, 10);
	
    rt_thread_startup(key_tid);
    rt_thread_startup(lcd_tid);
	rt_thread_startup(cmd_tid);
	
	

	
}

MSH_CMD_EXPORT(ox_game_start, Start OX Game);
