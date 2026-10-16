#!/usr/bin/env python3
"""Drive a real Xteink X4 over USB serial: press buttons, wait for log lines, take screenshots.

Needs the x4_th_test firmware for presses (CMD:PRESS / CMD:MEM, see platformio.personal.ini);
screenshots (CMD:SCREENSHOT) work with any CrossPoint build. pip install pyserial pillow.

  python scripts/device/x4test.py COM9 log.txt STEP...

STEP:
  wait:SECONDS              sleep
  press:KEY[:MS]            KEY = back|confirm|left|right|up|down|power (default hold 120 ms)
  expect:TEXT[:SECONDS]     wait until a log line contains TEXT (default 30 s); fails the run if not
  shot:FILE.png             grab the frame buffer (portrait 480x800)
  mem                       print free heap
  reset                     hardware reset; waits for the boot log (the X4 boots to Home)
Exit status 1 if any step failed.
"""
import sys
import threading
import time

import serial

SHOT_W, SHOT_H = 800, 480  # the raw frame buffer is landscape


class Device:
    def __init__(self, port: str, log_path: str):
        self.port = port
        self.ser = None
        self.ser_lock = threading.Lock()
        self.log = open(log_path, "a", encoding="utf-8", errors="replace")
        self.lines: list[str] = []
        self.lock = threading.Lock()
        self.shot: bytes | None = None
        self.shot_event = threading.Event()
        self.alive = True
        self._open()
        threading.Thread(target=self._reader, daemon=True).start()

    def _open(self) -> None:
        """(Re)open the port. USB-Serial/JTAG re-enumerates on every reset and boot stage."""
        with self.ser_lock:
            if self.ser is not None:
                try:
                    self.ser.close()
                except Exception:
                    pass
                self.ser = None
            for _ in range(150):
                try:
                    # Deassert DTR/RTS *before* opening: pyserial raises both on open by
                    # default, which the C3's USB-Serial/JTAG takes as a chip reset.
                    s = serial.Serial()
                    s.port, s.baudrate, s.timeout = self.port, 115200, 0.1
                    s.dtr = False
                    s.rts = False
                    s.open()
                    self.ser = s
                    return
                except serial.SerialException:
                    time.sleep(0.2)

    def _reader(self) -> None:
        buf = b""
        while self.alive:
            try:
                # Read under the lock: closing a port while another thread is inside
                # a native read() crashes the interpreter on Windows.
                with self.ser_lock:
                    chunk = self.ser.read(4096) if self.ser else None
            except Exception:
                chunk = None
            if chunk is None or self.ser is None:
                if not self.alive:
                    break
                self._open()
                continue
            if not chunk:
                continue
            buf += chunk
            while True:
                start = buf.find(b"SCREENSHOT_START:")
                nl = buf.find(b"\n")
                if start != -1 and (nl == -1 or start < nl):
                    end_hdr = buf.find(b"\n", start)
                    if end_hdr == -1:
                        break
                    size = int(buf[start + 17:end_hdr])
                    body = end_hdr + 1
                    if len(buf) < body + size:
                        break
                    self.shot = buf[body:body + size]
                    buf = buf[body + size:]
                    self.shot_event.set()
                    continue
                if nl == -1:
                    break
                line = buf[:nl].decode("utf-8", "replace").rstrip("\r")
                buf = buf[nl + 1:]
                with self.lock:
                    self.lines.append(line)
                self.log.write(line + "\n")
                self.log.flush()

    def send(self, cmd: str) -> None:
        for _ in range(40):
            try:
                with self.ser_lock:
                    self.ser.write(f"CMD:{cmd}\n".encode())
                    self.ser.flush()
                return
            except Exception:
                time.sleep(0.25)

    def expect(self, text: str, timeout: float, since: int) -> bool:
        end = time.time() + timeout
        while time.time() < end:
            with self.lock:
                if any(text in line for line in self.lines[since:]):
                    return True
            time.sleep(0.1)
        return False

    def screenshot(self, path: str) -> bool:
        from PIL import Image

        self.shot_event.clear()
        self.send("SCREENSHOT")
        if not self.shot_event.wait(20):
            return False
        data = self.shot
        if len(data) != SHOT_W * SHOT_H // 8:
            print(f"unexpected screenshot size {len(data)}")
            return False
        Image.frombytes("1", (SHOT_W, SHOT_H), data).transpose(Image.Transpose.ROTATE_270).save(path)
        return True

    def reset(self) -> None:
        """Hardware reset via RTS, as esptool does on USB-Serial/JTAG; the reader reopens the port."""
        with self.ser_lock:
            # usbser.sys only sends the control-line state when DTR is written, so
            # re-write DTR after each RTS change (esptool does the same).
            self.ser.dtr = False
            self.ser.rts = True
            self.ser.dtr = self.ser.dtr
            time.sleep(0.2)
            self.ser.rts = False
            self.ser.dtr = self.ser.dtr
        # On Windows a handle to a re-enumerated port can keep returning empty
        # reads instead of failing, so wait for the firmware's USB and reopen.
        time.sleep(3)
        self._open()

    def close(self) -> None:
        self.alive = False
        try:
            self.ser.close()
        except Exception:
            pass
        self.log.close()


def main() -> int:
    port, log_path, steps = sys.argv[1], sys.argv[2], sys.argv[3:]
    dev = Device(port, log_path)
    ok = True
    try:
        for step in steps:
            kind, _, arg = step.partition(":")
            mark = len(dev.lines)
            if kind == "wait":
                time.sleep(float(arg))
            elif kind == "press":
                key, _, ms = arg.partition(":")
                dev.send(f"PRESS {key} {ms or 120}")
                if not dev.expect("PRESS_OK", 3, mark):
                    print(f"FAIL press {key}: no PRESS_OK (is this the x4_th_test build?)")
                    ok = False
            elif kind == "expect":
                text, t = arg, "30"
                head, _, tail = arg.rpartition(":")
                if head and tail.isdigit():
                    text, t = head, tail
                if dev.expect(text, float(t), mark):
                    print(f"ok   expect '{text}'")
                else:
                    print(f"FAIL expect '{text}' within {t}s")
                    ok = False
            elif kind == "shot":
                good = dev.screenshot(arg)
                print(f"shot {arg}: {'ok' if good else 'FAILED'}")
                ok = ok and good
            elif kind == "mem":
                dev.send("MEM")
                dev.expect("MEM_FREE", 3, mark)
                with dev.lock:
                    print(next((l for l in dev.lines[mark:] if "MEM_FREE" in l), "mem: no reply"))
            elif kind == "reset":
                dev.reset()
                # Any log line from the new boot counts; which ones arrive before the
                # port reopens depends on timing and log level.
                booted = dev.expect("]", 30, mark)
                print("reset: " + ("booted" if booted else "FAILED (no boot log)"))
                ok = ok and booted
            else:
                sys.exit(f"unknown step {step}")
    finally:
        dev.close()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
