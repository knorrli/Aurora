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
const uint16_t HEADER_LEN = 16; // magic, format, count, keymap, one spare

LittleFS_Program fs;
bool mounted = false;

AuroraLibraryState liveState = LIBRARY_EMPTY;
uint8_t liveCount = 0;
uint8_t liveKeys[AURORA_KEYPAD_KEYS];

File    stageFile;
bool    staging   = false;
uint8_t stageCount = 0;
uint8_t nextIndex = 0;
uint8_t nextPiece = 0; // 0 is the head; 1..AURORA_PATCH_SETS are the sets

uint32_t patchOffset(uint8_t index) {
    return HEADER_LEN + (uint32_t)index * AURORA_PATCH_LEN;
}

void forgetLive() {
    liveState = LIBRARY_EMPTY;
    liveCount = 0;
    for (uint8_t i = 0; i < AURORA_KEYPAD_KEYS; i++) liveKeys[i] = 0;
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
    if (head[4] != AURORA_PATCH_FORMAT || head[5] > AURORA_PATCH_MAX) {
        liveState = LIBRARY_UNREADABLE;
        return;
    }
    // A file cut short by a power loss between the write and the rename
    // would still carry a valid header, so the length is what catches it.
    if (size != patchOffset(head[5])) { liveState = LIBRARY_UNREADABLE; return; }

    liveCount = head[5];
    for (uint8_t i = 0; i < AURORA_KEYPAD_KEYS; i++) liveKeys[i] = head[6 + i];
    liveState = LIBRARY_STORED;
}

bool readAt(uint8_t index, uint32_t offset, uint8_t *out, uint16_t len) {
    if (!mounted || liveState != LIBRARY_STORED || index >= liveCount) return false;

    File f = fs.open(LIVE_PATH, FILE_READ);
    if (!f) return false;
    const bool ok = f.seek(patchOffset(index) + offset)
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
uint8_t patchCount()         { return liveState == LIBRARY_STORED ? liveCount : 0; }
const uint8_t *keymap()      { return liveKeys; }

uint8_t stageBegin(uint8_t format, uint8_t count, const uint8_t *keys) {
    if (!mounted) return SYSEX_ERR_STORAGE;
    if (format != AURORA_PATCH_FORMAT) return SYSEX_ERR_FORMAT;
    if (count == 0 || count > AURORA_PATCH_MAX) return SYSEX_ERR_RANGE;

    stageAbort();

    stageFile = fs.open(STAGE_PATH, FILE_WRITE_BEGIN);
    if (!stageFile) return SYSEX_ERR_STORAGE;
    staging = true;

    uint8_t head[HEADER_LEN];
    for (uint8_t i = 0; i < HEADER_LEN; i++) head[i] = 0;
    for (uint8_t i = 0; i < 4; i++) head[i] = MAGIC[i];
    head[4] = AURORA_PATCH_FORMAT;
    head[5] = count;
    for (uint8_t i = 0; i < AURORA_KEYPAD_KEYS; i++) {
        head[6 + i] = keys[i] < count ? keys[i] : 0;
    }

    if (stageFile.write(head, HEADER_LEN) != HEADER_LEN) {
        stageAbort();
        return SYSEX_ERR_STORAGE;
    }

    stageCount = count;
    nextIndex  = 0;
    nextPiece  = 0;
    return SYSEX_OK;
}

uint8_t stageHead(uint8_t index, const uint8_t *head) {
    if (!staging) return SYSEX_ERR_SEQUENCE;
    if (index != nextIndex || nextPiece != 0) return SYSEX_ERR_SEQUENCE;

    if (stageFile.write(head, AURORA_PATCH_HEAD_LEN) != AURORA_PATCH_HEAD_LEN) {
        stageAbort();
        return SYSEX_ERR_STORAGE;
    }
    nextPiece = 1;
    return SYSEX_OK;
}

uint8_t stageSet(uint8_t index, uint8_t set, const uint8_t *cc) {
    if (!staging) return SYSEX_ERR_SEQUENCE;
    if (index != nextIndex || nextPiece == 0 || set != nextPiece - 1) {
        return SYSEX_ERR_SEQUENCE;
    }

    if (stageFile.write(cc, AURORA_PATCH_CC_COUNT) != AURORA_PATCH_CC_COUNT) {
        stageAbort();
        return SYSEX_ERR_STORAGE;
    }

    nextPiece++;
    if (nextPiece > AURORA_PATCH_SETS) {
        nextPiece = 0;
        nextIndex++;
    }
    return SYSEX_OK;
}

uint8_t stageCommit() {
    if (!staging) return SYSEX_ERR_SEQUENCE;
    if (nextIndex != stageCount || nextPiece != 0) {
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
    stageCount = 0;
    nextIndex  = 0;
    nextPiece  = 0;
}

bool readHead(uint8_t index, uint8_t *out) {
    return readAt(index, 0, out, AURORA_PATCH_HEAD_LEN);
}

bool readSet(uint8_t index, uint8_t set, uint8_t *out) {
    if (set >= AURORA_PATCH_SETS) return false;
    const uint32_t offset = AURORA_PATCH_HEAD_LEN
                          + (uint32_t)set * AURORA_PATCH_CC_COUNT;
    return readAt(index, offset, out, AURORA_PATCH_CC_COUNT);
}

} // namespace patch_store
