#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chess_engine.h"

/* Optional helper so custom --fen positions also work during testing */
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

    /* 1. Piece placement */
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

    /* 2. Side to move */
    if (*p == ' ') p++;
    if (*p == 'b') state->side = -1;
    while (*p && *p != ' ') p++;

    /* 3. Castling rights */
    if (*p == ' ') p++;
    while (*p && *p != ' ') {
        if (*p == 'K') state->castling |= CASTLE_WK;
        else if (*p == 'Q') state->castling |= CASTLE_WQ;
        else if (*p == 'k') state->castling |= CASTLE_BK;
        else if (*p == 'q') state->castling |= CASTLE_BQ;
        p++;
    }

    /* 4. En passant target square */
    if (*p == ' ') p++;
    if (*p >= 'a' && *p <= 'h' && p[1] >= '1' && p[1] <= '8') {
        state->ep_square = (int8_t)((p[1] - '1') * 8 + (p[0] - 'a'));
    }
    while (*p && *p != ' ') p++;

    /* 5. Halfmove clock */
    if (*p == ' ') p++;
    if (*p >= '0' && *p <= '9') {
        state->halfmove_clock = (uint16_t)atoi(p);
    }
}

int main(void) {
    /* Disable buffering so Python receives every printf immediately */
    setvbuf(stdout, NULL, _IONBF, 0);

    ChessState state;
    chess_init(&state);

    char line[4096];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0) {
            continue;
        }

        if (strcmp(line, "uci") == 0) {
            printf("id name PicoChess\n");
            printf("id author EmbeddedTeam\n");
            printf("uciok\n");
        } else if (strcmp(line, "isready") == 0) {
            printf("readyok\n");
        } else if (strcmp(line, "ucinewgame") == 0) {
            chess_init(&state);
        } else if (strncmp(line, "position", 8) == 0) {
            char *moves_ptr = strstr(line, "moves ");
            if (strstr(line, "startpos") != NULL) {
                chess_init(&state);
            } else {
                char *fen_ptr = strstr(line, "fen ");
                if (fen_ptr != NULL) {
                    fen_ptr += 4;
                    if (moves_ptr != NULL) {
                        *(moves_ptr - 1) = '\0';
                    }
                    load_fen(&state, fen_ptr);
                }
            }

            if (moves_ptr != NULL) {
                char *token = strtok(moves_ptr + 6, " ");
                while (token != NULL) {
                    chess_apply_uci(&state, token);
                    token = strtok(NULL, " ");
                }
            }
        } else if (strncmp(line, "go", 2) == 0) {
            ChessMove best;
            char uci[6];
            if (chess_find_best_move(&state, 3, &best)) {
                chess_move_to_uci(best, uci);
                printf("bestmove %s\n", uci);
            } else {
                printf("bestmove (none)\n");
            }
        } else if (strcmp(line, "quit") == 0) {
            break;
        }
    }

    return 0;
}