#include <FastLED.h>
#include <TouchScreen.h>

// ------------------------
// DEFINES
// ------------------------

// GLOBAL SETTINGS
#define UP 1
#define DOWN -1

// LED STRIP SETTINGS
#define NUMBER_OF_STRIPS 5
#define PIXELS_PER_STRIP 45
#define MAX_BRIGHTNESS 255

// LED SETTINGS
#define PIN_LED_OUTPUT 2

// PIXEL INDICES
#define PIXEL_INDEX_TOUCH_COLOR 0
#define PIXEL_INDEX_TOUCHPAD_START 1
#define PIXEL_INDEX_TOUCHPAD_END 10
#define PIXEL_INDEX_PRESET_COLOR 11
#define PIXEL_INDEX_STRIP_START 12
#define PIXEL_INDEX_STRIP_END PIXEL_INDEX_STRIP_START + (NUMBER_OF_STRIPS * PIXELS_PER_STRIP)

#define NUM_PIXELS_TOUCHPAD (PIXEL_INDEX_TOUCHPAD_END - PIXEL_INDEX_TOUCHPAD_START) + 1
#define NUM_PIXELS_STRIP (NUMBER_OF_STRIPS * PIXELS_PER_STRIP)
#define NUM_PIXELS_TOTAL (1 + NUM_PIXELS_TOUCHPAD + 1 + NUM_PIXELS_STRIP)

// TEMPO
#define PIN_TEMPO 3
#define PIN_TEMPO_LED 13
#define TEMPO_GATE_READ_DURATION 100
#define TEMPO_GATE_DURATION TEMPO_GATE_READ_DURATION

// COLOR FADERS
#define PIN_FADER_SATURATION A0
#define PIN_FADER_HUE A1
#define PIN_FADER_VALUE A2
#define PIN_FADER_MODE A7

// TRIGGER
#define PIN_TRIGGER_INPUT A3

// TOUCHSCREEN
#define PIN_HOLD_MODE A6
#define PIN_PRESET_MODE 4
#define PIN_VARIATION_MODE 5

// ------------------------
// VARIABLES
// ------------------------

// TEMPO
extern bool tempoGate;
extern unsigned long currentMillis;
extern unsigned long lastGateMillis;
extern unsigned long currentTempo;
extern unsigned long expectedNextGate;
extern unsigned long elapsedLoopTime;

// STATE
extern uint8_t currentPreset;
extern uint8_t selectedPreset;
extern CHSV touchColor;
extern CHSV presetColor;
extern bool muted;
extern bool holdModeEnabled;
extern bool presetAltModeEnabled;
extern bool variationModeEnabled;

// LED FRAMEBUFFER
extern CRGBArray<NUM_PIXELS_TOTAL> pixels;
extern CRGBSet touchpad;
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
// VARIABLES
// ------------------------

// --- HELPERS
extern bool isFaderAlternativeMode();
extern void renderColorIndicators();
extern CHSV randomColor();
extern uint8_t readBrightness();
extern void showBootIndicatorReady();
extern void readTempoGates();
/* extern void applyRainbow(); */

// --- INPUT ColorFaders
extern void setCurrentColor();
extern uint8_t readHue();
extern uint8_t readSaturation();
extern uint8_t readValue();

// --- INPUT Preset
extern void readPreset();
extern int8_t readKeypad();

// --- RENDER Preset
extern void renderPreset(uint8_t preset);
extern void resetPreset(uint8_t preset);

// --- RENDER Tempo
extern void renderTempo();

// --- RENDER Touchpad
extern void renderTouchControl();
extern void renderTouchpad();
extern void renderTouchAction();
extern void invertPresetPattern(TSPoint touchPosition);
extern void fillStripMirrored(TSPoint touchPosition);
extern void renderTouchPosition(TSPoint touchPosition);

// --- INPUT Trigger
extern bool readTrigger();
// --- RENDER Trigger
extern void renderTrigger();

// --- INPUT Touch
extern void readTouchInputs();
extern void setTouchColor();
extern TSPoint calculateGridPosition(TSPoint position);
extern bool isTouched();



// --- PRESETS
extern void FillStrips(CHSV color);
extern void FillStars(CHSV color);
extern void Pulse(CHSV color);
extern void resetPulse();
extern void XFill(CHSV color);
extern void resetXFill();
extern void RisingLines(CHSV color);
extern void RisingStars(CHSV color);
extern void FallingLines(CHSV color);
extern void FallingStars(CHSV color);
extern void MoveFill(CHSV color);
extern void MoveFill(CHSV color, uint8_t fillLength, uint8_t gap, int8_t direction = UP);
extern void resetMove();
extern void Bars(CHSV color);
extern void resetBars();
extern void RainFall(CHSV color);
extern void RainBounce(CHSV color);
extern void Rain(CHSV color, bool changeDirectionOnEnds);
extern void resetRain();
extern void Invert(CHSV color);
extern void resetInvert();
extern void OneOnOneOrdered(CHSV color);
extern void OneOnOneRandom(CHSV color);
extern void OneOnOne(CHSV color, uint8_t order[]);
extern void resetOneOnOne();
extern void StrobeStrips(CHSV color);
extern void StrobeMirrored(CHSV color);
extern void resetStrobe();
extern void StrobeUpDown(CHSV color);
extern void Chaos(CHSV color);
extern void resetChaos();
