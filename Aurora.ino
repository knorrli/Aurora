#include "aurora.h"

// TEMPO
bool tempoGate = LOW; // 1 byte
unsigned long currentMillis = 0;
unsigned long lastGateMillis = 0; // 4 bytes
unsigned long currentTempo = 0;
unsigned long ticks = 0;
uint16_t currentTick = 0;
uint8_t tickCounter = 0;

// STATE
bool linkModeEnabled = 0;
uint8_t currentPreset = 0; // 1 byte
uint8_t lastPreset = 0; // 1 byte
CHSV touchColor = CHSV(0, 0, 0);
CHSV presetColor = CHSV(0, 0, 0);

// LED FRAMEBUFFER
CRGBArray<NUM_PIXELS_TOTAL> pixels;
CRGBSet touchpad(pixels + PIXEL_INDEX_TOUCHPAD_START, NUM_PIXELS_TOUCHPAD);
CRGBSet strips(pixels + PIXEL_INDEX_STRIP_START, NUM_PIXELS_STRIP);
struct CRGB * strip[NUMBER_OF_STRIPS];


void setup() {
//  Serial.begin(9600);
  delay(1500); // Boot recovery, let it breathe

  pinMode(PIN_TEMPO, INPUT);
  pinMode(PIN_LED_OUTPUT, OUTPUT);
  pinMode(PIN_TEMPO_LED, OUTPUT);

  pinMode(PIN_FADER_RED, INPUT);
  pinMode(PIN_FADER_GREEN, INPUT);
  pinMode(PIN_FADER_BLUE, INPUT);
  pinMode(PIN_FADER_MODE, INPUT);

  pinMode(PIN_EFFECTS_MODE, INPUT);
  pinMode(PIN_TOUCH_MODE, INPUT);
  pinMode(PIN_LINK_MODE, INPUT);

  for (int stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    uint8_t pixelStartIndex = PIXEL_INDEX_STRIP_START + (stripIndex * PIXELS_PER_STRIP);
    strip[stripIndex] = pixels(pixelStartIndex, PIXELS_PER_STRIP - 1);
  }

  FastLED.setBrightness(BRIGHTNESS);

  FastLED.addLeds<NEOPIXEL, PIN_LED_OUTPUT>(pixels, NUM_PIXELS_TOTAL);

  showBootIndicatorReady();
}

void loop() {
  currentMillis = millis();
  tempoGate = readTempoGate();
  perform();
  render();
  isTempoDivision(4);
  if (tempoGate) {
    currentTick = 0;
    tickCounter = 0;
    ticks = (currentTempo / (millis() - currentMillis));
  } else {
    currentTick = min(currentTick + 1, ticks-1);
  }
}

void perform() {
  setCurrentColor();
  readPreset();
  readTouchInputs();
}

void render() {
  FastLED.clear(false);
  renderPreset(currentPreset);
  renderTouchControl();
  if (linkModeEnabled) {
    for (uint8_t stripIndex = 0; stripIndex < (NUMBER_OF_STRIPS / 2); stripIndex++) {
      for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
        strip[(NUMBER_OF_STRIPS - stripIndex - 1)][pixelIndex] = strip[stripIndex][pixelIndex];
      }
    }
  }
  renderTrigger();

  FastLED.show();
  renderTempo();
}
