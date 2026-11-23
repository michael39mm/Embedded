from pathlib import Path

# Path to the Stockfish binary (Windows)
STOCKFISH_PATH = Path("bin/pico_engine.exe")
ENGINE_TIME_SEC = 0.5  # How long Stockfish thinks per move

# Camera settings
CAMERA_INDEX = 1       # 0 = default webcam; try 1 if you have an external USB cam
FRAME_WIDTH = 1920
FRAME_HEIGHT = 1080
LOCK_EXPOSURE = False  # Keep False on DirectShow webcams that turn dark/grainy when locked
LOCK_AUTOFOCUS = False # Keep False unless your webcam keeps hunting focus between turns
CAPTURE_FRAMES = 5     # Average this many fresh frames per SPACE press to reduce grain

# Board warp settings (800x800 -> each of the 64 squares is 100x100 pixels)
BOARD_SIZE = 800
SQUARE_SIZE = BOARD_SIZE // 8
INNER_MARGIN = 20      # Ignore 20px border around each 100x100 square (keeps inner 60x60)

# Saved 4-corner calibration file
CORNERS_FILE = Path("corners.npy")

# Move detection thresholds
SCORE_PERCENTILE = 90    # Score the top 10% changed pixels per square (helps white-on-light pawns)
MIN_CHANGE_SCORE = 5.0   # Minimum change score (0..255) to count as moved
MIN_GAP_RATIO = 2.2      # Lowest moved square must be at least 2.2x bigger than highest untouched square