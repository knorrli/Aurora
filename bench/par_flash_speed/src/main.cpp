// Flashes both PARs' dimmer channel against a strip on the same clock,
// stepping the flash length down, so the shortest flash that still reads
// as one can be found by eye. See TODO.md "Measure how fast a PAR can be
// played" for what each outcome means.
//
// Each step is announced on the reference strip: one pixel lit for step
// one, two for step two, and so on, before the flashing starts.

#include <Arduino.h>
#include <FastLED.h>
#include <TeensyDMX.h>

namespace teensydmx = ::qindesign::teensydmx;

teensydmx::Sender dmx{Serial4};

constexpr uint8_t kPinStrips = 2;
constexpr uint16_t kPixelsPerStrip = 45;

// Strip 3 is the reference because it stands between the two PARs, which
// is the only placement where the eye can judge two things as
// simultaneous.
//
// The whole chain is driven even though only strip 3 lights, so that the
// strips above it are blanked rather than left holding whatever the last
// firmware wrote. Stray light across the room costs the measurement more
// than the extra transmission does: 225 pixels block for some 7 ms
// against 4, and that only widens the head start the PAR already gets
// below, which is the harmless direction.
constexpr uint16_t kStripCount = 5;
constexpr uint16_t kPixelCount = kPixelsPerStrip * kStripCount;
constexpr uint16_t kReferenceFirst = kPixelsPerStrip * 2;

CRGB pixels[kPixelCount];

// 8-channel personality, so the blocks are 8 apart and the dimmer is the
// first channel of each. See docs/wiring.md "Fixture profile — BeamZ
// BCC145".
constexpr uint16_t kFixtures[] = { 1, 9 };
constexpr uint16_t kOffsetDimmer = 0;
constexpr uint16_t kOffsetStrobe = 1;
constexpr uint16_t kOffsetRed = 2;
constexpr uint16_t kOffsetGreen = 3;
constexpr uint16_t kOffsetBlue = 4;
constexpr uint16_t kOffsetWhite = 5;
constexpr uint16_t kOffsetMacro = 6;
constexpr uint16_t kOffsetSpeed = 7;

// The smallest packet the DMX spec allows at full refresh rate, and it
// still covers three fixture blocks. It puts a frame at roughly 1.2 ms
// against the 23 ms of the default 513-slot packet — which at the short
// end of this test would be the thing being measured.
constexpr int kPacketSize = 25;

constexpr uint16_t kFlashMs[] = { 200, 100, 50, 25, 12 };
constexpr uint32_t kStepMs = 3000;
constexpr uint32_t kRestMs = 1500;

// Matching the strip's colour keeps the brightness comparison honest. A
// PAR at full still dwarfs a strip; drop this if that makes the short
// flashes impossible to judge.
constexpr uint8_t kFlashLevel = 255;
const CRGB kColour = CRGB(255, 85, 0);

static void setDimmer(uint8_t level) {
  for (uint16_t base : kFixtures) dmx.set(base + kOffsetDimmer, level);
}

static void setReference(const CRGB &colour) {
  const uint16_t end = kReferenceFirst + kPixelsPerStrip;
  for (uint16_t i = kReferenceFirst; i < end; i++) pixels[i] = colour;
}

static void announce(uint8_t step) {
  fill_solid(pixels, kPixelCount, CRGB::Black);
  for (uint8_t i = 0; i < step; i++) pixels[kReferenceFirst + i] = CRGB::White;
  FastLED.show();
  delay(700);

  fill_solid(pixels, kPixelCount, CRGB::Black);
  FastLED.show();
  delay(400);
}

// DMX is set before the strip because the strip is the slower of the two:
// a frame reaches the fixture within about 1.2 ms, while FastLED blocks
// for some 7 ms before the pixels latch. So the PAR is given a head start
// of roughly 6 ms, and a PAR that still looks late is late for real.
static void edge(bool on) {
  setDimmer(on ? kFlashLevel : 0);
  setReference(on ? kColour : CRGB::Black);
  FastLED.show();
}

static void flashAt(uint16_t flashMs) {
  const uint32_t startedAt = millis();
  uint32_t nextEdge = micros();
  bool on = false;

  while (millis() - startedAt < kStepMs) {
    on = !on;
    edge(on);

    // Edges are held to a fixed grid rather than delayed for `flashMs`
    // after the work: the strip's transmission would otherwise be added
    // to every interval, stretching a 12 ms flash to 19.
    nextEdge += (uint32_t)flashMs * 1000u;
    while ((int32_t)(micros() - nextEdge) < 0) {}
  }

  edge(false);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  FastLED.addLeds<NEOPIXEL, kPinStrips>(pixels, kPixelCount);
  FastLED.setBrightness(255);
  fill_solid(pixels, kPixelCount, CRGB::Black);
  FastLED.show();

  dmx.begin();
  if (!dmx.setPacketSize(kPacketSize)) {
    Serial.println("setPacketSize refused — frames stay at 513 slots and "
                   "the short steps will measure the wire, not the fixture");
  }

  // The dimmer carries the flash, so colour is held at full and the
  // fixture's own effects are pinned off. Macro above 50 starts an auto
  // sequence that overrides colour entirely.
  for (uint16_t base : kFixtures) {
    dmx.set(base + kOffsetStrobe, 0);
    dmx.set(base + kOffsetRed, kColour.r);
    dmx.set(base + kOffsetGreen, kColour.g);
    dmx.set(base + kOffsetBlue, kColour.b);
    dmx.set(base + kOffsetWhite, 0);
    dmx.set(base + kOffsetMacro, 0);
    dmx.set(base + kOffsetSpeed, 0);
  }
  setDimmer(0);

  Serial.printf("packet size %d, fixtures at 1 and 9\n", dmx.packetSize());
}

void loop() {
  for (uint8_t i = 0; i < sizeof(kFlashMs) / sizeof(kFlashMs[0]); i++) {
    announce(i + 1);
    Serial.printf("step %u — %u ms on, %u ms off\n", i + 1, kFlashMs[i], kFlashMs[i]);
    flashAt(kFlashMs[i]);
    delay(kRestMs);
  }

  delay(2000);
}
