#ifndef CHESS_ENGINE_H
#define CHESS_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHESS_MAX_DEPTH 4

/* Piece IDs: positive = White, negative = Black, 0 = empty */
enum {
    P_EMPTY  = 0,
    P_PAWN   = 1,
    P_KNIGHT = 2,
    P_BISHOP = 3,
    P_ROOK   = 4,
    P_QUEEN  = 5,
    P_KING   = 6
};

/* Castling bitmask */
enum {
    CASTLE_WK = 1 << 0,
    CASTLE_WQ = 1 << 1,
    CASTLE_BK = 1 << 2,
    CASTLE_BQ = 1 << 3
};

/* Special move flags */
enum {
    FLAG_NONE        = 0,
    FLAG_DOUBLE_PAWN = 1,
    FLAG_EN_PASSANT  = 2,
    FLAG_CASTLE      = 3
};

/* Game status returned by chess_get_status() */
typedef enum {
    GAME_ONGOING = 0,
    GAME_CHECKMATE,
    GAME_STALEMATE,
    GAME_DRAW_50_MOVE
} ChessStatus;

typedef struct {
    uint8_t from;   /* 0 = a1 .. 63 = h8  (square = rank*8 + file) */
    uint8_t to;     /* 0 = a1 .. 63 = h8 */
    uint8_t promo;  /* 0 or P_KNIGHT..P_QUEEN */
    uint8_t flags;  /* FLAG_* */
} ChessMove;

typedef struct {
    int8_t   board[64];
    int8_t   side;           /* +1 = White to move, -1 = Black to move */
    uint8_t  castling;       /* CASTLE_* bitmask */
    int8_t   ep_square;      /* -1 if none, else 0..63 */
    uint16_t halfmove_clock; /* For 50-move draw rule */
} ChessState;

/* Reset board to standard chess starting position */
void chess_init(ChessState *state);

/* Validate and apply a UCI move string like "e2e4" or "e7e8q". Returns false if illegal. */
bool chess_apply_uci(ChessState *state, const char *uci);

/* Apply a move struct already chosen by the engine */
void chess_apply_move(ChessState *state, ChessMove move);

/* Find the best move for the current side. depth = 1 (easy) to 4 (strong). */
bool chess_find_best_move(ChessState *state, int depth, ChessMove *out_move);

/* Convert a ChessMove to a null-terminated UCI string (out_uci needs >= 6 bytes) */
void chess_move_to_uci(ChessMove move, char out_uci[6]);

/* Check if the side to move is in check */
bool chess_in_check(const ChessState *state, int8_t side);

/* Check if the game is ongoing, checkmate, or draw */
ChessStatus chess_get_status(ChessState *state);

#ifdef __cplusplus
}
#endif

#endif /* CHESS_ENGINE_H */