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

#define CELL_SIZE      80
#define BOARD_OFFSET_X 40
#define BOARD_OFFSET_Y 40

// ��������
#define BUTTON_RIGHT GET_PIN(E, 4)  // Ҫ�к궨�� drv_gpio.h
#define BUTTON_DOWN  GET_PIN(E, 3)
#define BUTTON_LEFT  GET_PIN(E, 2)

#define BUTTON_OK    GET_PIN(A, 0)

// ͬ������
static rt_sem_t key_sem;
static rt_mutex_t lcd_mutex;
static OXGame game;
static rt_device_t lcd_dev;

// ��ɫ����
static const rt_uint16_t color_table[] = {
    [0] = WHITE,    // ��
    [1] = BRRED,    // X
    [2] = BLUE      // O
};

/* �жϷ����� */
static void button_isr(void *args)
{
    rt_base_t pin = (rt_base_t)args;
    rt_pin_irq_enable(pin, RT_FALSE); // ��ʱ�ر��ж�
    rt_sem_release(key_sem);          // �����ź���
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


static void reset_game(void)
{
	rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER); 
	// ��ʼ����Ϸ״̬
    rt_memset(&game, 0, sizeof(OXGame));
    game.player_x.symbol = 'X';
    game.player_o.symbol = 'O';
    game.current_player = 0;
    game.cursor_x = 1;
    game.cursor_y = 1;
	game.state = GAME_PLAYING;	//������Ϸ״̬
    game.state_changed = 1; // ��ʼ��Ҫˢ��
	rt_mutex_release(lcd_mutex);	//�ͷ���
}

/* ��Ϸ��ʼ�� */
void ox_game_init(void)
{
    // ��ʼ��ͬ������
    key_sem = rt_sem_create("key_sem", 0, RT_IPC_FLAG_FIFO);
    lcd_mutex = rt_mutex_create("lcd_mutex", RT_IPC_FLAG_FIFO);
    
    // ��ʼ��Ӳ��
    button_init();
    lcd_dev = rt_device_find("lcd");
    RT_ASSERT(lcd_dev != RT_NULL);
    rt_device_init(lcd_dev);

	// ��ʼ����Ϸ״̬
	reset_game();
   
    LCD_Clear(WHITE);
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
    while (1) {
        if (rt_sem_take(key_sem, RT_WAITING_FOREVER) == RT_EOK) {
            
			rt_thread_mdelay(20); // Ӳ������
            
            rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER);
            
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
                }
                break;
            }
            
            rt_mutex_release(lcd_mutex);
            rt_pin_irq_enable(active_pin, RT_TRUE); // ����ʹ���ж�
        }
    }
}



/* LCDˢ���߳� */
static void lcd_thread_entry(void *parameter)
{
    while (1) {
        rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER);
        
        if (game.state_changed) {
			ox_game_draw_board();
            
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
            reset_game();                // ������Ϸ
            rt_mutex_take(lcd_mutex, RT_WAITING_FOREVER);
        }
		
        rt_mutex_release(lcd_mutex);
        rt_thread_mdelay(500); // ����ˢ����
    }
}




/* ������Ϸ */
void ox_game_start(void)
{
    ox_game_init();
    
    rt_thread_t key_tid = rt_thread_create("key_th", key_thread_entry, 
                                        NULL, 1024, 6, 10);
    rt_thread_t lcd_tid = rt_thread_create("lcd_th", lcd_thread_entry,
                                        NULL, 2048, 7, 10);
                                        
    rt_thread_startup(key_tid);
    rt_thread_startup(lcd_tid);
}

MSH_CMD_EXPORT(ox_game_start, Start OX Game);
