#include "chess_engine.h"

#define MAX_MOVES_PER_PLY 256
#define MATE_SCORE        30000
#define INF_SCORE         32000

typedef struct {
    int8_t   captured;
    uint8_t  castling;
    int8_t   ep_square;
    uint16_t halfmove_clock;
} UndoInfo;

/* Static scratch buffers (~8 KB total RAM) to keep the RP2040 call stack tiny */
static ChessMove g_move_stack[CHESS_MAX_DEPTH + 1][MAX_MOVES_PER_PLY];
static int16_t   g_score_stack[CHESS_MAX_DEPTH + 1][MAX_MOVES_PER_PLY];
static UndoInfo  g_undo_stack[CHESS_MAX_DEPTH + 1];

static const int PIECE_VALUE[7] = {
    0, 100, 320, 330, 500, 900, 20000
};

/* Piece-Square Tables (written visually: top row = rank 8, bottom row = rank 1 for White) */
static const int8_t PST_PAWN[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

static const int8_t PST_KNIGHT[64] = {
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50
};

static const int8_t PST_BISHOP[64] = {
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
};

static const int8_t PST_ROOK[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

static const int8_t PST_QUEEN[64] = {
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20
};

static const int8_t PST_KING[64] = {
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};

/* Clears castling rights whenever a king or rook moves from/gets captured on a corner */
static const uint8_t CASTLE_MASK[64] = {
    13, 15, 15, 15, 12, 15, 15, 14, /* rank 1: a1(0), e1(4), h1(7) */
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
     7, 15, 15, 15,  3, 15, 15, 11  /* rank 8: a8(56), e8(60), h8(63) */
};

static inline int iabs(int x) { return x < 0 ? -x : x; }
static inline int sq_rank(int sq) { return sq >> 3; }
static inline int sq_file(int sq) { return sq & 7; }
static inline int make_sq(int r, int f) { return (r << 3) | f; }

void chess_init(ChessState *state) {
    static const int8_t back_rank[8] = {
        P_ROOK, P_KNIGHT, P_BISHOP, P_QUEEN, P_KING, P_BISHOP, P_KNIGHT, P_ROOK
    };

    for (int i = 0; i < 64; i++) {
        state->board[i] = P_EMPTY;
    }
    for (int f = 0; f < 8; f++) {
        state->board[make_sq(0, f)] = back_rank[f];
        state->board[make_sq(1, f)] = P_PAWN;
        state->board[make_sq(6, f)] = (int8_t)-P_PAWN;
        state->board[make_sq(7, f)] = (int8_t)-back_rank[f];
    }

    state->side           = 1; /* White starts */
    state->castling       = CASTLE_WK | CASTLE_WQ | CASTLE_BK | CASTLE_BQ;
    state->ep_square      = -1;
    state->halfmove_clock = 0;
}

static int find_king(const ChessState *state, int8_t side) {
    int8_t target = (int8_t)(side * P_KING);
    for (int sq = 0; sq < 64; sq++) {
        if (state->board[sq] == target) {
            return sq;
        }
    }
    return -1;
}

static bool is_square_attacked(const ChessState *state, int sq, int8_t by_side) {
    int r = sq_rank(sq);
    int f = sq_file(sq);

    /* 1. Pawn attacks */
    int pawn_r = r - by_side;
    if (pawn_r >= 0 && pawn_r < 8) {
        int8_t enemy_pawn = (int8_t)(by_side * P_PAWN);
        if (f > 0 && state->board[make_sq(pawn_r, f - 1)] == enemy_pawn) return true;
        if (f < 7 && state->board[make_sq(pawn_r, f + 1)] == enemy_pawn) return true;
    }

    /* 2. Knight attacks */
    static const int8_t kn_dr[8] = {-2, -2, -1, -1,  1,  1,  2,  2};
    static const int8_t kn_df[8] = {-1,  1, -2,  2, -2,  2, -1,  1};
    int8_t enemy_knight = (int8_t)(by_side * P_KNIGHT);
    for (int i = 0; i < 8; i++) {
        int nr = r + kn_dr[i];
        int nf = f + kn_df[i];
        if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            if (state->board[make_sq(nr, nf)] == enemy_knight) return true;
        }
    }

    /* 3. King attacks */
    int8_t enemy_king = (int8_t)(by_side * P_KING);
    for (int dr = -1; dr <= 1; dr++) {
        for (int df = -1; df <= 1; df++) {
            if (dr == 0 && df == 0) continue;
            int nr = r + dr;
            int nf = f + df;
            if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                if (state->board[make_sq(nr, nf)] == enemy_king) return true;
            }
        }
    }

    /* 4. Diagonal sliders (Bishop / Queen) */
    static const int8_t diag_dr[4] = {-1, -1,  1,  1};
    static const int8_t diag_df[4] = {-1,  1, -1,  1};
    int8_t enemy_bishop = (int8_t)(by_side * P_BISHOP);
    int8_t enemy_queen  = (int8_t)(by_side * P_QUEEN);
    for (int d = 0; d < 4; d++) {
        int nr = r + diag_dr[d];
        int nf = f + diag_df[d];
        while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            int8_t p = state->board[make_sq(nr, nf)];
            if (p != P_EMPTY) {
                if (p == enemy_bishop || p == enemy_queen) return true;
                break;
            }
            nr += diag_dr[d];
            nf += diag_df[d];
        }
    }

    /* 5. Orthogonal sliders (Rook / Queen) */
    static const int8_t orth_dr[4] = {-1, 1,  0, 0};
    static const int8_t orth_df[4] = { 0, 0, -1, 1};
    int8_t enemy_rook = (int8_t)(by_side * P_ROOK);
    for (int d = 0; d < 4; d++) {
        int nr = r + orth_dr[d];
        int nf = f + orth_df[d];
        while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            int8_t p = state->board[make_sq(nr, nf)];
            if (p != P_EMPTY) {
                if (p == enemy_rook || p == enemy_queen) return true;
                break;
            }
            nr += orth_dr[d];
            nf += orth_df[d];
        }
    }

    return false;
}

bool chess_in_check(const ChessState *state, int8_t side) {
    int ksq = find_king(state, side);
    if (ksq < 0) return true;
    return is_square_attacked(state, ksq, (int8_t)-side);
}

static void make_move(ChessState *state, ChessMove m, UndoInfo *undo) {
    int8_t piece = state->board[m.from];

    undo->castling       = state->castling;
    undo->ep_square      = state->ep_square;
    undo->halfmove_clock = state->halfmove_clock;

    if (m.flags == FLAG_EN_PASSANT) {
        int cap_sq = (int)m.to - 8 * state->side;
        undo->captured = state->board[cap_sq];
        state->board[cap_sq] = P_EMPTY;
    } else {
        undo->captured = state->board[m.to];
    }

    state->board[m.from] = P_EMPTY;
    state->board[m.to]   = m.promo ? (int8_t)(state->side * m.promo) : piece;

    if (m.flags == FLAG_CASTLE) {
        if (m.to == 6)       { state->board[5]  = state->board[7];  state->board[7]  = P_EMPTY; }
        else if (m.to == 2)  { state->board[3]  = state->board[0];  state->board[0]  = P_EMPTY; }
        else if (m.to == 62) { state->board[61] = state->board[63]; state->board[63] = P_EMPTY; }
        else if (m.to == 58) { state->board[59] = state->board[56]; state->board[56] = P_EMPTY; }
    }

    if (m.flags == FLAG_DOUBLE_PAWN) {
        state->ep_square = (int8_t)((m.from + m.to) / 2);
    } else {
        state->ep_square = -1;
    }

    state->castling &= (uint8_t)(CASTLE_MASK[m.from] & CASTLE_MASK[m.to]);

    if (iabs(piece) == P_PAWN || undo->captured != P_EMPTY) {
        state->halfmove_clock = 0;
    } else {
        state->halfmove_clock++;
    }

    state->side = (int8_t)-state->side;
}

static void unmake_move(ChessState *state, ChessMove m, const UndoInfo *undo) {
    state->side           = (int8_t)-state->side;
    state->castling       = undo->castling;
    state->ep_square      = undo->ep_square;
    state->halfmove_clock = undo->halfmove_clock;

    int8_t moved = m.promo ? (int8_t)(state->side * P_PAWN) : state->board[m.to];
    state->board[m.from] = moved;

    if (m.flags == FLAG_EN_PASSANT) {
        int cap_sq = (int)m.to - 8 * state->side;
        state->board[m.to]   = P_EMPTY;
        state->board[cap_sq] = undo->captured;
    } else {
        state->board[m.to] = undo->captured;
    }

    if (m.flags == FLAG_CASTLE) {
        if (m.to == 6)       { state->board[7]  = state->board[5];  state->board[5]  = P_EMPTY; }
        else if (m.to == 2)  { state->board[0]  = state->board[3];  state->board[3]  = P_EMPTY; }
        else if (m.to == 62) { state->board[63] = state->board[61]; state->board[61] = P_EMPTY; }
        else if (m.to == 58) { state->board[56] = state->board[59]; state->board[59] = P_EMPTY; }
    }
}

static void push_move(ChessMove *list, int *count, int from, int to, int promo, int flags) {
    if (*count < MAX_MOVES_PER_PLY) {
        list[*count].from  = (uint8_t)from;
        list[*count].to    = (uint8_t)to;
        list[*count].promo = (uint8_t)promo;
        list[*count].flags = (uint8_t)flags;
        (*count)++;
    }
}

static void push_pawn_move(ChessMove *list, int *count, int from, int to, int promo_rank) {
    if (sq_rank(to) == promo_rank) {
        push_move(list, count, from, to, P_QUEEN,  FLAG_NONE);
        push_move(list, count, from, to, P_ROOK,   FLAG_NONE);
        push_move(list, count, from, to, P_BISHOP, FLAG_NONE);
        push_move(list, count, from, to, P_KNIGHT, FLAG_NONE);
    } else {
        push_move(list, count, from, to, 0, FLAG_NONE);
    }
}

static int generate_pseudo_moves(const ChessState *state, ChessMove *list) {
    int count = 0;
    int8_t side = state->side;
    int8_t enemy = (int8_t)-side;

    static const int8_t kn_dr[8] = {-2, -2, -1, -1,  1,  1,  2,  2};
    static const int8_t kn_df[8] = {-1,  1, -2,  2, -2,  2, -1,  1};
    static const int8_t dir_dr[8] = {-1, -1,  1,  1, -1,  1,  0,  0};
    static const int8_t dir_df[8] = {-1,  1, -1,  1,  0,  0, -1,  1};

    for (int sq = 0; sq < 64; sq++) {
        int8_t p = state->board[sq];
        if (p == P_EMPTY || (p * side) < 0) continue;

        int type = iabs(p);
        int r = sq_rank(sq);
        int f = sq_file(sq);

        if (type == P_PAWN) {
            int start_rank = (side == 1) ? 1 : 6;
            int promo_rank = (side == 1) ? 7 : 0;
            int nr = r + side;

            if (nr >= 0 && nr < 8) {
                int one_step = make_sq(nr, f);
                if (state->board[one_step] == P_EMPTY) {
                    push_pawn_move(list, &count, sq, one_step, promo_rank);
                    if (r == start_rank) {
                        int two_step = make_sq(r + 2 * side, f);
                        if (state->board[two_step] == P_EMPTY) {
                            push_move(list, &count, sq, two_step, 0, FLAG_DOUBLE_PAWN);
                        }
                    }
                }
                /* Diagonal captures + en passant */
                for (int df = -1; df <= 1; df += 2) {
                    int nf = f + df;
                    if (nf < 0 || nf >= 8) continue;
                    int cap_sq = make_sq(nr, nf);
                    if (state->board[cap_sq] * side < 0) {
                        push_pawn_move(list, &count, sq, cap_sq, promo_rank);
                    } else if (cap_sq == state->ep_square) {
                        push_move(list, &count, sq, cap_sq, 0, FLAG_EN_PASSANT);
                    }
                }
            }
        } else if (type == P_KNIGHT) {
            for (int i = 0; i < 8; i++) {
                int nr = r + kn_dr[i];
                int nf = f + kn_df[i];
                if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                    int to = make_sq(nr, nf);
                    if (state->board[to] * side <= 0) {
                        push_move(list, &count, sq, to, 0, FLAG_NONE);
                    }
                }
            }
        } else if (type == P_BISHOP || type == P_ROOK || type == P_QUEEN) {
            int d_start = (type == P_ROOK)   ? 4 : 0;
            int d_end   = (type == P_BISHOP) ? 4 : 8;
            for (int d = d_start; d < d_end; d++) {
                int nr = r + dir_dr[d];
                int nf = f + dir_df[d];
                while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                    int to = make_sq(nr, nf);
                    int8_t target = state->board[to];
                    if (target == P_EMPTY) {
                        push_move(list, &count, sq, to, 0, FLAG_NONE);
                    } else {
                        if (target * side < 0) {
                            push_move(list, &count, sq, to, 0, FLAG_NONE);
                        }
                        break;
                    }
                    nr += dir_dr[d];
                    nf += dir_df[d];
                }
            }
        } else if (type == P_KING) {
            for (int d = 0; d < 8; d++) {
                int nr = r + dir_dr[d];
                int nf = f + dir_df[d];
                if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                    int to = make_sq(nr, nf);
                    if (state->board[to] * side <= 0) {
                        push_move(list, &count, sq, to, 0, FLAG_NONE);
                    }
                }
            }
            /* Castling */
            if (side == 1 && sq == 4) {
                if ((state->castling & CASTLE_WK) &&
                    state->board[5] == P_EMPTY && state->board[6] == P_EMPTY &&
                    !is_square_attacked(state, 4, enemy) &&
                    !is_square_attacked(state, 5, enemy) &&
                    !is_square_attacked(state, 6, enemy)) {
                    push_move(list, &count, 4, 6, 0, FLAG_CASTLE);
                }
                if ((state->castling & CASTLE_WQ) &&
                    state->board[1] == P_EMPTY && state->board[2] == P_EMPTY &&
                    state->board[3] == P_EMPTY &&
                    !is_square_attacked(state, 4, enemy) &&
                    !is_square_attacked(state, 3, enemy) &&
                    !is_square_attacked(state, 2, enemy)) {
                    push_move(list, &count, 4, 2, 0, FLAG_CASTLE);
                }
            } else if (side == -1 && sq == 60) {
                if ((state->castling & CASTLE_BK) &&
                    state->board[61] == P_EMPTY && state->board[62] == P_EMPTY &&
                    !is_square_attacked(state, 60, enemy) &&
                    !is_square_attacked(state, 61, enemy) &&
                    !is_square_attacked(state, 62, enemy)) {
                    push_move(list, &count, 60, 62, 0, FLAG_CASTLE);
                }
                if ((state->castling & CASTLE_BQ) &&
                    state->board[57] == P_EMPTY && state->board[58] == P_EMPTY &&
                    state->board[59] == P_EMPTY &&
                    !is_square_attacked(state, 60, enemy) &&
                    !is_square_attacked(state, 59, enemy) &&
                    !is_square_attacked(state, 58, enemy)) {
                    push_move(list, &count, 60, 58, 0, FLAG_CASTLE);
                }
            }
        }
    }

    return count;
}

static int generate_legal_moves(ChessState *state, ChessMove *list) {
    int pseudo_count = generate_pseudo_moves(state, list);
    int legal_count = 0;
    UndoInfo undo;

    for (int i = 0; i < pseudo_count; i++) {
        make_move(state, list[i], &undo);
        int8_t mover = (int8_t)-state->side;
        bool illegal = chess_in_check(state, mover);
        unmake_move(state, list[i], &undo);

        if (!illegal) {
            list[legal_count++] = list[i];
        }
    }
    return legal_count;
}

/* Score board from the current side's point of view */
static int evaluate(const ChessState *state) {
    int score = 0;

    for (int sq = 0; sq < 64; sq++) {
        int8_t p = state->board[sq];
        if (p == P_EMPTY) continue;

        int type = iabs(p);
        int r = sq_rank(sq);
        int f = sq_file(sq);
        int pst_idx = (p > 0) ? ((7 - r) * 8 + f) : (r * 8 + f);

        int bonus = 0;
        switch (type) {
            case P_PAWN:   bonus = PST_PAWN[pst_idx];   break;
            case P_KNIGHT: bonus = PST_KNIGHT[pst_idx]; break;
            case P_BISHOP: bonus = PST_BISHOP[pst_idx]; break;
            case P_ROOK:   bonus = PST_ROOK[pst_idx];   break;
            case P_QUEEN:  bonus = PST_QUEEN[pst_idx];  break;
            case P_KING:   bonus = PST_KING[pst_idx];   break;
            default: break;
        }

        int val = PIECE_VALUE[type] + bonus;
        score += (p > 0) ? val : -val;
    }

    return score * state->side;
}

/* Order captures & promotions first so Alpha-Beta skips bad branches faster */
static void score_moves(const ChessState *state, const ChessMove *list, int16_t *scores, int count) {
    for (int i = 0; i < count; i++) {
        int16_t s = 0;
        int8_t victim = state->board[list[i].to];
        if (list[i].flags == FLAG_EN_PASSANT) {
            s += 1050;
        } else if (victim != P_EMPTY) {
            int attacker = iabs(state->board[list[i].from]);
            s += (int16_t)(1000 + 10 * PIECE_VALUE[iabs(victim)] - PIECE_VALUE[attacker]);
        }
        if (list[i].promo) {
            s += (int16_t)(800 + PIECE_VALUE[list[i].promo]);
        }
        scores[i] = s;
    }
}

static void pick_next_move(ChessMove *list, int16_t *scores, int start, int count) {
    int best = start;
    for (int i = start + 1; i < count; i++) {
        if (scores[i] > scores[best]) {
            best = i;
        }
    }
    if (best != start) {
        ChessMove tmp_m = list[start];
        list[start] = list[best];
        list[best] = tmp_m;

        int16_t tmp_s = scores[start];
        scores[start] = scores[best];
        scores[best] = tmp_s;
    }
}

static int negamax(ChessState *state, int depth, int ply, int alpha, int beta) {
    if (state->halfmove_clock >= 100) {
        return 0;
    }
    if (depth == 0) {
        return evaluate(state);
    }

    ChessMove *moves  = g_move_stack[ply];
    int16_t   *scores = g_score_stack[ply];
    int count = generate_legal_moves(state, moves);

    if (count == 0) {
        if (chess_in_check(state, state->side)) {
            return -(MATE_SCORE - ply);
        }
        return 0; /* Stalemate */
    }

    score_moves(state, moves, scores, count);
    int best = -INF_SCORE;

    for (int i = 0; i < count; i++) {
        pick_next_move(moves, scores, i, count);

        make_move(state, moves[i], &g_undo_stack[ply]);
        int val = -negamax(state, depth - 1, ply + 1, -beta, -alpha);
        unmake_move(state, moves[i], &g_undo_stack[ply]);

        if (val > best)  best = val;
        if (val > alpha) alpha = val;
        if (alpha >= beta) break;
    }

    return best;
}

bool chess_find_best_move(ChessState *state, int depth, ChessMove *out_move) {
    if (depth < 1) depth = 1;
    if (depth > CHESS_MAX_DEPTH) depth = CHESS_MAX_DEPTH;

    ChessMove *moves  = g_move_stack[0];
    int16_t   *scores = g_score_stack[0];
    int count = generate_legal_moves(state, moves);

    if (count == 0) {
        return false;
    }

    score_moves(state, moves, scores, count);

    int alpha = -INF_SCORE;
    int beta  = INF_SCORE;
    int best_score = -INF_SCORE;
    ChessMove best_move = moves[0];

    for (int i = 0; i < count; i++) {
        pick_next_move(moves, scores, i, count);

        make_move(state, moves[i], &g_undo_stack[0]);
        int val = -negamax(state, depth - 1, 1, -beta, -alpha);
        unmake_move(state, moves[i], &g_undo_stack[0]);

        if (val > best_score) {
            best_score = val;
            best_move  = moves[i];
        }
        if (val > alpha) {
            alpha = val;
        }
    }

    if (out_move) {
        *out_move = best_move;
    }
    return true;
}

void chess_apply_move(ChessState *state, ChessMove move) {
    UndoInfo unused;
    make_move(state, move, &unused);
}

bool chess_apply_uci(ChessState *state, const char *uci) {
    if (!uci || uci[0] < 'a' || uci[0] > 'h' || uci[1] < '1' || uci[1] > '8' ||
                uci[2] < 'a' || uci[2] > 'h' || uci[3] < '1' || uci[3] > '8') {
        return false;
    }

    uint8_t from = (uint8_t)make_sq(uci[1] - '1', uci[0] - 'a');
    uint8_t to   = (uint8_t)make_sq(uci[3] - '1', uci[2] - 'a');
    uint8_t promo = 0;

    if (uci[4] == 'q' || uci[4] == 'Q')      promo = P_QUEEN;
    else if (uci[4] == 'r' || uci[4] == 'R') promo = P_ROOK;
    else if (uci[4] == 'b' || uci[4] == 'B') promo = P_BISHOP;
    else if (uci[4] == 'n' || uci[4] == 'N') promo = P_KNIGHT;

    ChessMove *moves = g_move_stack[0];
    int count = generate_legal_moves(state, moves);

    for (int i = 0; i < count; i++) {
        if (moves[i].from == from && moves[i].to == to) {
            /* Match exact promotion, or default to Queen if caller omitted the 5th char */
            if (moves[i].promo == promo || (promo == 0 && moves[i].promo == P_QUEEN)) {
                chess_apply_move(state, moves[i]);
                return true;
            }
        }
    }
    return false;
}

void chess_move_to_uci(ChessMove move, char out_uci[6]) {
    out_uci[0] = (char)('a' + sq_file(move.from));
    out_uci[1] = (char)('1' + sq_rank(move.from));
    out_uci[2] = (char)('a' + sq_file(move.to));
    out_uci[3] = (char)('1' + sq_rank(move.to));

    if (move.promo) {
        static const char promo_chars[7] = {0, 0, 'n', 'b', 'r', 'q', 0};
        out_uci[4] = promo_chars[move.promo];
        out_uci[5] = '\0';
    } else {
        out_uci[4] = '\0';
    }
}

ChessStatus chess_get_status(ChessState *state) {
    if (state->halfmove_clock >= 100) {
        return GAME_DRAW_50_MOVE;
    }
    int count = generate_legal_moves(state, g_move_stack[0]);
    if (count > 0) {
        return GAME_ONGOING;
    }
    return chess_in_check(state, state->side) ? GAME_CHECKMATE : GAME_STALEMATE;
}