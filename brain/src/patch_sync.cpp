#include "patch_sync.h"

#include <Arduino.h>

#include "aurora_protocol.h"
#include "patch_store.h"

namespace {

// The longest reply is a parameter set: frame, index, set, 128 bytes.
const uint16_t REPLY_MAX = AURORA_SYSEX_FRAME_LEN + 2 + AURORA_PATCH_CC_COUNT;

uint8_t reply[REPLY_MAX];

// The first thing to go wrong in a sync, held until the editor asks for a
// verdict. Without it one bad message would be answered 600 times over, as
// every message after it fails the sequence check in turn.
uint8_t pendingError = SYSEX_OK;

void send(uint8_t type, const uint8_t *payload, uint16_t payloadLen) {
    reply[0] = 0xF0;
    reply[1] = AURORA_SYSEX_ID;
    reply[2] = AURORA_SYSEX_SIG_A;
    reply[3] = AURORA_SYSEX_SIG_B;
    reply[4] = type;
    for (uint16_t i = 0; i < payloadLen; i++) reply[AURORA_SYSEX_HEADER_LEN + i] = payload[i];
    reply[AURORA_SYSEX_HEADER_LEN + payloadLen] = 0xF7;

    usbMIDI.sendSysEx(AURORA_SYSEX_FRAME_LEN + payloadLen, reply, true);
}

void ack(uint8_t inReplyTo, uint8_t status) {
    const uint8_t payload[2] = { inReplyTo, status };
    send(SYSEX_ACK, payload, 2);
}

// Data messages are answered only when they go wrong, and only the first
// time: the sync is over at that point, and everything still in flight from
// the editor will fail the same way.
void fail(uint8_t type, uint8_t status) {
    if (status == SYSEX_OK) return;
    patch_store::stageAbort();
    if (pendingError != SYSEX_OK) return;
    pendingError = status;
    ack(type, status);
}

void sendLibraryInfo() {
    uint8_t payload[5 + AURORA_KEYPAD_KEYS];
    payload[0] = AURORA_PROTOCOL_VERSION_MAJOR;
    payload[1] = AURORA_PROTOCOL_VERSION_MINOR;
    payload[2] = AURORA_PATCH_FORMAT;
    payload[3] = (uint8_t)patch_store::state();
    payload[4] = patch_store::patchCount();

    const uint8_t *keys = patch_store::keymap();
    for (uint8_t i = 0; i < AURORA_KEYPAD_KEYS; i++) payload[5 + i] = keys[i];

    send(SYSEX_LIBRARY_INFO, payload, sizeof(payload));
}

void sendPatch(uint8_t index) {
    uint8_t payload[2 + AURORA_PATCH_CC_COUNT];

    payload[0] = index;
    if (!patch_store::readHead(index, payload + 1)) {
        ack(SYSEX_QUERY_PATCH, SYSEX_ERR_RANGE);
        return;
    }
    send(SYSEX_PATCH_HEAD_OUT, payload, 1 + AURORA_PATCH_HEAD_LEN);

    for (uint8_t set = 0; set < AURORA_PATCH_SETS; set++) {
        payload[0] = index;
        payload[1] = set;
        if (!patch_store::readSet(index, set, payload + 2)) {
            ack(SYSEX_QUERY_PATCH, SYSEX_ERR_STORAGE);
            return;
        }
        send(SYSEX_PATCH_SET_OUT, payload, 2 + AURORA_PATCH_CC_COUNT);
    }
}

} // namespace

namespace patch_sync {

void onSysEx(const uint8_t *data, uint16_t length, bool complete) {
    // A message too long for the core's buffer arrives in pieces. Nothing
    // Aurora sends is anywhere near that size, so one can only be another
    // device's — and a sync part-way through must not be fed a fragment of
    // it, which is why this refuses rather than waits for the rest.
    if (!complete) return;

    if (length < AURORA_SYSEX_FRAME_LEN) return;
    if (data[0] != 0xF0 || data[length - 1] != 0xF7) return;
    if (data[1] != AURORA_SYSEX_ID) return;
    if (data[2] != AURORA_SYSEX_SIG_A || data[3] != AURORA_SYSEX_SIG_B) return;

    const uint8_t   type       = data[4];
    const uint8_t  *payload    = data + AURORA_SYSEX_HEADER_LEN;
    const uint16_t  payloadLen = length - AURORA_SYSEX_FRAME_LEN;

    switch (type) {
        case SYSEX_SYNC_BEGIN: {
            if (payloadLen != 2 + AURORA_KEYPAD_KEYS) {
                ack(type, SYSEX_ERR_RANGE);
                return;
            }
            pendingError = SYSEX_OK;
            ack(type, patch_store::stageBegin(payload[0], payload[1], payload + 2));
            return;
        }

        case SYSEX_PATCH_HEAD: {
            if (payloadLen != 1 + AURORA_PATCH_HEAD_LEN) {
                fail(type, SYSEX_ERR_RANGE);
                return;
            }
            fail(type, patch_store::stageHead(payload[0], payload + 1));
            return;
        }

        case SYSEX_PATCH_SET: {
            if (payloadLen != 2 + AURORA_PATCH_CC_COUNT) {
                fail(type, SYSEX_ERR_RANGE);
                return;
            }
            fail(type, patch_store::stageSet(payload[0], payload[1], payload + 2));
            return;
        }

        case SYSEX_SYNC_COMMIT: {
            // The commit answer is the one the editor waits for, so an
            // earlier failure is reported here too rather than only at the
            // moment it happened.
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
            if (payloadLen != 1) { ack(type, SYSEX_ERR_RANGE); return; }
            sendPatch(payload[0]);
            return;

        default:
            return;
    }
}

} // namespace patch_sync
