#pragma once

#include <stdint.h>

#include "aurora_protocol.h"

namespace patch_store {

void begin();

AuroraLibraryState state();

const uint8_t *libraryHead();
uint8_t defaultOneshot(uint8_t place);

uint8_t stageBegin(uint8_t format, const uint8_t *head);
uint8_t stageHead(uint8_t slot, const uint8_t *head);
uint8_t stageLayer(uint8_t slot, uint8_t layer, const uint8_t *cc);
uint8_t stageOneshot(uint8_t index, const uint8_t *oneshot);
uint8_t stageCommit();
void    stageAbort();

bool readHead(uint8_t slot, uint8_t *out);
bool readLayer(uint8_t slot, uint8_t layer, uint8_t *out);
bool readOneshot(uint8_t index, uint8_t *out);

}
