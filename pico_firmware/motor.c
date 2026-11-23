#include "motor.h"
#include "magnet.h"
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"

static int g_pos_x_steps = 0;
static int g_pos_y_steps = 0;
static bool g_is_homed = false;

static void pulse_single(uint step_pin, uint32_t delay_us) {
    gpio_put(step_pin, 1);
    sleep_us(delay_us);
    gpio_put(step_pin, 0);
    sleep_us(delay_us);
}

static int mm_to_steps(float mm) {
    return (int)(mm * STEPS_PER_MM + 0.5f);
}

static bool home_axis(const char *name, uint step_pin, uint dir_pin, uint limit_pin, int home_dir) {
    printf("info string Homing %s...\n", name);

    /* Back off first if switch is already pressed */
    if (gpio_get(limit_pin) == 0) {
        gpio_put(dir_pin, 1 - home_dir);
        for (int i = 0; i < BACKOFF_STEPS * 2; i++) {
            pulse_single(step_pin, 300);
        }
        sleep_ms(100);
    }

    gpio_put(dir_pin, home_dir);
    int steps = 0;
    int low_count = 0;

    while (steps < MAX_TRAVEL_STEPS) {
        if (gpio_get(limit_pin) == 0) {
            low_count++;
            if (low_count >= 5) {
                printf("info string [%s] Triggered at %d steps.\n", name, steps);
                break;
            }
        } else {
            low_count = 0;
        }

        int ramp = (150 - steps) * 3;
        uint32_t delay = (uint32_t)(HOME_DELAY_US + (ramp > 0 ? ramp : 0));
        pulse_single(step_pin, delay);
        steps++;
    }

    if (steps >= MAX_TRAVEL_STEPS) {
        printf("info string [ERROR] %s failed to hit limit switch!\n", name);
        return false;
    }

    sleep_ms(100);
    gpio_put(dir_pin, 1 - home_dir);
    for (int i = 0; i < BACKOFF_STEPS; i++) {
        pulse_single(step_pin, 400);
    }

    return gpio_get(limit_pin) == 1;
}

void motor_init(void) {
    const uint out_pins[] = {
        PIN_STEP_X, PIN_DIR_X, PIN_EN_X,
        PIN_STEP_Y, PIN_DIR_Y, PIN_EN_Y
    };
    for (size_t i = 0; i < sizeof(out_pins) / sizeof(out_pins[0]); i++) {
        gpio_init(out_pins[i]);
        gpio_set_dir(out_pins[i], GPIO_OUT);
        gpio_put(out_pins[i], 0);
    }

    gpio_init(PIN_LIMIT_X);
    gpio_set_dir(PIN_LIMIT_X, GPIO_IN);
    gpio_pull_up(PIN_LIMIT_X);

    gpio_init(PIN_LIMIT_Y);
    gpio_set_dir(PIN_LIMIT_Y, GPIO_IN);
    gpio_pull_up(PIN_LIMIT_Y);

    motor_disarm();
}

void motor_arm(void) {
    gpio_put(PIN_EN_X, 0);
    gpio_put(PIN_EN_Y, 0);
    sleep_ms(50);
}

void motor_disarm(void) {
    magnet_off();
    gpio_put(PIN_EN_X, 1);
    gpio_put(PIN_EN_Y, 1);
}

bool motor_home_all(void) {
    magnet_off();
    g_is_homed = false;
    if (!home_axis("X-AXIS", PIN_STEP_X, PIN_DIR_X, PIN_LIMIT_X, DIR_HOME_X)) {
        return false;
    }
    sleep_ms(200);
    if (!home_axis("Y-AXIS", PIN_STEP_Y, PIN_DIR_Y, PIN_LIMIT_Y, DIR_HOME_Y)) {
        return false;
    }

    g_pos_x_steps = 0;
    g_pos_y_steps = 0;
    g_is_homed = true;
    printf("info string [ORIGIN] Locked at (0.0 mm, 0.0 mm).\n");
    return true;
}

void motor_move_to_mm(float target_x_mm, float target_y_mm, uint32_t delay_us) {
    /* Clamp inside safe workspace bounds */
    if (target_x_mm < 0.0f) target_x_mm = 0.0f;
    if (target_x_mm > TOTAL_WIDTH_X_MM) target_x_mm = TOTAL_WIDTH_X_MM;
    if (target_y_mm < 0.0f) target_y_mm = 0.0f;
    if (target_y_mm > TOTAL_HEIGHT_Y_MM) target_y_mm = TOTAL_HEIGHT_Y_MM;

    int target_x = mm_to_steps(target_x_mm);
    int target_y = mm_to_steps(target_y_mm);

    int dx = target_x - g_pos_x_steps;
    int dy = target_y - g_pos_y_steps;
    if (dx == 0 && dy == 0) {
        return;
    }

    gpio_put(PIN_DIR_X, dx > 0 ? DIR_POS_X : DIR_HOME_X);
    gpio_put(PIN_DIR_Y, dy > 0 ? DIR_POS_Y : DIR_HOME_Y);

    int abs_dx = abs(dx);
    int abs_dy = abs(dy);

    if (abs_dx >= abs_dy) {
        int err = abs_dx / 2;
        for (int i = 0; i < abs_dx; i++) {
            gpio_put(PIN_STEP_X, 1);
            err -= abs_dy;
            bool step_y_now = (err < 0);
            if (step_y_now) {
                gpio_put(PIN_STEP_Y, 1);
                err += abs_dx;
            }
            sleep_us(delay_us);
            gpio_put(PIN_STEP_X, 0);
            if (step_y_now) {
                gpio_put(PIN_STEP_Y, 0);
            }
            sleep_us(delay_us);
        }
    } else {
        int err = abs_dy / 2;
        for (int i = 0; i < abs_dy; i++) {
            gpio_put(PIN_STEP_Y, 1);
            err -= abs_dx;
            bool step_x_now = (err < 0);
            if (step_x_now) {
                gpio_put(PIN_STEP_X, 1);
                err += abs_dy;
            }
            sleep_us(delay_us);
            gpio_put(PIN_STEP_Y, 0);
            if (step_x_now) {
                gpio_put(PIN_STEP_X, 0);
            }
            sleep_us(delay_us);
        }
    }

    g_pos_x_steps = target_x;
    g_pos_y_steps = target_y;
}

void motor_square_to_mm(uint8_t sq, float *out_x_mm, float *out_y_mm) {
    int rank = sq >> 3;  /* 0 = rank 1 .. 7 = rank 8 */
    int file = sq & 7;   /* 0 = file a .. 7 = file h */
    *out_x_mm = BOARD_ORIGIN_X_MM + (float)file * SQUARE_SIZE_MM;
    *out_y_mm = BOARD_ORIGIN_Y_MM + (float)rank * SQUARE_SIZE_MM;
}

static void move_piece_path(uint8_t from_sq, uint8_t to_sq) {
    float x1, y1, x2, y2;
    motor_square_to_mm(from_sq, &x1, &y1);
    motor_square_to_mm(to_sq, &x2, &y2);

    /* 1. Travel empty to the piece and stop */
    magnet_off();
    motor_move_to_mm(x1, y1, MOVE_DELAY_US);
    sleep_ms(MAGNET_SETTLE_MS);

    /* 2. Turn magnet ON to grab the piece */
    magnet_on();
    sleep_ms(MAGNET_SETTLE_MS);

    int df = (to_sq & 7) - (from_sq & 7);
    int dr = (to_sq >> 3) - (from_sq >> 3);

    /* 3. Carry piece to destination while magnet stays ON */
    if ((abs(df) == 1 && abs(dr) == 2) || (abs(df) == 2 && abs(dr) == 1)) {
        /* Knight jump: route along half-square grid lines to avoid bumping neighbors */
        float half = SQUARE_SIZE_MM * 0.5f;
        float mid_x = x1 + (df > 0 ? half : -half);
        float mid_y = y1 + (dr > 0 ? half : -half);

        if (abs(df) == 2) {
            motor_move_to_mm(x1, mid_y, MOVE_DELAY_US);
            motor_move_to_mm(x2, mid_y, MOVE_DELAY_US);
        } else {
            motor_move_to_mm(mid_x, y1, MOVE_DELAY_US);
            motor_move_to_mm(mid_x, y2, MOVE_DELAY_US);
        }
        motor_move_to_mm(x2, y2, MOVE_DELAY_US);
    } else {
        motor_move_to_mm(x2, y2, MOVE_DELAY_US);
    }

    /* 4. Stop at destination and turn magnet OFF */
    sleep_ms(MAGNET_SETTLE_MS);
    magnet_off();
    sleep_ms(MAGNET_SETTLE_MS);
}

void motor_execute_move(const ChessState *state_before, ChessMove move) {
    if (!g_is_homed) {
        return;
    }

    /* 1. Clear captured piece first (normal capture or en passant) */
    int cap_sq = -1;
    if (move.flags == FLAG_EN_PASSANT) {
        cap_sq = (int)move.to - 8 * state_before->side;
    } else if (state_before->board[move.to] != P_EMPTY) {
        cap_sq = (int)move.to;
    }

    if (cap_sq >= 0 && cap_sq < 64) {
        float cx, cy;
        motor_square_to_mm((uint8_t)cap_sq, &cx, &cy);

        magnet_off();
        motor_move_to_mm(cx, cy, MOVE_DELAY_US);
        sleep_ms(MAGNET_SETTLE_MS);

        magnet_on();
        sleep_ms(MAGNET_SETTLE_MS);

        motor_move_to_mm(GRAVEYARD_X_MM, GRAVEYARD_Y_MM, MOVE_DELAY_US);
        sleep_ms(MAGNET_SETTLE_MS);

        magnet_off();
        sleep_ms(MAGNET_SETTLE_MS);
    }

    /* 2. Move the active piece */
    move_piece_path(move.from, move.to);

    /* 3. If castling, also slide the rook */
    if (move.flags == FLAG_CASTLE) {
        if (move.to == 6)       move_piece_path(7, 5);    /* White O-O   */
        else if (move.to == 2)  move_piece_path(0, 3);    /* White O-O-O */
        else if (move.to == 62) move_piece_path(63, 61);  /* Black O-O   */
        else if (move.to == 58) move_piece_path(56, 59);  /* Black O-O-O */
    }
}