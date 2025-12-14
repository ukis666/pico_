#pragma once

#include <array>
#include <cstdint>

#include "hardware/uart.h"

struct CardMapping
{
    uint32_t id;
    uint16_t track;
};

inline const uart_inst_t *DFPLAYER_UART = uart1;
inline const uint DFPLAYER_TX_PIN = 4;  // Pico GPIO4 -> DFPlayer RX
inline const uint DFPLAYER_RX_PIN = 5;  // Pico GPIO5 <- DFPlayer TX (optional)

inline const uart_inst_t *RFID_UART = uart0;
inline const uint RFID_TX_PIN = 16;  // RDM6300 TX -> Pico RX (UART0)
inline const uint RFID_RX_PIN = 17;  // Pico TX -> RDM6300 RX (often unused)

inline const uint BUTTON_PIN = 14;  // Active-low button to GND

inline const uint32_t DFPLAYER_BAUD = 9600;
inline const uint32_t RFID_BAUD = 9600;

inline const uint16_t DEFAULT_VOLUME = 18;  // 0-30

inline const uint64_t BUTTON_DEBOUNCE_US = 30'000;
inline const uint64_t CARD_COOLDOWN_US = 500'000;

inline const std::array<CardMapping, 2> CARD_MAP = {{{7721066, 1}, {7942286, 2}}};
