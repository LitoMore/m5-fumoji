#!/usr/bin/env python3
"""USB framebuffer viewer for m5-fumoji.

Copyright (C) 2026 LitoMore
SPDX-License-Identifier: GPL-2.0-only
"""

from __future__ import annotations

import argparse
import shutil
import struct
import subprocess
import sys
import time
import zlib

import serial
from serial.tools import list_ports


MAGIC = b"FMJF"
HEADER = struct.Struct("<4sBBHHI")
WIDTH = 240
HEIGHT = 135
PAYLOAD_SIZE = WIDTH * HEIGHT // 8
ESPRESSIF_VID = 0x303A
USB_SERIAL_JTAG_PID = 0x1001
PIXELS = tuple(
    bytes(0 if value & (0x80 >> bit) else 255 for bit in range(8))
    for value in range(256)
)


def find_port(requested: str | None) -> str:
    if requested:
        return requested
    matches = [
        port.device
        for port in list_ports.comports()
        if port.vid == ESPRESSIF_VID and port.pid == USB_SERIAL_JTAG_PID
    ]
    if len(matches) == 1:
        return matches[0]
    if not matches:
        raise RuntimeError("No ESP32-S3 USB Serial/JTAG device found")
    raise RuntimeError(
        "Multiple ESP32-S3 devices found; pass --port followed by the device"
    )


def open_serial(port: str) -> serial.Serial:
    connection = serial.Serial(baudrate=115200, timeout=0.05)
    connection.dtr = False
    connection.rts = False
    connection.port = port
    connection.open()
    connection.reset_input_buffer()
    return connection


def expand_frame(payload: bytes) -> bytes:
    return b"".join(PIXELS[value] for value in payload)


def launch_ffplay(scale: int) -> subprocess.Popen[bytes]:
    executable = shutil.which("ffplay")
    if executable is None:
        raise RuntimeError("ffplay was not found; install ffmpeg first")
    return subprocess.Popen(
        [
            executable,
            "-loglevel",
            "warning",
            "-fflags",
            "nobuffer",
            "-flags",
            "low_delay",
            "-f",
            "rawvideo",
            "-pixel_format",
            "gray",
            "-video_size",
            f"{WIDTH}x{HEIGHT}",
            "-framerate",
            "20",
            "-vf",
            f"scale={WIDTH * scale}:{HEIGHT * scale}:flags=neighbor",
            "-window_title",
            "m5-fumoji — Cardputer screen",
            "-i",
            "pipe:0",
        ],
        stdin=subprocess.PIPE,
    )


def save_frame(path: str, payload: bytes) -> None:
    with open(path, "wb") as output:
        output.write(f"P5\n{WIDTH} {HEIGHT}\n255\n".encode())
        output.write(expand_frame(payload))


def run(
    port: str,
    scale: int,
    no_display: bool = False,
    max_frames: int = 0,
    output: str | None = None,
) -> int:
    connection = open_serial(port)
    player = None if no_display else launch_ffplay(scale)
    buffer = bytearray()
    last_ping = 0.0
    frames = 0
    if no_display:
        print(f"Checking framebuffer stream on {port}")
    else:
        print(f"Viewing {port}; press Ctrl+C or close the window to stop")
    try:
        connection.write(b"FMJSTREAM ON\n")
        while player is None or player.poll() is None:
            now = time.monotonic()
            if now - last_ping >= 1.0:
                connection.write(b"FMJSTREAM PING\n")
                last_ping = now
            chunk = connection.read(4096)
            if chunk:
                buffer.extend(chunk)
            marker = buffer.find(MAGIC)
            if marker < 0:
                if len(buffer) > len(MAGIC) - 1:
                    del buffer[: -(len(MAGIC) - 1)]
                continue
            if marker:
                del buffer[:marker]
            if len(buffer) < HEADER.size:
                continue
            magic, version, _flags, sequence, length, checksum = HEADER.unpack(
                buffer[: HEADER.size]
            )
            if magic != MAGIC or version != 1 or length != PAYLOAD_SIZE:
                del buffer[0]
                continue
            frame_end = HEADER.size + length
            if len(buffer) < frame_end:
                continue
            payload = bytes(buffer[HEADER.size:frame_end])
            del buffer[:frame_end]
            if zlib.crc32(payload) & 0xFFFFFFFF != checksum:
                print(f"Dropped corrupt frame {sequence}", file=sys.stderr)
                continue
            if player is not None:
                assert player.stdin is not None
                player.stdin.write(expand_frame(payload))
                player.stdin.flush()
            if output is not None:
                save_frame(output.format(sequence=sequence), payload)
            frames += 1
            if max_frames and frames >= max_frames:
                break
    except (BrokenPipeError, KeyboardInterrupt):
        pass
    finally:
        try:
            connection.write(b"FMJSTREAM OFF\n")
        except serial.SerialException:
            pass
        connection.close()
        if player is not None and player.poll() is None:
            player.terminate()
            player.wait(timeout=3)
    print(f"Stopped after {frames} frames")
    return 0


def self_test() -> int:
    source = bytes(range(256)) * (PAYLOAD_SIZE // 256) + bytes(
        range(PAYLOAD_SIZE % 256)
    )
    assert len(source) == PAYLOAD_SIZE
    expanded = expand_frame(source)
    assert len(expanded) == WIDTH * HEIGHT
    assert expanded[:8] == b"\xff" * 8
    assert expanded[8:16] == b"\xff" * 7 + b"\x00"
    packed = HEADER.pack(MAGIC, 1, 0, 42, len(source), zlib.crc32(source))
    assert HEADER.unpack(packed)[3:] == (42, PAYLOAD_SIZE, zlib.crc32(source))
    print("screen viewer protocol test passed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="serial device; auto-detected by default")
    parser.add_argument("--scale", type=int, default=5, choices=range(1, 9))
    parser.add_argument(
        "--no-display", action="store_true", help="validate frames without ffplay"
    )
    parser.add_argument(
        "--frames",
        type=int,
        default=0,
        help="exit after this many valid frames (0 keeps running)",
    )
    parser.add_argument(
        "--output",
        help=(
            "save received frames as binary PGM; include {sequence} in the "
            "path to retain every frame"
        ),
    )
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()
    if arguments.self_test:
        return self_test()
    try:
        if arguments.frames < 0:
            raise RuntimeError("--frames cannot be negative")
        return run(
            find_port(arguments.port),
            arguments.scale,
            arguments.no_display,
            arguments.frames,
            arguments.output,
        )
    except (RuntimeError, serial.SerialException) as error:
        print(f"screen viewer: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
