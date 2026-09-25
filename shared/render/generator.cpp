#include "render.h"

#include <math.h>

#include "aurora_protocol.h"
#include "palettes.h"

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
// state is what Motion carries and the path each tail is drawn from, and a
// tempo change is a change of rate, not a jump.
/////////////////////////////////

namespace render {

// Speed is an absolute distance per beat, because it is motion through real
// space and should not change when the count does. Width and edge are
// proportions of the shape instead — measured in pixels, their useful range
// collapses as the shapes get narrower, and most of each slider's travel
// stops doing anything.
static const uint8_t GEN_MAX_COUNT = 20;
static const float GEN_MAX_SPEED_PIXELS_PER_BEAT = 60.0f;

// The fan's wave runs across the strips, and five of them cannot sample
// anything faster than half a cycle each: at that setting every strip lands
// on the opposite point of the wave from its neighbors, and above it the wave
// folds back onto slower ones. So the fader stops there.
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

// The washes' LFO spread runs to half a cycle between neighbors in 16 steps
// each way, so a chase at a quarter and pairs at a half both land exactly.
static const float WASH_LFO_SPREAD_STEPS = 16.0f;

// How long the LFO takes to walk back onto the musical grid after its rate
// has been moved, measured in its own cycles so the correction is always the
// same fraction of a swell and never a visible lurch.
static const float GEN_LFO_ANCHOR_CYCLES = 2.0f;

// How long a stopped pattern takes to walk home to Position, in beats. Long
// enough that bringing Speed to a stop reads as settling rather than as a
// second move of its own.
static const float GEN_POSITION_SETTLE_BEATS = 2.0f;

// A frame this much later than the last is not the next frame: the transport
// jumped, or the editor's tab sat in the background. The path across the gap
// is not known, and joining its two ends would draw a streak.
static const float GEN_PATH_GAP_BEATS = 0.5f;

// Travel accumulates from rates and cannot jump; what can is where Position
// and the fan place the core, moved by a fader or a route with a hard edge.
// Faster than this in one frame is a jump rather than a movement, and a jump
// leaves no glow across the span it skipped: the tail starts again from where
// the core landed. A whole cell is not a jump at all but a circular control
// wrapping, the same place, and the record is lifted across it.
static const float GEN_JUMP_CELLS = 0.25f;

// How finely a cell's journey is divided to remember when the core last
// covered each point of it. At one shape a strip this is a bin every fifth of
// a pixel, finer than the four samples a pixel is read at.
static const uint16_t GEN_GLOW_BINS = 256;

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

// At full bend the fastest point on the span is 39 times the slowest. At 1 the
// slowest point would stop and a shape arriving there would never leave.
static const float GEN_BEND_MAX = 0.95f;
static const uint8_t GEN_BEND_STEPS = 64;

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
  uint8_t primitive;  // AuroraColorPrimitive
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
  float glowBeats;
  float positionCells;
  float speedPixels;
  float fanPosition;
  float fanLfo;
  float fanRate;
  float fanFreq;
  float fanPhase;
  float fanRandom;
  float bend;
  float bendAt;
  float lfoBeats;
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
  float scatterPlace;
  float scatterLightReach;
  float scatterHueReach;
  float scatterWhiteReach;

  Hsv color;
};

static inline float fract(float x) { return x - floorf(x); }

static float coreAt(float offset, float width, float edge);

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
// it out over the next couple of cycles walks the LFO back onto the grid
// without ever jumping.
//
// This is why the rate is stepped rather than continuous. Anchoring puts the
// cycle's zero on the music's zero; only a period a bar holds a whole number
// of keeps it there, which is what AURORA_LFO_PERIODS is.
static float anchoredLfoPhase(Motion &motion, float beats, float rate) {
  const float elapsed = beats - motion.lastLfoBeats;
  motion.lastLfoBeats = beats;

  // The transport restarted, and beat zero is a bar line by definition.
  if (elapsed < 0.0f) {
    motion.lfo.offset = 0.0f;
    motion.lfo.rate = rate;
    return beats * rate;
  }

  const float phase = trackedPhase(motion.lfo, beats, rate);
  const float drift = motion.lfo.offset - roundf(motion.lfo.offset);
  if (fabsf(drift) < 0.0001f) return phase;

  float pull = elapsed * rate / GEN_LFO_ANCHOR_CYCLES;
  if (pull > 1.0f) pull = 1.0f;
  motion.lfo.offset -= drift * pull;
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
static inline uint8_t hash8(uint32_t a, uint32_t b, uint32_t c) {
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
  if (field.primitive == COLOR_PRIMITIVE_GRADIENT) return (u - 0.5f) * 2.0f;
  const float cell = u * (float)field.count + drift;
  const float bump = coreAt(fract(cell) - 0.5f, field.width, field.edge);
  return field.primitive == COLOR_PRIMITIVE_INSIDE_OUT ? 1.0f - bump : bump;
}

float lightLeft(float dark) { return powf(DARK_FLOOR, -dark); }

// Pushes arrive summed, and both only ever lead away from a full color: toward
// white, and toward dark. Darkening rides a geometric taper because it is a
// ratio of light and the eye reads it as one; mapped linearly, nearly the whole
// travel was imperceptible and everything worth having sat in the last few
// steps. Measured on the wall — see docs/bench-facts.md.
static Hsv applyPushes(Hsv base, float hue, float white, float dark) {
  if (white > 1.0f) white = 1.0f;
  if (dark > 1.0f) dark = 1.0f;

  const float saturation = (float)base.s * (1.0f - white);
  const float value = (float)base.v * lightLeft(-dark);

  return { (uint8_t)(base.h + (int16_t)hue), (uint8_t)saturation, (uint8_t)value };
}

// `offset` is the signed distance from the core's center, in cells. Which
// shape a pixel is measured against is the caller's business, because that is
// a question about the strip's ends rather than about the shape.
//
// `width` is the solid core. `edge` reaches outward from it into the gap
// rather than eating into it, so softening a shape never makes it smaller.
// It is scaled by the gap that is actually available, which means edge at
// full always closes the gaps to the neighboring shapes — the two fades meet
// at zero and never have to be summed.
//
// The core and its edge fade are geometry: they sit around the core wherever
// it stands, the same on both sides. A tail is not — see walkPath.
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

// The one source with a position of its own. The LFO is a value over time
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
  const float clock = t * (1.0f + p.scatterStagger * rateSpread)
                    + p.scatterStagger * phaseOffset;
  const float age = fract(clock);

  // Centered on the middle of the cycle, so a cell runs dark, lights, holds and
  // fades rather than being cut off at the wrap.
  const float alive = coreAt(age - 0.5f, p.scatterWidth, p.scatterEdge);
  if (alive <= 0.0001f) return 0.0f;

  // Rolled from the life's own number, so the spot moves only while it is
  // dark, and kept far enough in that the core never crosses its cell's edge.
  const uint32_t life = (uint32_t)(int32_t)floorf(clock);
  const float room = 0.5f - 0.5f * p.scatterWidth;
  const float landing = (room > 0.0f)
      ? p.scatterPlace * room * ((float)hash8(stripIndex, cell * 131u + life, 61) / 255.0f * 2.0f - 1.0f)
      : 0.0f;
  const float center = 0.5f + landing + p.scatterDrift * (age - 0.5f);
  return alive * coreAt(u - center, p.scatterWidth, p.scatterEdge);
}

static Hsv colorAt(const Params &p, float placedLevel, uint8_t stripIndex,
                   uint8_t pixelIndex, float profile, float wanderT,
                   float scatter, bool wanderOn) {
  const float along01 = (float)pixelIndex / (float)(PIXELS - 1);
  const float wander = wanderOn ? wanderAt(p, stripIndex, along01, wanderT) : 0.0f;

  // Hue turns both ways, so a gradient's two ends and the wander's two swings
  // each have somewhere to go. White and dark go one way, so both ends of a
  // gradient depart from the fader color at its center, and the wander departs
  // where it swings high and leaves the rest alone.
  const float placedAway = fabsf(placedLevel);
  const float wanderAway = (wander > 0.0f) ? wander : 0.0f;
  return applyPushes(p.color,
      placedLevel * p.placed.hueReach + wander * p.wanderHueReach
          + profile * p.litHueReach + scatter * p.scatterHueReach,
      placedAway * p.placed.whiteReach + wanderAway * p.wanderWhiteReach
          + profile * p.litWhiteReach + scatter * p.scatterWhiteReach,
      placedAway * p.placed.darkReach + wanderAway * p.wanderDarkReach
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

void clearPaths(Paths &paths) { paths.empty = true; }

static inline uint16_t pathSlot(int32_t step) {
  const int32_t slot = step % (int32_t)PATH_STEPS;
  return (uint16_t)(slot < 0 ? slot + PATH_STEPS : slot);
}

// A step that fell between two frames takes the place on the straight line
// between where the core stood at each.
static void recordPath(Path &path, const Paths &paths, bool fresh, int32_t step,
                       float beats, float cells) {
  if (fresh) {
    path.firstStep = step + 1;
    path.lastCells = cells;
    return;
  }
  const float elapsed = beats - paths.lastBeats;
  for (int32_t at = paths.lastStep + 1; at <= step; at++) {
    const float k = (elapsed > 0.0f)
        ? ((float)at / (float)PATH_STEPS_PER_BEAT - paths.lastBeats) / elapsed : 1.0f;
    path.cells[pathSlot(at)] = path.lastCells + k * (cells - path.lastCells);
  }
  path.lastCells = cells;
}

// How long ago, in beats, the core's body last covered each point of one
// cell's journey; -1 where it has not within the glow. Every shape on a strip
// is an image of one core, so one journey serves them all.
struct Glow {
  float age[GEN_GLOW_BINS];
  // How far the core traveled while the glow lasts, which is how long the tail
  // stands on the wall.
  float tailCells;
  // Which way the core last moved, in cells, or 0 if it has not.
  float direction;
};

// Stamps the points in (from, to] the moment the body's edge reached them, on
// a stretch the core's center walked from `newer` to `older`: the edge stands
// `reach` beyond the center.
static void stampReached(Glow &glow, float from, float to, float reach,
                         float newerCells, float newerAge, float olderCells, float olderAge) {
  const float walked = olderCells - newerCells;
  const int32_t first = (int32_t)ceilf(from * (float)GEN_GLOW_BINS - 0.5f);
  for (int32_t bin = first;; bin++) {
    const float point = ((float)bin + 0.5f) / (float)GEN_GLOW_BINS;
    if (point > to) break;
    if (point <= from) continue;
    int32_t slot = bin % (int32_t)GEN_GLOW_BINS;
    if (slot < 0) slot += GEN_GLOW_BINS;
    if (glow.age[slot] >= 0.0f) continue;
    float k = (fabsf(walked) > 0.000001f) ? (point - reach - newerCells) / walked : 1.0f;
    if (k < 0.0f) k = 0.0f;
    else if (k > 1.0f) k = 1.0f;
    glow.age[slot] = newerAge + k * (olderAge - newerAge);
  }
}

// The tail is an afterglow of where the core has been, so only a moving shape
// has one: a pixel that goes dark because the shape narrowed or strobed was
// never left behind. Walking the path back from now, the span the body has
// covered only ever grows, so each point is stamped once, at the moment the
// body last left it — which is what folds a tail back on itself where the core
// turns, and lets a slowing core shorten its own tail.
static void walkPath(const Path &path, int32_t newestStep, float beats, float nowCells,
                     float halfWidth, float glowBeats, Glow &glow) {
  for (uint16_t i = 0; i < GEN_GLOW_BINS; i++) glow.age[i] = -1.0f;
  glow.tailCells = 0.0f;
  glow.direction = 0.0f;

  float low = nowCells - halfWidth;
  float high = nowCells + halfWidth;
  stampReached(glow, low - 0.000001f, high, 0.0f, nowCells, 0.0f, nowCells, 0.0f);

  float newerCells = nowCells;
  float newerAge = 0.0f;
  const int32_t oldest = newestStep - (int32_t)PATH_STEPS + 1;
  for (int32_t step = newestStep; step >= path.firstStep && step >= oldest; step--) {
    if (newerAge >= glowBeats) break;
    const float olderCells = path.cells[pathSlot(step)];
    const float olderAge = beats - (float)step / (float)PATH_STEPS_PER_BEAT;
    const float walked = newerCells - olderCells;

    if (glow.direction == 0.0f && fabsf(walked) > 0.00001f) {
      glow.direction = (walked > 0.0f) ? 1.0f : -1.0f;
    }
    const float within = (olderAge <= glowBeats || olderAge <= newerAge)
        ? 1.0f : (glowBeats - newerAge) / (olderAge - newerAge);
    glow.tailCells += fabsf(walked) * within;

    if (high - low < 1.0f) {
      if (olderCells + halfWidth > high) {
        const float reached = fminf(olderCells + halfWidth, low + 1.0f);
        stampReached(glow, high, reached, halfWidth,
                     newerCells, newerAge, olderCells, olderAge);
        high = reached;
      }
      if (olderCells - halfWidth < low) {
        const float reached = fmaxf(olderCells - halfWidth, high - 1.0f);
        stampReached(glow, reached, low, -halfWidth,
                     newerCells, newerAge, olderCells, olderAge);
        low = reached;
      }
    }
    newerCells = olderCells;
    newerAge = olderAge;
  }
}

static inline float ageAt(const Glow &glow, float posCells) {
  uint16_t bin = (uint16_t)(fract(posCells) * (float)GEN_GLOW_BINS);
  if (bin >= GEN_GLOW_BINS) bin = GEN_GLOW_BINS - 1;
  return glow.age[bin];
}

// Bend sets the speed by which pixel a shape is on: a cosine peaking at `at`
// along the span and falling to its slowest at whichever end is farther. With
// the peak in the middle that is one whole cycle across the span, so minus
// is the same curve turned over. The table holds, for each point along the
// span, where the pattern would stand there unbent: the time taken to reach
// that point at this speed, as a share of the whole trip. Measuring time
// rather than distance is what keeps a trip as long as the dialed speed makes
// it — slowing down costs more time than the same speeding up saves, so the
// whole curve is lifted until they balance.
struct BendTable {
  float amount = 0.0f;
  float at = 0.0f;
  bool ready = false;
  float inverse[GEN_BEND_STEPS + 1];
  // How long one trip takes at the unlifted curve, in trips at the dial: the
  // factor the whole curve is lifted by.
  float lift;
};

static float bendSpeed(float along, float amount, float at) {
  const float reach = fmaxf(at, 1.0f - at);
  return 1.0f + amount * GEN_BEND_MAX * cosf(0.5f * TURN * fabsf(along - at) / reach);
}

static const BendTable &bendTable(float amount, float at) {
  static BendTable table;
  if (table.ready && table.amount == amount && table.at == at) return table;
  table.amount = amount;
  table.at = at;
  table.ready = true;

  const uint8_t slices = 4;
  const float step = 1.0f / (float)(GEN_BEND_STEPS * slices);
  float elapsed = 0.0f;
  table.inverse[0] = 0.0f;
  for (uint8_t i = 0; i < GEN_BEND_STEPS; i++) {
    for (uint8_t j = 0; j < slices; j++) {
      const float along = ((float)(i * slices + j) + 0.5f) * step;
      elapsed += step / bendSpeed(along, amount, at);
    }
    table.inverse[i + 1] = elapsed;
  }
  for (uint8_t i = 1; i <= GEN_BEND_STEPS; i++) table.inverse[i] /= elapsed;
  table.lift = elapsed;
  return table;
}

static float unbend(const BendTable &table, float along) {
  const float at = (along < 0.0f ? 0.0f : (along > 1.0f ? 1.0f : along)) * (float)GEN_BEND_STEPS;
  const uint8_t i = (at >= (float)GEN_BEND_STEPS) ? GEN_BEND_STEPS - 1 : (uint8_t)at;
  const float t = at - (float)i;
  return table.inverse[i] + t * (table.inverse[i + 1] - table.inverse[i]);
}

// Where a point along the strip, in pixels, would stand in cells unbent.
// Null bends nothing.
static float bentCells(const BendTable *bend, bool perCell, float pixel, float cellLength,
                       uint8_t count) {
  const float cells = pixel / cellLength;
  if (!bend) return cells;
  if (!perCell) return (float)count * unbend(*bend, pixel / (float)PIXELS);
  float cell = floorf(cells);
  if (cell < 0.0f) cell = 0.0f;
  else if (cell > (float)(count - 1)) cell = (float)(count - 1);
  return cell + unbend(*bend, cells - cell);
}

static void readBend(const BendTable *bend, bool perCell, float cellLength, float *out) {
  for (uint8_t i = 0; i < BEND_POINTS; i++) {
    if (!bend) {
      out[i] = 1.0f;
      continue;
    }
    const float along = perCell ? fract((float)i / cellLength) : (float)i / (float)PIXELS;
    out[i] = bendSpeed(along, bend->amount, bend->at) * bend->lift;
  }
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
    case CC_HUE:             return ccToByte(value, 255);
    case CC_SATURATION:
    case CC_VALUE:
    case CC_WASH_LEVEL:
    case CC_WASH_HUE_OFFSET:
    case CC_WASH_SATURATION: return ccToByte(value, 255);
    case CC_WASH_HUE_SPREAD: return ccBipolar(value) * 128.0f;
    case CC_WASH_LFO_SPREAD: return roundf(ccBipolar(value) * WASH_LFO_SPREAD_STEPS)
                                    / (2.0f * WASH_LFO_SPREAD_STEPS);
    case CC_WASH_HUE_PERIOD: return aurora_lfo_period(value);

    case CC_GEN_COUNT:
    case CC_PLACED_COUNT:
    case CC_SCATTER_COUNT:   return ccCount(value);

    case CC_GEN_BEND:        return ccBipolar(value);
    case CC_GEN_BEND_AT:     return 0.5f + 0.5f * ccBipolar(value);
    case CC_GEN_POSITION:
    case CC_GEN_FAN:
    case CC_GEN_FAN_LFO:   return ccBipolar(value) * 0.5f;
    case CC_GEN_SPEED:
      return stillBelowThreshold(squaredRate(value, GEN_MAX_SPEED_PIXELS_PER_BEAT));
    // The same squared curve Speed runs on, so that mirroring one fader about
    // its center against the other cancels *exactly*: a still strip at the
    // wave's peak needs Speed to be the fan's opposite, and two controls on
    // different curves can only ever nearly cancel.
    case CC_GEN_FAN_RATE:    return squaredRate(value, GEN_MAX_SPEED_PIXELS_PER_BEAT);
    case CC_GEN_FAN_FREQ:    return fanFreqFrom(value);
    case CC_GEN_FAN_PHASE:   return (float)value / 128.0f;
    case CC_GEN_LFO_RATE:  return aurora_lfo_period(value);

    case CC_PLACED_HUE:      return ccBipolar(value) * PLACED_MAX_HUE;
    case CC_PLACED_SPEED:    return squaredRate(value, PLACED_MAX_CELLS_PER_BEAT);
    case CC_WANDER_HUE:      return ccBipolar(value) * WANDER_MAX_HUE;
    case CC_WANDER_RATE:     return squaredUnit(value, WANDER_MAX_CYCLES_PER_BEAT);
    case CC_WANDER_SCALE:    return 0.12f * powf(180.0f, ccUnit(value));
    case CC_LIT_HUE:         return ccBipolar(value) * LIT_MAX_HUE;
    case CC_SCATTER_RATE:    return squaredUnit(value, SCATTER_MAX_CYCLES_PER_BEAT);
    case CC_SCATTER_HUE:     return ccBipolar(value) * SCATTER_MAX_HUE;

    // Drift is a displacement rather than a rate — how far, and which way, a
    // spot slides across its own cell over its life, which is why it is not
    // called speed like everything else here.
    case CC_SCATTER_DRIFT:
    case CC_SCATTER_LIGHT:   return ccBipolar(value);

    case CC_GEN_TAIL:        return squaredUnit(value, (float)PATH_BEATS);

    case CC_GEN_WIDTH:
    case CC_GEN_EDGE:
    case CC_GEN_FAN_RANDOM:
    case CC_PLACED_WIDTH:
    case CC_PLACED_EDGE:
    // Toward white or dark only. The faders live at the ends — S at 0 or full,
    // V at the top — so a push toward the end already reached did nothing, and
    // still subtracted from the sources that did. See docs/generator.md
    // § "White and dark push one way".
    case CC_PLACED_WHITE:
    case CC_PLACED_DARK:
    case CC_WANDER_WHITE:
    case CC_WANDER_DARK:
    case CC_LIT_WHITE:
    case CC_LIT_DARK:
    case CC_SCATTER_WHITE:
    case CC_SCATTER_WIDTH:
    case CC_SCATTER_EDGE:
    case CC_SCATTER_PLACE:
    case CC_SCATTER_STAGGER:
    case CC_WASH_HUE_SHUFFLE:
    case CC_WASH_LFO_SHUFFLE: return ccUnit(value);

    default:                 return (float)value;
  }
}

static void readParams(const uint8_t *dialed, const Pushes *pushes, Params &p) {
  auto at = [&](uint8_t cc) { return convert(cc, routed(dialed, pushes, cc)); };

  p.width = at(CC_GEN_WIDTH);
  p.edge = at(CC_GEN_EDGE);
  p.glowBeats = at(CC_GEN_TAIL);
  p.count = (uint8_t)at(CC_GEN_COUNT);
  p.positionCells = at(CC_GEN_POSITION);
  p.speedPixels = at(CC_GEN_SPEED);
  p.fanPosition = at(CC_GEN_FAN);
  p.fanLfo = at(CC_GEN_FAN_LFO);
  p.fanRate = at(CC_GEN_FAN_RATE);
  p.fanFreq = at(CC_GEN_FAN_FREQ);
  p.fanPhase = at(CC_GEN_FAN_PHASE);
  p.fanRandom = at(CC_GEN_FAN_RANDOM);
  p.bend = at(CC_GEN_BEND);
  p.bendAt = at(CC_GEN_BEND_AT);
  p.lfoBeats = at(CC_GEN_LFO_RATE);

  // A switch has no middle for a push to land in, so it is read as dialed.
  p.bounce = aurora_cc_is_on(dialed[CC_GEN_BOUNCE]);
  p.placed.primitive = aurora_cc_band3(dialed[CC_COLOR_PRIMITIVE]);
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
  p.scatterPlace = at(CC_SCATTER_PLACE);
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
  fan.lfo = p.fanLfo * 2.0f;
  fan.scrambled = p.fanRandom;
}

Hsv colorFrom(const uint8_t *dialed, const Pushes *pushes) {
  auto at = [&](uint8_t cc) { return (uint8_t)convert(cc, routed(dialed, pushes, cc)); };
  return { at(CC_HUE), at(CC_SATURATION), at(CC_VALUE) };
}

// ---------------------------------------------------------------------------
// The washes
//
// Four lamps known only by the order they are plugged in, since a venue
// guarantees nothing about where they stand. Each spread moves a lamp by its
// slot, 0 to 3, and each shuffle deals the slots out to the lamps in a new
// order now and then. See DESIGN.md § "Four lamps in order, not four points on
// the wall".

// How far back a cycle that did not reshuffle looks for the last one that did.
// Past it the lamps fall back into their order, which at a chance low enough
// to reach it is where they stood almost every cycle anyway.
static const uint16_t WASH_DEAL_LOOKBACK = 128;
static const uint8_t WASH_HUE_SALT = 29;
static const uint8_t WASH_LFO_SALT = 71;

// Which slot each lamp takes in the cycle `cycles` falls in. Stateless: a
// cycle's roll and its order are hashes of its number, so the brain and the
// editor deal the same hands without sharing anything.
static void dealSlots(float cycles, float chance, uint8_t salt, uint8_t *slots) {
  for (uint8_t i = 0; i < WASHES; i++) slots[i] = i;
  if (chance < 0.001f) return;

  int32_t cycle = (int32_t)floorf(cycles);
  for (uint16_t back = 0; back < WASH_DEAL_LOOKBACK; back++, cycle--) {
    const float roll = ((float)hash8((uint32_t)cycle, WASHES, salt) + 0.5f) / 256.0f;
    if (roll >= chance) continue;
    for (uint8_t i = WASHES - 1; i > 0; i--) {
      const uint8_t j = hash8((uint32_t)cycle, i, salt) % (i + 1);
      const uint8_t held = slots[i];
      slots[i] = slots[j];
      slots[j] = held;
    }
    return;
  }
}

// The washes take the strips' dialed color and never its pushes: a push
// reaches one fixture family, and CC 33-35 are the strips'. Their own take
// theirs. Color is converted at full value and the light is carried by the
// fixture's own dimmer, so the emitters stay near full scale where they have
// the most resolution.
static Rgb hueToRgb(uint8_t palette, uint8_t hue, uint8_t sat, uint8_t val) {
  return withSatVal(paletteRgb(palette, hue), sat, val);
}

static Wash washAt(const uint8_t *dialed, const Pushes *pushes, float hueFromOffset) {
  auto at = [&](uint8_t cc) { return (uint8_t)convert(cc, routed(dialed, pushes, cc)); };
  const Hsv strips = colorFrom(dialed, nullptr);
  const int32_t hue = (int32_t)strips.h + at(CC_WASH_HUE_OFFSET) + (int32_t)lroundf(hueFromOffset);
  // A scale down from the strips' saturation rather than a setting: the
  // washes are a relationship to the strips.
  const uint8_t saturation = scale8(strips.s, at(CC_WASH_SATURATION));
  return { hueToRgb(dialed[CC_PALETTE], (uint8_t)(hue & 255), saturation, 255),
           scale8(strips.v, at(CC_WASH_LEVEL)) };
}

void stillWashes(const uint8_t *dialed, Wash *out) {
  const float hueStep = convert(CC_WASH_HUE_SPREAD, dialed[CC_WASH_HUE_SPREAD]);
  for (uint8_t i = 0; i < WASHES; i++) out[i] = washAt(dialed, nullptr, hueStep * (float)i);
}

// Each lamp gathers the routes again at its own shift of the LFO, so all three
// of its targets move on one clock.
static void readWashes(const uint8_t *dialed, const Pushes &pushes, float beats, float lfo,
                       float lfoBeats, Frame &out) {
  auto at = [&](uint8_t cc) { return convert(cc, routed(dialed, &pushes, cc)); };
  const float huePeriod = convert(CC_WASH_HUE_PERIOD, dialed[CC_WASH_HUE_PERIOD]);
  uint8_t hueSlots[WASHES];
  uint8_t lfoSlots[WASHES];
  dealSlots(beats / huePeriod, at(CC_WASH_HUE_SHUFFLE), WASH_HUE_SALT, hueSlots);
  dealSlots(lfo, at(CC_WASH_LFO_SHUFFLE), WASH_LFO_SALT, lfoSlots);
  const float hueStep = at(CC_WASH_HUE_SPREAD);
  const float lfoStep = at(CC_WASH_LFO_SPREAD);

  Pushes lamp;
  for (uint8_t i = 0; i < WASHES; i++) {
    out.washLfo[i] = lfo - lfoStep * (float)lfoSlots[i];
    gatherRoutes(dialed, lfoBeats, out.washLfo[i], out.washLfo[i], lamp);
    out.washes[i] = washAt(dialed, &lamp, hueStep * (float)hueSlots[i]);
  }
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

void renderGenerator(const uint8_t *dialed, float quarterNotes, Motion &motion, Paths &paths,
                     Frame &out) {
  const float beats = quarterNotes * (float)AURORA_TICKS_PER_BEAT
                    / (float)aurora_ticks_per_gate(dialed[CC_TEMPO_DIVISION]);
  for (uint16_t i = 0; i < STRIPS * PIXELS; i++) out.pixels[i] = { 0, 0, 0 };

  // Read three times. The first has no pushes in it, which is what the LFO's
  // own rate needs, since it is a destination routes refuse. The second
  // takes the plain reading of the LFO, and everything the strip loop is
  // built on comes from it. The third is each strip's own reading, so a push
  // rolls across the wall instead of landing on all five at once.
  Params p;
  readParams(dialed, nullptr, p);
  const float lfo = anchoredLfoPhase(motion, beats, 1.0f / p.lfoBeats);
  out.lfo = lfo;

  Pushes pushes;
  gatherRoutes(dialed, p.lfoBeats, lfo, lfo, pushes);
  readParams(dialed, &pushes, p);
  readWashes(dialed, pushes, beats, lfo, p.lfoBeats, out);
  readFan(p, out.fan);

  const float cellLength = (float)PIXELS / (float)p.count;
  const float countCells = (float)p.count;

  const BendTable *bend = (fabsf(p.bend) > 0.001f) ? &bendTable(p.bend, p.bendAt) : nullptr;
  readBend(bend, p.bounce, cellLength, out.bend);

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

  const int32_t step = (int32_t)floorf(beats * (float)PATH_STEPS_PER_BEAT);
  const bool freshPaths = paths.empty || beats < paths.lastBeats
                       || beats - paths.lastBeats > GEN_PATH_GAP_BEATS;
  Glow glow;

  // Every color rate goes through the tracker for the same reason travel and
  // the LFO do: beats only grows, so a small change of rate multiplied by a
  // large beat count is a large jump. A route's swing is added per strip.
  const float wanderDialed = trackedPhase(motion.wander, beats, p.wanderCycles);
  const float placedDialed = trackedPhase(motion.placed, beats, p.placed.cellsPerBeat);
  const float scatterDialed = trackedPhase(motion.scatter, beats, p.scatterRate);

  for (uint8_t stripIndex = 0; stripIndex < STRIPS; stripIndex++) {
    const float wave = fanWave(p, stripIndex);
    const float stripOffset = p.fanPosition * wave;

    // One wave, three amounts, so where a strip stands, how fast it runs and
    // where it is in the swell are dialed apart — a wall of staggered bars
    // can strobe in unison, which one shared offset could never do. The
    // washes never take it: they have CC 31.
    const float stripLfo = lfo + p.fanLfo * wave;
    out.stripLfo[stripIndex] = stripLfo;

    Params s;
    gatherRoutes(dialed, p.lfoBeats, lfo, stripLfo, pushes);
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

    // Under bounce the position amount offsets where a strip stands in its
    // own swing, so the five turn at different moments. It cannot offset the
    // core's position instead: an image standing past the strip's end is
    // clipped away by nearestOffset, so displacing it there shortens a strip
    // rather than staggering it.
    float pathCells;
    if (bouncing) {
      pathCells = halfCore + triangleSwing(fract(travelCycles + stripOffset)) * swingSpan;
    } else {
      pathCells = centerCells + stripOffset;
    }
    const float coreCenter = fract(pathCells);

    Path &path = paths.strips[stripIndex];
    if (freshPaths) path.lift = 0.0f;
    else if (bouncing != motion.lastBouncing) {
      path.lift += roundf(path.lastCells - (pathCells + path.lift));
    }
    const float placement = (bouncing ? 0.0f : s.positionCells) + stripOffset;
    bool jumped = false;
    if (!freshPaths) {
      const float moved = placement - path.lastPlacement;
      const float wraps = bouncing ? 0.0f : roundf(moved);
      path.lift -= wraps;
      jumped = fabsf(moved - wraps) > GEN_JUMP_CELLS;
    }
    path.lastPlacement = placement;
    pathCells += path.lift;
    recordPath(path, paths, freshPaths || jumped, step, beats, pathCells);

    const float width = s.width;
    const float halfWidth = width * 0.5f;
    const float glowBeats = s.glowBeats;
    const bool glowing = glowBeats > 0.0001f;
    if (glowing) walkPath(path, step, beats, pathCells, halfWidth, glowBeats, glow);

    // Which side is behind is the side the core came from, not the side the
    // dialed speed points away from: a route swinging speed through zero
    // turns the shape round without touching the dial.
    const float stripDirection = (glowing && glow.direction != 0.0f) ? glow.direction : direction;

    // The two sides of a shape are not the same length — a tail reaches much
    // further than an edge fade — so they are normalized separately. Halfway
    // between the two tips is not the core, and a region asked to sit at the
    // middle of a shape means the core every time.
    const float edgeSpread = s.edge * (1.0f - width) * 0.5f;
    const float tailCells = glowing ? glow.tailCells : 0.0f;
    const float shapeLead = halfWidth + edgeSpread;
    const float shapeTrail = halfWidth + fmaxf(edgeSpread, tailCells);

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
        const float posCells =
            bentCells(bend, p.bounce, (float)pixelIndex + 0.5f + acrossPixel, cellLength, p.count);

        float nearest = 0.0f;
        const bool onShape =
            nearestOffset(posCells, coreCenter, stripDirection, p.bounce, countCells, nearest);
        const float level = onShape ? coreAt(nearest, width, s.edge) : 0.0f;
        const float age = glowing ? ageAt(glow, posCells) : -1.0f;
        float afterglow = 0.0f;
        if (age >= 0.0f && age < glowBeats) {
          const float left = 1.0f - age / glowBeats;
          afterglow = left * left;
        }
        accumulated += fmaxf(level, afterglow);

        // Along the glow the ruler reads how long ago the core passed, as the
        // distance it would have covered at the pace it kept, so it meets the
        // core's own measure at the core's back edge.
        float shapeU = 0.5f;
        if (afterglow > level && shapeTrail > 0.0001f) {
          shapeU = 0.5f + 0.5f * (halfWidth + (age / glowBeats) * tailCells) / shapeTrail;
        } else if (onShape) {
          const float reach = (nearest < 0.0f) ? shapeLead : shapeTrail;
          if (reach > 0.0001f) shapeU = 0.5f + 0.5f * nearest / reach;
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
          scaleVideo(hueToRgb(dialed[CC_PALETTE], tint.h, tint.s, 255), (uint8_t)((float)tint.v * brightness));
    }
  }
  motion.lastBouncing = bouncing;
  paths.lastBeats = beats;
  paths.lastStep = step;
  paths.empty = false;
}

}  // namespace render
