#include <FastLED.h>
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

// Where the pulse reaches. One oscillator with one rate; each destination
// carries its own amount and its own wave, so the washes can breathe while
// the strips strobe. CC numbers are in shared/aurora_protocol.h; the order
// here is the order that block is laid out in.
enum PulseTarget : uint8_t {
  PULSE_TO_LIGHT = 0,   // the strips' own brightness — the pulse's home
  PULSE_TO_WIDTH,
  PULSE_TO_HUE,         // one push on the color layer's summed output
  PULSE_TO_PAR_LEVEL,
  PULSE_TO_PAR_HUE,
  PULSE_TO_PAR_SAT,
  PULSE_TARGET_COUNT,
};

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
// The parametric generator — shape comes from CC 70–80, 100–114 and 115, not from here
extern void Generator(CHSV color);
extern void setGeneratorAlternate(uint8_t value);
extern void setGeneratorBounce(uint8_t value);
// The pulse's destinations — CC 77/79/80 for the strips' brightness and
// 100–114 for the rest. An amount of zero is a destination the pulse is not
// using, never a connection that is not made.
extern void setPulseAmount(uint8_t target, uint8_t value);
extern void setPulseShape(uint8_t target, uint8_t value);
extern void setPulseSkew(uint8_t target, uint8_t value);
// The color layer — CC 41–57, and the scatter at 83–91. Hue, whiteness and
// darkness pushed away from the three faders by a placed field, a wander, the
// light level and the scatter. With every one centered the wall is exactly what
// the faders say. See P_Generator.cpp § "The color layer".
extern void setColorRegion(uint8_t value);
extern void setColorRuler(uint8_t value);
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
