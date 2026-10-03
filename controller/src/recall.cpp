#include "recall.h"

#include "aurora_protocol.h"
#include "midi_link.h"
#include "songs.h"

namespace {

enum Holder : uint8_t { NOBODY, KEYPAD, PEDAL };

const uint8_t NO_SONG = 0xFF;
const uint8_t PLACES = 2;

uint8_t bank = 1;
uint8_t key = 0;
uint8_t songPlace = NO_SONG;
uint8_t section = 0;
songs::Song song;
Holder holder = NOBODY;
uint8_t firedNotes[PLACES];

uint8_t slotOf(uint8_t inBank, uint8_t onKey) {
    return (uint8_t)((inBank - 1) * AURORA_KEYPAD_KEYS + onKey);
}

void hold(Holder by, uint8_t program) {
    if (holder != NOBODY) midi_link::sendNoteOff(NOTE_KEY_HELD);
    holder = by;
    midi_link::sendNoteOn(NOTE_KEY_HELD, recall::FULL_STRENGTH);
    midi_link::sendProgram(program);
}

void letGo(Holder by) {
    if (holder != by) return;
    holder = NOBODY;
    midi_link::sendNoteOff(NOTE_KEY_HELD);
}

void followSlot(uint8_t slot) {
    if (!aurora_is_patch_slot(slot)) return;
    key = (uint8_t)((slot - 1) % AURORA_KEYPAD_KEYS + 1);
}

bool songLoaded() { return songPlace != NO_SONG; }

uint8_t oneshotNote(uint8_t place) {
    if (songLoaded() && song.picks[place] < AURORA_ONESHOTS) return (uint8_t)(NOTE_ONESHOT_FIRST + song.picks[place]);
    return (uint8_t)(NOTE_PATCH_ONESHOT_FIRST + place);
}

void stepSection(int8_t direction) {
    const int16_t wanted = (int16_t)section + direction;
    if (wanted < 0 || wanted >= song.sections) return;
    section = (uint8_t)wanted;
    const uint8_t slot = song.slots[section];
    if (!aurora_is_patch_slot(slot)) return;
    followSlot(slot);
    hold(PEDAL, slot);
}

void stepKey(int8_t direction) {
    const int16_t wanted = key == 0 ? 1 : (int16_t)key + direction;
    if (wanted < 1 || wanted > AURORA_KEYPAD_KEYS) return;
    key = (uint8_t)wanted;
    hold(PEDAL, slotOf(bank, key));
}

void loadSong(uint8_t place) {
    songs::Song loaded;
    if (!songs::get(place, loaded)) return;
    song = loaded;
    songPlace = place;
    section = 0;
    const uint8_t slot = song.slots[0];
    if (!aurora_is_patch_slot(slot)) return;
    followSlot(slot);
    midi_link::sendProgram(slot);
}

}

namespace recall {

void setBank(uint8_t newBank) { bank = newBank; }

void keyDown(uint8_t pressed) {
    if (pressed == 0) {
        hold(KEYPAD, PROGRAM_BLACKOUT);
        return;
    }
    key = pressed;
    hold(KEYPAD, slotOf(bank, key));
}

void keyUp() { letGo(KEYPAD); }

void stepDown(int8_t direction) {
    if (songLoaded()) stepSection(direction);
    else stepKey(direction);
}

void stepUp() { letGo(PEDAL); }

void oneshotDown(uint8_t place, uint8_t velocity) {
    firedNotes[place] = oneshotNote(place);
    midi_link::sendNoteOn(firedNotes[place], velocity);
}

void oneshotUp(uint8_t place) { midi_link::sendNoteOff(firedNotes[place]); }

void forwardProgram(uint8_t program) {
    if (program >= PROGRAM_SONG_FIRST) {
        loadSong((uint8_t)(program - PROGRAM_SONG_FIRST));
        return;
    }
    followSlot(program);
    midi_link::sendProgram(program);
}

void songsReplaced() { songPlace = NO_SONG; }

}
