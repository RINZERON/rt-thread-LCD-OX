/* OXGame.c */
#include "oxgame.h"
#include <rtdevice.h>
#include <rtthread.h>
#include <drv_gpio.h>

#define CELL_SIZE      80
#define BOARD_OFFSET_X 40
#define BOARD_OFFSET_Y 40

#define BUTTON_RIGHT GET_PIN(E, 4)	// 要添加相关宏文件 drv_gpio.h
#define BUTTON_LEFT  GET_PIN(E, 2)
#define BUTTON_DOWN  GET_PIN(E, 3)	

#define BUTTON_OK    GET_PIN(A, 0)


#define BUTTON_DEBOUNCE_TICKS (RT_TICK_PER_SECOND/50) // 20ms消抖时间
#define BUTTON_REPEAT_TICKS   (RT_TICK_PER_SECOND/4)  // 250ms连击间隔

static OXGame game;
static rt_device_t lcd_dev;

// 按键互斥控制结构体
typedef struct {
    rt_bool_t is_locked;     // 互斥锁状态
    rt_base_t locked_button; // 当前锁定的按键
    rt_tick_t lock_time;     // 锁定时间
} ButtonMutex;


// 互斥锁控制结构体（保持全局唯一）
static struct {
    rt_bool_t locked;       // 锁定状态
    rt_base_t active_pin;   // 当前锁定引脚
    rt_tick_t lock_tick;    // 锁定时间戳
} btn_mutex = {RT_FALSE, RT_NULL, 0};



// 按键配置结构体（增加有效电平字段）
typedef struct {
    rt_base_t pin;          // GPIO引脚
    rt_uint8_t active_level;// 有效电平 (PIN_HIGH/PIN_LOW)
    rt_uint8_t last_state;  // 上次状态
    rt_uint8_t curr_state;  // 当前状态
    rt_tick_t last_tick;    // 最后有效时间
} ButtonState;


// 初始化按键配置（OK键高电平有效，其他低电平有效）
static ButtonState buttons[] = {
    {BUTTON_LEFT,  PIN_LOW,  PIN_HIGH, PIN_HIGH, 0},
    {BUTTON_RIGHT, PIN_LOW,  PIN_HIGH, PIN_HIGH, 0},
    {BUTTON_DOWN,  PIN_LOW,  PIN_HIGH, PIN_HIGH, 0},
    {BUTTON_OK,    PIN_HIGH, PIN_LOW,  PIN_LOW,  0}
};



// 颜色定义
static const rt_uint16_t color_table[] = {
    [0] = WHITE,    // 空
    [1] = RED,      // X
    [2] = BLUE      // O
};

// 初始化游戏
void ox_game_init(void)
{
	// 初始化按键
	rt_pin_mode(BUTTON_DOWN, PIN_MODE_INPUT_PULLUP);	// 按键为上拉电阻模式 才有相应
	rt_pin_mode(BUTTON_LEFT, PIN_MODE_INPUT_PULLUP);
	rt_pin_mode(BUTTON_RIGHT, PIN_MODE_INPUT_PULLUP);
	rt_pin_mode(BUTTON_OK, PIN_MODE_INPUT_PULLDOWN);	// 下拉电阻模式
	
	
    // 获取LCD设备
    lcd_dev = rt_device_find("lcd");
    RT_ASSERT(lcd_dev != RT_NULL);
    rt_device_init(lcd_dev);

    // 初始化游戏状态
    rt_memset(&game, 0, sizeof(OXGame));	// 配置内存数据大小
    game.player_x.symbol = 'X';
    game.player_o.symbol = 'O';
    game.current_player = 0;
    game.cursor_x = 1;
    game.cursor_y = 1;
    LCD_Clear(WHITE);
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

    if(color == RED) { // 画X
        LCD_DrawLine((rt_uint16_t[]){color}, px-20, py-20, px+20, py+20);
        LCD_DrawLine((rt_uint16_t[]){color}, px+20, py-20, px-20, py+20);
    } else { // 画O
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


// 改进的输入处理函数
void ox_game_process_input(void)
{
    static const rt_tick_t DEBOUNCE_TICKS = RT_TICK_PER_SECOND / 50; // 20ms消抖
    static const rt_tick_t REPEAT_TICKS = RT_TICK_PER_SECOND / 4;    // 250ms连击间隔
    const rt_tick_t now = rt_tick_get();

    /* 互斥锁超时检测（1秒自动解锁） */
    if (btn_mutex.locked && (now - btn_mutex.lock_tick) > RT_TICK_PER_SECOND) {
        btn_mutex.locked = RT_FALSE;
        btn_mutex.active_pin = RT_NULL;
    }

    /* 扫描所有按键 */
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
        ButtonState *btn = &buttons[i];
//        const rt_uint8_t curr_state = rt_pin_read(btn->pin);	// 不能设置成const啊 它要变化的
		rt_uint8_t curr_state = rt_pin_read(btn->pin);
		
        /* 状态变化检测 */
        if (curr_state != btn->last_state) {
            btn->last_state = curr_state;
            btn->last_tick = now;
            continue; // 跳过未稳定的状态
        }

        /* 消抖检测（状态稳定时间不足则跳过） */
        if (now - btn->last_tick < DEBOUNCE_TICKS) {
            continue;
        }

        /* 有效电平检测 */
        if (curr_state == btn->active_level) {
            /*---- 按键按下逻辑 ----*/
            if (!btn_mutex.locked) { // 无锁状态获取互斥锁
                btn_mutex.locked = RT_TRUE;
                btn_mutex.active_pin = btn->pin;
                btn_mutex.lock_tick = now;
            } else if (btn->pin != btn_mutex.active_pin) { 
                continue; // 其他按键被锁定，跳过处理
            }

            /* 处理按键事件（带连击间隔检测） */
            if (now - btn->last_tick >= REPEAT_TICKS) {
                switch (btn->pin) {
                case BUTTON_LEFT:
					game.cursor_x = (game.cursor_x == 0) ? 2 : (game.cursor_x - 1);
					break;

                case BUTTON_RIGHT:
                    game.cursor_x = (game.cursor_x + 1) % 3; // 右移
                    break;

                case BUTTON_DOWN:
                    game.cursor_y = (game.cursor_y < 2) ? (game.cursor_y + 1) : 0; // 下移循环
                    break;

                case BUTTON_OK: // 高电平有效键特殊处理
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
                
                btn->last_tick = now; // 更新处理时间戳
            }
        } else {
            /*---- 按键释放逻辑 ----*/
            if (btn->pin == btn_mutex.active_pin) {
                btn_mutex.locked = RT_FALSE;
                btn_mutex.active_pin = RT_NULL;
            }
        }
    }
}


// 游戏主线程
static void game_thread_entry(void *parameter)
{
    ox_game_init();

    while(1) {
        ox_game_process_input();
        ox_game_draw_board();
        
        if(game.state != GAME_PLAYING) {
            // 显示胜利信息
            char *msg = "";
            if(game.state == GAME_X_WIN) msg = "X Wins!";
            else if(game.state == GAME_O_WIN) msg = "O Wins!";
            else msg = "Draw!";
            
            LCD_ShowString(100, 200, 200, 24, 24, (rt_uint8_t*)msg);
            rt_thread_mdelay(2000);
            ox_game_init(); // 重置游戏
        }
        
        rt_thread_mdelay(100);
    }
}

// 启动游戏
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
