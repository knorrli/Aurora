#include "aurora.h"

#define TRIGGER_DEBOUNCE_DELAY 100
#define TRIGGER_EFFECT_DURATION 100

static unsigned long lastTriggerMillis = 0;
static CRGB triggerLedColor = CRGB::White;
static bool triggerGate = LOW;

void renderTrigger() {
  readTrigger();

  if (triggerGate || ((currentMillis - lastTriggerMillis) < TRIGGER_EFFECT_DURATION)) {
    strips.fill_solid(triggerLedColor);
  }
}

bool readTrigger() {
  if (currentMillis - lastTriggerMillis > TRIGGER_DEBOUNCE_DELAY) {
    if (digitalRead(PIN_TRIGGER_INPUT)) {
      lastTriggerMillis = currentMillis;
      triggerLedColor = randomColor();
      return HIGH;
    }
  }
  return LOW;
}
