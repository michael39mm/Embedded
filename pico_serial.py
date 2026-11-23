import time
import chess
import serial
import config


class PicoChessEngine:
    """Talks to your C UCI chess engine on a Raspberry Pi Pico over USB Serial."""

    def __init__(
        self,
        port: str = config.PICO_PORT,
        baud: int = config.PICO_BAUD,
        timeout: float = config.PICO_TIMEOUT_SEC,
    ) -> None:
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser: serial.Serial | None = None

    def __enter__(self) -> "PicoChessEngine":
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baud,
                timeout=self.timeout,
                write_timeout=2.0,
            )
        except serial.SerialException as exc:
            raise RuntimeError(
                f"Could not open Pico on {self.port}. Check the USB cable and COM port in config.py."
            ) from exc

        # Pico USB CDC requires DTR asserted before it flushes printf() output
        self.ser.dtr = True
        time.sleep(0.3)
        self.ser.reset_input_buffer()

        print(f"[PICO] Connected on {self.port}. Starting UCI handshake...")
        self._send("uci")
        self._read_until("uciok")

        self._send("ucinewgame")
        self._send("isready")
        self._read_until("readyok")

        print("[PICO] Engine ready!")
        return self

    def __exit__(self, _exc_type: object, _exc: object, _tb: object) -> None:
        if self.ser is not None and self.ser.is_open:
            try:
                self._send("quit")
            except serial.SerialException:
                pass
            self.ser.close()
            print("[PICO] Serial connection closed.")

    def _send(self, command: str) -> None:
        if self.ser is None:
            raise RuntimeError("Pico serial port is not open.")
        self.ser.write(f"{command}\n".encode("ascii"))
        self.ser.flush()

    def _read_until(self, prefix: str) -> str:
        if self.ser is None:
            raise RuntimeError("Pico serial port is not open.")

        while True:
            raw = self.ser.readline()
            if not raw:
                raise RuntimeError(
                    f"Pico timed out after {self.timeout}s waiting for '{prefix}'."
                )
            line = raw.decode("ascii", errors="ignore").strip()
            if not line:
                continue
            if line.startswith(prefix):
                return line

    def choose_move(self, board: chess.Board) -> chess.Move:
        moves_str = " ".join(m.uci() for m in board.move_stack)
        pos_cmd = f"position startpos moves {moves_str}" if moves_str else "position startpos"

        self._send(pos_cmd)
        self._send(f"go depth {config.PICO_SEARCH_DEPTH}")

        reply = self._read_until("bestmove ")
        parts = reply.split()
        if len(parts) < 2 or parts[1] in ("(none)", "0000"):
            raise RuntimeError(f"Pico returned no valid move: '{reply}'")

        move = chess.Move.from_uci(parts[1])
        if move not in board.legal_moves:
            raise RuntimeError(f"Pico returned illegal move '{move.uci()}' for current board!")

        return move