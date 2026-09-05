#include "aurora.h"

#include "midi_in.h"
#include "tempo.h"

// TEMPO
bool tempoGate = LOW;
unsigned long currentMillis = 0;
unsigned long lastGateMillis = 0;
unsigned long currentTempo = 500;
unsigned long elapsedLoopTime = 0;

// STATE
uint8_t brightness = MAX_BRIGHTNESS;
uint8_t currentPreset = 0;
CHSV presetColor = CHSV(0, 0, 0);
bool faderAltModeEnabled = false;
bool presetAltModeEnabled = false;

// LED FRAMEBUFFER
CRGBArray<NUM_PIXELS_TOTAL> pixels;
CRGBSet strips(pixels, NUM_PIXELS_TOTAL);
struct CRGB *strip[NUMBER_OF_STRIPS];

void setup()
{
  Serial.begin(115200);
  delay(500); // Boot recovery

  pinMode(PIN_TEMPO_LED, OUTPUT);

  for (int stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++)
  {
    strip[stripIndex] = pixels + (stripIndex * PIXELS_PER_STRIP);
  }

  FastLED.setBrightness(brightness);
  FastLED.addLeds<NEOPIXEL, PIN_LED_OUTPUT>(pixels, NUM_PIXELS_TOTAL);

  midi_in::begin();
  tempo::begin();

  showBootIndicatorReady();
}

void loop()
{
  midi_in::tick();
  tempo::tick();

  currentMillis = millis();
  tempoGate = tempo::pulsed();
  currentTempo = tempo::beatLengthMs();
  if (tempoGate) lastGateMillis = currentMillis;

  perform();
  render();

  elapsedLoopTime = millis() - currentMillis;

#ifdef AURORA_DEBUG
  reportState();
#endif
}

void perform()
{
  setCurrentColor();

  if (tempoGate && (currentPreset != selectedPreset))
  {
    previousPreset = currentPreset;
    currentPreset = selectedPreset;
  }
}

void render()
{
  FastLED.clear(false);
  renderPreset(currentPreset);
  renderTrigger();

  FastLED.show();
  renderTempo();
}

#ifdef AURORA_DEBUG
void reportState()
{
  if (!tempoGate) return;

  Serial.printf("pulse  %.1f BPM  %u tk/beat  beat %.2f  preset %u%s  hsv %u/%u/%u  frame %lu ms%s\n",
                tempo::bpm(), tempo::ticksPerAnimationBeat(), tempo::beats(),
                currentPreset, presetAltModeEnabled ? " alt" : "",
                presetColor.hue, presetColor.saturation, presetColor.value,
                elapsedLoopTime, tempo::running() ? "" : "  [stopped]");
}
#endif
