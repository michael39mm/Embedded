import argparse
from pathlib import Path
import chess
import cv2
import numpy as np
import config
from game import ChessEngine, detect_move, expected_squares
from vision import draw_grid, score_squares, warp_board


def load_aligned(path: str, corners: np.ndarray | None) -> np.ndarray:
    img = cv2.imread(path)
    if img is None:
        raise FileNotFoundError(f"Could not read image: {path}")
    if corners is not None:
        return warp_board(img, corners)
    return cv2.resize(img, (config.BOARD_SIZE, config.BOARD_SIZE))


def main() -> None:
    parser = argparse.ArgumentParser(description="Test move detection on two saved board images.")
    parser.add_argument("before", help="Path to 'before move' image.")
    parser.add_argument("after", help="Path to 'after move' image.")
    parser.add_argument("--corners", type=str, default=None, help="Optional corners.npy if images need perspective warp.")
    parser.add_argument("--fen", type=str, default=chess.STARTING_FEN, help="Starting board FEN.")
    parser.add_argument("--no-engine", action="store_true", help="Skip Stockfish reply.")
    parser.add_argument("--no-gui", action="store_true", help="Skip OpenCV preview window.")
    args = parser.parse_args()

    corners = np.load(args.corners) if args.corners else None
    before_800 = load_aligned(args.before, corners)
    after_800 = load_aligned(args.after, corners)

    board = chess.Board(args.fen)
    scores = score_squares(before_800, after_800)
    matches, ranked = detect_move(board, scores)

    print("\nTop 6 changed squares:")
    for sq, val in ranked[:6]:
        print(f"  {sq}: {val:.2f}")

    detected_move = None
    highlight = set()

    if len(matches) == 1:
        detected_move = matches[0]
        highlight = expected_squares(board, detected_move)
        print(f"\n[OK] Detected move: {detected_move.uci()} ({board.san(detected_move)})")
    elif len(matches) > 1 and all(m.promotion for m in matches):
        choice = input("Promotion detected! Choose piece (q/r/b/n): ").strip().lower()
        promo_map = {"q": chess.QUEEN, "r": chess.ROOK, "b": chess.BISHOP, "n": chess.KNIGHT}
        detected_move = next((m for m in matches if m.promotion == promo_map.get(choice)), None)
        if detected_move:
            highlight = expected_squares(board, detected_move)
            print(f"[OK] Selected promotion: {detected_move.uci()}")
    else:
        print("\n[REJECTED] No clear legal move matched. Board state NOT updated.")

    if detected_move is not None:
        board.push(detected_move)
        print("\nUpdated board:")
        print(board)
        print(f"FEN: {board.fen()}")

        if not args.no_engine and not board.is_game_over():
            if Path(config.STOCKFISH_PATH).exists():
                with ChessEngine() as engine:
                    reply = engine.choose_move(board)
                    print(f"\n[STOCKFISH] Reply: {reply.uci()} ({board.san(reply)})")
            else:
                print(f"\n[SKIP] Stockfish not found at {config.STOCKFISH_PATH}")

    if not args.no_gui:
        preview = draw_grid(after_800, highlight_squares=highlight, banner_text="Press any key to close")
        cv2.imshow("Test Result", preview)
        cv2.waitKey(0)
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()