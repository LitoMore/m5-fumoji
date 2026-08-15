<h1 align="center">m5-fumoji</h1>

<p align="center">
English | <a href="README.zh.md">中文</a>
</p>

Native _Fumo Ji_ (《伏魔记》) firmware for the **M5Stack Cardputer ADV**, with a compatible build for the original Cardputer. The firmware runs a portable GPL C engine directly on the ESP32-S3; it does not use Java, WebView, Kotlin/JS, or a dynamic JavaScript runtime.

<p align="center">
<img src="media/screenshot.webp" alt="m5-fumoji on Cardputer ADV" width="480">
</p>

## Current status

The firmware now builds with the complete upstream C game engine, including its story, combat, menu, item, magic, shop, and save logic. The Cardputer port currently provides:

- the original 160×96 monochrome framebuffer, scaled to the 240×135 display;
- Cardputer ADV TCA8418 and original Cardputer keyboard support through `M5Cardputer`;
- streamed 16 KB ROM-bank reads from `/FMJ.LIB`, without loading the whole ROM into RAM;
- on-demand `/HZK16` and `/ASC16` glyph reads;
- LittleFS-backed engine save files;
- an on-demand CRC-protected USB framebuffer stream for desktop debugging;
- a 32 KB dedicated FreeRTOS engine task;
- runtime board detection, including `board_M5CardputerADV`.

The Cardputer ADV build succeeds and currently uses approximately 577 KB of the 3 MB application partition and 124 KB of static RAM. Startup, story exploration, input, menus, combat, saving, and cold-start loading have been verified on a real Cardputer ADV. The firmware is considered a complete, playable Cardputer port.

The earlier partial C++ interpreter remains in `lib/FmjCore` for host tools and regression tests, but the firmware entry point now runs `lib/FmjCEngine`.

## Build and upload

[PlatformIO](https://platformio.org/) is required. The default target is the Cardputer ADV:

```sh
pio run
pio run -e cardputer_adv -t upload
```

Install the game data partition separately:

```sh
pio run -e cardputer_adv -t uploadfs
```

For the original Cardputer:

```sh
pio run -e cardputer -t upload
pio run -e cardputer -t uploadfs
```

After importing the assets, create a single Cardputer ADV image flashed at address `0x0000`:

```sh
sh scripts/build_release.sh
```

The output is `dist/m5-fumoji-cardputer-adv.bin`. It includes the bootloader, partition table, application, and the current contents of `data/`.

## Import game data

Game assets are intentionally not committed. Import them from a local checkout of the reference project:

```sh
node scripts/import_reference_assets.mjs /path/to/baye-fmj-app
```

The resulting files must be:

```text
data/FMJ.LIB
data/HZK16
data/ASC16
```

They are uploaded to LittleFS as `/FMJ.LIB`, `/HZK16`, and `/ASC16`.

## Controls

The primary directions form a physical diamond on the Cardputer's staggered keyboard:

```text
      W
Aa    A    S
```

- `W` / `Aa` / `A` / `S`: up / left / down / right;
- `;` / `,` / `.` / `/`: alternate up / left / down / right;
- `Enter`: confirm;
- `Del` / `Backspace`: open the in-game menu, cancel, or go back;
- `Tab` or `M`: original BBK Home/Menu key (not the in-game menu in the current engine);
- `[` / `]` or `Q` / `E`: Page Up / Page Down.
- `-`: decrease display brightness by 10%;
- `=` or `+`: increase display brightness by 10%.

Brightness is adjustable from 10% to 100% and is retained after restarting.

## Test

Run the existing host regression tests:

```sh
pio test -e native
```

Build the actual ADV firmware:

```sh
pio run -e cardputer_adv
```

## USB screen viewer

With the Cardputer connected over USB, mirror its 160×96 framebuffer to a desktop window:

```sh
sh scripts/view_screen.sh
```

The viewer auto-detects an ESP32-S3 USB Serial/JTAG device and uses `ffplay` for nearest-neighbour display. Specify a port or a different integer scale when needed:

```sh
sh scripts/view_screen.sh --port /dev/cu.usbmodem2201 --scale 6
```

For a non-graphical connection/CRC health check, receive one valid frame and exit automatically:

```sh
sh scripts/view_screen.sh --no-display --frames 1
```

Streaming is disabled by default. The viewer enables it on connection, sends a keepalive once per second, and disables it when exiting; the firmware also stops automatically after 3.5 seconds without a keepalive. Do not run a serial monitor on the same port while the viewer is open.

## Upstream and license

Copyright (C) 2026 LitoMore

This project is licensed under [GNU GPL version 2 only](LICENSE) (`GPL-2.0-only`). The C engine is adapted from [`erduoniba/baye-fmj-app/Fmj/fmj_c_engine`](https://github.com/erduoniba/baye-fmj-app/tree/main/Fmj/fmj_c_engine), pinned to commit `60c41ea2d9932b295833ece7004394497610596a`. Its upstream GPL-2.0 license and exact provenance are retained in [`lib/FmjCEngine`](lib/FmjCEngine).

The original game data, fonts, title, artwork, dialogue, and story are not licensed by this repository's GPL and are not included in source control. This is an independent, unofficial port and is not affiliated with or endorsed by the game's rights holders or M5Stack.

When distributing firmware binaries, GPL-2.0 requires offering the complete corresponding source and build scripts. A full-flash image additionally embeds the files in `data/`; do not distribute such an image unless you also have permission to redistribute those assets. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
