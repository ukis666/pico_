# Pico 2 DFPlayer + RDM6300 RFID Jukebox (C++)

A minimal Raspberry Pi Pico 2 (RP2350) project built with the official Pico SDK and CMake. It uses a DFPlayer Mini for audio playback and an RDM6300 (HW-205) RFID reader to trigger tracks.

## Hardware overview

### Pinout (Pico 2)

| Function | Pico GPIO | Pico Pin | Notes |
| --- | --- | --- | --- |
| DFPlayer TX <- Pico | GPIO4 | Pin 6 | UART1 TX to DFPlayer RX |
| DFPlayer RX -> Pico | GPIO5 | Pin 7 | UART1 RX from DFPlayer TX (optional, code does not depend on it) |
| RFID TX -> Pico | GPIO16 | Pin 21 | UART0 RX from RDM6300 TX (moved here to avoid conflict) |
| RFID RX <- Pico | GPIO17 | Pin 22 | UART0 TX to RDM6300 RX (often unused) |
| Button | GPIO14 | Pin 19 | Active-low to GND (Pin 18); uses internal pull-up |
| DFPlayer VCC | External 5V | — | Regulated 5V supply (not from Pico) |
| RDM6300 VCC | External 5V | — | Regulated 5V supply (not from Pico) |
| Ground | GND | Any | Common ground |

> **Important wiring correction**: The RDM6300 was originally noted as using pins 6/7 (GPIO4/5), but those pins are reserved for the DFPlayer on UART1. The RFID reader must be moved to GPIO16/17 on UART0. Update the wiring accordingly before powering the board.

**Power Architecture (critical):**

- DFPlayer Mini and RDM6300 modules **must be powered from an external, regulated 5V supply**. The Raspberry Pi Pico does **not** provide 5V rail current for these modules.
- Do **not** connect the modules' 5V lines to Pico VBUS, VSYS, or any GPIO pin. Those pins cannot safely source the required current and are not a substitute for a regulated supply.
- Power the Pico separately (USB or its own supply). Tie all grounds together to provide a common reference between the Pico UART signals and the modules.

**5V logic-level note:** The RDM6300 TX line is 5V. Protect the Pico RX (GPIO16) with a resistor divider or level shifter.

## SD card layout for DFPlayer

Place tracks on the DFPlayer SD card using one of these layouts:

- Preferred: `/mp3/0001.mp3` and `/mp3/0002.mp3`
- Fallback: `/01/001.mp3` and `/01/002.mp3`

## Behavior

- On boot: initializes DFPlayer (reset, set SD as source, sets volume to 18) and waits. No autoplay.
- RFID card handling:
  - Recognized IDs play immediately, restarting the track if already playing.
  - Supported decimal IDs: `7721066` → track 1, `7942286` → track 2.
  - Duplicate reads of the same card within ~500 ms are ignored to prevent spamming.
  - RDM6300 frames are validated (checksum) and both the raw 40-bit value and the facility/card composite `(facility << 16) | card` are computed. Either decimal ID can be used for mapping.
- Button (GPIO14, active-low):
  - Press toggles play/stop.
  - If stopped with a track selected, pressing plays that track from the start.
- Debounce: ≥30 ms on the button.

## Observing card IDs over USB serial

1. Connect the Pico 2 via USB. The firmware uses USB stdio (no UART stdio).
2. Open a terminal at 115200 baud (e.g., `minicom -b 115200 -o -D /dev/ttyACM0`).
3. Present a card. The console prints the raw 10-digit hex (`raw=0x...`), the `direct` decimal ID (last four data bytes), and the `facility_card` decimal ID. Add new mappings in `src/main.cpp` (`CARD_MAP`).

## Building on Linux

Prerequisites:

- CMake ≥3.13
- `gcc-arm-none-eabi` toolchain
- `build-essential`, `cmake`, `git`
- Pico SDK (either exported via `PICO_SDK_PATH` or fetched automatically)

### Configure and build

```bash
# From repo root
export PICO_SDK_PATH=/path/to/pico-sdk   # optional; otherwise fetched automatically
cmake -S . -B build -DPICO_BOARD=pico2
cmake --build build
```

The UF2 file will be in `build/rfid_dfplayer.uf2`.

### Flash

1. Hold BOOTSEL on the Pico 2 and plug in USB to enter mass-storage mode (drive label often contains `RP2350`).
2. Copy `build/rfid_dfplayer.uf2` to the mounted drive.
3. After reset, open the USB serial console to observe logs.

## Known caveats

- DFPlayer TX (to Pico) is optional; the code only transmits commands.
- The RDM6300 TX pin outputs 5V logic. Use a level shifter or resistor divider before connecting to GPIO16.
- Ensure the RFID reader is **moved to GPIO16/17 (UART0)**; GPIO4/5 are reserved for DFPlayer UART1.

## Repository layout

- `CMakeLists.txt` — project definition
- `pico_sdk_import.cmake` — helper to locate or fetch the Pico SDK
- `src/main.cpp` — application logic (DFPlayer protocol, RDM6300 parser, debounce)
- `.gitignore` — build products
- `README.md` — wiring, usage, build and flashing instructions
