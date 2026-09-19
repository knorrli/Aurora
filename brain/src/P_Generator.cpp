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

// ---------------------------------------------------------------------------
// The colour field
//
// Plasma and Aurora were the same three lines: take a number from where you
// are and when it is, add it to the hue. The only thing separating them was
// where the number came from — two stacked sines, or Perlin noise — plus four
// scaling constants each had hardcoded. Those are the parameters below.
//
// The field is sampled per pixel and applied to whatever the shape above has
// lit, so a full-width fill with the field wound up is Plasma, and the same
// field under a travelling shape colours the shape as it moves through it.
// At zero depth it costs nothing and the wall is one flat colour.
// ---------------------------------------------------------------------------

#define FIELD_MIN_GRAIN 2.0f
#define FIELD_GRAIN_RANGE 48.0f   // 2 units per pixel at one end, 96 at the other
#define FIELD_MAX_SPREAD 51.0f          // five strips across one sine cycle
#define FIELD_NOISE_SPREAD_SCALE 6.5f   // ...and 1.3 noise cells per strip
#define FIELD_MAX_CYCLES_PER_BEAT 1.0f

// How dark the field pulls a trough at full depth, as a fraction of what the
// pixel would otherwise be — and the knob reaches it geometrically, so each
// equal step is an equal ratio of light rather than an equal subtraction.
// Mapped linearly, nearly the whole travel was imperceptible and everything
// worth having sat in the last three steps.
//
// It stops short of zero because a WS2812 has eight linear bits and no gamma:
// at the bottom one step is a third of the light, so brightness quantises into
// lurches, and pixels crossing to zero pop out entirely.
#define FIELD_MIN_LEVEL 0.02f

static float fieldGrain = 8.0f;
static float fieldSpread = 40.0f;
static float fieldRate = 0.0f;
static float fieldHueDepth = 0.0f;
static float fieldSatDepth = 0.0f;
static float fieldValDepth = 0.0f;
static float fieldSource = 0.0f;
static float fieldSoftness = 1.0f;

static PhaseTracker fieldPhase = { 0.0f, 0.0f };

// inoise8 adds 64 to the raw gradient and doubles it, which is calibrated for
// the +-64 a single gradient can theoretically reach. The value returned is a
// trilinear blend of eight of them, and blending pulls the result toward the
// middle, so the output never arrives at either end. Doubling again about the
// centre gives noise the same authority as the sines, which do fill the range.
static inline uint8_t expandFromCentre(uint8_t value) {
  const int16_t swung = 128 + ((int16_t)value - 128) * 2;
  if (swung < 0) return 0;
  if (swung > 255) return 255;
  return (uint8_t)swung;
}

// Five strips is five samples, against forty-five along a strip, so the across
// axis needs a far coarser step to show anything — and the two sources need
// different steps for the same reason they are different sources.
//
// Spread fans the strips out from the middle one rather than from the first,
// so winding it up opens the wall symmetrically instead of pinning strip 1 and
// leaving the last strip to do all the moving.
static uint8_t fieldAt(uint8_t stripIndex, uint8_t pixelIndex, uint16_t z) {
  const float fromCentre = (float)stripIndex - (float)(NUMBER_OF_STRIPS - 1) * 0.5f;
  const int32_t across = (int32_t)(fromCentre * fieldSpread);

  // Spread displaces the field ALONG the strip rather than shifting its level.
  // Shifting the level leaves every strip with its blobs at the same pixels
  // and only their colour differing, which reads as one striped pattern rather
  // than as a field with any depth in it.
  const int32_t along = (int32_t)((float)pixelIndex * fieldGrain) + across;

  // Each term drifts at its own fraction of the rate so they never settle into
  // a visible period. The halving has to happen on the wide counter and the
  // truncation afterwards: halving an already-truncated byte ramps it 0-127
  // and snaps back, which is a discontinuity in the middle of a sine.
  const uint8_t sines = (uint8_t)(((uint16_t)sin8((uint8_t)(along + z))
                                 + (uint16_t)sin8((uint8_t)((along >> 1) + (z >> 1)))
                                 + (uint16_t)sin8((uint8_t)(across + (z >> 2)))) / 3);

  // Perlin noise returns exactly its midpoint wherever the input lands on the
  // integer lattice, and FastLED's cells are 256 units wide. Stepping the
  // strips by a whole number of cells puts every one of them on the same
  // lattice line, and they come out sharing features however far apart they
  // are. So the step is deliberately not a multiple of 256, and the bias keeps
  // the middle strip off the lattice as well.
  const uint16_t noiseAcross =
      (uint16_t)(4200 + (int32_t)(fromCentre * fieldSpread * FIELD_NOISE_SPREAD_SCALE));
  const uint8_t noise = expandFromCentre(inoise8((uint16_t)(along + 4200), noiseAcross, z));

  return (uint8_t)((float)sines + ((float)noise - (float)sines) * fieldSource);
}

static CHSV fieldColor(CHSV base, uint8_t sample) {
  // A smooth ramp of brightness has no edge anywhere for the eye to catch, so
  // even a 50:1 range reads as barely there. Steepening the field toward a
  // hard boundary is the same move that makes a strobe reachable from a sine,
  // and it is what turns the field's regions into things you can see as
  // regions rather than as a general unevenness.
  float unit = ((float)sample / 255.0f - 0.5f) / fieldSoftness + 0.5f;
  if (unit < 0.0f) unit = 0.0f;
  else if (unit > 1.0f) unit = 1.0f;

  const float trough = powf(FIELD_MIN_LEVEL, fieldValDepth);
  return CHSV((uint8_t)(base.hue + (int16_t)((unit - 0.5f) * 256.0f * fieldHueDepth)),
              (uint8_t)((float)base.saturation * (1.0f - fieldSatDepth * unit)),
              (uint8_t)((float)base.value * (trough + (1.0f - trough) * unit)));
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

  const bool fieldActive = fieldHueDepth > 0.0001f
                        || fieldSatDepth > 0.0001f
                        || fieldValDepth > 0.0001f;
  // inoise8 takes a uint16 z and noise has no period, so the drift jumps once
  // every 256 cycles where the counter wraps. Wrapping in float first keeps
  // the conversion in range; a float past UINT16_MAX converts to nothing
  // defined.
  const float fieldCycles = trackedPhase(fieldPhase, beats, fieldRate);
  const uint16_t fieldZ = (uint16_t)(fract(fieldCycles * (1.0f / 256.0f)) * 65536.0f);

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
      const CHSV tint =
          fieldActive ? fieldColor(color, fieldAt(stripIndex, pixelIndex, fieldZ)) : color;
      CRGB lit = CHSV(tint.hue, tint.saturation, 255);
      strip[stripIndex][pixelIndex] =
          lit.nscale8_video((uint8_t)((float)tint.value * brightness));
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

void setFieldGrain(uint8_t value)    { fieldGrain = FIELD_MIN_GRAIN * powf(FIELD_GRAIN_RANGE, ccUnit(value)); }
void setFieldSpread(uint8_t value)   { fieldSpread = ccUnit(value) * FIELD_MAX_SPREAD; }
void setFieldHueDepth(uint8_t value) { fieldHueDepth = ccUnit(value); }
void setFieldSatDepth(uint8_t value) { fieldSatDepth = ccUnit(value); }
void setFieldValDepth(uint8_t value) { fieldValDepth = ccUnit(value); }
void setFieldSource(uint8_t value)   { fieldSource = ccUnit(value); }

// Precomputed on receipt rather than per pixel: powf on every one of the 225
// would cost more than the whole rest of the field.
void setFieldEdge(uint8_t value) { fieldSoftness = 0.02f * powf(50.0f, ccUnit(value)); }

// Bipolar around 64 like the travel speed, and squared for the same reason:
// the slow end is where a colour field that reads as depth rather than as an
// effect actually lives.
void setFieldRate(uint8_t value) {
  const float x = ((float)value - 64.0f) / 63.0f;
  fieldRate = (x < 0.0f ? -1.0f : 1.0f) * x * x * FIELD_MAX_CYCLES_PER_BEAT;
}
