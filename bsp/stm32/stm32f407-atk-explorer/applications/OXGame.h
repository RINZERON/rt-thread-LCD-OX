/* oxgame.h */
#ifndef __OXGAME_H
#define __OXGAME_H

#include "drv_lcd.h"
#include <rtthread.h>

// 游戏状态枚举
typedef enum {
    GAME_PLAYING,
    GAME_X_WIN,
    GAME_O_WIN,
    GAME_DRAW
} GameState;

// 玩家结构体
typedef struct {
    char symbol;
    uint8_t positions[3][2]; // 保存棋子位置[x,y]
    uint8_t move_step;
} Player;

// 游戏主结构体
typedef struct {
    uint8_t board[3][3];      // 棋盘 0:空 1:X 2:O
    Player player_x;
    Player player_o;
    uint8_t current_player;   // 0:X 1:O
    uint8_t total_steps;
    GameState state;
    uint8_t cursor_x, cursor_y; // 光标位置
} OXGame;

// 游戏API
void ox_game_init(void);
void ox_game_start(void);
void ox_game_draw_board(void);
void ox_game_process_input(void);

#endif
