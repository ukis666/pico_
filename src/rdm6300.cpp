#include "rdm6300.h"

#include "constants.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

void Rdm6300::init()
{
    uart_init(RFID_UART, RFID_BAUD);
    uart_set_format(RFID_UART, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(RFID_UART, false, false);
    uart_set_fifo_enabled(RFID_UART, false);
    gpio_set_function(RFID_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(RFID_RX_PIN, GPIO_FUNC_UART);
}

bool Rdm6300::poll(RdmCard &out_card)
{
    while (uart_is_readable(RFID_UART))
    {
        uint8_t byte = uart_getc(RFID_UART);
        if (processByte(byte, out_card))
        {
            return true;
        }
    }
    return false;
}

int Rdm6300::hexToNibble(uint8_t c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    return -1;
}

bool Rdm6300::processByte(uint8_t byte, RdmCard &out_card)
{
    // Ignore CR/LF anywhere
    if (byte == '\r' || byte == '\n')
    {
        return false;
    }

    if (!in_frame_)
    {
        if (byte == 0x02)
        {
            index_ = 0;
            in_frame_ = true;
        }
        return false;
    }

    if (byte == 0x03)
    {
        in_frame_ = false;
        if (index_ == 12 && parseFrame(out_card))
        {
            return true;
        }
        index_ = 0;
        return false;
    }

    if (index_ < static_cast<int>(buffer_.size()))
    {
        buffer_[index_++] = static_cast<char>(byte);
    }
    else
    {
        // Overflow, reset
        in_frame_ = false;
        index_ = 0;
    }
    return false;
}

bool Rdm6300::parseFrame(RdmCard &out_card)
{
    // buffer_: [0..9] data hex, [10..11] checksum hex
    for (int i = 0; i < 10; ++i)
    {
        out_card.raw_hex_string[i] = buffer_[i];
    }
    out_card.raw_hex_string[10] = '\0';

    uint64_t raw_value = 0;
    for (int i = 0; i < 10; ++i)
    {
        int nibble = hexToNibble(buffer_[i]);
        if (nibble < 0)
        {
            return false;
        }
        raw_value = (raw_value << 4) | static_cast<uint64_t>(nibble);
    }
    out_card.raw_hex_value = raw_value;

    for (int i = 0; i < 5; ++i)
    {
        int hi = hexToNibble(buffer_[2 * i]);
        int lo = hexToNibble(buffer_[2 * i + 1]);
        if (hi < 0 || lo < 0)
        {
            return false;
        }
        out_card.data[i] = static_cast<uint8_t>((hi << 4) | lo);
    }

    int chk_hi = hexToNibble(buffer_[10]);
    int chk_lo = hexToNibble(buffer_[11]);
    if (chk_hi < 0 || chk_lo < 0)
    {
        return false;
    }
    out_card.checksum = static_cast<uint8_t>((chk_hi << 4) | chk_lo);

    uint8_t xor_sum = 0;
    for (auto b : out_card.data)
    {
        xor_sum ^= b;
    }
    out_card.computed_checksum = xor_sum;
    if (xor_sum != out_card.checksum)
    {
        return false;
    }

    // The RDM6300's printed decimal IDs often correspond to the last four bytes.
    out_card.direct_id = (static_cast<uint32_t>(out_card.data[1]) << 24) |
                         (static_cast<uint32_t>(out_card.data[2]) << 16) |
                         (static_cast<uint32_t>(out_card.data[3]) << 8) |
                         static_cast<uint32_t>(out_card.data[4]);

    uint16_t facility = (static_cast<uint16_t>(out_card.data[1]) << 8) | out_card.data[2];
    uint16_t card = (static_cast<uint16_t>(out_card.data[3]) << 8) | out_card.data[4];
    out_card.facility_card_id = (static_cast<uint32_t>(facility) << 16) | card;
    return true;
}
