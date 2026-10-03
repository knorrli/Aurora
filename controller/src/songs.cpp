#include "songs.h"

#include <Arduino.h>
#include <EEPROM.h>

#include "recall.h"

namespace {

const uint8_t STORE_FORMAT = 1;
const int FORMAT_ADDRESS = 0;
const int SONGS_ADDRESS = 1;

songs::Song stored[AURORA_SONGS];
songs::Song staged[AURORA_SONGS];
bool staging = false;

uint8_t reply[AURORA_SYSEX_FRAME_LENGTH + 2];

void ack(uint8_t inReplyTo, uint8_t status) {
    reply[0] = 0xF0;
    reply[1] = AURORA_SYSEX_ID;
    reply[2] = AURORA_SYSEX_SIGNATURE_A;
    reply[3] = AURORA_SYSEX_SIGNATURE_B;
    reply[4] = SYSEX_ACK;
    reply[5] = inReplyTo;
    reply[6] = status;
    reply[7] = 0xF7;
    usbMIDI.sendSysEx(sizeof(reply), reply, true);
}

void forget(songs::Song *list) {
    memset(list, 0, sizeof(songs::Song) * AURORA_SONGS);
}

bool isPick(uint8_t pick) { return pick < AURORA_ONESHOTS || pick == AURORA_NO_ONESHOT; }

bool isSectionSlot(uint8_t slot) { return slot <= AURORA_LAST_PATCH_SLOT; }

uint8_t stage(const uint8_t *payload, uint16_t length) {
    if (!staging) return SYSEX_ERROR_SEQUENCE;
    if (length < AURORA_SONG_HEAD_LENGTH) return SYSEX_ERROR_RANGE;
    const uint8_t place = payload[0];
    const uint8_t sections = payload[3];
    if (place >= AURORA_SONGS || sections > AURORA_SONG_SECTIONS) return SYSEX_ERROR_RANGE;
    if (length != AURORA_SONG_HEAD_LENGTH + sections) return SYSEX_ERROR_RANGE;
    if (!isPick(payload[1]) || !isPick(payload[2])) return SYSEX_ERROR_RANGE;
    songs::Song &song = staged[place];
    song.picks[0] = payload[1];
    song.picks[1] = payload[2];
    song.sections = sections;
    for (uint8_t section = 0; section < sections; section++) {
        const uint8_t slot = payload[AURORA_SONG_HEAD_LENGTH + section];
        if (!isSectionSlot(slot)) return SYSEX_ERROR_RANGE;
        song.slots[section] = slot;
    }
    return SYSEX_OK;
}

uint8_t commit() {
    if (!staging) return SYSEX_ERROR_SEQUENCE;
    staging = false;
    memcpy(stored, staged, sizeof(stored));
    EEPROM.put(SONGS_ADDRESS, stored);
    EEPROM.update(FORMAT_ADDRESS, STORE_FORMAT);
    recall::songsReplaced();
    return SYSEX_OK;
}

}

namespace songs {

void begin() {
    if (EEPROM.read(FORMAT_ADDRESS) == STORE_FORMAT) EEPROM.get(SONGS_ADDRESS, stored);
    else forget(stored);
}

bool get(uint8_t place, Song &out) {
    if (place >= AURORA_SONGS || stored[place].sections == 0) return false;
    out = stored[place];
    return true;
}

void onSysEx(const uint8_t *data, uint16_t length, bool complete) {
    if (!complete || length < AURORA_SYSEX_FRAME_LENGTH) return;
    if (data[0] != 0xF0 || data[length - 1] != 0xF7) return;
    if (data[1] != AURORA_SYSEX_ID) return;
    if (data[2] != AURORA_SYSEX_SIGNATURE_A || data[3] != AURORA_SYSEX_SIGNATURE_B) return;

    const uint8_t type = data[4];
    const uint8_t *payload = data + AURORA_SYSEX_HEADER_LENGTH;
    const uint16_t payloadLength = length - AURORA_SYSEX_FRAME_LENGTH;

    switch (type) {
        case SYSEX_SONGS_BEGIN:
            forget(staged);
            staging = true;
            ack(type, SYSEX_OK);
            return;

        case SYSEX_SONG: {
            const uint8_t status = stage(payload, payloadLength);
            if (status == SYSEX_OK) return;
            staging = false;
            ack(type, status);
            return;
        }

        case SYSEX_SONGS_COMMIT:
            ack(type, commit());
            return;

        default:
            return;
    }
}

}
