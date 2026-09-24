#include <FastLED.h>
#include <render.h>
#include "aurora_protocol.h"

// GLOBAL SETTINGS
#define UP 1
#define DOWN -1

// PINS — Teensy 4.0; see docs/wiring.md "Brain pin map — Teensy 4.0"
#define PIN_LED_OUTPUT 2
#define PIN_TEMPO_LED LED_BUILTIN

// LED STRIP SETTINGS
#define NUMBER_OF_STRIPS 5
#define PIXELS_PER_STRIP 45
#define MAX_BRIGHTNESS 255
#define NUM_PIXELS_TOTAL (NUMBER_OF_STRIPS * PIXELS_PER_STRIP)

// TEMPO
#define TEMPO_LED_PULSE_MS 40

// ------------------------
// VARIABLES
// ------------------------

// TEMPO — published by the main loop from the tempo module, in the shape
// the presets were written against. Retired preset by preset as each moves
// to tempo::cyclePosition(); see TODO.md Phase 3.
extern bool tempoGate;
extern unsigned long currentMillis;
extern unsigned long lastGateMillis;
extern unsigned long currentTempo;
extern unsigned long elapsedLoopTime;

// STATE
extern uint8_t brightness;
extern uint8_t currentPreset;
extern uint8_t selectedPreset;
extern uint8_t previousPreset;
extern CHSV presetColor;
extern bool faderAltModeEnabled;
extern bool presetAltModeEnabled;

// LED FRAMEBUFFERS
extern CRGBArray<NUM_PIXELS_TOTAL> pixels;
extern CRGBSet strips;
extern struct CRGB * strip[NUMBER_OF_STRIPS];

struct PositionDirection {
  uint8_t stripIndex;
  uint8_t pixelIndex;
  int8_t direction;
};

struct PositionColor {
  uint8_t stripIndex;
  uint8_t pixelIndex;
  CRGB color;
};

// ------------------------
// FUNCTIONS
// ------------------------

// --- HELPERS
extern CHSV randomColor();
extern void showBootIndicatorReady();
extern uint8_t mirroredStrip(uint8_t stripIndex);
extern void ShowStripOrder();
extern void showRendered(const render::Rgb *rendered);

// --- INPUT Color
extern void setCurrentColor();

// --- RENDER Preset
extern void renderPreset(uint8_t preset);
extern void resetPreset(uint8_t preset);

// --- RENDER Tempo
extern void renderTempo();

// --- INPUT / RENDER Trigger
extern void fireTrigger();
extern void renderTrigger();

// --- PRESETS
// Row 1 (ambient)
extern void FillStrips(CHSV color);
extern void Starfield(CHSV color);
extern void Breathe(CHSV color);
extern void Wave(CHSV color);
extern void Plasma(CHSV color);
extern void Aurora(CHSV color);
// Row 2 (groove)
extern void PulseFill(CHSV color);
extern void resetPulseFill();
extern void Bars(CHSV color);
extern void resetBars();
extern void Sweep(CHSV color);
extern void CrossSweep(CHSV color);
extern void resetCrossSweep();
extern void RainFall(CHSV color);
extern void Storm(CHSV color);
extern void resetStorm();
// Row 3 (intensity)
extern void StripByStripOrdered(CHSV color);
extern void Comet(CHSV color);
extern void resetComet();
extern void StrobeStrips(CHSV color);
extern void Stutter(CHSV color);
extern void resetStutter();
extern void Chaos(CHSV color);
extern void resetChaos();
extern void Glitch(CHSV color);
// The parametric generator — see shared/render/generator.cpp. The washes
// take its wash while it is the pattern being drawn.
extern void Generator(CHSV color);
extern const render::Wash &generatorWash();
// Shared helpers (still active)
extern void MovingBlocks(CHSV color, uint8_t fillLength, uint8_t gap, int8_t direction = UP);
extern void resetMovingBlocks();
extern void Rain(CHSV color, bool changeDirectionOnEnds);
extern void resetRain();
extern void StripByStrip(CHSV color, uint8_t order[]);
extern void StripByStripMirrored(CHSV color);
extern void resetStripByStrip();
extern void StrobeUpDown(CHSV color);
extern void resetStrobe();
