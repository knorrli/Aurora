#include "patch_store.h"

#include <LittleFS.h>

namespace {
const uint32_t FILESYSTEM_BYTES = 256u * 1024u;

const char *LIVE_PATH  = "/library.bin";
const char *STAGE_PATH = "/library.new";

const uint8_t  MAGIC[4]  = { 'A', 'U', 'R', 'L' };
const uint8_t  HEAD_AT   = 5;
const uint16_t HEADER_LENGTH = HEAD_AT + AURORA_LIBRARY_HEAD_LENGTH;
const uint8_t  SLOTS_END = AURORA_LAST_PATCH_SLOT + 1;

const uint8_t *slotMapOf(const uint8_t *head) { return head; }
const uint8_t *kitMapOf(const uint8_t *head)  { return head + AURORA_SLOT_MAP_LENGTH; }
const uint8_t *defaultsOf(const uint8_t *head) { return head + AURORA_SLOT_MAP_LENGTH + AURORA_KIT_MAP_LENGTH; }

LittleFS_Program filesystem;
bool mounted = false;

AuroraLibraryState liveState = LIBRARY_EMPTY;
uint8_t liveHead[AURORA_LIBRARY_HEAD_LENGTH];

File    stageFile;
bool    staging   = false;
uint8_t stageHeadBytes[AURORA_LIBRARY_HEAD_LENGTH];
uint8_t nextSlot  = SLOTS_END;
uint8_t nextPiece = 0;
uint8_t nextOneshot = AURORA_ONESHOTS;

uint8_t filledFrom(const uint8_t *map, uint16_t index, uint8_t end) {
    while (index < end && !aurora_map_has(map, index)) index++;
    return index;
}

uint8_t filledBelow(const uint8_t *map, uint8_t index) {
    uint8_t count = 0;
    for (uint8_t below = 0; below < index; below++) count += aurora_map_has(map, below);
    return count;
}

bool mapFits(const uint8_t *map, uint8_t length, bool (*allowed)(uint8_t)) {
    for (uint16_t bit = 0; bit < length * 7u; bit++) {
        if (aurora_map_has(map, bit) && !allowed(bit)) return false;
    }
    return true;
}

bool isKitPlace(uint8_t index) { return index < AURORA_ONESHOTS; }

bool isPick(uint8_t pick) { return pick == AURORA_NO_ONESHOT || pick < AURORA_ONESHOTS; }

bool headFits(const uint8_t *head) {
    const uint8_t *defaults = defaultsOf(head);
    return mapFits(slotMapOf(head), AURORA_SLOT_MAP_LENGTH, aurora_is_patch_slot)
        && mapFits(kitMapOf(head), AURORA_KIT_MAP_LENGTH, isKitPlace)
        && isPick(defaults[0]) && isPick(defaults[1]);
}

uint32_t patchOffset(uint8_t filledBefore) {
    return HEADER_LENGTH + (uint32_t)filledBefore * AURORA_PATCH_LENGTH;
}

uint32_t oneshotOffset(const uint8_t *head, uint8_t filledBefore) {
    return patchOffset(filledBelow(slotMapOf(head), SLOTS_END)) + (uint32_t)filledBefore * AURORA_ONESHOT_LENGTH;
}

void forgetLive() {
    liveState = LIBRARY_EMPTY;
    for (uint8_t i = 0; i < AURORA_LIBRARY_HEAD_LENGTH; i++) liveHead[i] = 0;
    uint8_t *defaults = liveHead + AURORA_SLOT_MAP_LENGTH + AURORA_KIT_MAP_LENGTH;
    defaults[0] = defaults[1] = AURORA_NO_ONESHOT;
}

void loadLive() {
    forgetLive();
    if (!mounted || !filesystem.exists(LIVE_PATH)) return;

    File file = filesystem.open(LIVE_PATH, FILE_READ);
    if (!file) { liveState = LIBRARY_UNREADABLE; return; }

    uint8_t header[HEADER_LENGTH];
    const bool readable = file.read(header, HEADER_LENGTH) == (int)HEADER_LENGTH;
    const uint32_t size = file.size();
    file.close();

    if (!readable) { liveState = LIBRARY_UNREADABLE; return; }
    for (uint8_t i = 0; i < 4; i++) {
        if (header[i] != MAGIC[i]) { liveState = LIBRARY_UNREADABLE; return; }
    }
    const uint8_t *head = header + HEAD_AT;
    if (header[4] != AURORA_PATCH_FORMAT || !headFits(head)) { liveState = LIBRARY_UNREADABLE; return; }
    const uint8_t patches = filledBelow(slotMapOf(head), SLOTS_END);
    const uint8_t oneshots = filledBelow(kitMapOf(head), AURORA_ONESHOTS);
    if (patches == 0 || size != oneshotOffset(head, oneshots)) { liveState = LIBRARY_UNREADABLE; return; }

    for (uint8_t i = 0; i < AURORA_LIBRARY_HEAD_LENGTH; i++) liveHead[i] = head[i];
    liveState = LIBRARY_STORED;
}

bool readFrom(uint32_t offset, uint8_t *out, uint16_t length) {
    File file = filesystem.open(LIVE_PATH, FILE_READ);
    if (!file) return false;
    const bool ok = file.seek(offset) && file.read(out, length) == (int)length;
    file.close();
    return ok;
}

bool readPatchAt(uint8_t slot, uint32_t offset, uint8_t *out, uint16_t length) {
    if (!mounted || liveState != LIBRARY_STORED) return false;
    if (!aurora_is_patch_slot(slot) || !aurora_map_has(slotMapOf(liveHead), slot)) return false;
    return readFrom(patchOffset(filledBelow(slotMapOf(liveHead), slot)) + offset, out, length);
}

uint8_t writeStage(const uint8_t *bytes, uint16_t length) {
    if (stageFile.write(bytes, length) == length) return SYSEX_OK;
    patch_store::stageAbort();
    return SYSEX_ERROR_STORAGE;
}

void finishPatches() {
    nextPiece = 0;
    nextSlot = filledFrom(slotMapOf(stageHeadBytes), (uint16_t)nextSlot + 1, SLOTS_END);
    if (nextSlot == SLOTS_END) nextOneshot = filledFrom(kitMapOf(stageHeadBytes), 0, AURORA_ONESHOTS);
}

}

namespace patch_store {

void begin() {
    forgetLive();
    mounted = filesystem.begin(FILESYSTEM_BYTES);
    if (!mounted) { liveState = LIBRARY_UNREADABLE; return; }

    if (filesystem.exists(STAGE_PATH)) filesystem.remove(STAGE_PATH);

    loadLive();
}

AuroraLibraryState state()     { return liveState; }
const uint8_t *libraryHead()   { return liveHead; }
uint8_t defaultOneshot(uint8_t place) { return defaultsOf(liveHead)[place]; }

uint8_t stageBegin(uint8_t format, const uint8_t *head) {
    if (!mounted) return SYSEX_ERROR_STORAGE;
    if (format != AURORA_PATCH_FORMAT) return SYSEX_ERROR_FORMAT;
    if (!headFits(head) || filledFrom(slotMapOf(head), 0, SLOTS_END) == SLOTS_END) return SYSEX_ERROR_RANGE;

    stageAbort();

    stageFile = filesystem.open(STAGE_PATH, FILE_WRITE_BEGIN);
    if (!stageFile) return SYSEX_ERROR_STORAGE;
    staging = true;

    uint8_t header[HEADER_LENGTH];
    for (uint8_t i = 0; i < 4; i++) header[i] = MAGIC[i];
    header[4] = AURORA_PATCH_FORMAT;
    for (uint8_t i = 0; i < AURORA_LIBRARY_HEAD_LENGTH; i++) header[HEAD_AT + i] = stageHeadBytes[i] = head[i];
    const uint8_t status = writeStage(header, HEADER_LENGTH);
    if (status != SYSEX_OK) return status;

    nextSlot = filledFrom(slotMapOf(stageHeadBytes), 0, SLOTS_END);
    nextPiece = 0;
    nextOneshot = AURORA_ONESHOTS;
    return SYSEX_OK;
}

uint8_t stageHead(uint8_t slot, const uint8_t *head) {
    if (!staging || slot != nextSlot || nextPiece != 0) return SYSEX_ERROR_SEQUENCE;
    const uint8_t status = writeStage(head, AURORA_PATCH_HEAD_LENGTH);
    if (status == SYSEX_OK) nextPiece = 1;
    return status;
}

uint8_t stageLayer(uint8_t slot, uint8_t layer, const uint8_t *cc) {
    if (!staging || slot != nextSlot || nextPiece == 0 || layer != nextPiece - 1) return SYSEX_ERROR_SEQUENCE;
    const uint8_t status = writeStage(cc, AURORA_PATCH_CC_COUNT);
    if (status != SYSEX_OK) return status;
    nextPiece++;
    if (nextPiece > AURORA_PATCH_LAYERS) finishPatches();
    return SYSEX_OK;
}

uint8_t stageOneshot(uint8_t index, const uint8_t *oneshot) {
    if (!staging || nextSlot != SLOTS_END || index != nextOneshot) return SYSEX_ERROR_SEQUENCE;
    const uint8_t status = writeStage(oneshot, AURORA_ONESHOT_LENGTH);
    if (status == SYSEX_OK) nextOneshot = filledFrom(kitMapOf(stageHeadBytes), (uint16_t)index + 1, AURORA_ONESHOTS);
    return status;
}

uint8_t stageCommit() {
    if (!staging) return SYSEX_ERROR_SEQUENCE;
    if (nextSlot != SLOTS_END || nextOneshot != AURORA_ONESHOTS) {
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
    nextSlot  = SLOTS_END;
    nextPiece = 0;
    nextOneshot = AURORA_ONESHOTS;
}

bool readHead(uint8_t slot, uint8_t *out) {
    return readPatchAt(slot, 0, out, AURORA_PATCH_HEAD_LENGTH);
}

bool readLayer(uint8_t slot, uint8_t layer, uint8_t *out) {
    if (layer >= AURORA_PATCH_LAYERS) return false;
    const uint32_t offset = AURORA_PATCH_HEAD_LENGTH + (uint32_t)layer * AURORA_PATCH_CC_COUNT;
    return readPatchAt(slot, offset, out, AURORA_PATCH_CC_COUNT);
}

bool readOneshot(uint8_t index, uint8_t *out) {
    if (!mounted || liveState != LIBRARY_STORED || !isKitPlace(index)) return false;
    const uint8_t *kitMap = kitMapOf(liveHead);
    if (!aurora_map_has(kitMap, index)) return false;
    return readFrom(oneshotOffset(liveHead, filledBelow(kitMap, index)), out, AURORA_ONESHOT_LENGTH);
}

}
