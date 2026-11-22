from pathlib import Path
import chess
import chess.engine
import config


def expected_squares(board: chess.Board, move: chess.Move) -> set[str]:
    """Return the exact set of squares that physically change when `move` is played."""
    changed = {
        chess.square_name(move.from_square),
        chess.square_name(move.to_square),
    }

    if board.is_castling(move):
        rank = "1" if board.turn == chess.WHITE else "8"
        if board.is_kingside_castling(move):
            changed.update({f"h{rank}", f"f{rank}"})
        else:
            changed.update({f"a{rank}", f"d{rank}"})

    elif board.is_en_passant(move):
        captured_sq = chess.square(
            chess.square_file(move.to_square),
            chess.square_rank(move.from_square),
        )
        changed.add(chess.square_name(captured_sq))

    return changed


def detect_move(
    board: chess.Board,
    scores: dict[str, float],
    min_score: float = config.MIN_CHANGE_SCORE,
    min_ratio: float = config.MIN_GAP_RATIO,
) -> tuple[list[chess.Move], list[tuple[str, float]]]:
    """
    Match the highest-scoring squares against all legal moves.
    Returns (matching_moves, ranked_scores).
      - len(matching_moves) == 1 -> clean match
      - len(matching_moves) == 4 -> pawn promotion (same 2 squares, 4 piece choices)
      - len(matching_moves) == 0 -> rejected / unclear move
    """
    ranked = sorted(scores.items(), key=lambda item: item[1], reverse=True)
    matches: list[chess.Move] = []

    for move in board.legal_moves:
        exp = expected_squares(board, move)
        k = len(exp)

        top_k_names = {sq for sq, _ in ranked[:k]}
        if top_k_names != exp:
            continue

        min_inside = min(scores[sq] for sq in exp)
        max_outside = ranked[k][1]  # Highest score among all untouched squares

        if min_inside < min_score:
            continue
        if max_outside > 0 and (min_inside / max_outside) < min_ratio:
            continue

        matches.append(move)

    return matches, ranked


class ChessEngine:
    """Thin wrapper around Stockfish over UCI."""

    def __init__(self, path: Path = config.STOCKFISH_PATH, think_time: float = config.ENGINE_TIME_SEC):
        if not Path(path).exists():
            raise FileNotFoundError(
                f"Stockfish not found at '{path}'. Download it and place stockfish.exe there."
            )
        self.engine = chess.engine.SimpleEngine.popen_uci(str(path))
        self.think_time = think_time

    def choose_move(self, board: chess.Board) -> chess.Move:
        result = self.engine.play(board, chess.engine.Limit(time=self.think_time))
        if result.move is None:
            raise RuntimeError("Stockfish did not return a move.")
        return result.move

    def close(self) -> None:
        self.engine.quit()

    def __enter__(self) -> "ChessEngine":
        return self

    def __exit__(self, *_args: object) -> None:
        self.close()