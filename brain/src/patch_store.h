// patch_store — the patch library in LittleFS on the program flash.
//
// The editor holds the master and pushes all of it, so this is a mirror
// rather than a source: nothing here is the only copy of anything, which is
// why the format may change and cost only a re-sync. See DESIGN.md §
// "Patch storage" and shared/aurora_protocol.h § "System Exclusive".
//
// A sync streams in strictly ordered and is appended to a staging file. The
// staged file becomes the library at commit and not before, so a sync that
// stops partway leaves the previous library whole.

#ifndef AURORA_BRAIN_PATCH_STORE_H
#define AURORA_BRAIN_PATCH_STORE_H

#include <stdint.h>

#include "aurora_protocol.h"

namespace patch_store {

void begin();

AuroraLibraryState state();
uint8_t patchCount();

// AURORA_KEYPAD_KEYS entries, each a patch index. Which patch a numpad key
// selects is decided in the editor and arrives with the sync; the table
// lives here because the brain is the only box present in every way the rig
// gets driven — DESIGN.md § "Three ways the rig gets driven".
const uint8_t *keymap();

// Each returns an AuroraSysExStatus. The pieces must arrive in order: every
// patch's head, then its five sets, for each patch below the declared count.
uint8_t stageBegin(uint8_t format, uint8_t count, const uint8_t *keys);
uint8_t stageHead(uint8_t index, const uint8_t *head);
uint8_t stageSet(uint8_t index, uint8_t set, const uint8_t *cc);
uint8_t stageCommit();
void    stageAbort();

bool readHead(uint8_t index, uint8_t *out);
bool readSet(uint8_t index, uint8_t set, uint8_t *out);

} // namespace patch_store

#endif // AURORA_BRAIN_PATCH_STORE_H
