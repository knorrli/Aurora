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
static float shapeAt(float offset, float width, float edge, float tail);

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
// The colour layer
//
// A colour is hue, whiteness and darkness. Everything else is a push on those
// three, and the pushes add. Three sources push:
//
//   the placed field  something aimed — a slide across a ruler, or regions
//                     sitting on it
//   the wander        the wall never quite the same in two places, and where
//                     it differs keeps moving
//   the light level   colour read off how lit the shape branch left a pixel
//
// The layer reads the SHAPE branch's light level and never its own. Feed its
// own darkness back in and colour depends on colour: pull the wall down for a
// quiet verse and the hue slides with it.
//
// Designed and dialled in tools/preview.js before any of it was flashed; the
// numbers here and there are meant to stay identical.
// ---------------------------------------------------------------------------

// How dark a full push pulls a pixel, as a fraction of what it would
// otherwise be. It stops short of zero because a WS2812 has eight linear bits
// and no gamma: at the bottom one step is a third of the light, so brightness
// quantises into lurches and pixels crossing to zero pop out entirely.
#define DARK_FLOOR 0.02f

// Half the wheel each way. Past about half, the hue fader stops meaning
// anything and the wall becomes a spectrum rather than one colour with depth
// in it; the bench put usable settings at a fifth to a half of the wheel.
#define PLACED_MAX_HUE 128.0f
#define WANDER_MAX_HUE 128.0f

// A quarter wheel each way. Red through to yellow is 64 units, which is the
// whole of the cooling ramp anyone is likely to want.
#define LIT_MAX_HUE 64.0f

#define WANDER_MAX_CYCLES_PER_BEAT 0.5f
#define PLACED_MAX_CELLS_PER_BEAT 1.0f

// Two terms whose rates sit at the golden ratio, so they never come back into
// step and the wall never repeats. Deliberately not a control: dialling how
// far apart the two speeds are is operating the mechanism rather than the
// look.
#define GOLD 0.6180339887f

enum ColourRuler : uint8_t {
  RULER_WALL = 0,   // which of the five strips a pixel is on
  RULER_STRIP = 1,  // how far along its strip a pixel is
  RULER_SHAPE = 2,  // leading tip of a shape through to the end of its tail
};

static bool placedIsRegion = false;
static uint8_t placedRuler = RULER_STRIP;
static float placedHueReach = 0.0f;
static float placedWhiteReach = 0.0f;
static float placedDarkReach = 0.0f;
static uint8_t placedCount = 1;
static float placedWidth = 0.5f;
static float placedEdge = 0.5f;
static float placedCells = 0.0f;

static float wanderHueReach = 0.0f;
static float wanderWhiteReach = 0.0f;
static float wanderDarkReach = 0.0f;
static float wanderCycles = 0.0f;
static float wanderScale = 0.0f;

static float litHueReach = 0.0f;
static float litWhiteReach = 0.0f;
static float litDarkReach = 0.0f;

static PhaseTracker wanderPhase = { 0.0f, 0.0f };
static PhaseTracker placedPhase = { 0.0f, 0.0f };

static bool placedActive() {
  return fabsf(placedHueReach) > 0.5f
      || fabsf(placedWhiteReach) > 0.001f
      || fabsf(placedDarkReach) > 0.001f;
}

static bool wanderActive() {
  return fabsf(wanderHueReach) > 0.5f
      || fabsf(wanderWhiteReach) > 0.001f
      || fabsf(wanderDarkReach) > 0.001f;
}

static bool litActive() {
  return fabsf(litHueReach) > 0.5f
      || fabsf(litWhiteReach) > 0.001f
      || fabsf(litDarkReach) > 0.001f;
}

// The base colour sits at zero, so two terms that rarely reach their ends cost
// nothing: a sum huddled around the middle is the wall sitting at the colour
// that was dialled. There is no floor here for a colour to fall off.
static float wanderAt(uint8_t stripIndex, float along01, float t) {
  // Measured from the middle strip, not the first. Fanned from the first,
  // strip one never moves and the last does all the travelling, which reads as
  // a one-sided ramp rather than the wall opening — see docs/bench-facts.md
  // § "A field built as along-plus-across".
  const float acrossFromCentre =
      ((float)stripIndex - (float)(NUMBER_OF_STRIPS - 1) * 0.5f)
      / (float)(NUMBER_OF_STRIPS - 1);
  const float cyclesAlong = 0.12f * powf(180.0f, wanderScale);
  float cyclesAcross = cyclesAlong * 0.3f;
  if (cyclesAcross > 1.4f) cyclesAcross = 1.4f;

  const float a = sinf(2.0f * (float)PI
      * (cyclesAlong * along01 + cyclesAcross * acrossFromCentre + t));
  const float b = sinf(2.0f * (float)PI
      * (cyclesAlong * GOLD * along01 - cyclesAcross * 1.37f * acrossFromCentre + t * GOLD));
  return (a + b) * 0.5f;
}

// A slide is monotone with the base colour at the ruler's centre, so the reach
// is how far ONE end departs and the two ends land twice that apart. A region
// is a bump — base, departure, back to base — built from the shape branch's
// own core and fades, which is what makes count, width and edge mean the same
// thing in both branches.
static float placedAt(float u, float drift) {
  if (!placedIsRegion) return (u - 0.5f) * 2.0f;
  const float cell = u * (float)placedCount + drift;
  return shapeAt(fract(cell) - 0.5f, placedWidth, placedEdge, 0.0f);
}

// Pushes arrive summed and normalised. Darkening rides a geometric taper
// because it is a ratio of light and the eye reads it as one; mapped linearly,
// nearly the whole travel was imperceptible and everything worth having sat in
// the last few steps. Brightening is a plain ride to full and only has room
// when the V fader is left below the top. Both measured on the wall — see
// docs/bench-facts.md.
static CHSV applyPushes(CHSV base, float hue, float white, float dark) {
  if (white < -1.0f) white = -1.0f; else if (white > 1.0f) white = 1.0f;
  if (dark < -1.0f) dark = -1.0f; else if (dark > 1.0f) dark = 1.0f;

  const float satTarget = (white >= 0.0f) ? 0.0f : 255.0f;
  const float saturation = (float)base.saturation
      + fabsf(white) * (satTarget - (float)base.saturation);

  float value;
  if (dark >= 0.0f) value = (float)base.value + dark * (255.0f - (float)base.value);
  else value = (float)base.value * powf(DARK_FLOOR, -dark);

  return CHSV((uint8_t)(base.hue + (int16_t)hue),
              (uint8_t)saturation,
              (uint8_t)value);
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
// The core and its edge fade are geometry: they sit around the core wherever
// it stands, the same on both sides.
static float coreAt(float offset, float width, float edge) {
  const float halfCore = width * 0.5f;
  const float distance = fabsf(offset);
  if (distance <= halfCore) return 1.0f;

  const float spread = edge * (1.0f - width) * 0.5f;
  const float beyond = distance - halfCore;
  if (spread > 0.0001f && beyond < spread) {
    const float k = 1.0f - (beyond / spread);
    return k * k * (3.0f - 2.0f * k);
  }
  return 0.0f;
}

// The tail is not geometry. It is how far the core has travelled since it was
// last at this point, so `behind` is a path length and never a straight line.
static float tailAt(float behind, float width, float tail) {
  if (tail <= 0.0001f) return 0.0f;
  const float beyond = behind - width * 0.5f;
  if (beyond <= 0.0f) return 0.0f;

  const float tailLength = tail * (1.0f - width);
  if (tailLength <= 0.0001f || beyond >= tailLength) return 0.0f;
  const float k = 1.0f - (beyond / tailLength);
  return k * k;
}

// While travel runs one way, how far behind the core a point lies and how long
// ago the core was there are the same number, which is why a straight offset
// serves for both. They come apart only where the core turns.
static float shapeAt(float offset, float width, float edge, float tail) {
  float brightness = coreAt(offset, width, edge);
  if (offset > 0.0f) {
    const float trailing = tailAt(offset, width, tail);
    if (trailing > brightness) brightness = trailing;
  }
  return brightness;
}

// Every cell runs the same journey, and an odd strip runs it backwards, so a
// position on the strip becomes a position in that journey before the trail
// can be measured against it.
static inline float journeyIn(float posCells, bool mirrored) {
  const float withinCell = posCells - floorf(posCells);
  return mirrored ? (1.0f - withinCell) : withinCell;
}

// Under bounce the core's position is a triangle, so "when was the core last
// here" has an answer in closed form: every point on the swing is crossed
// exactly twice a cycle, going up and coming down, and the more recent of the
// two is the one whose trail is still lying there. Distance is the path the
// core walked in that time, which is what folds the trail back on itself at a
// turn instead of moving it.
//
// Points inside half a core width of the cell's ends are never reached by the
// centre, only swept by the body at the turn, so they measure from the turn
// and add the straight remainder.
static float trailBehind(float journey, float phase, float halfCore,
                         float swingSpan) {
  if (swingSpan <= 0.0001f) return fabsf(journey - 0.5f);

  const float far = 1.0f - halfCore;
  const float reachable = (journey < halfCore) ? halfCore
                        : ((journey > far) ? far : journey);
  const float rising = 0.5f * ((reachable - halfCore) / swingSpan);
  const float up = fract(phase - rising);
  const float down = fract(phase - (1.0f - rising));
  const float elapsed = (up < down) ? up : down;
  return elapsed * 2.0f * swingSpan + fabsf(journey - reachable);
}

// 0 at one end of the ruler, 1 at the other.
static float rulerAt(uint8_t stripIndex, uint8_t pixelIndex, float shapeU) {
  if (placedRuler == RULER_WALL) {
    return (float)stripIndex / (float)(NUMBER_OF_STRIPS - 1);
  }
  if (placedRuler == RULER_SHAPE) return shapeU;
  return (float)pixelIndex / (float)(PIXELS_PER_STRIP - 1);
}

static CHSV colourAt(CHSV base, uint8_t stripIndex, uint8_t pixelIndex,
                     float shapeU, float profile, float drift, float wanderT,
                     bool placedOn, bool wanderOn) {
  const float along01 = (float)pixelIndex / (float)(PIXELS_PER_STRIP - 1);

  const float placed = placedOn ? placedAt(rulerAt(stripIndex, pixelIndex, shapeU), drift) : 0.0f;
  const float wander = wanderOn ? wanderAt(stripIndex, along01, wanderT) : 0.0f;

  return applyPushes(base,
      placed * placedHueReach + wander * wanderHueReach + profile * litHueReach,
      placed * placedWhiteReach + wander * wanderWhiteReach + profile * litWhiteReach,
      placed * placedDarkReach + wander * wanderDarkReach + profile * litDarkReach);
}

// The shape repeats once per cell, so the only images that can reach a sample
// are the two standing either side of it. Under bounce the strip is a line and
// an image off its end is not there to be seen — which is what stops a fade
// leaving one end of the strip and arriving at the other.
static bool nearestOffset(float posCells, float coreCentre, float stripDirection,
                          bool bounce, float countCells, float &out) {
  const float firstImage = coreCentre + floorf(posCells - coreCentre);
  bool lit = false;
  for (uint8_t image = 0; image < 2; image++) {
    const float imagePos = firstImage + (float)image;
    if (bounce && (imagePos < 0.0f || imagePos > countCells)) continue;
    const float offset = -stripDirection * (posCells - imagePos);
    if (!lit || fabsf(offset) < fabsf(out)) {
      out = offset;
      lit = true;
    }
  }
  return lit;
}

void Generator(CHSV color) {
  const float beats = tempo::beats();
  const float cellLength = (float)PIXELS_PER_STRIP / (float)genCount;
  const float countCells = (float)genCount;

  // Under bounce the core swings inside its own cell, turning where its own
  // edge meets the cell's boundary the way a ball meets a wall, so nothing
  // ever crosses into a neighbouring cell. Taking the rate from the cell is
  // what keeps speed an absolute distance: adding shapes shrinks the cell and
  // quickens the turn, and the core still crosses the wall at the pixels per
  // beat on the dial. At full width the swing closes to nothing, which is
  // right — a shape filling its cell has nowhere to go.
  const float halfCore = genWidth * 0.5f;
  const float swingSpan = 1.0f - genWidth;
  const bool bouncing = genBounce && fabsf(genSpeedPixels) > 0.0001f;

  float travelCycles = 0.0f;
  float centreCells = 0.5f;
  const float direction = (genSpeedPixels >= 0.0f) ? 1.0f : -1.0f;
  if (bouncing) {
    const float rate = (swingSpan > 0.0001f)
        ? fabsf(genSpeedPixels) / (2.0f * swingSpan * cellLength)
        : 0.0f;
    travelCycles = trackedPhase(travelPhase, beats, rate);
  } else {
    // Half a cell, so a still shape sits in the middle of its cell rather than
    // straddling the boundary — which at count 1 is the strip's two ends.
    centreCells = 0.5f + trackedPhase(travelPhase, beats, genSpeedPixels / cellLength);
  }

  const float pulse = trackedPhase(pulsePhase, beats, 1.0f / genPulseBeats);

  const bool placedOn = placedActive();
  const bool wanderOn = wanderActive();
  const bool colourFlat = !placedOn && !wanderOn && !litActive();

  // Both colour rates go through the tracker for the same reason travel and
  // the pulse do: beats only grows, so a small change of rate multiplied by a
  // large beat count is a large jump.
  const float wanderT = trackedPhase(wanderPhase, beats, wanderCycles);
  const float placedDrift = trackedPhase(placedPhase, beats, placedCells);

  // The two sides of a shape are not the same length — a tail reaches much
  // further than an edge fade — so they are normalised separately. Halfway
  // between the two tips is not the core, and a region asked to sit at the
  // middle of a shape means the core every time.
  const float shapeGap = 1.0f - genWidth;
  const float shapeLead = genWidth * 0.5f + genEdge * shapeGap * 0.5f;
  const float shapeTrail = genWidth * 0.5f
      + fmaxf(genEdge * shapeGap * 0.5f, genTail * shapeGap);

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    const float stripPhase = genFan * ((float)stripIndex / (float)NUMBER_OF_STRIPS);

    const bool mirrored = genAlternate && (stripIndex & 1);

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
    //
    // Under bounce fan offsets where a strip stands in its own swing, so the
    // five turn at different moments. It cannot offset the core's position
    // instead: an image standing past the strip's end is clipped away by
    // nearestOffset, so displacing it there shortens a strip rather than
    // staggering it.
    float coreCentre;
    float stripDirection;
    float triangle = 0.0f;
    if (bouncing) {
      triangle = fract(travelCycles + stripPhase);
      const bool rising = triangle < 0.5f;
      const float swing = rising ? (triangle * 2.0f) : ((1.0f - triangle) * 2.0f);
      const float place = halfCore + swing * swingSpan;
      coreCentre = mirrored ? (1.0f - place) : place;
      stripDirection = rising ? 1.0f : -1.0f;
      if (mirrored) stripDirection = -stripDirection;
    } else {
      const float centreHere = mirrored ? (countCells - centreCells) : centreCells;
      coreCentre = fract(centreHere + stripPhase);
      stripDirection = mirrored ? -direction : direction;
    }

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
        if (bouncing) {
          // The core stays inside its cell, so its trail does too: at a turn
          // the core walks back out through what it laid down rather than the
          // trail changing sides.
          const float journey = journeyIn(posCells, mirrored);
          float level = 0.0f;
          float nearest = 0.0f;
          if (nearestOffset(posCells, coreCentre, stripDirection, true, countCells, nearest)) {
            level = coreAt(nearest, width, genEdge);
          }
          const float trailing = tailAt(
              trailBehind(journey, triangle, halfCore, swingSpan), width, genTail);
          accumulated += (trailing > level) ? trailing : level;
        } else {
          float nearest = 0.0f;
          if (nearestOffset(posCells, coreCentre, stripDirection, genBounce, countCells, nearest)) {
            accumulated += shapeAt(nearest, width, genEdge, genTail);
          }
        }
      }

      const float profile = accumulated / (float)GEN_SUBSAMPLES;
      const float brightness = profile * jitterLevel * swell;
      if (brightness <= 0.002f) continue;

      // Scaling the RGB rather than handing a low value to CHSV keeps the hue
      // where it was set: converting at a low value lets a channel truncate to
      // zero before its neighbour, which is what turns a dim yellow red.
      CHSV tint = color;
      if (!colourFlat) {
        const float centrePos = ((float)pixelIndex + 0.5f) / cellLength + jitterOffset;
        float centreOffset = 0.0f;
        const bool onShape = nearestOffset(centrePos, coreCentre, stripDirection,
                                           genBounce, countCells, centreOffset);
        float shapeU = 0.5f;
        if (bouncing) {
          // The ruler's trailing half has to be the same measure the tail is
          // drawn from, or colour along a tail paints where the tail is not.
          const float behind =
              trailBehind(journeyIn(centrePos, mirrored), triangle, halfCore, swingSpan);
          if (behind < shapeTrail && shapeTrail > 0.0001f) {
            shapeU = 0.5f + 0.5f * behind / shapeTrail;
          } else if (onShape && shapeLead > 0.0001f) {
            shapeU = 0.5f - 0.5f * fabsf(centreOffset) / shapeLead;
          }
        } else if (onShape) {
          const float reach = (centreOffset < 0.0f) ? shapeLead : shapeTrail;
          if (reach > 0.0001f) shapeU = 0.5f + 0.5f * centreOffset / reach;
        }
        if (shapeU < 0.0f) shapeU = 0.0f;
        else if (shapeU > 1.0f) shapeU = 1.0f;
        tint = colourAt(color, stripIndex, pixelIndex, shapeU, profile,
                        placedDrift, wanderT, placedOn, wanderOn);
      }
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

// Bipolar around 64: the centre has to be "no departure at all", because
// these are what decide how far a push sits from the colour on the faders.
static inline float ccBipolar(uint8_t value) {
  return value < 64 ? ((float)value - 64.0f) / 64.0f
                    : ((float)value - 64.0f) / 63.0f;
}

void setColourFlags(uint8_t value) {
  placedIsRegion = value & COLOUR_FLAG_REGION;
  const uint8_t ruler = (value & COLOUR_RULER_MASK) >> 1;
  placedRuler = (ruler > RULER_SHAPE) ? RULER_SHAPE : ruler;
}

void setPlacedHue(uint8_t value)   { placedHueReach = ccBipolar(value) * PLACED_MAX_HUE; }
void setPlacedWhite(uint8_t value) { placedWhiteReach = ccBipolar(value); }
void setPlacedDark(uint8_t value)  { placedDarkReach = ccBipolar(value); }
void setPlacedWidth(uint8_t value) { placedWidth = ccUnit(value); }
void setPlacedEdge(uint8_t value)  { placedEdge = ccUnit(value); }

void setPlacedCount(uint8_t value) {
  placedCount = 1 + (uint8_t)((uint16_t)value * (GEN_MAX_COUNT - 1) / 127);
}

// Bipolar and squared like the shape branch's travel, for the same reason:
// the slow end is where a colour that reads as depth rather than as an effect
// actually lives.
void setPlacedSpeed(uint8_t value) {
  const float x = ((float)value - 64.0f) / 63.0f;
  placedCells = (x < 0.0f ? -1.0f : 1.0f) * x * x * PLACED_MAX_CELLS_PER_BEAT;
}

void setWanderHue(uint8_t value)   { wanderHueReach = ccBipolar(value) * WANDER_MAX_HUE; }
void setWanderWhite(uint8_t value) { wanderWhiteReach = ccBipolar(value); }
void setWanderDark(uint8_t value)  { wanderDarkReach = ccBipolar(value); }
void setWanderRate(uint8_t value)  { wanderCycles = ccUnit(value) * WANDER_MAX_CYCLES_PER_BEAT; }
void setWanderScale(uint8_t value) { wanderScale = ccUnit(value); }

void setLitHue(uint8_t value)   { litHueReach = ccBipolar(value) * LIT_MAX_HUE; }
void setLitWhite(uint8_t value) { litWhiteReach = ccUnit(value); }
void setLitDark(uint8_t value)  { litDarkReach = ccBipolar(value); }
