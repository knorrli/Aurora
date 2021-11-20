#include "aurora.h"

#define FADER_MIN_VAL 10
#define FADER_MAX_VAL 1020
#define MIN_COLOR_VALUE 0
#define MAX_COLOR_VALUE 250
#define MIN_BRIGHTNESS 0
#define MAX_BRIGHTNESS 255

void setCurrentColor() {
    if (analogRead(PIN_FADER_MODE) > 511) {
    touchColor = CHSV(readColorFader(PIN_FADER_RED), (MAX_COLOR_VALUE - readColorFader(PIN_FADER_GREEN)), readColorFader(PIN_FADER_BLUE));
  } else {
    presetColor = CHSV(readColorFader(PIN_FADER_RED), (MAX_COLOR_VALUE - readColorFader(PIN_FADER_GREEN)), readColorFader(PIN_FADER_BLUE));
  }
}

uint8_t readColorFader(byte pin) {
  uint16_t analogValue = analogRead(pin);
  return constrain(map((1023 - analogValue), FADER_MIN_VAL, FADER_MAX_VAL, MIN_BRIGHTNESS, MAX_BRIGHTNESS), MIN_COLOR_VALUE, MAX_COLOR_VALUE);
}
