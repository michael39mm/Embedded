import os
import time
import cv2
import numpy as np
import config

FILES = "abcdefgh"


def open_camera(index: int = config.CAMERA_INDEX) -> cv2.VideoCapture:
    """Open the webcam at 1080p with MJPG, let focus/exposure settle, and print actual size."""
    backend = cv2.CAP_DSHOW if os.name == "nt" else cv2.CAP_ANY
    cap = cv2.VideoCapture(index, backend)
    if not cap.isOpened():
        raise RuntimeError(f"Could not open webcam at index {index}.")

    fourcc = cv2.VideoWriter_fourcc(*"MJPG")
    cap.set(cv2.CAP_PROP_FOURCC, fourcc)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, config.FRAME_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, config.FRAME_HEIGHT)
    cap.set(cv2.CAP_PROP_FOURCC, fourcc)

    # Keep autofocus on while warming up so the lens actually locks onto the board
    cap.set(cv2.CAP_PROP_AUTOFOCUS, 1)

    # Let auto-focus and auto-exposure settle for ~1 second
    for _ in range(25):
        cap.read()
        time.sleep(0.03)

    if config.LOCK_AUTOFOCUS:
        cap.set(cv2.CAP_PROP_AUTOFOCUS, 0)

    if config.LOCK_EXPOSURE:
        # 0.25 disables auto-exposure on most Windows DirectShow webcams
        cap.set(cv2.CAP_PROP_AUTO_EXPOSURE, 0.25)

    actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f"[CAMERA {index}] Active resolution: {actual_w}x{actual_h}")

    return cap


def warp_board(image: np.ndarray, corners: np.ndarray) -> np.ndarray:
    """
    Stretch the 4 outer board corners into a flat 800x800 top-down image.
    Corner order must be: [top-left (a8), top-right (h8), bottom-right (h1), bottom-left (a1)].
    """
    size = config.BOARD_SIZE
    src = np.array(corners, dtype=np.float32)
    dst = np.array(
        [[0, 0], [size, 0], [size, size], [0, size]],
        dtype=np.float32,
    )
    matrix = cv2.getPerspectiveTransform(src, dst)
    return cv2.warpPerspective(image, matrix, (size, size))


def score_squares(before_800: np.ndarray, after_800: np.ndarray) -> dict[str, float]:
    """Compare two aligned 800x800 board images and return a change score for every square."""
    before_gray = cv2.cvtColor(before_800, cv2.COLOR_BGR2GRAY)
    after_gray = cv2.cvtColor(after_800, cv2.COLOR_BGR2GRAY)

    # Light blur smooths out single-pixel camera grain
    before_gray = cv2.GaussianBlur(before_gray, (5, 5), 0)
    after_gray = cv2.GaussianBlur(after_gray, (5, 5), 0)

    diff = cv2.absdiff(before_gray, after_gray)

    scores: dict[str, float] = {}
    sq = config.SQUARE_SIZE
    m = config.INNER_MARGIN

    for row in range(8):
        rank = 8 - row  # Top row = rank 8, bottom row = rank 1 (White near camera)
        for col in range(8):
            r0, r1 = row * sq + m, (row + 1) * sq - m
            c0, c1 = col * sq + m, (col + 1) * sq - m
            inner_patch = diff[r0:r1, c0:c1]
            square_name = f"{FILES[col]}{rank}"
            scores[square_name] = float(np.mean(inner_patch))

    return scores


def draw_grid(
    warped: np.ndarray,
    highlight_squares: set[str] | None = None,
    banner_text: str = "",
) -> np.ndarray:
    """Draw an 8x8 green alignment grid, square labels, and optional highlighted squares."""
    out = warped.copy()
    sq = config.SQUARE_SIZE
    highlight_squares = highlight_squares or set()

    for row in range(8):
        rank = 8 - row
        for col in range(8):
            x0, y0 = col * sq, row * sq
            x1, y1 = x0 + sq, y0 + sq
            name = f"{FILES[col]}{rank}"

            if name in highlight_squares:
                cv2.rectangle(out, (x0 + 3, y0 + 3), (x1 - 3, y1 - 3), (0, 255, 255), 4)
            else:
                cv2.rectangle(out, (x0, y0), (x1, y1), (0, 200, 0), 1)

            cv2.putText(
                out,
                name,
                (x0 + 6, y0 + 20),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.5,
                (0, 255, 0),
                1,
                cv2.LINE_AA,
            )

    if banner_text:
        cv2.rectangle(out, (0, 0), (config.BOARD_SIZE, 36), (20, 20, 20), -1)
        cv2.putText(
            out,
            banner_text,
            (10, 25),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.65,
            (255, 255, 255),
            2,
            cv2.LINE_AA,
        )

    return out