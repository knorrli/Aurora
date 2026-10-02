#include "stored_library.h"

#include <string.h>

#include "patch_store.h"

bool StoredLibrary::patch(uint8_t slot, playback::Patch &out) {
    uint8_t head[AURORA_PATCH_HEAD_LENGTH];
    if (!patch_store::readHead(slot, head)) return false;
    out.transitionTime = head[0];
    out.accentTime = head[1];
    out.oneshots[0] = head[2];
    out.oneshots[1] = head[3];
    for (uint8_t layer = 0; layer < AURORA_PATCH_LAYERS; layer++) {
        if (!patch_store::readLayer(slot, layer, out.layers[layer])) return false;
    }
    return true;
}

bool StoredLibrary::oneshot(uint8_t index, playback::Oneshot &out) {
    uint8_t stored[AURORA_ONESHOT_LENGTH];
    if (!patch_store::readOneshot(index, stored)) return false;
    out.length = stored[0];
    const uint8_t *marks = stored + 1 + AURORA_PATCH_NAME_LENGTH;
    for (uint8_t cc = 0; cc < AURORA_PATCH_CC_COUNT; cc++) out.marks[cc] = aurora_map_has(marks, cc);
    memcpy(out.controls, marks + AURORA_MARK_MAP_LENGTH, AURORA_PATCH_CC_COUNT);
    return true;
}

uint8_t StoredLibrary::defaultOneshot(uint8_t place) { return patch_store::defaultOneshot(place); }
