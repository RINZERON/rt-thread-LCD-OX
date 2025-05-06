/* OXGame.c */
#include "oxgame.h"
#include <rtdevice.h>
#include <rtthread.h>
#include <drv_gpio.h>

#define CELL_SIZE      80
#define BOARD_OFFSET_X 40
#define BOARD_OFFSET_Y 40

#define BUTTON_RIGHT GET_PIN(E, 4)	// Ҫ�����غ��ļ� drv_gpio.h
#define BUTTON_LEFT  GET_PIN(E, 2)
#define BUTTON_DOWN  GET_PIN(E, 3)	

#define BUTTON_OK    GET_PIN(A, 0)


#define BUTTON_DEBOUNCE_TICKS (RT_TICK_PER_SECOND/50) // 20ms����ʱ��
#define BUTTON_REPEAT_TICKS   (RT_TICK_PER_SECOND/4)  // 250ms�������

static OXGame game;
static rt_device_t lcd_dev;

// ����������ƽṹ��
typedef struct {
    rt_bool_t is_locked;     // ������״̬
    rt_base_t locked_button; // ��ǰ�����İ���
    rt_tick_t lock_time;     // ����ʱ��
} ButtonMutex;


// ���������ƽṹ�壨����ȫ��Ψһ��
static struct {
    rt_bool_t locked;       // ����״̬
    rt_base_t active_pin;   // ��ǰ��������
    rt_tick_t lock_tick;    // ����ʱ���
} btn_mutex = {RT_FALSE, RT_NULL, 0};



// �������ýṹ�壨������Ч��ƽ�ֶΣ�
typedef struct {
    rt_base_t pin;          // GPIO����
    rt_uint8_t active_level;// ��Ч��ƽ (PIN_HIGH/PIN_LOW)
    rt_uint8_t last_state;  // �ϴ�״̬
    rt_uint8_t curr_state;  // ��ǰ״̬
    rt_tick_t last_tick;    // �����Чʱ��
} ButtonState;


// ��ʼ���������ã�OK���ߵ�ƽ��Ч�������͵�ƽ��Ч��
static ButtonState buttons[] = {
    {BUTTON_LEFT,  PIN_LOW,  PIN_HIGH, PIN_HIGH, 0},
    {BUTTON_RIGHT, PIN_LOW,  PIN_HIGH, PIN_HIGH, 0},
    {BUTTON_DOWN,  PIN_LOW,  PIN_HIGH, PIN_HIGH, 0},
    {BUTTON_OK,    PIN_HIGH, PIN_LOW,  PIN_LOW,  0}
};



// ��ɫ����
static const rt_uint16_t color_table[] = {
    [0] = WHITE,    // ��
    [1] = RED,      // X
    [2] = BLUE      // O
};

// ��ʼ����Ϸ
void ox_game_init(void)
{
	// ��ʼ������
	rt_pin_mode(BUTTON_DOWN, PIN_MODE_INPUT_PULLUP);	// ����Ϊ��������ģʽ ������Ӧ
	rt_pin_mode(BUTTON_LEFT, PIN_MODE_INPUT_PULLUP);
	rt_pin_mode(BUTTON_RIGHT, PIN_MODE_INPUT_PULLUP);
	rt_pin_mode(BUTTON_OK, PIN_MODE_INPUT_PULLDOWN);	// ��������ģʽ
	
	
    // ��ȡLCD�豸
    lcd_dev = rt_device_find("lcd");
    RT_ASSERT(lcd_dev != RT_NULL);
    rt_device_init(lcd_dev);

    // ��ʼ����Ϸ״̬
    rt_memset(&game, 0, sizeof(OXGame));	// �����ڴ����ݴ�С
    game.player_x.symbol = 'X';
    game.player_o.symbol = 'O';
    game.current_player = 0;
    game.cursor_x = 1;
    game.cursor_y = 1;
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

    if(color == RED) { // ��X
        LCD_DrawLine((rt_uint16_t[]){color}, px-20, py-20, px+20, py+20);
        LCD_DrawLine((rt_uint16_t[]){color}, px+20, py-20, px-20, py+20);
    } else { // ��O
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


// �Ľ������봦����
void ox_game_process_input(void)
{
    static const rt_tick_t DEBOUNCE_TICKS = RT_TICK_PER_SECOND / 50; // 20ms����
    static const rt_tick_t REPEAT_TICKS = RT_TICK_PER_SECOND / 4;    // 250ms�������
    const rt_tick_t now = rt_tick_get();

    /* ��������ʱ��⣨1���Զ������� */
    if (btn_mutex.locked && (now - btn_mutex.lock_tick) > RT_TICK_PER_SECOND) {
        btn_mutex.locked = RT_FALSE;
        btn_mutex.active_pin = RT_NULL;
    }

    /* ɨ�����а��� */
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
        ButtonState *btn = &buttons[i];
//        const rt_uint8_t curr_state = rt_pin_read(btn->pin);	// �������ó�const�� ��Ҫ�仯��
		rt_uint8_t curr_state = rt_pin_read(btn->pin);
		
        /* ״̬�仯��� */
        if (curr_state != btn->last_state) {
            btn->last_state = curr_state;
            btn->last_tick = now;
            continue; // ����δ�ȶ���״̬
        }

        /* ������⣨״̬�ȶ�ʱ�䲻���������� */
        if (now - btn->last_tick < DEBOUNCE_TICKS) {
            continue;
        }

        /* ��Ч��ƽ��� */
        if (curr_state == btn->active_level) {
            /*---- ���������߼� ----*/
            if (!btn_mutex.locked) { // ����״̬��ȡ������
                btn_mutex.locked = RT_TRUE;
                btn_mutex.active_pin = btn->pin;
                btn_mutex.lock_tick = now;
            } else if (btn->pin != btn_mutex.active_pin) { 
                continue; // ������������������������
            }

            /* �������¼��������������⣩ */
            if (now - btn->last_tick >= REPEAT_TICKS) {
                switch (btn->pin) {
                case BUTTON_LEFT:
					game.cursor_x = (game.cursor_x == 0) ? 2 : (game.cursor_x - 1);
					break;

                case BUTTON_RIGHT:
                    game.cursor_x = (game.cursor_x + 1) % 3; // ����
                    break;

                case BUTTON_DOWN:
                    game.cursor_y = (game.cursor_y < 2) ? (game.cursor_y + 1) : 0; // ����ѭ��
                    break;

                case BUTTON_OK: // �ߵ�ƽ��Ч�����⴦��
                    if (game.board[game.cursor_x][game.cursor_y] == 0) {
                        game.board[game.cursor_x][game.cursor_y] = game.current_player + 1;
                        game.total_steps++;

                        Player *p = game.current_player ? &game.player_o : &game.player_x;
                        p->positions[p->move_step][0] = game.cursor_x;
                        p->positions[p->move_step][1] = game.cursor_y;
                        p->move_step = (p->move_step + 1) % 3;

                        game.state = check_win();
                        if (game.state == GAME_PLAYING) {
                            game.current_player = !game.current_player;
                        }
                    }
                    break;
                }
                
                btn->last_tick = now; // ���´���ʱ���
            }
        } else {
            /*---- �����ͷ��߼� ----*/
            if (btn->pin == btn_mutex.active_pin) {
                btn_mutex.locked = RT_FALSE;
                btn_mutex.active_pin = RT_NULL;
            }
        }
    }
}


// ��Ϸ���߳�
static void game_thread_entry(void *parameter)
{
    ox_game_init();

    while(1) {
        ox_game_process_input();
        ox_game_draw_board();
        
        if(game.state != GAME_PLAYING) {
            // ��ʾʤ����Ϣ
            char *msg = "";
            if(game.state == GAME_X_WIN) msg = "X Wins!";
            else if(game.state == GAME_O_WIN) msg = "O Wins!";
            else msg = "Draw!";
            
            LCD_ShowString(100, 200, 200, 24, 24, (rt_uint8_t*)msg);
            rt_thread_mdelay(2000);
            ox_game_init(); // ������Ϸ
        }
        
        rt_thread_mdelay(100);
    }
}

// ������Ϸ
void ox_game_start(void)
{

    rt_thread_t tid = rt_thread_create("ox_game",
                                      game_thread_entry, 
                                      RT_NULL,
                                      2048,
                                      20,
                                      10);
    rt_thread_startup(tid);
}

MSH_CMD_EXPORT(ox_game_start, Start OX Game);
