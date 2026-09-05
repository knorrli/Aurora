#include "aurora.h"

/////////////////////////////////
// FILL_STRIPS
/////////////////////////////////
void FillStrips(CHSV color) {
  strips.fill_solid(color);
}

/////////////////////////////////
// STARFIELD — evenly-spaced stars, each twinkling on its own phase
/////////////////////////////////
#define STARFIELD_GAP 4
void Starfield(CHSV color) {
  uint8_t t = (uint8_t)(millis() >> 4);
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    uint8_t stripOffset = (stripIndex * 2) % STARFIELD_GAP;
    for (uint8_t pixelIndex = stripOffset; pixelIndex < PIXELS_PER_STRIP; pixelIndex += STARFIELD_GAP) {
      uint8_t phase = (pixelIndex * 23) + (stripIndex * 67);
      uint8_t twinkle = sin8(t + phase);
      strip[stripIndex][pixelIndex] = CHSV(color.hue, color.saturation, scale8(color.value, twinkle));
    }
  }
}

/////////////////////////////////
// BREATHE — whole-wall slow brightness LFO
/////////////////////////////////
void Breathe(CHSV color) {
  uint8_t wave = sin8((uint8_t)(millis() >> 5));
  uint8_t brightness = 50 + scale8(wave, 205);
  strips.fill_solid(CHSV(color.hue, color.saturation, scale8(color.value, brightness)));
}

/////////////////////////////////
// WAVE — brightness wave traveling up each strip
/////////////////////////////////
void Wave(CHSV color) {
  uint8_t timePhase = (uint8_t)(millis() >> 4);
  for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
    uint8_t wave = sin8(timePhase + (uint8_t)(pixelIndex * 6));
    uint8_t brightness = 30 + scale8(wave, 225);
    CHSV pixelColor = CHSV(color.hue, color.saturation, scale8(color.value, brightness));
    for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
      strip[stripIndex][pixelIndex] = pixelColor;
    }
  }
}

/////////////////////////////////
// PLASMA — flowing hue field driven by combined sin8
/////////////////////////////////
void Plasma(CHSV color) {
  uint8_t t = (uint8_t)(millis() >> 3);
  // The half-speed phase has to come from millis() directly: halving the
  // already-truncated t ramps it 0-127 and snaps back, which is a
  // half-cycle discontinuity in sin8 instead of a seamless 256 wrap.
  uint8_t tHalf = (uint8_t)(millis() >> 4);
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
      uint8_t wave1 = sin8((uint8_t)(pixelIndex * 8) + t);
      uint8_t wave2 = sin8((uint8_t)(stripIndex * 40) + tHalf);
      uint8_t hueShift = ((uint16_t)wave1 + (uint16_t)wave2) >> 2;
      strip[stripIndex][pixelIndex] = CHSV(color.hue + hueShift - 64, color.saturation, color.value);
    }
  }
}

/////////////////////////////////
// AURORA — organic hue field driven by Perlin noise
/////////////////////////////////
void Aurora(CHSV color) {
  uint16_t t = millis() >> 4;
  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
      uint8_t noiseVal = inoise8(pixelIndex * 40, stripIndex * 80, t);
      strip[stripIndex][pixelIndex] = CHSV(color.hue + (noiseVal >> 1) - 64, color.saturation, color.value);
    }
  }
}

/////////////////////////////////
// PULSE
/////////////////////////////////
#define PULSE_STEPS_PER_GATE 11
static uint8_t pulsePosition = 0;
static uint8_t pulseDirection = UP;
static uint8_t pulseGateCounter = 0;
static unsigned long lastPulseGate = 0;

void PulseFill(CHSV color) {
  if (tempoGate) {
    pulseGateCounter = 0;
  }
  if ((pulseGateCounter < PULSE_STEPS_PER_GATE) && (tempoGate || ((millis() > (lastPulseGate + (currentTempo / PULSE_STEPS_PER_GATE) - (elapsedLoopTime/2)))))) {
    pulseGateCounter += 1;
    lastPulseGate = currentMillis;
    pulsePosition += pulseDirection;
    if ((pulsePosition > ((PIXELS_PER_STRIP / 2) - 1)) || (pulsePosition <= 0)) {
      pulseDirection *= -1;
    }
  }

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    for (uint8_t pixelIndex = pulsePosition; pixelIndex <= ((PIXELS_PER_STRIP - 1) - pulsePosition); pixelIndex++) {
      strip[stripIndex][pixelIndex] = color;
    }
  }
}

void resetPulseFill() {
  pulseGateCounter = 0;
  lastPulseGate = currentMillis;
  pulsePosition = 0;
  pulseDirection = UP;
  return;
}

// Retired: FillStars, XFill. See P_Retired.cpp.
