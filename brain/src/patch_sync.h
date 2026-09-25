#pragma once

#include <stdint.h>

namespace patch_sync {

void onSysEx(const uint8_t *data, uint16_t length, bool complete);

}
