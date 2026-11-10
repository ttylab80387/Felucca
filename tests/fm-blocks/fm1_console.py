# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 ttylab80387
# read-only FM-1 serial console: sends each argument as a command, prints the reply (never send uboot)
import os, sys, termios, time
port = sys.argv[1]
fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd)
a[0] = 0; a[1] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[3] = 0
a[4] = a[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, a)
def read_all(t):
    end, out = time.time() + t, b""
    while time.time() < end:
        try:
            out += os.read(fd, 4096)
        except BlockingIOError:
            time.sleep(0.05)
    return out.decode(errors="replace")
read_all(0.3)
for c in sys.argv[2:]:
    assert c.split()[0] in ("status", "memr", "crash", "dbg"), c
    os.write(fd, (c + "\r\n").encode())
    print(">>>", c); print(read_all(1.0).strip()[:800])
os.close(fd)
