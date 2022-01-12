#include "aurora.h"

// TEMPO
bool tempoGate = LOW; // 1 byte
unsigned long currentMillis = 0;
unsigned long lastGateMillis = 0; // 4 bytes
unsigned long currentTempo = 100;
unsigned long ticks = 0;
uint16_t currentTick = 0;
bool stripLengthGate = LOW;
uint16_t stripLengthTick = 0;
uint16_t stripGateTempoDivision = 100;

// STATE
uint8_t currentPreset = 0; // 1 byte
CHSV touchColor = CHSV(0, 0, 0);
CHSV presetColor = CHSV(0, 0, 0);

// LED FRAMEBUFFER
CRGBArray<NUM_PIXELS_TOTAL> pixels;
CRGBSet touchpad(pixels + PIXEL_INDEX_TOUCHPAD_START, NUM_PIXELS_TOUCHPAD);
CRGBSet strips(pixels + PIXEL_INDEX_STRIP_START, NUM_PIXELS_STRIP);
struct CRGB * strip[NUMBER_OF_STRIPS];


void setup() {
//  Serial.begin(150200);
  delay(1500); // Boot recovery

  pinMode(PIN_TEMPO, INPUT);
  pinMode(PIN_LED_OUTPUT, OUTPUT);
  pinMode(PIN_TEMPO_LED, OUTPUT);

  pinMode(PIN_FADER_HUE, INPUT);
  pinMode(PIN_FADER_SATURATION, INPUT);
  pinMode(PIN_FADER_VALUE, INPUT);
  pinMode(PIN_FADER_MODE, INPUT);

  pinMode(PIN_HOLD_MODE, INPUT);
  pinMode(PIN_PRESET_MODE, INPUT);
  pinMode(PIN_VARIATION_MODE, INPUT);

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
  if (stripLengthTick > stripGateTempoDivision) {
    stripLengthGate = HIGH;
    stripLengthTick = 0;
  } else {
    stripLengthGate = LOW;
  }
  perform();
  render();
  if (tempoGate) {
    currentTick = 0;
    stripLengthTick = 0;
    ticks = (currentTempo / (millis() - currentMillis));
    stripGateTempoDivision = (currentTempo / ((PIXELS_PER_STRIP-1) * 4.225));
  } else {
    currentTick = min(currentTick + 1, ticks - 1);
    stripLengthTick++;
  }
}

void perform() {
  setCurrentColor();
  readPreset();
  readTouchInputs();
}

void render() {
  FastLED.clear(false);
  renderColorIndicators();
  renderPreset(currentPreset);
  if (analogRead(PIN_FADER_MODE) > 511) {
    applyRainbow();
  }
  renderTouchAction();
  renderTrigger();

  FastLED.show();
  renderTempo();
}
