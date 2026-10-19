# [chiaki-ng](https://streetpea.github.io/chiaki-ng/) for Wii u (by MysticFlipFlop)

<img width="100" height="100" alt="ppic" src="https://github.com/user-attachments/assets/dab711e4-2a5b-4183-8e0e-25528a574615" />



Full port of **[chiaki-ng](https://github.com/chiaki-ng/chiaki-ng)** to the **Nintendo Wii U**. Receives video and audio and sends your PS4's controls to the TV **and** the GamePad.

- Simultaneous output: TV and GamePad
- Installs to the SD card and launches from **HBL, Aroma, or Tiramisu** (`.rpx` + `.wuhb`)
- Custom GX2 renderer without SDL, **zero-copy NV12** video path, no downloads or HTTP at runtime
- Validated on real hardware (Cemu doesnt work for some reason) — latest build: `BUILD wiiu-2026-10-16 gui-80`

Sorry for... the interface being in spanish, there are not too much words btw. And i don´t know how to make compiling tutorials :(

## Features

### Video

- Decoding and presentation through the GX2 **zero-copy NV12** path
- **9 quality presets** (more low-end options, since its screen is 854×480) (recommended preset by me: 540p60fps):

| Preset | Resolution | Bitrate |
|---|---|---|
| `720p60 5M` | 1280×720 | 5 Mbps |
| `720p60 4M` | 1280×720 | 4 Mbps |
| `720p60 3M` | 1280×720 | 3 Mbps |
| `720p60 2.5M` | 1280×720 | 2.5 Mbps |
| `720p60 2M` | 1280×720 | 2 Mbps |
| `720p60 1.5M` | 1280×720 | 1.5 Mbps *(Moonlight-style starting point)* |
| `540p60 5M` | 960×540 | 5 Mbps *(maximum sharpness on the physical screen)* |
| `540p60 3M` | 960×540 | 3 Mbps *(lighter frame)* |
| `540p60 2M` | 960×540 | 2 Mbps *(emergency)* |

- **Optional VSync**

### Audio and microphone

- Remote Opus audio played through the native **AX voice at 48 kHz** (system mix)
- **GamePad microphone** (`mic.rpl`): encoded with Opus and sent via takion with `connect_microphone`
- Every session starts **muted**; the icon in the bottom-left corner (Gamepad only) toggles mute with a tap
- **Microphone gain from 0 to 200%** in 25% steps: a long press (~0.4 s) on the icon opens a vertical slider with the percentage on the right; it is saved to the configuration
- Session icons (stop and microphone) are drawn **only on the GamePad**, never on the TV

### Controls

The GamePad presents itself to the PS4 as a DualShock 4 with a **physical-position layout** (no touch controls on the console that don't exist on the original controller):

| GamePad Button | DualShock 4 |
|---|---|
| A | Circle ○ |
| B | Cross × |
| X | Triangle △ |
| Y | Square □ |
| ← ↑ → ↓ | D-pad |
| L / R | L1 / R1 |
| ZL / ZR | L2 / R2 |
| MINUS | PS Button |
| PLUS | Options |
| Left/Right sticks | Analog sticks |

- **A** connects Remote Play (wakes the PS4 if it is in standby); **B** cancels while connecting
- With the stream active, all keys go to the game; the session stops by tapping the power icon

### GamePad Touchpad

- **Dedicated touch area** (top-right corner of the DRC): drag = slide the PS4 cursor, short tap = touchpad click
- **Faint silver surface** at rest; brightness follows actual contact without lag
- **Double tap = zoom** of the image with a smooth transition (also during the stream)
- **Light worm**-style visual feedback: pure white trail with a flash on touch, dissolving in ~0.4 s when the finger is lifted
- No *pinch* emulation because the gamepad isn´t multitouch

### Interface (main menu)

- Top bar with shortcuts and their key chips: **PIN (↑)**, **ID (SELECT)**, **IP (X)**, **Search (Y)**
- Touch gear button at the bottom left (translucent circle like the stream buttons) that **opens Settings**, same as pressing ↓
- Status: *Console detected - press ↑ to register*, *Searching for consoles on the network...*, last session message, and build label in the footer

### Settings

- Rows: **PSN ID**, **IP**, **Quality**, **VSync** (checkbox), and **Remove console**; all touchable on the Gamepad
- **Remove console**: modal dialog without spurious navigation — only **[A] Remove** and **[B] back**
- **SELECT** (outside a session) edits the Account ID at any time

### Registration and connection

- **Automatic discovery** via LAN broadcast; **Y** re-sends the broadcast if the console doesn't appear; **X** lets you type the IP manually
- Registration with the Wii U's **system keyboard**: PSN account and the **8-digit PIN** shown by the PS4 (*Settings → Link with another device*)
- **PSN Account ID in base64 format** (e.g. `wLEMeFoyimY=`), entered manually — copy it directly from services like [this page that finds your account id for Chiaki](https://www.psntools.com/psn/checker/)

### Files, log, and configuration

| Path (SD) | Contents |
|---|---|
| `sd:/wiiu/apps/chiaki/log.txt` | Full app and chiaki-lib log (INFO/WARNING/ERROR) |
| `sd:/wiiu/apps/chiaki/config.ini` | Account ID, last console, quality preset, VSync, and microphone gain |

- WHB crash handler: panic, registers, and backtrace are dumped to the SD log
- Clean exit with **HOME**
- I recommend that you have an ftp or file explorer on the Wii u to delete the log because it can get too heavy. I will fix that later.

## Installation

1. Copy `chiaki-wiiu.wuhb` to `sd:/wiiu/apps/` (for HBL, use the `chiaki-wiiu.rpx`) and launch it from the homebrew launcher (Aroma / Tiramisu / HBL).
2. Wii U and PS4 on the same network — **PS4 over wired Ethernet** recommended and Wii U with Ethernet adapter (I suppose, I don't have one)
3. Run the app; it creates `sd:/wiiu/apps/chiaki/` with its log and configuration.

### First steps

1. **SELECT** → enter your **base64 Account ID** (paste it from the link above).
2. **↑** → register the console: the system keyboard will ask for the Account ID and the **8-digit PIN** shown by the PS4 in *Settings → Link with another device*.
3. The console appears in the list; **A** connects.

## Building from source

Requirements: [devkitPro](https://devkitpro.org/) with **devkitPPC**, **wut**, **portlibs/wiiu**, and the CMake toolchain (`cmake/WiiU.cmake`), plus CMake and a host C/C++ compiler.

```sh
in progress, sorry
```
On Windows the toolchain path is `C:\devkitPro\cmake\WiiU.cmake`. The submodules (`nanopb`, `jerasure`, `gf-complete`, `munit`) are resolved with `--recursive`.

**Output:** `build-wiiu/wiiu/chiaki-wiiu.rpx` and `build-wiiu/wiiu/chiaki-wiiu.wuhb`.

## Limitations and design decisions

- **No SHARE button**: left out on purpose, i hate that button
- **No *pinch-zoom***: double-tap zoom is available, pinch is not.
- **No gyroscope/motion**: it was implemented, tested on hardware, but i don't know how to translate gamepad gyro inputs to ds4 gyro inputs. The port does not send motion data. (it can be implemented in future updates)
- Wi-Fi only: the Wii U has no Ethernet port of its own.
- The Account ID is entered manually (base64) — there is no PSN login in the app.

## Credits and license

- Based on **[chiaki-ng](https://github.com/chiaki-ng/chiaki-ng)** and the original **chiaki** by [thestr4ng3r](https://github.com/thestr4ng3r/chiaki) and contributors. Without them this port wouldn't exist.
- **GNU AGPL v3** license — same as chiaki-ng; see [`COPYING`](COPYING) and [`LICENSES/`](LICENSES).
- Dependencies with their own licenses: FFmpeg, Opus, nanopb, Jerasure, gf-complete, etc. (submodules and portlibs).




## Disclaimer
This project is not endorsed or certified by Sony Interactive Entertainment LLC.

Chiaki is a Free and Open Source Software Client for PlayStation 4 and PlayStation 5 Remote Play
for Linux, FreeBSD, OpenBSD, Android, macOS, Windows, Nintendo Switch and potentially even more platforms.
