#include "dfplayer.h"

#include "constants.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

void DFPlayer::init()
{
    uart_init(DFPLAYER_UART, DFPLAYER_BAUD);
    uart_set_format(DFPLAYER_UART, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(DFPLAYER_UART, false, false);
    uart_set_fifo_enabled(DFPLAYER_UART, false);
    gpio_set_function(DFPLAYER_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(DFPLAYER_RX_PIN, GPIO_FUNC_UART);

    // Allow the module to power up fully
    sleep_ms(1000);
    reset();
    sleep_ms(1200);
    selectSd();
    setVolume(DEFAULT_VOLUME);
}

void DFPlayer::playTrack(uint16_t track)
{
    sendCommand(0x12, track);
}

void DFPlayer::stop()
{
    sendCommand(0x16, 0x0000);
}

void DFPlayer::reset()
{
    sendCommand(0x0C, 0x0000);
}

void DFPlayer::selectSd()
{
    // CMD 0x09, parameter 0x0002 selects TF/SD
    sendCommand(0x09, 0x0002);
}

void DFPlayer::setVolume(uint8_t volume)
{
    uint8_t clamped = volume > 30 ? 30 : volume;
    sendCommand(0x06, clamped);
}

void DFPlayer::sendCommand(uint8_t command, uint16_t parameter)
{
    const uint8_t start = 0x7E;
    const uint8_t version = 0xFF;
    const uint8_t length = 0x06;
    const uint8_t feedback = 0x00;
    const uint8_t param_msb = static_cast<uint8_t>((parameter >> 8) & 0xFF);
    const uint8_t param_lsb = static_cast<uint8_t>(parameter & 0xFF);

    int16_t checksum = 0 - static_cast<int16_t>(version + length + command + feedback + param_msb + param_lsb);
    uint8_t checksum_msb = static_cast<uint8_t>((checksum >> 8) & 0xFF);
    uint8_t checksum_lsb = static_cast<uint8_t>(checksum & 0xFF);

    uint8_t frame[10] = {start,   version, length, command, feedback,
                         param_msb, param_lsb, checksum_msb, checksum_lsb, 0xEF};

    uart_write_blocking(DFPLAYER_UART, frame, sizeof(frame));
}
