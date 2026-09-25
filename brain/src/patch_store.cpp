#include "patch_store.h"

#include <LittleFS.h>

namespace {
const uint32_t FILESYSTEM_BYTES = 256u * 1024u;

const char *LIVE_PATH  = "/library.bin";
const char *STAGE_PATH = "/library.new";

const uint8_t  MAGIC[4]  = { 'A', 'U', 'R', 'L' };
const uint8_t  KEYS_AT   = 5;
const uint8_t  MAP_AT    = KEYS_AT + AURORA_KEYPAD_KEYS;
const uint16_t HEADER_LENGTH = MAP_AT + AURORA_SLOT_MAP_LENGTH;

LittleFS_Program filesystem;
bool mounted = false;

AuroraLibraryState liveState = LIBRARY_EMPTY;
uint8_t liveKeys[AURORA_KEYPAD_KEYS];
uint8_t liveMap[AURORA_SLOT_MAP_LENGTH];

File    stageFile;
bool    staging   = false;
uint8_t stageMap[AURORA_SLOT_MAP_LENGTH];
uint8_t nextSlot  = AURORA_PATCH_MAX;
uint8_t nextPiece = 0;

uint8_t filledFrom(const uint8_t *map, uint16_t slot) {
    while (slot < AURORA_PATCH_MAX && !aurora_slot_filled(map, slot)) slot++;
    return slot;
}

uint8_t filledBelow(const uint8_t *map, uint8_t slot) {
    uint8_t n = 0;
    for (uint8_t s = 0; s < slot; s++) n += aurora_slot_filled(map, s);
    return n;
}

bool mapFits(const uint8_t *map) {
    for (uint16_t bit = AURORA_PATCH_MAX; bit < AURORA_SLOT_MAP_LENGTH * 7u; bit++) {
        if ((map[bit / 7] >> (bit % 7)) & 1) return false;
    }
    return true;
}

uint32_t patchOffset(uint8_t filledBefore) {
    return HEADER_LENGTH + (uint32_t)filledBefore * AURORA_PATCH_LENGTH;
}

void forgetLive() {
    liveState = LIBRARY_EMPTY;
    for (uint8_t i = 0; i < AURORA_KEYPAD_KEYS; i++) liveKeys[i] = 0;
    for (uint8_t i = 0; i < AURORA_SLOT_MAP_LENGTH; i++) liveMap[i] = 0;
}

void loadLive() {
    forgetLive();
    if (!mounted || !filesystem.exists(LIVE_PATH)) return;

    File file = filesystem.open(LIVE_PATH, FILE_READ);
    if (!file) { liveState = LIBRARY_UNREADABLE; return; }

    uint8_t head[HEADER_LENGTH];
    const bool readable = file.read(head, HEADER_LENGTH) == (int)HEADER_LENGTH;
    const uint32_t size = file.size();
    file.close();

    if (!readable) { liveState = LIBRARY_UNREADABLE; return; }
    for (uint8_t i = 0; i < 4; i++) {
        if (head[i] != MAGIC[i]) { liveState = LIBRARY_UNREADABLE; return; }
    }
    if (head[4] != AURORA_PATCH_FORMAT || !mapFits(head + MAP_AT)) {
        liveState = LIBRARY_UNREADABLE;
        return;
    }
    const uint8_t count = filledBelow(head + MAP_AT, AURORA_PATCH_MAX);
    if (count == 0 || size != patchOffset(count)) { liveState = LIBRARY_UNREADABLE; return; }

    for (uint8_t i = 0; i < AURORA_KEYPAD_KEYS; i++) liveKeys[i] = head[KEYS_AT + i];
    for (uint8_t i = 0; i < AURORA_SLOT_MAP_LENGTH; i++) liveMap[i] = head[MAP_AT + i];
    liveState = LIBRARY_STORED;
}

bool readAt(uint8_t slot, uint32_t offset, uint8_t *out, uint16_t length) {
    if (!mounted || liveState != LIBRARY_STORED) return false;
    if (slot >= AURORA_PATCH_MAX || !aurora_slot_filled(liveMap, slot)) return false;

    File file = filesystem.open(LIVE_PATH, FILE_READ);
    if (!file) return false;
    const bool ok = file.seek(patchOffset(filledBelow(liveMap, slot)) + offset)
                 && file.read(out, length) == (int)length;
    file.close();
    return ok;
}

}

namespace patch_store {

void begin() {
    mounted = filesystem.begin(FILESYSTEM_BYTES);
    if (!mounted) { liveState = LIBRARY_UNREADABLE; return; }

    if (filesystem.exists(STAGE_PATH)) filesystem.remove(STAGE_PATH);

    loadLive();
}

AuroraLibraryState state()   { return liveState; }
const uint8_t *keymap()      { return liveKeys; }
const uint8_t *slotMap()     { return liveMap; }

uint8_t stageBegin(uint8_t format, const uint8_t *keys, const uint8_t *map) {
    if (!mounted) return SYSEX_ERROR_STORAGE;
    if (format != AURORA_PATCH_FORMAT) return SYSEX_ERROR_FORMAT;
    if (!mapFits(map) || filledFrom(map, 0) == AURORA_PATCH_MAX) return SYSEX_ERROR_RANGE;

    stageAbort();

    stageFile = filesystem.open(STAGE_PATH, FILE_WRITE_BEGIN);
    if (!stageFile) return SYSEX_ERROR_STORAGE;
    staging = true;

    uint8_t head[HEADER_LENGTH];
    for (uint8_t i = 0; i < HEADER_LENGTH; i++) head[i] = 0;
    for (uint8_t i = 0; i < 4; i++) head[i] = MAGIC[i];
    head[4] = AURORA_PATCH_FORMAT;
    for (uint8_t i = 0; i < AURORA_KEYPAD_KEYS; i++) head[KEYS_AT + i] = keys[i];
    for (uint8_t i = 0; i < AURORA_SLOT_MAP_LENGTH; i++) head[MAP_AT + i] = map[i];

    if (stageFile.write(head, HEADER_LENGTH) != HEADER_LENGTH) {
        stageAbort();
        return SYSEX_ERROR_STORAGE;
    }

    for (uint8_t i = 0; i < AURORA_SLOT_MAP_LENGTH; i++) stageMap[i] = map[i];
    nextSlot  = filledFrom(stageMap, 0);
    nextPiece = 0;
    return SYSEX_OK;
}

uint8_t stageHead(uint8_t slot, const uint8_t *head) {
    if (!staging) return SYSEX_ERROR_SEQUENCE;
    if (slot != nextSlot || nextPiece != 0) return SYSEX_ERROR_SEQUENCE;

    if (stageFile.write(head, AURORA_PATCH_HEAD_LENGTH) != AURORA_PATCH_HEAD_LENGTH) {
        stageAbort();
        return SYSEX_ERROR_STORAGE;
    }
    nextPiece = 1;
    return SYSEX_OK;
}

uint8_t stagePart(uint8_t slot, uint8_t part, const uint8_t *cc) {
    if (!staging) return SYSEX_ERROR_SEQUENCE;
    if (slot != nextSlot || nextPiece == 0 || part != nextPiece - 1) {
        return SYSEX_ERROR_SEQUENCE;
    }

    if (stageFile.write(cc, AURORA_PATCH_CC_COUNT) != AURORA_PATCH_CC_COUNT) {
        stageAbort();
        return SYSEX_ERROR_STORAGE;
    }

    nextPiece++;
    if (nextPiece > AURORA_PATCH_PARTS) {
        nextPiece = 0;
        nextSlot  = filledFrom(stageMap, (uint16_t)nextSlot + 1);
    }
    return SYSEX_OK;
}

uint8_t stageCommit() {
    if (!staging) return SYSEX_ERROR_SEQUENCE;
    if (nextSlot != AURORA_PATCH_MAX || nextPiece != 0) {
        stageAbort();
        return SYSEX_ERROR_INCOMPLETE;
    }

    stageFile.flush();
    stageFile.close();
    staging = false;

    if (filesystem.exists(LIVE_PATH) && !filesystem.remove(LIVE_PATH)) {
        filesystem.remove(STAGE_PATH);
        return SYSEX_ERROR_STORAGE;
    }
    if (!filesystem.rename(STAGE_PATH, LIVE_PATH)) {
        filesystem.remove(STAGE_PATH);
        loadLive();
        return SYSEX_ERROR_STORAGE;
    }

    loadLive();
    return liveState == LIBRARY_STORED ? SYSEX_OK : SYSEX_ERROR_STORAGE;
}

void stageAbort() {
    if (staging) {
        stageFile.close();
        staging = false;
    }
    if (mounted && filesystem.exists(STAGE_PATH)) filesystem.remove(STAGE_PATH);
    nextSlot  = AURORA_PATCH_MAX;
    nextPiece = 0;
}

bool readHead(uint8_t slot, uint8_t *out) {
    return readAt(slot, 0, out, AURORA_PATCH_HEAD_LENGTH);
}

bool readPart(uint8_t slot, uint8_t part, uint8_t *out) {
    if (part >= AURORA_PATCH_PARTS) return false;
    const uint32_t offset = AURORA_PATCH_HEAD_LENGTH
                          + (uint32_t)part * AURORA_PATCH_CC_COUNT;
    return readAt(slot, offset, out, AURORA_PATCH_CC_COUNT);
}

}
