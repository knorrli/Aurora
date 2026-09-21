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

// Count is blobs along one strip — the same unit the shape layer counts in, so
// the two Count controls mean the same thing and a number carries between them.
#define FIELD_MIN_COUNT 0.35f
#define FIELD_COUNT_RANGE 48.0f
#define FIELD_MAX_FAN 51.0f          // five strips across one sine cycle
#define FIELD_NOISE_FAN_SCALE 6.5f   // ...and 1.3 noise cells per strip
#define FIELD_MAX_CYCLES_PER_BEAT 1.0f

// How dark the field pulls a pixel at full reach, as a fraction of what it
// would otherwise be — and the knob reaches it geometrically, so each equal
// step is an equal ratio of light rather than an equal subtraction. Mapped
// linearly, nearly the whole travel was imperceptible and everything worth
// having sat in the last three steps.
//
// It stops short of zero because a WS2812 has eight linear bits and no gamma:
// at the bottom one step is a third of the light, so brightness quantises into
// lurches, and pixels crossing to zero pop out entirely.
#define FIELD_MIN_LEVEL 0.02f

// Half the wheel each way. Past about half, the hue fader stops meaning
// anything and the wall becomes a spectrum rather than one colour with depth
// in it; the bench put usable settings at a fifth to a half of the wheel.
#define FIELD_MAX_HUE_REACH 128.0f

static float fieldStep = 8.0f;   // units of field per pixel
static float fieldFan = 40.0f;
static float fieldSpeed = 0.0f;
static float fieldSource = 0.0f;
static float fieldSoftness = 1.0f;

// Signed, and all three measured FROM the faders rather than around them. See
// fieldColor().
static float fieldHueReach = 0.0f;   // +-FIELD_MAX_HUE_REACH
static float fieldSatReach = 0.0f;   // +1 to white, -1 to a pure hue
static float fieldValReach = 0.0f;   // +1 to full, -1 to FIELD_MIN_LEVEL

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
// Fan is symmetric about the centre strip, because a cosine is even: strips one
// either side of the middle land on the same value, and so do the outer two. So
// fan gives three colours mirrored across the wall rather than five distinct
// ones, with the middle strip on the faders' colour. Five distinct needs the
// fan centre to move off the middle — the same parameter the shape branch wants
// for its chevron, in docs/generator.md § Open.
//
// Fan opens the strips out from the middle one rather than from the first,
// so winding it up opens the wall symmetrically instead of pinning strip 1 and
// leaving the last strip to do all the moving.
static uint8_t fieldAt(uint8_t stripIndex, uint8_t pixelIndex, uint16_t z) {
  const float fromCentre = (float)stripIndex - (float)(NUMBER_OF_STRIPS - 1) * 0.5f;
  const int32_t across = (int32_t)(fromCentre * fieldFan);

  // Fan displaces the field ALONG the strip rather than shifting its level.
  // Shifting the level leaves every strip with its blobs at the same pixels
  // and only their colour differing, which reads as one striped pattern rather
  // than as a field with any depth in it.
  const int32_t along = (int32_t)((float)pixelIndex * fieldStep) + across;

  // One term, not three. Three sines at unrelated rates never line up, so the
  // average huddled around the middle instead of spanning its range — measured
  // at 0.21 to 0.79 on the centre strip, which left no floor for the faders'
  // colour to sit at and no ceiling for a patch to reach. Two of the three
  // also ran at the wrong rate: one at half the count and one constant along
  // the strip, so a count of two produced four humps rather than two.
  //
  // The quarter-turn puts the trough at phase zero. Count, fan and speed all
  // measure from there, which is what makes the start of a strip, and the
  // centre strip under fan, come out as the colour on the faders exactly.
  const uint8_t wave = sin8((uint8_t)(along + z + 192));

  // Perlin noise returns exactly its midpoint wherever the input lands on the
  // integer lattice, and FastLED's cells are 256 units wide. Stepping the
  // strips by a whole number of cells puts every one of them on the same
  // lattice line, and they come out sharing features however far apart they
  // are. So the step is deliberately not a multiple of 256, and the bias keeps
  // the middle strip off the lattice as well.
  const uint16_t noiseAcross =
      (uint16_t)(4200 + (int32_t)(fromCentre * fieldFan * FIELD_NOISE_FAN_SCALE));
  const uint8_t noise = expandFromCentre(inoise8((uint16_t)(along + 4200), noiseAcross, z));

  // Noise has no trough at phase zero, so winding Source up loosens the anchor
  // that puts the faders' colour at a knowable place. One more reason it is on
  // the chopping block.
  return (uint8_t)((float)wave + ((float)noise - (float)wave) * fieldSource);
}

// All three reaches are anchored at the SAME end of the field — where the
// field is at its floor, the pixel is exactly what the three faders say, and
// the field's patches are a departure from it.
//
// They were each anchored somewhere different before, which meant the colour
// on the faders appeared in three different places at once and, with all
// three wound up, nowhere at all: hue put it at the field's midpoint, to-white
// at the floor, to-dark at the peak. A red wall came out dim purple in the
// troughs and pale orange in the peaks with no red anywhere.
static CHSV fieldColor(CHSV base, uint8_t sample) {
  // A smooth ramp of brightness has no edge anywhere for the eye to catch, so
  // even a 50:1 range reads as barely there. Steepening the field toward a
  // hard boundary is the same move that makes a strobe reachable from a sine,
  // and it is what turns the field's regions into things you can see as
  // regions rather than as a general unevenness.
  float unit = ((float)sample / 255.0f - 0.5f) / fieldSoftness + 0.5f;
  if (unit < 0.0f) unit = 0.0f;
  else if (unit > 1.0f) unit = 1.0f;

  const float satTarget = (fieldSatReach >= 0.0f) ? 0.0f : 255.0f;
  const float saturation = (float)base.saturation
      + unit * fabsf(fieldSatReach) * (satTarget - (float)base.saturation);

  // Darkening keeps the geometric taper, because it is a ratio of light and
  // the eye reads it as one. Brightening is a plain ride to full, and only has
  // anywhere to go when the V fader is left below the top.
  float value;
  if (fieldValReach >= 0.0f) {
    value = (float)base.value + unit * fieldValReach * (255.0f - (float)base.value);
  } else {
    const float floorLevel = powf(FIELD_MIN_LEVEL, -fieldValReach);
    value = (float)base.value * (1.0f + unit * (floorLevel - 1.0f));
  }

  return CHSV((uint8_t)(base.hue + (int16_t)(unit * fieldHueReach)),
              (uint8_t)saturation,
              (uint8_t)value);
}

// ---------------------------------------------------------------------------
// Colour that follows how lit a pixel is
//
// Every shape the generator makes is a brightness ramp — a core, an edge fade,
// a tail — and colour threw all of it away, so a comet's tail was its head in
// the same colour with less light behind it. That reads as a region being
// dimmed rather than as an object with heat in it.
//
// Both reaches are anchored at the shape's DIM end, matching the field above:
// the faders are what the fade runs out to, and the core is the departure.
// Fed the shape's own profile, before jitter and the pulse, so a flash does
// not wash the whole strip out and jitter does not scatter colour as well as
// light — each of those is its own question.
// ---------------------------------------------------------------------------

// A quarter wheel each way. Red through to yellow is 64 units, which is the
// whole of the cooling ramp anyone is likely to want.
#define LIT_MAX_HUE_REACH 64.0f

static float litSatReach = 0.0f;   // 0 to 1, toward white at the core
static float litHueReach = 0.0f;   // +-LIT_MAX_HUE_REACH

static CHSV litColor(CHSV base, float profile) {
  return CHSV((uint8_t)(base.hue + (int16_t)(profile * litHueReach)),
              (uint8_t)((float)base.saturation * (1.0f - litSatReach * profile)),
              base.value);
}

// `offset` is the signed distance from the core's centre, in cells, positive
// on the trailing side. Which shape a pixel is measured against is the
// caller's business, because that is a question about the strip's ends
// rather than about the shape.
//
// `width` is the solid core. `edge` and `tail` both reach outward from it
// into the gap rather than eating into it, so softening a shape never makes
// it smaller. Both are scaled by the gap that is actually available, which
// means edge at full always closes the gaps to the neighbouring shapes — the
// two fades meet at zero and never have to be summed.
static float shapeAt(float offset, float width, float edge, float tail) {
  const float halfCore = width * 0.5f;
  const float gap = 1.0f - width;
  const float spread = edge * gap * 0.5f;

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
  const float countCells = (float)genCount;

  // Where the core's centre sits, measured in cells.
  float centreCells;
  float direction;
  if (genBounce && fabsf(genSpeedPixels) > 0.0001f) {
    const float rate = fabsf(genSpeedPixels) / (2.0f * (float)PIXELS_PER_STRIP);
    const float triangle = fract(trackedPhase(travelPhase, beats, rate));
    const bool rising = triangle < 0.5f;
    const float swing = rising ? (triangle * 2.0f) : ((1.0f - triangle) * 2.0f);
    // The turn comes when the core's own edge reaches the strip end, the way a
    // ball meets a wall, so nothing ever leaves the strip and reappears
    // opposite. At full width the span closes to a point, which is right: a
    // shape filling the strip has nowhere to go.
    const float halfCore = genWidth * 0.5f;
    centreCells = halfCore + swing * ((float)genCount - 2.0f * halfCore);
    direction = rising ? 1.0f : -1.0f;
  } else {
    // Half a cell, so a still shape sits in the middle of its cell rather than
    // straddling the boundary — which at count 1 is the strip's two ends.
    centreCells = 0.5f + trackedPhase(travelPhase, beats, genSpeedPixels / cellLength);
    direction = (genSpeedPixels >= 0.0f) ? 1.0f : -1.0f;
  }

  const float pulse = trackedPhase(pulsePhase, beats, 1.0f / genPulseBeats);

  const bool fieldActive = fabsf(fieldHueReach) > 0.5f
                        || fabsf(fieldSatReach) > 0.0001f
                        || fabsf(fieldValReach) > 0.0001f;
  const bool litActive = litSatReach > 0.0001f || fabsf(litHueReach) > 0.5f;
  // inoise8 takes a uint16 z and noise has no period, so the drift jumps once
  // every 256 cycles where the counter wraps. Wrapping in float first keeps
  // the conversion in range; a float past UINT16_MAX converts to nothing
  // defined.
  const float fieldCycles = trackedPhase(fieldPhase, beats, fieldSpeed);
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

    // Odd strips run the journey backwards, rather than only mirroring the
    // shape where it stands. Travel is one value every strip shares, so
    // flipping the direction alone left the shape moving the same way and
    // showed up on nothing but the side a tail fell on.
    const float centreHere =
        (genAlternate && (stripIndex & 1)) ? (countCells - centreCells) : centreCells;

    // Where the core's centre sits inside a cell.
    const float coreCentre = fract(centreHere + stripPhase);

    // Jitter re-rolls once per swell, at the point in the cycle where the
    // pulse is darkest, so a flashing shape lands somewhere new each time
    // instead of being smeared where it stands. On a grid of its own it
    // could never coincide with a flash, which is all it used to do.
    const uint8_t jitterBucket = (uint8_t)floorf(pulse + stripPhase);

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
        const float posCells = samplePosition / cellLength + jitterOffset;

        // The shape repeats once per cell, so the only images that can light
        // this sample are the two standing either side of it. Under bounce
        // the strip is a line, and an image off its end is not there to be
        // seen — which is what stops a fade leaving one end of the strip and
        // arriving at the other.
        const float firstImage = coreCentre + floorf(posCells - coreCentre);
        float nearest = 0.0f;
        bool lit = false;
        for (uint8_t image = 0; image < 2; image++) {
          const float imagePos = firstImage + (float)image;
          if (genBounce && (imagePos < 0.0f || imagePos > countCells)) continue;
          const float offset = -stripDirection * (posCells - imagePos);
          if (!lit || fabsf(offset) < fabsf(nearest)) {
            nearest = offset;
            lit = true;
          }
        }
        if (lit) accumulated += shapeAt(nearest, width, genEdge, genTail);
      }

      const float profile = accumulated / (float)GEN_SUBSAMPLES;
      const float brightness = profile * jitterLevel * swell;
      if (brightness <= 0.002f) continue;

      // Scaling the RGB rather than handing a low value to CHSV keeps the hue
      // where it was set: converting at a low value lets a channel truncate to
      // zero before its neighbour, which is what turns a dim yellow red.
      CHSV tint =
          fieldActive ? fieldColor(color, fieldAt(stripIndex, pixelIndex, fieldZ)) : color;
      if (litActive) tint = litColor(tint, profile);
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

// One blob is one period of the sine, which is 256 units wide, so a count
// across the strip converts to the per-pixel step the field's maths wants.
//
// Subtracting one from the geometric ride lets the bottom of the knob reach
// zero blobs, where the field holds still along each strip and only Fan
// separates them — which is how a colour per strip is asked for. A plain
// geometric law bottoms out at its minimum and can never arrive there.
void setFieldCount(uint8_t value) {
  const float count = FIELD_MIN_COUNT * (powf(FIELD_COUNT_RANGE, ccUnit(value)) - 1.0f);
  fieldStep = count * 256.0f / (float)PIXELS_PER_STRIP;
}
void setFieldFan(uint8_t value)   { fieldFan = ccUnit(value) * FIELD_MAX_FAN; }
void setFieldSource(uint8_t value)   { fieldSource = ccUnit(value); }

// Centred: 64 is no departure at all, and either side is a direction. The
// three of them are what decides how far the field's patches sit from the
// colour on the faders, so the centre has to be "the wall is one colour".
static inline float ccBipolar(uint8_t value) {
  return value < 64 ? ((float)value - 64.0f) / 64.0f
                    : ((float)value - 64.0f) / 63.0f;
}

void setFieldHueDepth(uint8_t value) { fieldHueReach = ccBipolar(value) * FIELD_MAX_HUE_REACH; }
void setFieldSatDepth(uint8_t value) { fieldSatReach = ccBipolar(value); }
void setFieldValDepth(uint8_t value) { fieldValReach = ccBipolar(value); }

void setLitSatReach(uint8_t value) { litSatReach = ccUnit(value); }
void setLitHueReach(uint8_t value) { litHueReach = ccBipolar(value) * LIT_MAX_HUE_REACH; }

// Precomputed on receipt rather than per pixel: powf on every one of the 225
// would cost more than the whole rest of the field.
void setFieldEdge(uint8_t value) { fieldSoftness = 0.02f * powf(50.0f, ccUnit(value)); }

// Bipolar around 64 like the travel speed, and squared for the same reason:
// the slow end is where a colour field that reads as depth rather than as an
// effect actually lives.
void setFieldSpeed(uint8_t value) {
  const float x = ((float)value - 64.0f) / 63.0f;
  fieldSpeed = (x < 0.0f ? -1.0f : 1.0f) * x * x * FIELD_MAX_CYCLES_PER_BEAT;
}
