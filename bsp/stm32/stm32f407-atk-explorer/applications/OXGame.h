/* oxgame.h */
#ifndef __OXGAME_H
#define __OXGAME_H

#include "drv_lcd.h"
#include <rtthread.h>

// ��Ϸ״̬ö��
typedef enum {
    GAME_PLAYING,
    GAME_X_WIN,
    GAME_O_WIN,
    GAME_DRAW
} GameState;

// ������Ϸ״̬ö��
typedef enum {
    RESET_CURRENT,  // ��������Ϸ״̬
    RESET_FULL      // ��ȫ���ã��������Ϣ��
} ResetType;

// ��ҽṹ��
typedef struct {
    char symbol;
    uint8_t positions[3][2]; // ��������λ��[x,y]
    uint8_t move_step;
	char name[16];	//���ڴ���������
} Player;

// ��Ϸ���ṹ��
typedef struct {
    uint8_t board[3][3];      // ���� 0:�� 1:X 2:O
    Player player_x;
    Player player_o;
    uint8_t current_player;   // 0:X 1:O
    uint8_t total_steps;
    GameState state;
	rt_bool_t state_changed;
    uint8_t cursor_x, cursor_y; // ���λ��
} OXGame;

// ��ϷAPI
void ox_game_init(void);
void ox_game_start(void);
void ox_game_draw_board(void);
void ox_game_process_input(void);
void reset_game(ResetType type);
void input_player_name(Player *player);

#endif
