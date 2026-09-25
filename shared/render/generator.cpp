#include "render.h"

#include <math.h>

#include "aurora_protocol.h"

/////////////////////////////////
// GENERATOR — one parametric pattern, driven entirely over CC.
//
// An experiment in replacing the fixed roster with a continuous space:
// a shape, repeated `count` times along each strip, optionally traveling,
// with the five strips optionally run out of step with each other. Most of
// the roster sits somewhere inside it — Sweep is a hard-edged shape at
// count 1, Rain is the same with the strips fanned, Starfield is many
// narrow shapes scattered, Strobe is full width pulsed to zero.
//
// Everything is recomputed each frame from the musical position, so the only
// state is what Motion carries, and a tempo change is a change of rate, not a
// jump.
/////////////////////////////////

namespace render {

// Speed is an absolute distance per beat, because it is motion through real
// space and should not change when the count does. Width, edge and tail are
// proportions of the shape instead — measured in pixels, their useful range
// collapses as the shapes get narrower, and most of each slider's travel
// stops doing anything.
static const uint8_t GEN_MAX_COUNT = 20;
static const float GEN_MAX_SPEED_PIXELS_PER_BEAT = 60.0f;

// The fan's wave runs across the strips, and five of them cannot sample
// anything faster than half a cycle each: at that setting every strip lands
// on the opposite point of the wave from its neighbors, which is alternate,
// and above it the wave folds back onto slower ones. So the fader stops
// there.
//
// Stepped to eighths of a turn across the wall, with the phase on 128ths of
// one, because the two together have to be able to read *exactly* zero on a
// strip. Read near zero and a strip is not still, it crawls: a wave of 0.04
// against a rate amount of +-12 px/beat is half a pixel a beat, which is a
// quarter of the strip in a minute and five strip-lengths in a song. Still
// has to mean still here for the same reason it does on Speed, and 0.125
// cycles a strip is not a number 127 steps can land on.
//
// The phase divides by 128 rather than 127 because a whole turn is the same
// wall as none, so the fader covers the turn and stops short of repeating its
// own start.
static const float GEN_FAN_FREQ_STEPS = 16.0f;
static const float GEN_FAN_MAX_CYCLES_PER_STRIP = 0.5f;

// Which salt the random fan draws its five offsets through. Of the 256, this
// one puts the strips 0.094, 0.137, 0.200, 0.239 and 0.329 of a cycle apart
// round the circle: none close enough to read as two strips in unison, and
// no two gaps alike, which is what stops a random spread arriving as just
// another pattern.
static const uint8_t GEN_FAN_HASH_SALT = 118;

// How long the pulse takes to walk back onto the musical grid after its rate
// has been moved, measured in its own cycles so the correction is always the
// same fraction of a swell and never a visible lurch.
static const float GEN_PULSE_ANCHOR_CYCLES = 2.0f;

// How long a stopped pattern takes to walk home to Position, in beats. Long
// enough that bringing Speed to a stop reads as settling rather than as a
// second move of its own.
static const float GEN_POSITION_SETTLE_BEATS = 2.0f;

// Below this a travel is a pixel a minute — slower than anything the roster
// wants and slow enough to read as a standstill that quietly drifts.
static const float GEN_STILL_PIXELS_PER_BEAT = 0.05f;

// Each pixel averages this many samples across its own width. Point-sampling
// at the pixel center aliases once a cell is only a pixel or two across: the
// shape strobes as it moves instead of fading out. Averaging makes detail
// finer than the strip can resolve wash out smoothly, which is what it
// should do.
static const uint8_t GEN_SUBSAMPLES = 4;

// How dark a full push pulls a pixel, as a fraction of what it would
// otherwise be. It stops short of zero because a WS2812 has eight linear bits
// and no gamma: at the bottom one step is a third of the light, so brightness
// quantizes into lurches and pixels crossing to zero pop out entirely.
static const float DARK_FLOOR = 0.02f;

// Half the wheel each way. Past about half, the hue fader stops meaning
// anything and the wall becomes a spectrum rather than one color with depth
// in it; the bench put usable settings at a fifth to a half of the wheel.
static const float PLACED_MAX_HUE = 128.0f;
static const float WANDER_MAX_HUE = 128.0f;
static const float SCATTER_MAX_HUE = 128.0f;

// A quarter wheel each way. Red through to yellow is 64 units, which is the
// whole of the cooling ramp anyone is likely to want.
static const float LIT_MAX_HUE = 64.0f;

static const float WANDER_MAX_CYCLES_PER_BEAT = 0.5f;
static const float PLACED_MAX_CELLS_PER_BEAT = 1.0f;

// The scatter's fastest clock. Four relights a beat is a sixteenth note,
// which is where a flicker stops reading as separate events at stage
// distance.
static const float SCATTER_MAX_CYCLES_PER_BEAT = 4.0f;

// Two terms whose rates sit at the golden ratio, so they never come back into
// step and the wall never repeats. Deliberately not a control: dialing how
// far apart the two speeds are is operating the mechanism rather than the
// look.
static const float GOLD = 0.6180339887f;

static const float TURN = 6.28318530718f;

enum ColorRuler : uint8_t {
  RULER_WALL = 0,   // which of the five strips a pixel is on
  RULER_STRIP = 1,  // how far along its strip a pixel is
  RULER_SHAPE = 2,  // leading tip of a shape through to the end of its tail
};

// Everything one placed field is, carried together so a second field is a
// second instance rather than a second set of fields. Both rulers at once — a
// strip painted with a gradient and shapes crossing it carrying their own —
// is the look that wants one.
struct PlacedField {
  bool isRegion;
  uint8_t ruler;
  float hueReach;
  float whiteReach;
  float darkReach;
  uint8_t count;
  float width;
  float edge;
  float cellsPerBeat;
};

// Every control as the renderer wants it, converted from the byte after the
// routes have pushed it, since a push arrives in CC units and the conversion
// has to see the pushed value.
struct Params {
  float width;
  uint8_t count;
  float edge;
  float tail;
  float positionCells;
  float speedPixels;
  float fanPosition;
  float fanPulse;
  float fanRate;
  float fanFreq;
  float fanPhase;
  float fanRandom;
  float pulseBeats;
  bool alternate;
  bool bounce;

  PlacedField placed;

  float wanderHueReach;
  float wanderWhiteReach;
  float wanderDarkReach;
  float wanderCycles;
  float wanderCyclesAlong;

  float litHueReach;
  float litWhiteReach;
  float litDarkReach;

  float scatterRate;
  uint8_t scatterCount;
  float scatterWidth;
  float scatterEdge;
  float scatterStagger;
  float scatterDrift;
  float scatterLightReach;
  float scatterHueReach;
  float scatterWhiteReach;

  Hsv color;
};

static inline float fract(float x) { return x - floorf(x); }

static float shapeAt(float offset, float width, float edge, float tail);

// A phase derived as `beats * rate` teleports whenever the rate changes,
// because beats is large and only grows: a small change of rate is a large
// change of product. Carrying an offset across each change keeps the phase
// where it was and alters only how fast it advances from there — which is
// what makes a rate reachable with a fader instead of only at setup time.
static float trackedPhase(Tracker &tracker, float beats, float rate) {
  if (rate != tracker.rate) {
    tracker.offset += beats * (tracker.rate - rate);
    tracker.rate = rate;
  }
  return beats * rate + tracker.offset;
}

// The tracker's offset is what stops a rate change teleporting, and it is
// also what leaves the cycle's zero wherever the rate was last touched —
// never a bar line, so a deep slow swell peaks wherever it happens to. A
// whole cycle of offset is invisible, so only the fraction has to go: easing
// it out over the next couple of cycles walks the pulse back onto the grid
// without ever jumping.
//
// This is why the rate is stepped rather than continuous. Anchoring puts the
// cycle's zero on the music's zero; only a period a bar holds a whole number
// of keeps it there, which is what AURORA_PULSE_PERIODS is.
static float anchoredPulsePhase(Motion &motion, float beats, float rate) {
  const float elapsed = beats - motion.lastPulseBeats;
  motion.lastPulseBeats = beats;

  // The transport restarted, and beat zero is a bar line by definition.
  if (elapsed < 0.0f) {
    motion.pulse.offset = 0.0f;
    motion.pulse.rate = rate;
    return beats * rate;
  }

  const float phase = trackedPhase(motion.pulse, beats, rate);
  const float drift = motion.pulse.offset - roundf(motion.pulse.offset);
  if (fabsf(drift) < 0.0001f) return phase;

  float pull = elapsed * rate / GEN_PULSE_ANCHOR_CYCLES;
  if (pull > 1.0f) pull = 1.0f;
  motion.pulse.offset -= drift * pull;
  return phase - drift * pull;
}

// The tracker's offset is what stops a speed change teleporting the pattern,
// and it is also what leaves a stopped pattern standing wherever the last
// traveling one ran out — so a patch saved still comes back somewhere else
// every time. A whole cell of offset is invisible, since the shape repeats
// once per cell, so only the fraction has to go and home is never further
// than half a cell away. Easing it out walks the pattern to Position without
// the jump that bringing a fader to rest must not produce.
//
// While travel is running the offset is where the pattern stands, so there is
// nothing to settle and this leaves it alone.
static float settledTravel(Tracker &tracker, float beats, float elapsed, float rate) {
  const float travel = trackedPhase(tracker, beats, rate);
  if (fabsf(rate) > 0.0001f || elapsed <= 0.0f) return travel;

  const float drift = tracker.offset - roundf(tracker.offset);
  if (fabsf(drift) < 0.0001f) return travel;

  float pull = elapsed / GEN_POSITION_SETTLE_BEATS;
  if (pull > 1.0f) pull = 1.0f;
  tracker.offset -= drift * pull;
  return travel - drift * pull;
}

// Under bounce the core's position is a triangle: out to one wall of its cell
// and back, once per cycle.
static inline float triangleSwing(float phase) {
  return (phase < 0.5f) ? (phase * 2.0f) : ((1.0f - phase) * 2.0f);
}

// Which phase stands the core where it already stands, for the mode being
// entered. Position is read out of the travel phase differently in each — a
// fraction of a cell under wrap, a triangle between the cell's two walls
// under bounce — and the tracker keeps the phase continuous rather than the
// position, so flipping the switch would otherwise teleport the shape.
//
// One thing it cannot preserve: bounce cannot put a core within half its own
// width of a cell wall, because that is where it turns — a shape standing
// there snaps out to the wall, by at most half its width. Every strip is
// solved for separately, so a fanned wall keeps its stagger across the flip
// rather than only the strip the wave reads zero at. A swung speed's shift is
// added on top of the tracker, so it is taken off what the tracker is asked
// for, in the phase units of the mode being entered.
static void reanchorTravel(Tracker &tracker, float beats, bool bouncing,
                           float speedPixels, float coreCells,
                           float halfCore, float swingSpan, float direction,
                           float cellLength, float positionCells, float shift) {
  float rate;
  float wanted;
  if (bouncing) {
    rate = (swingSpan > 0.0001f)
        ? fabsf(speedPixels) / (2.0f * swingSpan * cellLength) : 0.0f;
    float swing = (swingSpan > 0.0001f) ? (coreCells - halfCore) / swingSpan : 0.0f;
    if (swing < 0.0f) swing = 0.0f;
    else if (swing > 1.0f) swing = 1.0f;

    // The half of the swing already traveling the way the shape is, so it
    // carries on and turns at the end it was heading for.
    wanted = (direction >= 0.0f) ? (swing * 0.5f) : (1.0f - swing * 0.5f);
  } else {
    rate = speedPixels / cellLength;
    wanted = coreCells - 0.5f - positionCells;
  }
  tracker.rate = rate;
  tracker.offset = wanted - shift - beats * rate;
}

// A push is a fraction of the way from the dialed value to one of its two
// limits, and its sign picks which. Nothing can clip, and a control already
// sitting at a limit simply has nowhere to go that way.
static inline float pushToward(float base, float push, float low, float high) {
  const float limit = (push >= 0.0f) ? high : low;
  return base + fabsf(push) * (limit - base);
}

static inline float ccUnit(uint8_t value) { return (float)value / 127.0f; }

// Bipolar around 64: the center has to be "no departure at all", because
// every control spread this way measures how far something sits from where
// the faders put it.
static inline float ccBipolar(uint8_t value) {
  return value < 64 ? ((float)value - 64.0f) / 64.0f
                    : ((float)value - 64.0f) / 63.0f;
}

// Rounds to nearest, as the Teensy core's map() does.
static inline uint8_t ccToByte(uint8_t value, uint8_t top) {
  return (uint8_t)(((uint16_t)value * top + 63) / 127);
}

// Count is geometric because what reads on the wall is the ratio: one shape
// against two changes everything, sixteen against seventeen is invisible.
// Spread evenly instead, more than half the fader sits above eight shapes,
// where moving it does nothing anyone can see.
static uint8_t ccCount(uint8_t value) {
  const long shapes = lroundf(powf((float)GEN_MAX_COUNT, ccUnit(value)));
  return (uint8_t)(shapes < 1 ? 1 : shapes > GEN_MAX_COUNT ? GEN_MAX_COUNT : shapes);
}

// Stable per-pixel noise: the same (strip, pixel, bucket) always hashes to
// the same byte, so a draw holds still between re-rolls instead of boiling.
static inline uint8_t hash8(uint8_t a, uint8_t b, uint8_t c) {
  uint32_t h = (uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u ^ (uint32_t)c * 83492791u;
  h ^= h >> 13;
  h *= 0x5bd1e995u;
  h ^= h >> 15;
  return (uint8_t)h;
}

static inline float fanTriangle(float u) {
  return (u < 0.5f) ? (4.0f * u - 1.0f) : (3.0f - 4.0f * u);
}

// The fan's wave, running across the strips rather than through time. A
// triangle rather than a sine because it is what stands five strips at evenly
// spaced offsets; a sine bunches the middle three and a staircase stops
// looking straight.
//
// At the top of the frequency range neighbors sit half a cycle apart, so the
// wave reads the same two points whatever the phase — moving phase there only
// scales how deep the alternation is rather than moving it, and at a quarter
// and three quarters of a turn it reads zero on every strip and the fan goes
// quiet.
//
// Randomize crossfades each strip toward a fixed draw. It is the one
// arrangement no frequency reaches: every setting of a wave is orderly, and
// what the wall asked for was comets that do not look placed.
static float fanWave(const Params &p, uint8_t stripIndex) {
  const float ordered = fanTriangle(fract(p.fanPhase + p.fanFreq * (float)stripIndex));
  if (p.fanRandom < 0.0001f) return ordered;
  const float drawn = (float)hash8(stripIndex, 0, GEN_FAN_HASH_SALT) / 255.0f * 2.0f - 1.0f;
  return ordered + (drawn - ordered) * p.fanRandom;
}

// ---------------------------------------------------------------------------
// The color layer
//
// A color is hue, whiteness and darkness. Everything else is a push on those
// three, and the pushes add. Four sources push:
//
//   the placed field  something aimed — a gradient across a ruler, or regions
//                     sitting on it
//   the wander        the wall never quite the same in two places, and where
//                     it differs keeps moving
//   the light level   color read off how lit the shape branch left a pixel
//   the scatter       spots on a grid of cells, each on its own clock
//
// The scatter also reaches the light level itself, which none of the others
// do — they only push darkness, which is the color's own value. That push
// happens outside this layer, on what the shape branch left and before an
// unlit pixel is culled, which is what lets a spot appear in a gap.
//
// The layer reads the SHAPE branch's light level and never its own. Feed its
// own darkness back in and color depends on color: pull the wall down for a
// quiet verse and the hue slides with it.
// ---------------------------------------------------------------------------

static bool placedActive(const Params &p) {
  return fabsf(p.placed.hueReach) > 0.5f
      || fabsf(p.placed.whiteReach) > 0.001f
      || fabsf(p.placed.darkReach) > 0.001f;
}

static bool wanderActive(const Params &p) {
  return fabsf(p.wanderHueReach) > 0.5f
      || fabsf(p.wanderWhiteReach) > 0.001f
      || fabsf(p.wanderDarkReach) > 0.001f;
}

static bool litActive(const Params &p) {
  return fabsf(p.litHueReach) > 0.5f
      || fabsf(p.litWhiteReach) > 0.001f
      || fabsf(p.litDarkReach) > 0.001f;
}

static bool scatterTints(const Params &p) {
  return fabsf(p.scatterHueReach) > 0.5f
      || fabsf(p.scatterWhiteReach) > 0.001f;
}

static bool scatterActive(const Params &p) {
  return fabsf(p.scatterLightReach) > 0.001f || scatterTints(p);
}

// The base color sits at zero, so two terms that rarely reach their ends cost
// nothing: a sum huddled around the middle is the wall sitting at the color
// that was dialed. There is no floor here for a color to fall off.
static float wanderAt(const Params &p, uint8_t stripIndex, float along01, float t) {
  // Measured from the middle strip, not the first. Fanned from the first,
  // strip one never moves and the last does all the traveling, which reads as
  // a one-sided ramp rather than the wall opening — see docs/bench-facts.md
  // § "A field built as along-plus-across".
  const float acrossFromCenter =
      ((float)stripIndex - (float)(STRIPS - 1) * 0.5f) / (float)(STRIPS - 1);
  const float cyclesAlong = p.wanderCyclesAlong;
  float cyclesAcross = cyclesAlong * 0.3f;
  if (cyclesAcross > 1.4f) cyclesAcross = 1.4f;

  const float a = sinf(TURN
      * (cyclesAlong * along01 + cyclesAcross * acrossFromCenter + t));
  const float b = sinf(TURN
      * (cyclesAlong * GOLD * along01 - cyclesAcross * 1.37f * acrossFromCenter + t * GOLD));
  return (a + b) * 0.5f;
}

// A gradient is monotone with the base color at the ruler's center, so the reach
// is how far ONE end departs and the two ends land twice that apart. A region
// is a bump — base, departure, back to base — built from the shape branch's
// own core and fades, which is what makes count, width and edge mean the same
// thing in both branches.
static float placedAt(const PlacedField &field, float u, float drift) {
  if (!field.isRegion) return (u - 0.5f) * 2.0f;
  const float cell = u * (float)field.count + drift;
  return shapeAt(fract(cell) - 0.5f, field.width, field.edge, 0.0f);
}

float lightLeft(float dark) { return powf(DARK_FLOOR, -dark); }

// Pushes arrive summed and normalized. Darkening rides a geometric taper
// because it is a ratio of light and the eye reads it as one; mapped linearly,
// nearly the whole travel was imperceptible and everything worth having sat in
// the last few steps. Brightening is a plain ride to full and only has room
// when the V fader is left below the top. Both measured on the wall — see
// docs/bench-facts.md.
static Hsv applyPushes(Hsv base, float hue, float white, float dark) {
  if (white < -1.0f) white = -1.0f; else if (white > 1.0f) white = 1.0f;
  if (dark < -1.0f) dark = -1.0f; else if (dark > 1.0f) dark = 1.0f;

  const float satTarget = (white >= 0.0f) ? 0.0f : 255.0f;
  const float saturation = (float)base.s + fabsf(white) * (satTarget - (float)base.s);

  float value;
  if (dark >= 0.0f) value = (float)base.v + dark * (255.0f - (float)base.v);
  else value = (float)base.v * lightLeft(dark);

  return { (uint8_t)(base.h + (int16_t)hue), (uint8_t)saturation, (uint8_t)value };
}

// `offset` is the signed distance from the core's center, in cells, positive
// on the trailing side. Which shape a pixel is measured against is the
// caller's business, because that is a question about the strip's ends
// rather than about the shape.
//
// `width` is the solid core. `edge` and `tail` both reach outward from it
// into the gap rather than eating into it, so softening a shape never makes
// it smaller. Both are scaled by the gap that is actually available, which
// means edge at full always closes the gaps to the neighboring shapes — the
// two fades meet at zero and never have to be summed.
//
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

// The tail is not geometry. It is how far the core has traveled since it was
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
// center, only swept by the body at the turn, so they measure from the turn
// and add the straight remainder.
static float trailBehind(float journey, float phase, float halfCore, float swingSpan) {
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

// 0 at one end of the ruler, 1 at the other. `alongPixels` is fractional
// because the field is read several times across one pixel, and the strip
// ruler's whole numbers are pixel centers: the shape branch's pixel runs from
// `pixelIndex` to `pixelIndex + 1`, this one is centered on `pixelIndex`.
static float rulerAt(const PlacedField &field, uint8_t stripIndex,
                     float alongPixels, float shapeU) {
  if (field.ruler == RULER_WALL) return (float)stripIndex / (float)(STRIPS - 1);
  if (field.ruler == RULER_SHAPE) return shapeU;
  return alongPixels / (float)(PIXELS - 1);
}

// The one source with a position of its own. The pulse is a value over time
// with nowhere on the wall; the wander is smooth over both. This one is random
// over both — a grid of cells, each with its own clock, each lighting a spot
// that appears, holds, fades, and may slide across its own cell while it does.
//
// Stateless: a cell's clock is its hash, so there is no particle list and
// nothing to advance. Width is the spot's core on both axes at once — how much
// of its cell it covers, and how much of its cycle it is lit — and edge softens
// both the same way.
//
// Moving stagger re-keys every cell, so everything in flight jumps. That is the
// price of holding no state, and it is confined to that one control: the rate
// runs through the phase tracker like every other rate here.
static float scatterAt(const Params &p, uint8_t stripIndex, float alongPixels, float t) {
  const float cellF = (alongPixels / (float)PIXELS) * (float)p.scatterCount;
  const uint8_t cell = (uint8_t)cellF;
  const float u = cellF - (float)cell;

  const float rateSpread = (float)hash8(stripIndex, cell, 17) / 255.0f - 0.5f;
  const float phaseOffset = (float)hash8(stripIndex, cell, 43) / 255.0f;
  const float age = fract(t * (1.0f + p.scatterStagger * rateSpread)
                          + p.scatterStagger * phaseOffset);

  // Centered on the middle of the cycle, so a cell runs dark, lights, holds and
  // fades rather than being cut off at the wrap.
  const float alive = coreAt(age - 0.5f, p.scatterWidth, p.scatterEdge);
  if (alive <= 0.0001f) return 0.0f;

  const float center = 0.5f + p.scatterDrift * (age - 0.5f);
  return alive * coreAt(u - center, p.scatterWidth, p.scatterEdge);
}

static Hsv colorAt(const Params &p, float placedLevel, uint8_t stripIndex,
                   uint8_t pixelIndex, float profile, float wanderT,
                   float scatter, bool wanderOn) {
  const float along01 = (float)pixelIndex / (float)(PIXELS - 1);
  const float wander = wanderOn ? wanderAt(p, stripIndex, along01, wanderT) : 0.0f;

  return applyPushes(p.color,
      placedLevel * p.placed.hueReach + wander * p.wanderHueReach
          + profile * p.litHueReach + scatter * p.scatterHueReach,
      placedLevel * p.placed.whiteReach + wander * p.wanderWhiteReach
          + profile * p.litWhiteReach + scatter * p.scatterWhiteReach,
      placedLevel * p.placed.darkReach + wander * p.wanderDarkReach
          + profile * p.litDarkReach);
}

// The shape repeats once per cell, so the only images that can reach a sample
// are the two standing either side of it. Under bounce the strip is a line and
// an image off its end is not there to be seen — which is what stops a fade
// leaving one end of the strip and arriving at the other.
static bool nearestOffset(float posCells, float coreCenter, float stripDirection,
                          bool bounce, float countCells, float &out) {
  const float firstImage = coreCenter + floorf(posCells - coreCenter);
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

// Bipolar around 64, squared so the slow end — where every pattern in the
// roster actually lives — gets most of the travel.
static float squaredRate(uint8_t value, float max) {
  const float x = ((float)value - 64.0f) / 63.0f;
  return (x < 0.0f ? -1.0f : 1.0f) * x * x * max;
}

// One step either side of center is a crawl of a pixel a minute, which is not
// a speed anyone dials: it is a pattern that will not sit where Position puts
// it, since the settle runs only at a standstill. Snapping it to nothing
// costs the two steps that already read as still and makes still mean still.
static inline float stillBelowThreshold(float pixels) {
  return (fabsf(pixels) < GEN_STILL_PIXELS_PER_BEAT) ? 0.0f : pixels;
}

static float fanFreqFrom(uint8_t value) {
  const long step = lroundf((float)value * GEN_FAN_FREQ_STEPS / 127.0f);
  return (float)step * (GEN_FAN_MAX_CYCLES_PER_STRIP / GEN_FAN_FREQ_STEPS);
}

// Squared, like every rate here: what reads as depth rather than as an effect
// lives at the slow end, and spread evenly that end is a few steps of fader.
// Unipolar unlike the travel speeds where the look is symmetric, so frozen
// sits at the end of the throw rather than at a center this taper makes hard
// to tell from a crawl.
static inline float squaredUnit(uint8_t value, float max) {
  const float x = ccUnit(value);
  return x * x * max;
}

float convert(uint8_t cc, uint8_t value) {
  switch (cc) {
    case CC_HUE:             return ccToByte(value, 250);
    case CC_SATURATION:
    case CC_VALUE:
    case CC_WASH_LEVEL:
    case CC_WASH_HUE_OFFSET:
    case CC_WASH_SATURATION: return ccToByte(value, 255);

    case CC_GEN_COUNT:
    case CC_PLACED_COUNT:
    case CC_SCATTER_COUNT:   return ccCount(value);

    case CC_GEN_POSITION:
    case CC_GEN_FAN:
    case CC_GEN_FAN_PULSE:   return ccBipolar(value) * 0.5f;
    case CC_GEN_SPEED:
      return stillBelowThreshold(squaredRate(value, GEN_MAX_SPEED_PIXELS_PER_BEAT));
    // The same squared curve Speed runs on, so that mirroring one fader about
    // its center against the other cancels *exactly*: a still strip at the
    // wave's peak needs Speed to be the fan's opposite, and two controls on
    // different curves can only ever nearly cancel.
    case CC_GEN_FAN_RATE:    return squaredRate(value, GEN_MAX_SPEED_PIXELS_PER_BEAT);
    case CC_GEN_FAN_FREQ:    return fanFreqFrom(value);
    case CC_GEN_FAN_PHASE:   return (float)value / 128.0f;
    case CC_GEN_PULSE_RATE:  return aurora_pulse_period(value);

    case CC_PLACED_HUE:      return ccBipolar(value) * PLACED_MAX_HUE;
    case CC_PLACED_SPEED:    return squaredRate(value, PLACED_MAX_CELLS_PER_BEAT);
    case CC_WANDER_HUE:      return ccBipolar(value) * WANDER_MAX_HUE;
    case CC_WANDER_RATE:     return squaredUnit(value, WANDER_MAX_CYCLES_PER_BEAT);
    case CC_WANDER_SCALE:    return 0.12f * powf(180.0f, ccUnit(value));
    case CC_LIT_HUE:         return ccBipolar(value) * LIT_MAX_HUE;
    case CC_SCATTER_RATE:    return squaredUnit(value, SCATTER_MAX_CYCLES_PER_BEAT);
    case CC_SCATTER_HUE:     return ccBipolar(value) * SCATTER_MAX_HUE;

    case CC_PLACED_WHITE:
    case CC_PLACED_DARK:
    case CC_WANDER_WHITE:
    case CC_WANDER_DARK:
    case CC_LIT_DARK:
    // Drift is a displacement rather than a rate — how far, and which way, a
    // spot slides across its own cell over its life, which is why it is not
    // called speed like everything else here.
    case CC_SCATTER_DRIFT:
    case CC_SCATTER_LIGHT:
    case CC_SCATTER_WHITE:   return ccBipolar(value);

    case CC_GEN_WIDTH:
    case CC_GEN_EDGE:
    case CC_GEN_TAIL:
    case CC_GEN_FAN_RANDOM:
    case CC_PLACED_WIDTH:
    case CC_PLACED_EDGE:
    case CC_LIT_WHITE:
    case CC_SCATTER_WIDTH:
    case CC_SCATTER_EDGE:
    case CC_SCATTER_STAGGER: return ccUnit(value);

    default:                 return (float)value;
  }
}

static void readParams(const uint8_t *dialed, const Pushes *pushes, Params &p) {
  auto at = [&](uint8_t cc) { return convert(cc, routed(dialed, pushes, cc)); };

  p.width = at(CC_GEN_WIDTH);
  p.edge = at(CC_GEN_EDGE);
  p.tail = at(CC_GEN_TAIL);
  p.count = (uint8_t)at(CC_GEN_COUNT);
  p.positionCells = at(CC_GEN_POSITION);
  p.speedPixels = at(CC_GEN_SPEED);
  p.fanPosition = at(CC_GEN_FAN);
  p.fanPulse = at(CC_GEN_FAN_PULSE);
  p.fanRate = at(CC_GEN_FAN_RATE);
  p.fanFreq = at(CC_GEN_FAN_FREQ);
  p.fanPhase = at(CC_GEN_FAN_PHASE);
  p.fanRandom = at(CC_GEN_FAN_RANDOM);
  p.pulseBeats = at(CC_GEN_PULSE_RATE);

  // A switch has no middle for a push to land in, so it is read as dialed.
  p.alternate = aurora_cc_is_on(dialed[CC_GEN_ALTERNATE]);
  p.bounce = aurora_cc_is_on(dialed[CC_GEN_BOUNCE]);
  p.placed.isRegion = aurora_cc_is_on(dialed[CC_COLOR_REGION]);
  const uint8_t ruler = aurora_cc_band3(dialed[CC_COLOR_RULER]);
  p.placed.ruler = (ruler > RULER_SHAPE) ? RULER_SHAPE : ruler;

  p.placed.hueReach = at(CC_PLACED_HUE);
  p.placed.whiteReach = at(CC_PLACED_WHITE);
  p.placed.darkReach = at(CC_PLACED_DARK);
  p.placed.width = at(CC_PLACED_WIDTH);
  p.placed.edge = at(CC_PLACED_EDGE);
  p.placed.count = (uint8_t)at(CC_PLACED_COUNT);
  p.placed.cellsPerBeat = at(CC_PLACED_SPEED);

  p.wanderHueReach = at(CC_WANDER_HUE);
  p.wanderWhiteReach = at(CC_WANDER_WHITE);
  p.wanderDarkReach = at(CC_WANDER_DARK);
  p.wanderCycles = at(CC_WANDER_RATE);
  p.wanderCyclesAlong = at(CC_WANDER_SCALE);

  p.litHueReach = at(CC_LIT_HUE);
  p.litWhiteReach = at(CC_LIT_WHITE);
  p.litDarkReach = at(CC_LIT_DARK);

  // The scatter's grid is its own, not the shape branch's, so a fine texture
  // can lie over one wide bar.
  p.scatterRate = at(CC_SCATTER_RATE);
  p.scatterCount = (uint8_t)at(CC_SCATTER_COUNT);
  p.scatterWidth = at(CC_SCATTER_WIDTH);
  p.scatterEdge = at(CC_SCATTER_EDGE);
  p.scatterStagger = at(CC_SCATTER_STAGGER);
  p.scatterDrift = at(CC_SCATTER_DRIFT);
  p.scatterLightReach = at(CC_SCATTER_LIGHT);
  p.scatterHueReach = at(CC_SCATTER_HUE);
  p.scatterWhiteReach = at(CC_SCATTER_WHITE);

  p.color = colorFrom(dialed, pushes);
}

static void readFan(const Params &p, FanReading &fan) {
  for (uint8_t i = 0; i < STRIPS; i++) fan.values[i] = fanWave(p, i);
  for (uint16_t i = 0; i < FAN_CURVE_POINTS; i++) {
    const float at = (float)i / (float)FAN_CURVE_STEPS_PER_STRIP;
    fan.curve[i] = fanTriangle(fract(p.fanPhase + p.fanFreq * at));
  }
  fan.turns = p.fanFreq * (float)(STRIPS - 1);
  fan.stillAt = (fabsf(p.fanRate) > 0.0001f) ? -p.speedPixels / p.fanRate : 2.0f;
  fan.position = p.fanPosition * 2.0f;
  fan.rate = p.fanRate / GEN_MAX_SPEED_PIXELS_PER_BEAT;
  fan.pulse = p.fanPulse * 2.0f;
  fan.scrambled = p.fanRandom;
}

Hsv colorFrom(const uint8_t *dialed, const Pushes *pushes) {
  auto at = [&](uint8_t cc) { return (uint8_t)convert(cc, routed(dialed, pushes, cc)); };
  return { at(CC_HUE), at(CC_SATURATION), at(CC_VALUE) };
}

// The washes take the strips' dialed color and never its pushes: a push
// reaches one fixture family, and CC 33-35 are the strips'. Their own three
// take theirs. Color is converted at full value and the light is carried by
// the fixture's own dimmer, so the emitters stay near full scale where they
// have the most resolution.
Wash washFrom(const uint8_t *dialed, const Pushes *pushes) {
  auto at = [&](uint8_t cc) { return (uint8_t)convert(cc, routed(dialed, pushes, cc)); };
  const Hsv strips = colorFrom(dialed, nullptr);
  const uint8_t level = at(CC_WASH_LEVEL);
  const uint8_t hueOffset = at(CC_WASH_HUE_OFFSET);
  // A scale down from the strips' saturation rather than a setting: the
  // washes are a relationship to the strips.
  const uint8_t saturation = at(CC_WASH_SATURATION);

  return { hsvRainbow((uint8_t)(strips.h + hueOffset), scale8(strips.s, saturation), 255),
           scale8(strips.v, level) };
}

// Hues run red to blue in index order, spaced widely enough to stay
// distinguishable at a distance and through a phone camera.
void renderStripOrder(Rgb *pixels) {
  static const uint8_t hues[STRIPS] = { 0, 40, 96, 130, 165 };
  for (uint8_t stripIndex = 0; stripIndex < STRIPS; stripIndex++) {
    const Rgb flat = hsvRainbow(hues[stripIndex], 255, 200);
    for (uint8_t pixelIndex = 0; pixelIndex < PIXELS; pixelIndex++) {
      pixels[stripIndex * PIXELS + pixelIndex] = flat;
    }
  }
}

void renderGenerator(const uint8_t *dialed, float beats, Motion &motion, Frame &out) {
  for (uint16_t i = 0; i < STRIPS * PIXELS; i++) out.pixels[i] = { 0, 0, 0 };

  // Read three times. The first has no pushes in it, which is what the clock's
  // own rate needs, since it is a destination routes refuse. The second
  // takes the plain reading of the clock, and everything the strip loop is
  // built on comes from it. The third is each strip's own reading, so a push
  // rolls across the wall instead of landing on all five at once.
  Params p;
  readParams(dialed, nullptr, p);
  const float pulse = anchoredPulsePhase(motion, beats, 1.0f / p.pulseBeats);
  out.clock = pulse;

  Pushes pushes;
  gatherRoutes(dialed, p.pulseBeats, pulse, pulse, pushes);
  readParams(dialed, &pushes, p);
  out.wash = washFrom(dialed, &pushes);
  readFan(p, out.fan);

  const float cellLength = (float)PIXELS / (float)p.count;
  const float countCells = (float)p.count;

  // Under bounce the core swings inside its own cell, turning where its own
  // edge meets the cell's boundary the way a ball meets a wall, so nothing
  // ever crosses into a neighboring cell. Taking the rate from the cell is
  // what keeps speed an absolute distance: adding shapes shrinks the cell and
  // quickens the turn, and the core still crosses the wall at the pixels per
  // beat on the dial. At full width the swing closes to nothing, which is
  // right — a shape filling its cell has nowhere to go.
  const float halfCore = p.width * 0.5f;
  const float swingSpan = 1.0f - p.width;

  // Speed is what the strip the wave reads zero at travels at, and the fan's
  // rate amount is measured from there. So a wall can be turning with Speed
  // at a standstill, and bounce has to ask the five rather than the one dial
  // — and the routes, since a swing moves a wall whose dials are all still.
  float stripSpeeds[STRIPS];
  bool anyMoving = routeAims(dialed, CC_GEN_SPEED) || routeAims(dialed, CC_GEN_FAN_RATE);
  for (uint8_t i = 0; i < STRIPS; i++) {
    stripSpeeds[i] = stillBelowThreshold(p.speedPixels + p.fanRate * fanWave(p, i));
    if (stripSpeeds[i] != 0.0f) anyMoving = true;
  }
  const bool bouncing = p.bounce && anyMoving;

  const float travelElapsed = beats - motion.lastTravelBeats;
  motion.lastTravelBeats = beats;

  // Every color rate goes through the tracker for the same reason travel and
  // the pulse do: beats only grows, so a small change of rate multiplied by a
  // large beat count is a large jump. A route's swing is added per strip.
  const float wanderDialed = trackedPhase(motion.wander, beats, p.wanderCycles);
  const float placedDialed = trackedPhase(motion.placed, beats, p.placed.cellsPerBeat);
  const float scatterDialed = trackedPhase(motion.scatter, beats, p.scatterRate);

  for (uint8_t stripIndex = 0; stripIndex < STRIPS; stripIndex++) {
    const float wave = fanWave(p, stripIndex);
    const float stripOffset = p.fanPosition * wave;

    const bool mirrored = p.alternate && (stripIndex & 1);

    // One wave, three amounts, so where a strip stands, how fast it runs and
    // where it is in the swell are dialed apart — a wall of staggered bars
    // can strobe in unison, which one shared offset could never do. The
    // washes take the unfanned phase whatever these say: a PAR is one
    // position with no strip to be offset from.
    const float stripPulse = pulse + p.fanPulse * wave;
    out.stripClock[stripIndex] = stripPulse;

    Params s;
    gatherRoutes(dialed, p.pulseBeats, pulse, stripPulse, pushes);
    readParams(dialed, &pushes, s);

    const float wanderT = wanderDialed + pushes.shift[CC_WANDER_RATE];
    const float placedDrift = placedDialed + pushes.shift[CC_PLACED_SPEED];
    const float scatterT = scatterDialed + pushes.shift[CC_SCATTER_RATE];
    const float shiftPixels =
        pushes.shift[CC_GEN_SPEED] + pushes.shift[CC_GEN_FAN_RATE] * wave;

    const bool placedOn = placedActive(s);
    const bool wanderOn = wanderActive(s);
    const bool scatterOn = scatterActive(s);
    const bool colorFlat = !placedOn && !wanderOn && !litActive(s) && !scatterTints(s);

    // A switch belongs to the patch, so the one moment it moves is an arrival
    // the performer caused and is watching — see DESIGN.md § "Switches belong
    // to the patch". That is the moment a jump would be most visible, so the
    // phase is solved for rather than carried across.
    const float stripSpeed = stripSpeeds[stripIndex];
    const float direction = (stripSpeed >= 0.0f) ? 1.0f : -1.0f;
    // Under bounce the tracker runs at the speed's magnitude and the direction
    // is applied after, so a shift toward the dialed direction is more of the
    // swing and one against it is less.
    const float shiftCycles = (swingSpan > 0.0001f)
        ? direction * shiftPixels / (2.0f * swingSpan * cellLength) : 0.0f;
    const float shiftCells = shiftPixels / cellLength;
    Tracker &tracker = motion.travel[stripIndex];
    if (bouncing != motion.lastBouncing) {
      reanchorTravel(tracker, beats, bouncing, stripSpeed,
                     motion.lastCoreCells[stripIndex], halfCore, swingSpan,
                     direction, cellLength, s.positionCells,
                     bouncing ? shiftCycles : shiftCells);
    }

    float travelCycles = 0.0f;
    float centerCells = 0.5f;
    if (bouncing) {
      const float rate = (swingSpan > 0.0001f)
          ? fabsf(stripSpeed) / (2.0f * swingSpan * cellLength) : 0.0f;
      travelCycles = trackedPhase(tracker, beats, rate) + shiftCycles;
    } else {
      // Half a cell puts a still shape in the middle of its cell rather than
      // straddling the boundary — which at count 1 is the strip's two ends.
      // Position slides it from there.
      centerCells = 0.5f + s.positionCells
          + settledTravel(tracker, beats, travelElapsed, stripSpeed / cellLength)
          + shiftCells;
    }
    motion.lastCoreCells[stripIndex] = bouncing
        ? (halfCore + triangleSwing(fract(travelCycles)) * swingSpan)
        : fract(centerCells);

    const float width = s.width;

    // The two sides of a shape are not the same length — a tail reaches much
    // further than an edge fade — so they are normalized separately. Halfway
    // between the two tips is not the core, and a region asked to sit at the
    // middle of a shape means the core every time.
    const float shapeGap = 1.0f - width;
    const float shapeLead = width * 0.5f + s.edge * shapeGap * 0.5f;
    const float shapeTrail = width * 0.5f + fmaxf(s.edge * shapeGap * 0.5f, s.tail * shapeGap);

    // Odd strips run the journey backwards, rather than only mirroring the
    // shape where it stands. Travel is one value every strip shares, so
    // flipping the direction alone left the shape moving the same way and
    // showed up on nothing but the side a tail fell on.
    //
    // Under bounce the position amount offsets where a strip stands in its
    // own swing, so the five turn at different moments. It cannot offset the
    // core's position instead: an image standing past the strip's end is
    // clipped away by nearestOffset, so displacing it there shortens a strip
    // rather than staggering it.
    float coreCenter;
    float stripDirection;
    float triangle = 0.0f;
    if (bouncing) {
      triangle = fract(travelCycles + stripOffset);
      const bool rising = triangle < 0.5f;
      const float place = halfCore + triangleSwing(triangle) * swingSpan;
      coreCenter = mirrored ? (1.0f - place) : place;
      stripDirection = rising ? 1.0f : -1.0f;
      if (mirrored) stripDirection = -stripDirection;
    } else {
      const float centerHere = mirrored ? (countCells - centerCells) : centerCells;
      coreCenter = fract(centerHere + stripOffset);
      stripDirection = mirrored ? -direction : direction;
    }

    for (uint8_t pixelIndex = 0; pixelIndex < PIXELS; pixelIndex++) {
      // The placed field is read at the same samples the shape is, and for
      // the same reason: read once at the pixel's center it aliases as soon
      // as its regions get down to a pixel or two across, which is the fault
      // docs/bench-facts.md § "Point-sampling a pattern aliases" records
      // against the shape branch. The wander needs none of this — it is
      // sines, and smooth by construction — and the light level reads the
      // averaged profile already.
      float accumulated = 0.0f;
      float placedAccumulated = 0.0f;
      float scatterAccumulated = 0.0f;
      for (uint8_t sampleIndex = 0; sampleIndex < GEN_SUBSAMPLES; sampleIndex++) {
        const float acrossPixel =
            ((float)sampleIndex + 0.5f) / (float)GEN_SUBSAMPLES - 0.5f;
        const float posCells = ((float)pixelIndex + 0.5f + acrossPixel) / cellLength;
        float shapeU = 0.5f;

        if (bouncing) {
          // The core stays inside its cell, so its trail does too: at a turn
          // the core walks back out through what it laid down rather than the
          // trail changing sides.
          const float journey = journeyIn(posCells, mirrored);
          float level = 0.0f;
          float nearest = 0.0f;
          const bool onShape =
              nearestOffset(posCells, coreCenter, stripDirection, true, countCells, nearest);
          if (onShape) level = coreAt(nearest, width, s.edge);
          const float behind = trailBehind(journey, triangle, halfCore, swingSpan);
          const float trailing = tailAt(behind, width, s.tail);
          accumulated += (trailing > level) ? trailing : level;

          // The ruler's trailing half has to be the same measure the tail is
          // drawn from, or color along a tail paints where the tail is not.
          if (behind < shapeTrail && shapeTrail > 0.0001f) {
            shapeU = 0.5f + 0.5f * behind / shapeTrail;
          } else if (onShape && shapeLead > 0.0001f) {
            shapeU = 0.5f - 0.5f * fabsf(nearest) / shapeLead;
          }
        } else {
          float nearest = 0.0f;
          if (nearestOffset(posCells, coreCenter, stripDirection, p.bounce, countCells, nearest)) {
            accumulated += shapeAt(nearest, width, s.edge, s.tail);
            const float reach = (nearest < 0.0f) ? shapeLead : shapeTrail;
            if (reach > 0.0001f) shapeU = 0.5f + 0.5f * nearest / reach;
          }
        }

        if (placedOn) {
          if (shapeU < 0.0f) shapeU = 0.0f;
          else if (shapeU > 1.0f) shapeU = 1.0f;
          const float u = rulerAt(s.placed, stripIndex,
                                  (float)pixelIndex + acrossPixel, shapeU);
          placedAccumulated += placedAt(s.placed, u, placedDrift);
        }

        // Read at the same samples the shape and the placed field are, and for
        // the same reason: at twenty cells a cell is 2.2 pixels, and a spot
        // that size aliases into a flicker if it is read once.
        if (scatterOn) {
          scatterAccumulated +=
              scatterAt(s, stripIndex, (float)pixelIndex + 0.5f + acrossPixel, scatterT);
        }
      }

      const float profile = accumulated / (float)GEN_SUBSAMPLES;
      const float scatter = scatterAccumulated / (float)GEN_SUBSAMPLES;

      // The push runs from what the shape branch left toward one of the two
      // limits, which is what makes a spot invisible inside a fully lit shape
      // and visible in the gap beside it — no occlusion rule anywhere. It
      // therefore has to be applied before an unlit pixel is culled, or the one
      // place a spot has the furthest to travel is the one place it could never
      // appear.
      float brightness = profile;
      if (scatterOn) {
        brightness = pushToward(brightness, scatter * s.scatterLightReach, 0.0f, 1.0f);
      }
      if (brightness <= 0.002f) continue;

      // Scaling the RGB rather than handing a low value to the conversion keeps
      // the hue where it was set: converting at a low value lets a channel
      // truncate to zero before its neighbor, which is what turns a dim yellow
      // red.
      const Hsv tint = colorFlat
          ? s.color
          : colorAt(s, placedAccumulated / (float)GEN_SUBSAMPLES, stripIndex,
                    pixelIndex, profile, wanderT, scatter, wanderOn);
      out.pixels[stripIndex * PIXELS + pixelIndex] =
          scaleVideo(hsvRainbow(tint.h, tint.s, 255), (uint8_t)((float)tint.v * brightness));
    }
  }
  motion.lastBouncing = bouncing;
}

}  // namespace render
