#ifndef MOTOR_H
#define MOTOR_H

#include <stdbool.h>
#include <stdint.h>
#include "chess_engine.h"

/* --- X-Axis Driver & Limit Switch --- */
#define PIN_STEP_X   4
#define PIN_DIR_X    7
#define PIN_EN_X     6
#define PIN_LIMIT_X  9

/* --- Y-Axis Driver & Limit Switch --- */
#define PIN_STEP_Y   10
#define PIN_DIR_Y    11
#define PIN_EN_Y     12
#define PIN_LIMIT_Y  8

/* --- Calibration & Travel Limits --- */
#define STEPS_PER_MM      40.0f
#define BACKOFF_STEPS     120
#define HOME_DELAY_US     200
#define MOVE_DELAY_US     200
#define MAX_TRAVEL_STEPS  14000

#define TOTAL_WIDTH_X_MM  285.58f
#define TOTAL_HEIGHT_Y_MM 265.00f

/* Homing & Positive Travel Directions */
#define DIR_HOME_X  1
#define DIR_HOME_Y  0
#define DIR_POS_X   (1 - DIR_HOME_X)
#define DIR_POS_Y   (1 - DIR_HOME_Y)

/* --- Chessboard Placement Inside Workspace (MEASURE & UPDATE THESE) --- */
#define SQUARE_SIZE_MM     30.0f   /* Center-to-center distance between squares */
#define BOARD_ORIGIN_X_MM  37.79f  /* X mm of square a1 center */
#define BOARD_ORIGIN_Y_MM  27.50f  /* Y mm of square a1 center */
#define GRAVEYARD_X_MM     5.0f    /* Drop-off X mm for captured pieces */
#define GRAVEYARD_Y_MM     5.0f    /* Drop-off Y mm for captured pieces */

void motor_init(void);
void motor_arm(void);
void motor_disarm(void);
bool motor_home_all(void);
void motor_move_to_mm(float target_x_mm, float target_y_mm, uint32_t delay_us);
void motor_square_to_mm(uint8_t sq, float *out_x_mm, float *out_y_mm);
void motor_execute_move(const ChessState *state_before, ChessMove move);

#endif /* MOTOR_H */