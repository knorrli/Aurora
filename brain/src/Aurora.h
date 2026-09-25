#include <FastLED.h>
#include <render.h>
#include "aurora_protocol.h"

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

// TEMPO — published by the main loop from the tempo module.
extern bool tempoGate;
extern unsigned long currentMillis;
extern unsigned long lastGateMillis;
extern unsigned long elapsedLoopTime;

// STATE
extern uint8_t currentPreset;
extern uint8_t selectedPreset;

// LED FRAMEBUFFERS
extern CRGBArray<NUM_PIXELS_TOTAL> pixels;
extern CRGBSet strips;
extern struct CRGB * strip[NUMBER_OF_STRIPS];

// ------------------------
// FUNCTIONS
// ------------------------

// --- HELPERS
extern void showBootIndicatorReady();
extern void ShowStripOrder();
extern void showRendered(const render::Rgb *rendered);

// --- RENDER Preset
extern void renderPreset(uint8_t preset);
extern void resetPreset(uint8_t preset);

// --- RENDER Tempo
extern void renderTempo();

// --- INPUT / RENDER Trigger
extern void fireTrigger();
extern void renderTrigger();

// The parametric generator — see shared/render/generator.cpp. The washes
// take its washes while it is the pattern being drawn.
extern void Generator();
extern const render::Wash *generatorWashes();
extern void resetGenerator();
