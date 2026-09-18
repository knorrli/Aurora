#include "Aurora.h"
#include "tempo.h"

#include <math.h>

/////////////////////////////////
// GENERATOR — one parametric pattern, driven entirely over CC.
//
// An experiment in replacing the fixed roster with a continuous space:
// a shape, repeated `count` times along each strip, optionally travelling,
// with the five strips optionally run out of step with each other. Most of
// the roster sits somewhere inside it — Sweep is a hard-edged shape at
// count 1, Rain is the same with the strips fanned, Starfield is many
// narrow shapes with jitter, Strobe is full width pulsed to zero.
//
// Everything is recomputed each frame from tempo::beats(), so there is no
// state to reset and a tempo change is a change of rate, not a jump.
//
// The question this exists to answer is whether the space *between* the
// known-good settings is interesting or is mush. It is not meant to
// replace the hand-written patterns until that is answered on the wall.
/////////////////////////////////

// Speed is an absolute distance per beat, because it is motion through real
// space and should not change when the count does. Width, edge and tail are
// proportions of the shape instead — measured in pixels, their useful range
// collapses as the shapes get narrower, and most of each slider's travel
// stops doing anything.
#define GEN_MAX_COUNT 20
#define GEN_MAX_SPEED_PIXELS_PER_BEAT 60.0f
#define GEN_SLOWEST_PULSE_BEATS 16.0f
#define GEN_PULSE_RATE_OCTAVES 6.0f
#define GEN_JITTER_REROLLS_PER_BEAT 4.0f

// Each pixel averages this many samples across its own width. Point-sampling
// at the pixel centre aliases once a cell is only a pixel or two across: the
// shape strobes as it moves instead of fading out. Averaging makes detail
// finer than the strip can resolve wash out smoothly, which is what it
// should do.
#define GEN_SUBSAMPLES 4

static float genWidth = 0.3f;
static uint8_t genCount = 1;
static float genEdge = 0.15f;
static float genTail = 0.0f;
static float genSpeedPixels = 0.0f;
static float genFan = 0.0f;
static float genJitter = 0.0f;
static float genPulseDepth = 0.0f;
static float genPulseBeats = 4.0f;
static float genPulseShape = 1.0f;
static bool genAlternate = false;
static bool genBounce = false;

static inline float fract(float x) { return x - floorf(x); }

// A phase derived as `beats * rate` teleports whenever the rate changes,
// because beats is large and only grows: a small change of rate is a large
// change of product. Carrying an offset across each change keeps the phase
// where it was and alters only how fast it advances from there — which is
// what makes a rate reachable with a fader instead of only at setup time.
struct PhaseTracker {
  float offset;
  float rate;
};

static float trackedPhase(PhaseTracker &tracker, float beats, float rate) {
  if (rate != tracker.rate) {
    tracker.offset += beats * (tracker.rate - rate);
    tracker.rate = rate;
  }
  return beats * rate + tracker.offset;
}

static PhaseTracker travelPhase = { 0.0f, 0.0f };
static PhaseTracker pulsePhase = { 0.0f, 0.0f };

static inline float ccUnit(uint8_t value) { return (float)value / 127.0f; }

// Stable per-pixel noise: the same (strip, pixel, bucket) always hashes to
// the same byte, so jitter holds still between re-rolls instead of boiling.
static inline uint8_t hash8(uint8_t a, uint8_t b, uint8_t c) {
  uint32_t h = (uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u ^ (uint32_t)c * 83492791u;
  h ^= h >> 13;
  h *= 0x5bd1e995u;
  h ^= h >> 15;
  return (uint8_t)h;
}

// `d` is distance behind the head within one cell, 0..1.
//
// `width` is the solid core. `edge` and `tail` both reach outward from it
// into the gap rather than eating into it, so softening a shape never makes
// it smaller. Both are scaled by the gap that is actually available, which
// means edge at full always closes the gaps to the neighbouring shapes — the
// two fades meet at zero and never have to be summed.
static float shapeAt(float d, float width, float edge, float tail) {
  const float halfCore = width * 0.5f;
  const float gap = 1.0f - width;
  const float spread = edge * gap * 0.5f;

  // Signed distance from the core's centre, wrapped into one cell, so the
  // fade is symmetric across the boundary instead of stopping at it.
  float offset = d - halfCore;
  if (offset > 0.5f) offset -= 1.0f;
  else if (offset < -0.5f) offset += 1.0f;

  const float distance = fabsf(offset);
  if (distance <= halfCore) return 1.0f;

  const float beyond = distance - halfCore;

  float brightness = 0.0f;
  if (spread > 0.0001f && beyond < spread) {
    const float k = 1.0f - (beyond / spread);
    brightness = k * k * (3.0f - 2.0f * k);
  }

  // Positive offset is the trailing side, so the tail only ever falls behind.
  if (tail > 0.0001f && offset > 0.0f) {
    const float tailLength = tail * gap;
    if (beyond < tailLength) {
      const float k = 1.0f - (beyond / tailLength);
      const float trailing = k * k;
      if (trailing > brightness) brightness = trailing;
    }
  }

  return brightness;
}

void Generator(CHSV color) {
  const float beats = tempo::beats();
  const float cellLength = (float)PIXELS_PER_STRIP / (float)genCount;
  const uint8_t jitterBucket = (uint8_t)(beats * GEN_JITTER_REROLLS_PER_BEAT);

  float travel;
  float direction;
  if (genBounce && fabsf(genSpeedPixels) > 0.0001f) {
    const float rate = fabsf(genSpeedPixels) / (2.0f * (float)PIXELS_PER_STRIP);
    const float triangle = fract(trackedPhase(travelPhase, beats, rate));
    const bool rising = triangle < 0.5f;
    travel = (rising ? (triangle * 2.0f) : ((1.0f - triangle) * 2.0f)) * (float)genCount;
    direction = rising ? 1.0f : -1.0f;
  } else {
    travel = trackedPhase(travelPhase, beats, genSpeedPixels / cellLength);
    direction = (genSpeedPixels >= 0.0f) ? 1.0f : -1.0f;
  }

  const float pulse = trackedPhase(pulsePhase, beats, 1.0f / genPulseBeats);

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    const float stripPhase = genFan * ((float)stripIndex / (float)NUMBER_OF_STRIPS);

    float stripDirection = direction;
    if (genAlternate && (stripIndex & 1)) stripDirection = -stripDirection;

    // The pulse drives brightness only. Letting it drive width too made the
    // shape retract toward its head as it shrank, so a swell read as a fill
    // from one end of the strip rather than as the whole thing breathing.
    const float lfo = 0.5f - 0.5f * cosf(2.0f * (float)PI * fract(pulse + stripPhase));

    // Steepening the sine toward a square is what makes a strobe reachable;
    // no amount of depth on a sine ever produces an on/off edge.
    const float softness = 0.02f * powf(50.0f, genPulseShape);
    float shaped = (lfo - 0.5f) / softness + 0.5f;
    if (shaped < 0.0f) shaped = 0.0f;
    else if (shaped > 1.0f) shaped = 1.0f;

    const float swell = 1.0f - genPulseDepth + genPulseDepth * shaped;

    const float width = genWidth;
    const float head = fract(travel + stripPhase);

    for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
      float jitterOffset = 0.0f;
      float jitterLevel = 1.0f;
      if (genJitter > 0.0001f) {
        const uint8_t offsetNoise = hash8(stripIndex, pixelIndex, jitterBucket);
        jitterOffset = genJitter * ((offsetNoise / 255.0f) - 0.5f);
        const uint8_t levelNoise = hash8(pixelIndex, stripIndex, jitterBucket ^ 0x5A);
        jitterLevel = 1.0f - genJitter * (levelNoise / 255.0f);
      }

      float accumulated = 0.0f;
      for (uint8_t sampleIndex = 0; sampleIndex < GEN_SUBSAMPLES; sampleIndex++) {
        const float samplePosition =
            (float)pixelIndex + ((float)sampleIndex + 0.5f) / (float)GEN_SUBSAMPLES;
        const float withinCell = fract(samplePosition / cellLength);
        const float d = (stripDirection >= 0.0f) ? fract(head - withinCell)
                                                 : fract(withinCell - head);
        accumulated += shapeAt(fract(d + jitterOffset), width, genEdge, genTail);
      }

      const float brightness =
          (accumulated / (float)GEN_SUBSAMPLES) * jitterLevel * swell;
      if (brightness <= 0.002f) continue;

      // Scaling the RGB rather than handing a low value to CHSV keeps the hue
      // where it was set: converting at a low value lets a channel truncate to
      // zero before its neighbour, which is what turns a dim yellow red.
      CRGB lit = CHSV(color.hue, color.saturation, 255);
      strip[stripIndex][pixelIndex] =
          lit.nscale8_video((uint8_t)((float)color.value * brightness));
    }
  }
}

void setGeneratorWidth(uint8_t value) { genWidth = ccUnit(value); }
void setGeneratorEdge(uint8_t value) { genEdge = ccUnit(value); }
void setGeneratorTail(uint8_t value) { genTail = ccUnit(value); }
void setGeneratorFan(uint8_t value) { genFan = ccUnit(value); }
void setGeneratorJitter(uint8_t value) { genJitter = ccUnit(value); }
void setGeneratorPulseDepth(uint8_t value) { genPulseDepth = ccUnit(value); }
void setGeneratorPulseShape(uint8_t value) { genPulseShape = ccUnit(value); }

void setGeneratorCount(uint8_t value) {
  genCount = 1 + (uint8_t)((uint16_t)value * (GEN_MAX_COUNT - 1) / 127);
}

// Bipolar around 64, squared so the slow end — where every pattern in the
// roster actually lives — gets most of the travel.
void setGeneratorSpeed(uint8_t value) {
  const float x = ((float)value - 64.0f) / 63.0f;
  genSpeedPixels = (x < 0.0f ? -1.0f : 1.0f) * x * x * GEN_MAX_SPEED_PIXELS_PER_BEAT;
}

void setGeneratorPulseRate(uint8_t value) {
  genPulseBeats = GEN_SLOWEST_PULSE_BEATS * powf(0.5f, ccUnit(value) * GEN_PULSE_RATE_OCTAVES);
}

void setGeneratorFlags(uint8_t value) {
  genAlternate = value & GEN_FLAG_ALTERNATE;
  genBounce = value & GEN_FLAG_BOUNCE;
}
