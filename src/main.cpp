#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

namespace
{
uart_inst_t *const DFPLAYER_UART = uart1;
constexpr uint DFPLAYER_TX_PIN = 4;  // Pico GPIO4 -> DFPlayer RX
constexpr uint DFPLAYER_RX_PIN = 5;  // Pico GPIO5 <- DFPlayer TX (optional)

uart_inst_t *const RFID_UART = uart0;
constexpr uint RFID_TX_PIN = 16;  // Pico GPIO16 (UART0 TX) -> RDM6300 RX (optional)
constexpr uint RFID_RX_PIN = 17;  // RDM6300 TX -> Pico GPIO17 (UART0 RX)

constexpr uint BUTTON_PIN = 14;  // Active-low button to GND

constexpr uint32_t DFPLAYER_BAUD = 9600;
constexpr uint32_t RFID_BAUD = 9600;

constexpr uint16_t DEFAULT_VOLUME = 18;  // 0-30

constexpr uint64_t BUTTON_DEBOUNCE_US = 30'000;
constexpr uint64_t CARD_COOLDOWN_US = 500'000;

struct CardMapping
{
    uint32_t id;
    uint16_t track;
};

constexpr std::array<CardMapping, 2> CARD_MAP = {{{7721066, 1}, {7942286, 2}}};

struct RdmCard
{
    uint64_t raw_hex_value = 0;        // 40-bit value from the 10 hex digits
    std::array<uint8_t, 5> data{};     // Decoded data bytes
    uint8_t checksum = 0;              // Provided checksum byte
    uint8_t computed_checksum = 0;     // XOR of the five data bytes
    uint32_t direct_id = 0;            // Convenience: last 4 data bytes as a decimal ID
    uint32_t facility_card_id = 0;     // facility << 16 | card
    char raw_hex_string[11] = {0};
};

class DFPlayer
{
public:
    void init()
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

    void playTrack(uint16_t track)
    {
        sendCommand(0x12, track);
    }

    void stop()
    {
        sendCommand(0x16, 0x0000);
    }

private:
    void reset()
    {
        sendCommand(0x0C, 0x0000);
    }

    void selectSd()
    {
        // CMD 0x09, parameter 0x0002 selects TF/SD
        sendCommand(0x09, 0x0002);
    }

    void setVolume(uint8_t volume)
    {
        uint8_t clamped = volume > 30 ? 30 : volume;
        sendCommand(0x06, clamped);
    }

    void sendCommand(uint8_t command, uint16_t parameter)
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
};

class Button
{
public:
    void init()
    {
        gpio_init(BUTTON_PIN);
        gpio_set_dir(BUTTON_PIN, GPIO_IN);
        gpio_pull_up(BUTTON_PIN);
        last_state_ = gpio_get(BUTTON_PIN);
        stable_state_ = last_state_;
        last_change_ = get_absolute_time();
    }

    bool update()
    {
        bool raw_state = gpio_get(BUTTON_PIN);
        absolute_time_t now = get_absolute_time();

        if (raw_state != last_state_)
        {
            last_state_ = raw_state;
            last_change_ = now;
        }

        if ((absolute_time_diff_us(last_change_, now) >= BUTTON_DEBOUNCE_US) && (stable_state_ != raw_state))
        {
            stable_state_ = raw_state;
            if (!stable_state_)
            {
                return true; // active low
            }
        }
        return false;
    }

private:
    bool last_state_ = true;
    bool stable_state_ = true;
    absolute_time_t last_change_{};
};

class Rdm6300
{
public:
    void init()
    {
        uart_init(RFID_UART, RFID_BAUD);
        uart_set_format(RFID_UART, 8, 1, UART_PARITY_NONE);
        uart_set_hw_flow(RFID_UART, false, false);
        uart_set_fifo_enabled(RFID_UART, false);
        gpio_set_function(RFID_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(RFID_RX_PIN, GPIO_FUNC_UART);
    }

    bool poll(RdmCard &out_card)
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

private:
    static int hexToNibble(uint8_t c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        return -1;
    }

    bool processByte(uint8_t byte, RdmCard &out_card)
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

    bool parseFrame(RdmCard &out_card)
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

    std::array<char, 12> buffer_{};
    int index_ = 0;
    bool in_frame_ = false;
};

uint16_t mapCardToTrack(const RdmCard &card)
{
    for (const auto &m : CARD_MAP)
    {
        if (card.direct_id == m.id || card.facility_card_id == m.id)
        {
            return m.track;
        }
    }
    return 0;
}

} // namespace

int main()
{
    stdio_init_all();

    Button button;
    button.init();

    Rdm6300 rfid;
    rfid.init();

    DFPlayer player;
    player.init();

    bool playing = false;
    uint16_t last_track = 0;

    absolute_time_t last_card_time = get_absolute_time();
    uint32_t last_card_id = 0;

    printf("RFID jukebox ready.\n");

    while (true)
    {
        // Handle RFID reader
        RdmCard card;
        if (rfid.poll(card))
        {
            absolute_time_t now = get_absolute_time();
            if (!last_card_id || card.direct_id != last_card_id ||
                (absolute_time_diff_us(last_card_time, now) >= CARD_COOLDOWN_US))
            {
                last_card_time = now;
                last_card_id = card.direct_id;
                uint16_t track = mapCardToTrack(card);
                printf("Card raw=0x%s direct=%u facility_card=%u\n", card.raw_hex_string, card.direct_id, card.facility_card_id);
                if (track > 0)
                {
                    last_track = track;
                    player.playTrack(track);
                    playing = true;
                    printf("Playing track %u\n", track);
                }
                else
                {
                    printf("Card not mapped.\n");
                }
            }
        }

        // Handle button press
        if (button.update())
        {
            if (playing)
            {
                player.stop();
                playing = false;
                printf("Playback stopped by button.\n");
            }
            else if (last_track > 0)
            {
                player.playTrack(last_track);
                playing = true;
                printf("Resuming track %u\n", last_track);
            }
        }

        tight_loop_contents();
    }
}

