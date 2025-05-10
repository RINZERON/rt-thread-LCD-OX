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

#define CELL_SIZE      80	//���Ӵ�С
#define BOARD_OFFSET_X 40	//����xy��ʾ��
#define BOARD_OFFSET_Y 200

// ��������
#define BUTTON_RIGHT GET_PIN(E, 4)  // Ҫ�к궨�� drv_gpio.h
#define BUTTON_DOWN  GET_PIN(E, 3)
#define BUTTON_LEFT  GET_PIN(E, 2)

#define BUTTON_OK    GET_PIN(A, 0)


// ��������
#define UART_NAME       "uart1"    // ����ʵ��ʹ�õĴ����޸�
#define CMD_BUF_SIZE    16



// ͬ������
static rt_sem_t key_sem;
static rt_mutex_t lcd_mutex;
static  rt_sem_t uart_rx_sem;
static struct rt_ringbuffer *cmd_rb;


static OXGame game;
static rt_device_t lcd_dev;






// ��ɫ����
static const rt_uint16_t color_table[] = {
    [0] = WHITE,    // ��
    [1] = GGREEN,    // X
    [2] = BLUE      // O
};


/* ���ڽ��ջص� �����ֽڡ��Ż��λ����������ź� */
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size)
{
	char ch;
	while (rt_device_read(dev, 0, &ch, 1) == 1)
    {
        rt_ringbuffer_putchar(cmd_rb, (rt_uint8_t)ch);
    }
	
    /* �ͷŽ����ź��� */
    rt_sem_release(uart_rx_sem);
    
	return RT_EOK;
}



/* �����жϷ����� */
static void button_isr(void *args)
{
    rt_base_t pin = (rt_base_t)args;
    rt_pin_irq_enable(pin, RT_FALSE); // ��ʱ�ر��ж�
    rt_sem_release(key_sem);          // �����ź���
//	rt_kprintf("release key_sem! key_sem value is: %d \n", key_sem->value);
}


/* ������ʼ�� */
static void button_init(void)
{
    // �����ж�ģʽ
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
    

    // �������ò���
    game.cursor_x = 1;
    game.cursor_y = 1;
    game.total_steps = 0;
	game.player_x.symbol = 'x';
	game.player_o.symbol = 'o';
    game.state = GAME_PLAYING;
    game.current_player = 0;
    rt_memset(game.board, 0, sizeof(game.board));
    game.state_changed = 1;
	
	LCD_Clear(WHITE);
	
	char *p = "X and O name:";
	LCD_ShowString(40, 150, 600, 24, 24, (rt_uint8_t*)p);
	
	if (type == RESET_FULL) {
        // ��ȫ����ʱ��������Ϣ
        rt_memset(game.player_x.name, 0, sizeof(game.player_x.name));
        rt_memset(game.player_o.name, 0, sizeof(game.player_o.name));
		//	�������
		input_player_name(&game.player_x);
		input_player_name(&game.player_o);
		
	}
	
	LCD_ShowString(40, 200, 600, 24, 24, (rt_uint8_t*)game.player_x.name);
	LCD_ShowString(40, 250, 600, 24, 24, (rt_uint8_t*)game.player_o.name);
	// ����ʱһ��ʱ��������ʾ
	rt_thread_mdelay(2000);
	 
    rt_mutex_release(lcd_mutex);
}



/*  �ӻ��λ�������װ����� */
static void input_player_name(Player *player)
{
    char buf[16] = {0};
    rt_uint8_t len = 0;
    char ch;
	
	
	rt_kprintf("Please input %c's name (max 15 chars):\n", player->symbol);
	
	
	    // ��վ�����
    rt_ringbuffer_reset(cmd_rb);
	
	while (len < sizeof(buf) - 1)
    {
        if (rt_ringbuffer_getchar(cmd_rb, (rt_uint8_t *)&ch) <= 0)
        {
            rt_sem_take(uart_rx_sem, RT_WAITING_FOREVER);
            continue;
        }
		// len = 0 Ĭ����\n
//        if ( len > 0 && (ch == '\r' || ch == '\n'))
//		{
//			break;
//		}
//		if (len == 0 && (ch == '\r' || ch == '\n'))
//		{
//			continue;
//		}
		
		if(ch == '\r' || ch == '\n')
		{
			if(len > 0){
				break;
			}
			else continue;	//��������׶ε�while ��ִ�к����� buf[len++] = ch(\n or \r);...
		}
		
        buf[len++] = ch;
        rt_kprintf("%c !!!", ch);
    }
    buf[len] = '\0';
    rt_strncpy(player->name, buf, sizeof(player->name));
    rt_kprintf("\nName set: %s\n", player->name);
    
}






/* ��Ϸ��ʼ�� */
void ox_game_init(void)
{
    // ��ʼ��ͬ������
    key_sem = rt_sem_create("key_sem", 0, RT_IPC_FLAG_FIFO);
    lcd_mutex = rt_mutex_create("lcd_mutex", RT_IPC_FLAG_FIFO);
	uart_rx_sem = rt_sem_create("uart_rx_sem", 0, RT_IPC_FLAG_FIFO);
	
	cmd_rb = rt_ringbuffer_create(CMD_BUF_SIZE);
    
    // ��ʼ������
    rt_device_t uart_device = rt_device_find(UART_NAME);
    RT_ASSERT(uart_device != RT_NULL);
	// ���жϽ��շ�ʽ�򿪴����豸
    rt_device_open(uart_device, RT_DEVICE_FLAG_INT_RX);
    // ���ý��ܻص����� ֻ����rt_err_t uart_rx_ind
	rt_device_set_rx_indicate(uart_device, uart_rx_ind);
	
	
	// ��ʼ��Ӳ��
    button_init();
    lcd_dev = rt_device_find("lcd");
    RT_ASSERT(lcd_dev != RT_NULL);
    rt_device_init(lcd_dev);
	
	
	LCD_Clear(WHITE);
	


	// ��ʼ����Ϸ״̬
	reset_game(RESET_FULL);
   
    
}








// ������������
static void draw_grid(void)
{
    // ����
    LCD_DrawLine((rt_uint16_t[]){BLACK}, BOARD_OFFSET_X + CELL_SIZE, BOARD_OFFSET_Y,
                BOARD_OFFSET_X + CELL_SIZE, BOARD_OFFSET_Y + CELL_SIZE*3);
    LCD_DrawLine((rt_uint16_t[]){BLACK}, BOARD_OFFSET_X + CELL_SIZE*2, BOARD_OFFSET_Y,
                BOARD_OFFSET_X + CELL_SIZE*2, BOARD_OFFSET_Y + CELL_SIZE*3);

    // ����
    LCD_DrawLine((rt_uint16_t[]){BLACK}, BOARD_OFFSET_X, BOARD_OFFSET_Y + CELL_SIZE,
                BOARD_OFFSET_X + CELL_SIZE*3, BOARD_OFFSET_Y + CELL_SIZE);
    LCD_DrawLine((rt_uint16_t[]){BLACK}, BOARD_OFFSET_X, BOARD_OFFSET_Y + CELL_SIZE*2,
                BOARD_OFFSET_X + CELL_SIZE*3, BOARD_OFFSET_Y + CELL_SIZE*2);
}

// ��������
static void draw_piece(rt_uint8_t x, rt_uint8_t y, rt_uint16_t color)
{
    rt_uint16_t px = BOARD_OFFSET_X + x*CELL_SIZE + CELL_SIZE/2;
    rt_uint16_t py = BOARD_OFFSET_Y + y*CELL_SIZE + CELL_SIZE/2;

    if(color == color_table[1]) { // ��X
        LCD_DrawLine((rt_uint16_t[]){color}, px-20, py-20, px+20, py+20);
        LCD_DrawLine((rt_uint16_t[]){color}, px+20, py-20, px-20, py+20);
		
		// ������
		LCD_DrawThickLine(color, px-20, py-20, px+20, py+20, 5);
		LCD_DrawThickLine(color, px+20, py-20, px-20, py+20, 5);
		
    } else if (color == color_table[2]) { // ��O
        LCD_DrawCircle(px, py, 20, color);
    }
}

// ������������
void ox_game_draw_board(void)
{
    LCD_Clear(WHITE);
    draw_grid();
	
    // ������������
    for(int y=0; y<3; y++) {
        for(int x=0; x<3; x++) {
            if(game.board[x][y] != 0) {
                draw_piece(x, y, color_table[game.board[x][y]]);
            }
        }
    }
	
	
	 // ���������Ϣ��ʾ
    char info_buf[40];
    
	// ��ʾ���X��Ϣ
    rt_snprintf(info_buf, sizeof(info_buf), "X: %s", game.player_x.name);
    LCD_ShowString(300, 50, 200, 24, 24, (rt_uint8_t*)info_buf);
	
	// ��ʾ���O��Ϣ
    rt_snprintf(info_buf, sizeof(info_buf), "O: %s", game.player_o.name);
    LCD_ShowString(300, 100, 200, 24, 24, (rt_uint8_t*)info_buf);
    // ��ʾ��ǰ���
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
	
	
    // ���ƹ��
    rt_uint16_t cursor_x = BOARD_OFFSET_X + game.cursor_x*CELL_SIZE;
    rt_uint16_t cursor_y = BOARD_OFFSET_Y + game.cursor_y*CELL_SIZE;
    LCD_DrawRect(cursor_x, cursor_y, CELL_SIZE, CELL_SIZE, YELLOW);
}

// ���ʤ������
static GameState check_win(void)
{
    // �����
    for(int y=0; y<3; y++) {
        if(game.board[0][y] && 
           game.board[0][y] == game.board[1][y] &&
           game.board[1][y] == game.board[2][y]) {
            return (game.board[0][y] == 1) ? GAME_X_WIN : GAME_O_WIN;
        }
    }

    // �����
    for(int x=0; x<3; x++) {
        if(game.board[x][0] && 
           game.board[x][0] == game.board[x][1] &&
           game.board[x][1] == game.board[x][2]) {
            return (game.board[x][0] == 1) ? GAME_X_WIN : GAME_O_WIN;
        }
    }

    // ���Խ���
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

    // ���ƽ��
    if(game.total_steps >= 9) return GAME_DRAW;

    return GAME_PLAYING;
}


/* ���������߳� */
static void key_thread_entry(void *parameter)
{
	rt_err_t result_key;
	rt_err_t result_lcd;
	
    while (1) {
		
		result_key = rt_sem_take(key_sem, RT_WAITING_FOREVER); 
        if (result_key == RT_EOK) {
            
			rt_thread_mdelay(20); // Ӳ������
            
			result_lcd = rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER);
            if( result_lcd == RT_EOK)// �ȴ�LCD��Ļmutex
			{	
				// ��ȡ��������
				rt_base_t active_pin = RT_NULL;
				if (rt_pin_read(BUTTON_LEFT) == PIN_LOW) active_pin = BUTTON_LEFT;
				else if (rt_pin_read(BUTTON_RIGHT) == PIN_LOW) active_pin = BUTTON_RIGHT;
				else if (rt_pin_read(BUTTON_DOWN) == PIN_LOW) active_pin = BUTTON_DOWN;
				else if (rt_pin_read(BUTTON_OK) == PIN_HIGH) active_pin = BUTTON_OK;

				// ������
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
						rt_kprintf("ȷ��������[x,y]=[%d,%d]\n",game.cursor_x,game.cursor_y);
					}
					break;
				}
			
				rt_mutex_release(lcd_mutex);
//				rt_kprintf("realse lcd_mutex!\n");
				rt_pin_irq_enable(active_pin, RT_TRUE); // ����ʹ���ж�
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



/* LCDˢ���߳� */
static void lcd_thread_entry(void *parameter)
{
	rt_err_t result;
	
    while (1) {
        result = rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER);
        
		if (result == RT_EOK){
		
			if (game.state_changed) {
				ox_game_draw_board();	//�������� ������������
				
				game.state_changed = 0;
			}
			
			// ����ʤ��״̬
			if (game.state != GAME_PLAYING) 
			{
				// ��ʾʤ����Ϣ
				char *msg = "";
				if(game.state == GAME_X_WIN) msg = "X Wins!";
				else if(game.state == GAME_O_WIN) msg = "O Wins!";
				else msg = "Draw!";
				
				LCD_ShowString(100, 200, 200, 24, 24, (rt_uint8_t*)msg);
				
				rt_mutex_release(lcd_mutex); // �ͷ���������������
				rt_thread_mdelay(2000);     // ��ʾ��Ϣ2��
				reset_game(RESET_CURRENT);                // ������Ϸ
				rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER);
			}
			
			rt_mutex_release(lcd_mutex);
			rt_thread_mdelay(500); // ����ˢ����
		
		
		}
		
        
    }
}


/* ָ����߳� */
static void cmd_thread_entry(void *parameter)
{
    rt_err_t result;

	rt_device_t uart_device = rt_device_find(UART_NAME);
	
    while (1) {
        
		
		/* �ȴ����ڽ����ź��� */
        rt_sem_take(uart_rx_sem, RT_WAITING_FOREVER);

		
		
        /* ���������е����п���ָ�� */
        while (1) {
            char cmd;
            int count = rt_ringbuffer_getchar(cmd_rb, (rt_uint8_t*)&cmd);
            
            if (count <= 0) break;  // ������������ʱ�˳�ѭ��

            /* ��ȡLCD������ */
            result = rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER);
            if (result != RT_EOK) {
                rt_kprintf("Take LCD mutex failed!\n");
                continue;
            }

            /* ִ��ָ��� */
            switch (cmd) {
            case 'a': // ����
                game.cursor_x = (game.cursor_x == 0) ? 2 : (game.cursor_x - 1);
                game.state_changed = 1;
                rt_kprintf("[CMD] Left move to [%d,%d]\n", game.cursor_x, game.cursor_y);
                break;

            case 'd': // ����
                game.cursor_x = (game.cursor_x + 1) % 3;
                game.state_changed = 1;
                rt_kprintf("[CMD] Right move to [%d,%d]\n", game.cursor_x, game.cursor_y);
                break;

            case 's': // ����
                game.cursor_y = (game.cursor_y < 2) ? (game.cursor_y + 1) : 0;
                game.state_changed = 1;
                rt_kprintf("[CMD] Down move to [%d,%d]\n", game.cursor_x, game.cursor_y);
                break;

            case 'w': // ����
                game.cursor_y = (game.cursor_y == 0) ? 2 : (game.cursor_y - 1);
                game.state_changed = 1;
                rt_kprintf("[CMD] Up move to [%d,%d]\n", game.cursor_x, game.cursor_y);
                break;

            case 'e': // ȷ������
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

            case 'r': // ������Ϸ����ȫ���ã�
                reset_game(RESET_FULL);
                rt_kprintf("[CMD] Full reset executed\n");
                break;

            case 'c': // ���������õ�ǰ��Ϸ�����������Ϣ��
                reset_game(RESET_CURRENT);
                rt_kprintf("[CMD] Current game reset\n");
                break;

			case '\n':	// ������
				break;
			case '\r':	// ����س�
				break;
			
            default:  // ��Чָ���
                rt_kprintf("[CMD] Invalid command: 0x%02X\n", cmd);
                break;
            }

            /* �ͷŻ����� */
            rt_mutex_release(lcd_mutex);
        }
		
		rt_sem_release(uart_rx_sem);

        /* �����ͷ�CPU */
        rt_thread_mdelay(5);
    }
}




/* ������Ϸ */
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
