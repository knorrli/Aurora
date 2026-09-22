// patch_sync — the SysEx side of the editor conversation.
//
// Parses and answers the messages in shared/aurora_protocol.h § "System
// Exclusive"; patch_store owns the flash. Nothing here runs during a song:
// SysEx only ever arrives over USB from the editor.

#ifndef AURORA_BRAIN_PATCH_SYNC_H
#define AURORA_BRAIN_PATCH_SYNC_H

#include <stdint.h>

namespace patch_sync {

void onSysEx(const uint8_t *data, uint16_t length, bool complete);

} // namespace patch_sync

#endif // AURORA_BRAIN_PATCH_SYNC_H
