#pragma once

#include <stdint.h>

namespace pad {

const uint8_t COLUMNS = 5;
const uint8_t ROWS = 2;

void begin();
void sendAll();
void update(uint32_t micros);

uint8_t mode();
uint8_t width();
bool gaps();
bool touching();
bool latched();
uint8_t column();
uint8_t row();

}
