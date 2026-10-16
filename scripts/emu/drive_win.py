#!/usr/bin/env python3
"""Boot a CrossPoint image in eink-qemu (Xteink X4 machine) and run scripted steps.

A Windows-friendly take on eink-qemu's drive.py: the QEMU monitor listens on
localhost TCP instead of a Unix socket, so it works with the release archive's
qemu-system-riscv32.exe and plain Windows Python.

  python scripts/emu/drive_win.py QEMU_DIR flash.bin sd.img log.txt STEP...

STEP: wait:SECONDS | press:KEY[:MS] | shot:FILE.png | hmp:COMMAND
KEY:  back | confirm | left | right | up | down | power   (X4 front/side buttons)
flash.bin is a 16 MiB image from eink-qemu/mkflash.py --machine x4.
"""
import os
import shutil
import socket
import subprocess
import sys
import time


def main() -> None:
    qemu_dir, img, sd, log, steps = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5:]
    qemu = os.path.join(qemu_dir, "bin", "qemu-system-riscv32.exe")
    machine = os.environ.get("X4MACHINE", "x4")
    run_img = img + ".run"
    shutil.copyfile(img, run_img)  # the firmware writes flash (NVS, OTA data)
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    p = subprocess.Popen(
        [qemu, "-machine", machine, "-icount", "shift=0,sleep=on", "-display", "none", "-serial", "null",
         "-drive", f"file={run_img},if=mtd,format=raw", "-drive", f"file={sd},if=sd,format=raw",
         "-chardev", f"file,id=cdc,path={log}",
         "-global", "driver=misc.esp32s3.usb_serial_jtag,property=chardev,value=cdc",
         "-monitor", f"tcp:127.0.0.1:{port},server,nowait"] + os.environ.get("QEMU_EXTRA", "").split(),
        stdout=subprocess.DEVNULL, stderr=sys.stderr)
    s = None
    for _ in range(100):
        try:
            s = socket.create_connection(("127.0.0.1", port), timeout=1)
            break
        except OSError:
            time.sleep(0.1)
    if s is None:
        p.kill()
        sys.exit("could not reach the QEMU monitor")
    s.settimeout(0.3)

    def hmp(cmd: str) -> str:
        s.sendall(cmd.encode() + b"\n")
        out = b""
        try:
            while True:
                chunk = s.recv(65536)
                if not chunk:
                    break
                out += chunk
        except socket.timeout:
            pass
        lines = out.decode(errors="replace").replace("\r", "").split("\n")
        return "\n".join(l for l in lines[1:] if not l.startswith("(qemu)")).strip()

    hmp("")
    try:
        for step in steps:
            kind, _, arg = step.partition(":")
            if kind == "wait":
                time.sleep(float(arg))
            elif kind == "press":
                key, _, ms = arg.partition(":")
                hmp(f"qom-set /machine/x3-keys {key} true")
                time.sleep(int(ms or 150) / 1000)
                hmp(f"qom-set /machine/x3-keys {key} false")
            elif kind == "shot":
                path = os.path.abspath(arg).replace("\\", "/")
                print(hmp(f"screendump {path} -f png panel") or f"shot {arg}", flush=True)
            elif kind == "hmp":
                print(hmp(arg), flush=True)
            else:
                sys.exit(f"unknown step {step}")
    finally:
        p.kill()
        p.wait()
        os.remove(run_img)


if __name__ == "__main__":
    main()
