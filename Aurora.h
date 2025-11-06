#include <FastLED.h>
#include <TouchScreen.h>

// GLOBAL SETTINGS
#define UP 1
#define DOWN -1

// DIGITAL PINS
#define PIN_LED_OUTPUT 2
#define PIN_TEMPO 3
#define PIN_TOUCHPAD_EFFECT_MODE 4
#define PIN_HOLD_MODE 5
#define PIN_TOUCHPAD_XP 6
#define PIN_TOUCHPAD_YM 7
// 8-12 used for keypad
#define PIN_TEMPO_LED 13

// ANALOG PINS
#define PIN_FADER_SATURATION A0
#define PIN_FADER_HUE A1
#define PIN_FADER_VALUE A2
#define PIN_TRIGGER_INPUT A3
#define PIN_TOUCHPAD_YP A4
#define PIN_TOUCHPAD_XM A5
#define PIN_TOUCHPAD_STRIP_MODE A6
#define PIN_FADER_AND_PRESET_MODE A7

// LED STRIP SETTINGS
#define NUMBER_OF_STRIPS 5
#define PIXELS_PER_STRIP 45
#define MAX_BRIGHTNESS 255

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
#define TEMPO_GATE_READ_DURATION 100
#define TEMPO_GATE_DURATION TEMPO_GATE_READ_DURATION

// TOUCHPAD
#define TOUCHPAD_VERTICAL_MODE_SATURATION 0
#define TOUCHPAD_VERTICAL_MODE_COLOR 1
#define TOUCHPAD_STRIP_MODE_MIRRORED 0
#define TOUCHPAD_STRIP_MODE_MIRRORED_EXCLUSIVE 1
#define TOUCHPAD_STRIP_MODE_ALL 2
#define TOUCHPAD_EFFECT_MODE_FILL 0
#define TOUCHPAD_EFFECT_MODE_INVERT 1

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
extern uint8_t brightness;
extern uint8_t touchpadVerticalMode;
extern uint8_t currentPreset;
extern uint8_t selectedPreset;
extern uint8_t previousPreset;
extern CHSV touchColor;
extern CHSV presetColor;
extern bool muted;
extern bool faderAltModeEnabled;
extern bool presetAltModeEnabled;
extern uint8_t touchpadStripMode;
extern bool touchpadEffectMode;
extern bool holdModeEnabled;

// LED FRAMEBUFFERS
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
// FUNCTIONS
// ------------------------

// --- HELPERS
extern bool isFaderAlternativeMode();
extern void renderColorIndicators();
extern CHSV randomColor();
extern void setBootSettings();
extern void showBootIndicatorReady();
extern void readTempoGates();
extern void readInputSwitches();
extern uint8_t mirroredStrip(uint8_t stripIndex);

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
extern void renderTouchpad();
extern void renderTouchAction();
extern void affectExclusive(TSPoint touchPosition);
extern void affectAll();
extern void affectMirrored(TSPoint touchPosition);
extern void fill(TSPoint touchPosition);
extern void fillAll();
extern void fillMirrored(TSPoint touchPosition);
extern void invert(TSPoint touchPosition);
extern void invertAll();
extern void invertMirrored(TSPoint touchPosition);
extern void invertExclusive(TSPoint touchPosition);
extern void invertExclusiveMirrored();
extern void renderTouchPosition(TSPoint touchPosition);
extern void invertStrip(uint8_t stripIndex);
extern void clearUntouchedStrips(TSPoint touchPosition);

// --- INPUT Trigger
extern bool readTrigger();
// --- RENDER Trigger
extern void renderTrigger();

// --- INPUT Touch
extern void readTouchPadMode();
extern void readTouchpad();
extern void setTouchColor();
extern TSPoint calculateGridPosition(TSPoint position);
extern bool isTouched();

// --- PRESETS
extern void FillStrips(CHSV color);
extern void FillStars(CHSV color);
extern void PulseFill(CHSV color);
extern void resetPulseFill();
extern void XFill(CHSV color);
extern void resetXFill();
extern void RisingBlocks(CHSV color);
extern void RisingStars(CHSV color);
extern void FallingBlocks(CHSV color);
extern void FallingStars(CHSV color);
extern void MovingBlocks(CHSV color, uint8_t fillLength, uint8_t gap, int8_t direction = UP);
extern void resetMovingBlocks();
extern void Bars(CHSV color);
extern void resetBars();
extern void RainFall(CHSV color);
extern void RainBounce(CHSV color);
extern void Rain(CHSV color, bool changeDirectionOnEnds);
extern void resetRain();
extern void Invert(CHSV color);
extern void resetInvert();
extern void StripByStripOrdered(CHSV color);
extern void StripByStripRandom(CHSV color);
extern void StripByStrip(CHSV color, uint8_t order[]);
extern void StripByStripMirrored(CHSV color);
extern void resetStripByStrip();
extern void StrobeStrips(CHSV color);
extern void resetStrobe();
extern void StrobeUpDown(CHSV color);
extern void Chaos(CHSV color);
extern void resetChaos();
extern void AlbtraumBreath();
extern void AlbtraumChaossaft();
extern void AlbtraumParty();
extern void TransitionRainbow();
extern void TransitionStrobo();
extern void ErnstBreath();
extern void ErnstParty();
extern void ErnstDark();
extern void Breath(uint8_t &hue, uint8_t min_hue, uint8_t max_hue);