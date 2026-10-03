#pragma once

#include <stdint.h>

namespace midi_link {

void begin();
void read(uint32_t micros);

void sendNoteOn(uint8_t note, uint8_t velocity);
void sendNoteOff(uint8_t note);
void sendProgram(uint8_t program);
void sendControl(uint8_t control, uint8_t value);
void sendClock();
void sendStart();
void sendContinue();
void sendStop();

}
