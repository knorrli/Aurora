#include "Aurora.h"

#include "destinations.h"

#define TRIGGER_DEBOUNCE_DELAY 300
#define TRIGGER_EFFECT_DURATION 700
#define TRIGGER_BLACKPHASE 200
#define TRIGGER_FADE_STEP ((TRIGGER_EFFECT_DURATION - TRIGGER_BLACKPHASE) / 100)

static unsigned long lastTriggerMillis = 0;
static CHSV triggerLedColor = CHSV(0, 0, 255);
static bool triggerPending = false;

void fireTrigger()
{
  triggerPending = true;
}

static void readTrigger()
{
  if (!triggerPending) return;
  triggerPending = false;

  if (currentMillis - lastTriggerMillis <= TRIGGER_DEBOUNCE_DELAY) return;

  lastTriggerMillis = currentMillis;
  triggerLedColor = CHSV(render::colorFrom(destinations::all(), nullptr).h, 0, 255);
}

void renderTrigger()
{
  readTrigger();

  if ((currentMillis - lastTriggerMillis) < TRIGGER_EFFECT_DURATION)
  {
    strips.fill_solid(triggerLedColor);
    triggerLedColor.saturation = min(triggerLedColor.saturation + TRIGGER_FADE_STEP, 255);
    triggerLedColor.value = max(triggerLedColor.value - TRIGGER_FADE_STEP, 0);
  }
}
