#include "patch_sync.h"

#include <Arduino.h>

#include "aurora_protocol.h"
#include "patch_store.h"

namespace {
const uint16_t REPLY_MAX = AURORA_SYSEX_FRAME_LENGTH + 2 + AURORA_PATCH_CC_COUNT;

uint8_t reply[REPLY_MAX];

uint8_t pendingError = SYSEX_OK;

void send(uint8_t type, const uint8_t *payload, uint16_t payloadLength) {
    reply[0] = 0xF0;
    reply[1] = AURORA_SYSEX_ID;
    reply[2] = AURORA_SYSEX_SIGNATURE_A;
    reply[3] = AURORA_SYSEX_SIGNATURE_B;
    reply[4] = type;
    for (uint16_t i = 0; i < payloadLength; i++) reply[AURORA_SYSEX_HEADER_LENGTH + i] = payload[i];
    reply[AURORA_SYSEX_HEADER_LENGTH + payloadLength] = 0xF7;

    usbMIDI.sendSysEx(AURORA_SYSEX_FRAME_LENGTH + payloadLength, reply, true);
}

void ack(uint8_t inReplyTo, uint8_t status) {
    const uint8_t payload[2] = { inReplyTo, status };
    send(SYSEX_ACK, payload, 2);
}

void fail(uint8_t type, uint8_t status) {
    if (status == SYSEX_OK) return;
    patch_store::stageAbort();
    if (pendingError != SYSEX_OK) return;
    pendingError = status;
    ack(type, status);
}

void sendLibraryInfo() {
    const uint8_t KEYS_AT = 4, MAP_AT = KEYS_AT + AURORA_KEYPAD_KEYS;
    uint8_t payload[MAP_AT + AURORA_SLOT_MAP_LENGTH];
    payload[0] = AURORA_PROTOCOL_VERSION_MAJOR;
    payload[1] = AURORA_PROTOCOL_VERSION_MINOR;
    payload[2] = AURORA_PATCH_FORMAT;
    payload[3] = (uint8_t)patch_store::state();

    const uint8_t *keys = patch_store::keymap();
    for (uint8_t i = 0; i < AURORA_KEYPAD_KEYS; i++) payload[KEYS_AT + i] = keys[i];
    const uint8_t *map = patch_store::slotMap();
    for (uint8_t i = 0; i < AURORA_SLOT_MAP_LENGTH; i++) payload[MAP_AT + i] = map[i];

    send(SYSEX_LIBRARY_INFO, payload, sizeof(payload));
}

void sendPatch(uint8_t slot) {
    uint8_t payload[2 + AURORA_PATCH_CC_COUNT];

    payload[0] = slot;
    if (!patch_store::readHead(slot, payload + 1)) {
        ack(SYSEX_QUERY_PATCH, SYSEX_ERROR_RANGE);
        return;
    }
    send(SYSEX_PATCH_HEAD_OUT, payload, 1 + AURORA_PATCH_HEAD_LENGTH);

    for (uint8_t part = 0; part < AURORA_PATCH_PARTS; part++) {
        payload[0] = slot;
        payload[1] = part;
        if (!patch_store::readPart(slot, part, payload + 2)) {
            ack(SYSEX_QUERY_PATCH, SYSEX_ERROR_STORAGE);
            return;
        }
        send(SYSEX_PATCH_PART_OUT, payload, 2 + AURORA_PATCH_CC_COUNT);
    }
}

}

namespace patch_sync {

void onSysEx(const uint8_t *data, uint16_t length, bool complete) {
    if (!complete) return;

    if (length < AURORA_SYSEX_FRAME_LENGTH) return;
    if (data[0] != 0xF0 || data[length - 1] != 0xF7) return;
    if (data[1] != AURORA_SYSEX_ID) return;
    if (data[2] != AURORA_SYSEX_SIGNATURE_A || data[3] != AURORA_SYSEX_SIGNATURE_B) return;

    const uint8_t   type       = data[4];
    const uint8_t  *payload    = data + AURORA_SYSEX_HEADER_LENGTH;
    const uint16_t  payloadLength = length - AURORA_SYSEX_FRAME_LENGTH;

    switch (type) {
        case SYSEX_SYNC_BEGIN: {
            if (payloadLength != 1 + AURORA_KEYPAD_KEYS + AURORA_SLOT_MAP_LENGTH) {
                ack(type, SYSEX_ERROR_RANGE);
                return;
            }
            pendingError = SYSEX_OK;
            ack(type, patch_store::stageBegin(payload[0], payload + 1,
                                              payload + 1 + AURORA_KEYPAD_KEYS));
            return;
        }

        case SYSEX_PATCH_HEAD: {
            if (payloadLength != 1 + AURORA_PATCH_HEAD_LENGTH) {
                fail(type, SYSEX_ERROR_RANGE);
                return;
            }
            fail(type, patch_store::stageHead(payload[0], payload + 1));
            return;
        }

        case SYSEX_PATCH_PART: {
            if (payloadLength != 2 + AURORA_PATCH_CC_COUNT) {
                fail(type, SYSEX_ERROR_RANGE);
                return;
            }
            fail(type, patch_store::stagePart(payload[0], payload[1], payload + 2));
            return;
        }

        case SYSEX_SYNC_COMMIT: {
            const uint8_t status = pendingError != SYSEX_OK
                                 ? pendingError
                                 : patch_store::stageCommit();
            pendingError = SYSEX_OK;
            ack(type, status);
            return;
        }

        case SYSEX_SYNC_ABORT:
            patch_store::stageAbort();
            pendingError = SYSEX_OK;
            ack(type, SYSEX_OK);
            return;

        case SYSEX_QUERY_LIBRARY:
            sendLibraryInfo();
            return;

        case SYSEX_QUERY_PATCH:
            if (payloadLength != 1) { ack(type, SYSEX_ERROR_RANGE); return; }
            sendPatch(payload[0]);
            return;

        default:
            return;
    }
}

}
