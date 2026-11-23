import argparse
import cv2
import numpy as np
import config
from vision import draw_grid, open_camera, warp_board

CORNER_LABELS = [
    "1: Top-Left (a8 outer corner)",
    "2: Top-Right (h8 outer corner)",
    "3: Bottom-Right (h1 outer corner)",
    "4: Bottom-Left (a1 outer corner)",
]


def main() -> None:
    parser = argparse.ArgumentParser(description="Click the 4 outer corners of the chessboard.")
    parser.add_argument("--image", type=str, default=None, help="Optional static image path instead of webcam.")
    parser.add_argument("--camera", type=int, default=config.CAMERA_INDEX, help="Webcam index.")
    args = parser.parse_args()

    clicked_points: list[tuple[int, int]] = []
    cap = None
    static_img = None
    preview_open = False

    if args.image:
        static_img = cv2.imread(args.image)
        if static_img is None:
            raise FileNotFoundError(f"Could not read image: {args.image}")
    else:
        cap = open_camera(args.camera)

    def on_mouse(event: int, x: int, y: int, _flags: int, _param: object) -> None:
        if event == cv2.EVENT_LBUTTONDOWN and len(clicked_points) < 4:
            clicked_points.append((x, y))
            print(f"Clicked {CORNER_LABELS[len(clicked_points) - 1]} at ({x}, {y})")

    win_name = "Calibrate Board (Click 4 Corners)"
    preview_name = "Warped Preview"
    cv2.namedWindow(win_name, cv2.WINDOW_NORMAL)
    cv2.resizeWindow(win_name, 960, 540)
    cv2.setMouseCallback(win_name, on_mouse)

    print("Click the 4 outer corners in order: a8 -> h8 -> h1 -> a1.")
    print("Keys: [S] Save | [R] Reset clicks | [Q] Quit")

    try:
        while True:
            if static_img is not None:
                frame = static_img.copy()
            else:
                ok, frame = cap.read()
                if not ok:
                    continue

            display = frame.copy()
            for idx, (px, py) in enumerate(clicked_points):
                cv2.circle(display, (px, py), 8, (0, 0, 255), -1)
                cv2.putText(
                    display,
                    str(idx + 1),
                    (px + 10, py - 10),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.9,
                    (0, 0, 255),
                    2,
                )

            if len(clicked_points) < 4:
                prompt = f"Click {CORNER_LABELS[len(clicked_points)]}"
            else:
                prompt = "Check Warped Preview -> Press [S] to Save or [R] to Reset"
                if not preview_open:
                    cv2.namedWindow(preview_name, cv2.WINDOW_NORMAL)
                    cv2.resizeWindow(preview_name, 600, 600)
                    cv2.moveWindow(preview_name, 950, 50)
                    preview_open = True
                warped = warp_board(frame, np.array(clicked_points, dtype=np.float32))
                cv2.imshow(preview_name, draw_grid(warped))

            cv2.rectangle(display, (0, 0), (display.shape[1], 42), (0, 0, 0), -1)
            cv2.putText(display, prompt, (12, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)
            cv2.imshow(win_name, display)

            key = cv2.waitKey(30) & 0xFF
            if key in (ord("r"), ord("R")):
                clicked_points.clear()
                if preview_open and cv2.getWindowProperty(preview_name, cv2.WND_PROP_VISIBLE) >= 1:
                    cv2.destroyWindow(preview_name)
                preview_open = False
                print("Reset corners.")
            elif key in (ord("s"), ord("S")) and len(clicked_points) == 4:
                np.save(config.CORNERS_FILE, np.array(clicked_points, dtype=np.float32))
                print(f"Saved corners to {config.CORNERS_FILE}")
                break
            elif key in (ord("q"), ord("Q")):
                break
    finally:
        if cap is not None:
            cap.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()