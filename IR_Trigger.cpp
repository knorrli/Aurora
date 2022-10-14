#include "aurora.h"

#define TRIGGER_DEBOUNCE_DELAY 250 // TODO: match FPS to get around 22 FPS for maximum effect
#define TRIGGER_LIGHT_PHASE 250
#define TRIGGER_BLACK_PHASE
#define RANDOMIZE_COLOR_INTERVAL 1
#define MOVEMENT_FACTOR 5

static uint8_t randomizeColorCounter = 0;
static unsigned long lastTriggerMillis = 0;
static CHSV triggerLedColor = CHSV(124, 0, 255);
static bool triggerGate = LOW;

void renderTrigger() {
  readTrigger();

  if (triggerGate || ((currentMillis - lastTriggerMillis) < TRIGGER_LIGHT_PHASE)) {
    triggerLedColor.saturation += 2*MOVEMENT_FACTOR;
    triggerLedColor.value -= MOVEMENT_FACTOR;
    strips.fill_solid(triggerLedColor);
  }
}

bool readTrigger() {
  if ((currentMillis - lastTriggerMillis) > TRIGGER_DEBOUNCE_DELAY) {
    if (digitalRead(PIN_TRIGGER_INPUT)) {
      lastTriggerMillis = currentMillis;
      randomizeColorCounter += 1;
      if (randomizeColorCounter > RANDOMIZE_COLOR_INTERVAL) {
        triggerLedColor = randomColor();
        triggerLedColor.saturation = 0;
        randomizeColorCounter = 0;
      }
      return HIGH;
    }
  }
  return LOW;
}
