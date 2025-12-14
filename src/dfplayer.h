#pragma once

#include <cstdint>

class DFPlayer
{
public:
    void init();
    void playTrack(uint16_t track);
    void stop();

private:
    void reset();
    void selectSd();
    void setVolume(uint8_t volume);
    void sendCommand(uint8_t command, uint16_t parameter);
};
