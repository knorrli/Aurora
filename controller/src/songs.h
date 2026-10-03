#pragma once

#include <stdint.h>

#include "aurora_protocol.h"

namespace songs {

struct Song {
    uint8_t sections;
    uint8_t picks[2];
    uint8_t slots[AURORA_SONG_SECTIONS];
};

void begin();
bool get(uint8_t place, Song &out);
void onSysEx(const uint8_t *data, uint16_t length, bool complete);

}
