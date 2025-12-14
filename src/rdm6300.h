#pragma once

#include <array>
#include <cstdint>

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

class Rdm6300
{
public:
    void init();
    bool poll(RdmCard &out_card);

private:
    static int hexToNibble(uint8_t c);
    bool processByte(uint8_t byte, RdmCard &out_card);
    bool parseFrame(RdmCard &out_card);

    std::array<char, 12> buffer_{};
    int index_ = 0;
    bool in_frame_ = false;
};
