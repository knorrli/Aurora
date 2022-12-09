#include "aurora.h"

// TEMPO
bool tempoGate = LOW;
unsigned long currentMillis = 0;
unsigned long lastGateMillis = 0;
unsigned long currentTempo = 5500;
unsigned long expectedNextGate = millis() + currentTempo;
unsigned long elapsedLoopTime = 0;

// STATE
bool muted = false;
uint8_t currentPreset = 0;
CHSV touchColor = CHSV(0, 0, 0);
CHSV presetColor = CHSV(0, 0, 0);

// LED FRAMEBUFFER
CRGBArray<NUM_PIXELS_TOTAL> pixels;
CRGBSet touchpad(pixels + PIXEL_INDEX_TOUCHPAD_START, NUM_PIXELS_TOUCHPAD);
CRGBSet strips(pixels + PIXEL_INDEX_STRIP_START, NUM_PIXELS_STRIP);
struct CRGB *strip[NUMBER_OF_STRIPS];

void setup()
{
  // Serial.begin(115200);
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

  for (int stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++)
  {
    uint8_t pixelStartIndex = PIXEL_INDEX_STRIP_START + (stripIndex * PIXELS_PER_STRIP);
    strip[stripIndex] = pixels(pixelStartIndex, PIXELS_PER_STRIP - 1);
  }

  uint8_t brightness = readBrightness();

  FastLED.setBrightness(brightness);

  FastLED.addLeds<NEOPIXEL, PIN_LED_OUTPUT>(pixels, NUM_PIXELS_TOTAL);

  showBootIndicatorReady();
}

void loop()
{
  currentMillis = millis();
  readTempoGates();

  perform();
  render();


  if (tempoGate)
  {
    elapsedLoopTime = millis() - currentMillis;
    currentTempo = millis() - lastGateMillis;
    expectedNextGate = millis() + currentTempo;
    lastGateMillis = millis();
  }
}

void perform()
{
  setCurrentColor();
  readPreset();
  if (tempoGate && (currentPreset != selectedPreset))
  {
    currentPreset = selectedPreset;
  }
  readTouchInputs();
}

void render()
{
  FastLED.clear(false);
  renderColorIndicators();
  if (!muted) {
    renderPreset(currentPreset);
    renderTouchAction();
  }
  renderTrigger();

  FastLED.show();
  renderTempo();
}
