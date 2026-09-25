#pragma once

#include <stdint.h>

#include "aurora_protocol.h"

namespace patch_store {

void begin();

AuroraLibraryState state();

const uint8_t *slotMap();

const uint8_t *keymap();

uint8_t stageBegin(uint8_t format, const uint8_t *keys, const uint8_t *map);
uint8_t stageHead(uint8_t slot, const uint8_t *head);
uint8_t stagePart(uint8_t slot, uint8_t part, const uint8_t *cc);
uint8_t stageCommit();
void    stageAbort();

bool readHead(uint8_t slot, uint8_t *out);
bool readPart(uint8_t slot, uint8_t part, uint8_t *out);

}
