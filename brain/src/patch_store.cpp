#include "patch_store.h"

#include <LittleFS.h>

namespace {

// Claimed out of the 1.9 MB the firmware leaves free. A full library is
// about 85 KB; the rest is headroom for LittleFS's own bookkeeping and for
// whatever else ends up stored beside the patches.
const uint32_t FS_BYTES = 256u * 1024u;

const char *LIVE_PATH  = "/library.bin";
const char *STAGE_PATH = "/library.new";

const uint8_t  MAGIC[4]  = { 'A', 'U', 'R', 'L' };
const uint8_t  KEYS_AT   = 5;
const uint8_t  MAP_AT    = KEYS_AT + AURORA_KEYPAD_KEYS;
const uint16_t HEADER_LEN = MAP_AT + AURORA_SLOT_MAP_LEN; // magic, format, keymap, slot map

LittleFS_Program fs;
bool mounted = false;

AuroraLibraryState liveState = LIBRARY_EMPTY;
uint8_t liveKeys[AURORA_KEYPAD_KEYS];
uint8_t liveMap[AURORA_SLOT_MAP_LEN];

File    stageFile;
bool    staging   = false;
uint8_t stageMap[AURORA_SLOT_MAP_LEN];
uint8_t nextSlot  = AURORA_PATCH_MAX; // AURORA_PATCH_MAX once every patch is in
uint8_t nextPiece = 0; // 0 is the head; 1..AURORA_PATCH_SETS are the sets

uint8_t filledFrom(const uint8_t *map, uint16_t slot) {
    while (slot < AURORA_PATCH_MAX && !aurora_slot_filled(map, slot)) slot++;
    return slot;
}

uint8_t filledBelow(const uint8_t *map, uint8_t slot) {
    uint8_t n = 0;
    for (uint8_t s = 0; s < slot; s++) n += aurora_slot_filled(map, s);
    return n;
}

// The last byte of the map has bits past the final slot, and a map with one
// of them set was not written by anything that speaks this format.
bool mapFits(const uint8_t *map) {
    for (uint16_t bit = AURORA_PATCH_MAX; bit < AURORA_SLOT_MAP_LEN * 7u; bit++) {
        if ((map[bit / 7] >> (bit % 7)) & 1) return false;
    }
    return true;
}

uint32_t patchOffset(uint8_t filledBefore) {
    return HEADER_LEN + (uint32_t)filledBefore * AURORA_PATCH_LEN;
}

void forgetLive() {
    liveState = LIBRARY_EMPTY;
    for (uint8_t i = 0; i < AURORA_KEYPAD_KEYS; i++) liveKeys[i] = 0;
    for (uint8_t i = 0; i < AURORA_SLOT_MAP_LEN; i++) liveMap[i] = 0;
}

void loadLive() {
    forgetLive();
    if (!mounted || !fs.exists(LIVE_PATH)) return;

    File f = fs.open(LIVE_PATH, FILE_READ);
    if (!f) { liveState = LIBRARY_UNREADABLE; return; }

    uint8_t head[HEADER_LEN];
    const bool readable = f.read(head, HEADER_LEN) == (int)HEADER_LEN;
    const uint32_t size = f.size();
    f.close();

    if (!readable) { liveState = LIBRARY_UNREADABLE; return; }
    for (uint8_t i = 0; i < 4; i++) {
        if (head[i] != MAGIC[i]) { liveState = LIBRARY_UNREADABLE; return; }
    }
    if (head[4] != AURORA_PATCH_FORMAT || !mapFits(head + MAP_AT)) {
        liveState = LIBRARY_UNREADABLE;
        return;
    }
    // A file cut short by a power loss between the write and the rename
    // would still carry a valid header, so the length is what catches it.
    const uint8_t count = filledBelow(head + MAP_AT, AURORA_PATCH_MAX);
    if (count == 0 || size != patchOffset(count)) { liveState = LIBRARY_UNREADABLE; return; }

    for (uint8_t i = 0; i < AURORA_KEYPAD_KEYS; i++) liveKeys[i] = head[KEYS_AT + i];
    for (uint8_t i = 0; i < AURORA_SLOT_MAP_LEN; i++) liveMap[i] = head[MAP_AT + i];
    liveState = LIBRARY_STORED;
}

bool readAt(uint8_t slot, uint32_t offset, uint8_t *out, uint16_t len) {
    if (!mounted || liveState != LIBRARY_STORED) return false;
    if (slot >= AURORA_PATCH_MAX || !aurora_slot_filled(liveMap, slot)) return false;

    File f = fs.open(LIVE_PATH, FILE_READ);
    if (!f) return false;
    const bool ok = f.seek(patchOffset(filledBelow(liveMap, slot)) + offset)
                 && f.read(out, len) == (int)len;
    f.close();
    return ok;
}

} // namespace

namespace patch_store {

void begin() {
    mounted = fs.begin(FS_BYTES);
    if (!mounted) { liveState = LIBRARY_UNREADABLE; return; }

    // A staging file left behind is a sync that never committed. Nothing
    // can be recovered from it — the library it belonged to is still on the
    // editor — so it goes rather than sitting in the way of the next one.
    if (fs.exists(STAGE_PATH)) fs.remove(STAGE_PATH);

    loadLive();
}

AuroraLibraryState state()   { return liveState; }
const uint8_t *keymap()      { return liveKeys; }
const uint8_t *slotMap()     { return liveMap; }

uint8_t stageBegin(uint8_t format, const uint8_t *keys, const uint8_t *map) {
    if (!mounted) return SYSEX_ERR_STORAGE;
    if (format != AURORA_PATCH_FORMAT) return SYSEX_ERR_FORMAT;
    if (!mapFits(map) || filledFrom(map, 0) == AURORA_PATCH_MAX) return SYSEX_ERR_RANGE;

    stageAbort();

    stageFile = fs.open(STAGE_PATH, FILE_WRITE_BEGIN);
    if (!stageFile) return SYSEX_ERR_STORAGE;
    staging = true;

    uint8_t head[HEADER_LEN];
    for (uint8_t i = 0; i < HEADER_LEN; i++) head[i] = 0;
    for (uint8_t i = 0; i < 4; i++) head[i] = MAGIC[i];
    head[4] = AURORA_PATCH_FORMAT;
    for (uint8_t i = 0; i < AURORA_KEYPAD_KEYS; i++) head[KEYS_AT + i] = keys[i];
    for (uint8_t i = 0; i < AURORA_SLOT_MAP_LEN; i++) head[MAP_AT + i] = map[i];

    if (stageFile.write(head, HEADER_LEN) != HEADER_LEN) {
        stageAbort();
        return SYSEX_ERR_STORAGE;
    }

    for (uint8_t i = 0; i < AURORA_SLOT_MAP_LEN; i++) stageMap[i] = map[i];
    nextSlot  = filledFrom(stageMap, 0);
    nextPiece = 0;
    return SYSEX_OK;
}

uint8_t stageHead(uint8_t slot, const uint8_t *head) {
    if (!staging) return SYSEX_ERR_SEQUENCE;
    if (slot != nextSlot || nextPiece != 0) return SYSEX_ERR_SEQUENCE;

    if (stageFile.write(head, AURORA_PATCH_HEAD_LEN) != AURORA_PATCH_HEAD_LEN) {
        stageAbort();
        return SYSEX_ERR_STORAGE;
    }
    nextPiece = 1;
    return SYSEX_OK;
}

uint8_t stageSet(uint8_t slot, uint8_t set, const uint8_t *cc) {
    if (!staging) return SYSEX_ERR_SEQUENCE;
    if (slot != nextSlot || nextPiece == 0 || set != nextPiece - 1) {
        return SYSEX_ERR_SEQUENCE;
    }

    if (stageFile.write(cc, AURORA_PATCH_CC_COUNT) != AURORA_PATCH_CC_COUNT) {
        stageAbort();
        return SYSEX_ERR_STORAGE;
    }

    nextPiece++;
    if (nextPiece > AURORA_PATCH_SETS) {
        nextPiece = 0;
        nextSlot  = filledFrom(stageMap, (uint16_t)nextSlot + 1);
    }
    return SYSEX_OK;
}

uint8_t stageCommit() {
    if (!staging) return SYSEX_ERR_SEQUENCE;
    if (nextSlot != AURORA_PATCH_MAX || nextPiece != 0) {
        stageAbort();
        return SYSEX_ERR_INCOMPLETE;
    }

    stageFile.flush();
    stageFile.close();
    staging = false;

    // The rename is the commit: until it lands the old library is what the
    // brain has, and after it lands the new one is, with no moment in
    // between where the file is half of each.
    if (fs.exists(LIVE_PATH) && !fs.remove(LIVE_PATH)) {
        fs.remove(STAGE_PATH);
        return SYSEX_ERR_STORAGE;
    }
    if (!fs.rename(STAGE_PATH, LIVE_PATH)) {
        fs.remove(STAGE_PATH);
        loadLive();
        return SYSEX_ERR_STORAGE;
    }

    loadLive();
    return liveState == LIBRARY_STORED ? SYSEX_OK : SYSEX_ERR_STORAGE;
}

void stageAbort() {
    if (staging) {
        stageFile.close();
        staging = false;
    }
    if (mounted && fs.exists(STAGE_PATH)) fs.remove(STAGE_PATH);
    nextSlot  = AURORA_PATCH_MAX;
    nextPiece = 0;
}

bool readHead(uint8_t slot, uint8_t *out) {
    return readAt(slot, 0, out, AURORA_PATCH_HEAD_LEN);
}

bool readSet(uint8_t slot, uint8_t set, uint8_t *out) {
    if (set >= AURORA_PATCH_SETS) return false;
    const uint32_t offset = AURORA_PATCH_HEAD_LEN
                          + (uint32_t)set * AURORA_PATCH_CC_COUNT;
    return readAt(slot, offset, out, AURORA_PATCH_CC_COUNT);
}

} // namespace patch_store
