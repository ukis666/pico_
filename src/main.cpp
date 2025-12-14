#include <cstdio>

#include "button.h"
#include "constants.h"
#include "dfplayer.h"
#include "pico/stdlib.h"
#include "rdm6300.h"

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
