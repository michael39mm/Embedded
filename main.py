import chess
import cv2
import numpy as np
import config
from game import ChessEngine, detect_move, expected_squares
from vision import draw_grid, open_camera, score_squares, warp_board


def send_to_robot(move: chess.Move) -> None:
    """
    Placeholder for sending the move to your RP2040 over USB Serial.
    Later, replace this print with:
        ser.write(f"MOVE {move.uci()}\\n".encode())
    """
    print(f"\n[ROBOT COMMAND] MOVE {move.uci()}")
    print("-> Move the black piece on the board (simulating the robot), then press SPACE to verify.")


def ask_promotion_piece(matches: list[chess.Move]) -> chess.Move | None:
    choice = input("Pawn promotion! Enter piece (q/r/b/n): ").strip().lower()
    promo_map = {"q": chess.QUEEN, "r": chess.ROOK, "b": chess.BISHOP, "n": chess.KNIGHT}
    return next((m for m in matches if m.promotion == promo_map.get(choice)), None)


def print_top_scores(ranked: list[tuple[str, float]]) -> None:
    summary = ", ".join(f"{sq}:{val:.1f}" for sq, val in ranked[:5])
    print(f"Top 5 squares -> {summary}")


def main() -> None:
    if not config.CORNERS_FILE.exists():
        print(f"Missing {config.CORNERS_FILE}. Run 'python calibrate.py' first.")
        return

    corners = np.load(config.CORNERS_FILE)
    board = chess.Board()
    cap = open_camera(config.CAMERA_INDEX)

    baseline: np.ndarray | None = None
    pending_robot_move: chess.Move | None = None
    last_highlight: set[str] = set()

    print("\n=== AI Chessboard Live Mode ===")
    print("Controls (click the OpenCV window first):")
    print("  [SPACE] Capture baseline / confirm move / verify robot move")
    print("  [M]     Type an on-screen move manually (e.g., e2e4)")
    print("  [B]     Re-capture baseline image (if lighting changed)")
    print("  [F]     Force-accept robot move (only if camera verification fails)")
    print("  [Q]     Quit\n")

    with ChessEngine() as engine:
        try:
            while not board.is_game_over():
                ok, frame = cap.read()
                if not ok:
                    continue

                warped = warp_board(frame, corners)

                if baseline is None:
                    banner = "Setup board -> Press [SPACE] to capture starting image"
                elif pending_robot_move is not None:
                    banner = f"ROBOT MOVE {pending_robot_move.uci()} -> Press [SPACE] to verify"
                else:
                    turn_str = "WHITE" if board.turn == chess.WHITE else "BLACK"
                    banner = f"{turn_str} TURN -> Move piece & press [SPACE] | [M] Manual | [Q] Quit"

                cv2.imshow("AI Chessboard", draw_grid(warped, last_highlight, banner))
                key = cv2.waitKey(30) & 0xFF

                if key == ord("q"):
                    break

                # Re-sync baseline if camera or lighting shifted
                if key == ord("b"):
                    baseline = warped.copy()
                    print("[INFO] Baseline image updated.")
                    continue

                # Force-accept robot move during testing if needed
                if key == ord("f") and pending_robot_move is not None:
                    board.push(pending_robot_move)
                    baseline = warped.copy()
                    print(f"[FORCED] Accepted robot move {pending_robot_move.uci()}.")
                    pending_robot_move = None
                    continue

                # On-screen manual move entry (updates the exact same game state)
                if key == ord("m") and baseline is not None and pending_robot_move is None:
                    uci_text = input("Enter move in UCI format (e.g., e2e4): ").strip().lower()
                    try:
                        move = chess.Move.from_uci(uci_text)
                    except ValueError:
                        print("[ERROR] Invalid format. Use format like e2e4 or e7e8q.")
                        continue

                    if move not in board.legal_moves:
                        print(f"[ILLEGAL] {uci_text} is not legal in this position.")
                        continue

                    last_highlight = expected_squares(board, move)
                    board.push(move)
                    print(f"[MANUAL OK] Played {move.uci()}. Sync the physical board and press [B] if needed.")
                    print(board)

                    if not board.is_game_over():
                        pending_robot_move = engine.choose_move(board)
                        send_to_robot(pending_robot_move)
                    continue

                # Main SPACE trigger
                if key == ord(" "):
                    # 1. Capture initial baseline
                    if baseline is None:
                        baseline = warped.copy()
                        print("[READY] Starting board captured. Make your move!")
                        continue

                    scores = score_squares(baseline, warped)
                    matches, ranked = detect_move(board, scores)
                    print_top_scores(ranked)

                    # 2. Verify robot's physical move
                    if pending_robot_move is not None:
                        if pending_robot_move in matches:
                            last_highlight = expected_squares(board, pending_robot_move)
                            board.push(pending_robot_move)
                            baseline = warped.copy()
                            print(f"[VERIFIED] Robot move {pending_robot_move.uci()} confirmed!")
                            print(board)
                            pending_robot_move = None
                        else:
                            print(
                                f"[VERIFY FAILED] Expected {pending_robot_move.uci()}. "
                                "Fix the physical board and press [SPACE] again (or [F] to force)."
                            )
                        continue

                    # 3. Detect player's physical move
                    if len(matches) == 1:
                        player_move = matches[0]
                    elif len(matches) > 1 and all(m.promotion for m in matches):
                        player_move = ask_promotion_piece(matches)
                    else:
                        print("[REJECTED] Move not recognized — check the board and press [SPACE] again.")
                        continue

                    if player_move is None:
                        print("[REJECTED] Invalid promotion choice.")
                        continue

                    last_highlight = expected_squares(board, player_move)
                    print(f"[PLAYER OK] Detected {player_move.uci()} ({board.san(player_move)})")
                    board.push(player_move)
                    baseline = warped.copy()
                    print(board)

                    if not board.is_game_over():
                        pending_robot_move = engine.choose_move(board)
                        send_to_robot(pending_robot_move)

            if board.is_game_over():
                print(f"\n=== GAME OVER: {board.result()} ({board.outcome().termination.name}) ===")

        finally:
            cap.release()
            cv2.destroyAllWindows()


if __name__ == "__main__":
    main()