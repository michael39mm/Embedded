#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "chess_engine.h"
#include "motor.h"

/* Placed in static SRAM so they never overflow the RP2040's 2 KB call stack */
static ChessState g_state;
static char g_line[2048];

static void load_fen(ChessState *state, const char *fen) {
    for (int i = 0; i < 64; i++) {
        state->board[i] = P_EMPTY;
    }
    state->castling = 0;
    state->ep_square = -1;
    state->halfmove_clock = 0;
    state->side = 1;

    int r = 7, f = 0;
    const char *p = fen;

    while (*p && *p != ' ') {
        if (*p == '/') {
            r--;
            f = 0;
        } else if (*p >= '1' && *p <= '8') {
            f += (*p - '0');
        } else {
            int8_t piece = P_EMPTY;
            switch (*p) {
                case 'P': piece =  P_PAWN;   break;
                case 'N': piece =  P_KNIGHT; break;
                case 'B': piece =  P_BISHOP; break;
                case 'R': piece =  P_ROOK;   break;
                case 'Q': piece =  P_QUEEN;  break;
                case 'K': piece =  P_KING;   break;
                case 'p': piece = -P_PAWN;   break;
                case 'n': piece = -P_KNIGHT; break;
                case 'b': piece = -P_BISHOP; break;
                case 'r': piece = -P_ROOK;   break;
                case 'q': piece = -P_QUEEN;  break;
                case 'k': piece = -P_KING;   break;
                default: break;
            }
            if (r >= 0 && r < 8 && f >= 0 && f < 8) {
                state->board[r * 8 + f] = piece;
            }
            f++;
        }
        p++;
    }

    if (*p == ' ') p++;
    if (*p == 'b') state->side = -1;
    while (*p && *p != ' ') p++;

    if (*p == ' ') p++;
    while (*p && *p != ' ') {
        if (*p == 'K') state->castling |= CASTLE_WK;
        else if (*p == 'Q') state->castling |= CASTLE_WQ;
        else if (*p == 'k') state->castling |= CASTLE_BK;
        else if (*p == 'q') state->castling |= CASTLE_BQ;
        p++;
    }

    if (*p == ' ') p++;
    if (*p >= 'a' && *p <= 'h' && p[1] >= '1' && p[1] <= '8') {
        state->ep_square = (int8_t)((p[1] - '1') * 8 + (p[0] - 'a'));
    }
    while (*p && *p != ' ') p++;

    if (*p == ' ') p++;
    if (*p >= '0' && *p <= '9') {
        state->halfmove_clock = (uint16_t)atoi(p);
    }
}

static int read_usb_line(char *buf, int max_len) {
    int idx = 0;
    while (idx < max_len - 1) {
        int ch = getchar();
        if (ch == EOF) {
            tight_loop_contents();
            continue;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            break;
        }
        buf[idx++] = (char)ch;
    }
    buf[idx] = '\0';
    return idx;
}

int main(void) {
    stdio_init_all();
    setvbuf(stdout, NULL, _IONBF, 0);

    chess_init(&g_state);
    motor_init();

    while (true) {
        int len = read_usb_line(g_line, (int)sizeof(g_line));
        if (len == 0) {
            continue;
        }

        if (strcmp(g_line, "uci") == 0) {
            printf("id name PicoChess\n");
            printf("id author EmbeddedTeam\n");
            printf("uciok\n");
        } else if (strcmp(g_line, "isready") == 0) {
            printf("readyok\n");
        } else if (strcmp(g_line, "ucinewgame") == 0) {
            chess_init(&g_state);
            motor_arm();
            motor_home_all();
        } else if (strncmp(g_line, "position", 8) == 0) {
            char *moves_ptr = strstr(g_line, "moves ");
            if (strstr(g_line, "startpos") != NULL) {
                chess_init(&g_state);
            } else {
                char *fen_ptr = strstr(g_line, "fen ");
                if (fen_ptr != NULL) {
                    fen_ptr += 4;
                    if (moves_ptr != NULL) {
                        *(moves_ptr - 1) = '\0';
                    }
                    load_fen(&g_state, fen_ptr);
                }
            }

            if (moves_ptr != NULL) {
                char *token = strtok(moves_ptr + 6, " ");
                while (token != NULL) {
                    chess_apply_uci(&g_state, token);
                    token = strtok(NULL, " ");
                }
            }
        } else if (strncmp(g_line, "go", 2) == 0) {
            int depth = 3;
            char *depth_ptr = strstr(g_line, "depth ");
            if (depth_ptr != NULL) {
                depth = atoi(depth_ptr + 6);
                if (depth < 1) depth = 1;
                if (depth > CHESS_MAX_DEPTH) depth = CHESS_MAX_DEPTH;
            }

            ChessMove best;
            char uci[6];
            if (chess_find_best_move(&g_state, depth, &best)) {
                /* Move gantry using g_state BEFORE applying move so captures/castling are known */
                motor_execute_move(&g_state, best);
                chess_apply_move(&g_state, best);
                chess_move_to_uci(best, uci);
                printf("bestmove %s\n", uci);
            } else {
                printf("bestmove (none)\n");
            }
        } else if (strcmp(g_line, "quit") == 0) {
            chess_init(&g_state);
            motor_move_to_mm(0.0f, 0.0f, MOVE_DELAY_US);
            motor_disarm();
        }
    }

    return 0;
}