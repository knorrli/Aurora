#include "Aurora.h"
#include "dmx_out.h"
#include "tempo.h"

#include <math.h>

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
#define GEN_FAN_FREQ_STEPS 16
#define GEN_FAN_MAX_CYCLES_PER_STRIP 0.5f

// Which salt the random fan draws its five offsets through. Of the 256, this
// one puts the strips 0.094, 0.137, 0.200, 0.239 and 0.329 of a cycle apart
// round the circle: none close enough to read as two strips in unison, and
// no two gaps alike, which is what stops a random spread arriving as just
// another pattern.
#define GEN_FAN_HASH_SALT 118

// Half the wheel each way, matching the placed field's reach. A pulse that
// can only nudge the hue is not a destination anyone would spend a fader on.
#define GEN_PULSE_MAX_HUE 128.0f

// How long the pulse takes to walk back onto the musical grid after its rate
// has been moved, measured in its own cycles so the correction is always the
// same fraction of a swell and never a visible lurch.
#define GEN_PULSE_ANCHOR_CYCLES 2.0f

// How long a stopped pattern takes to walk home to Position, in beats. Long
// enough that bringing Speed to a stop reads as settling rather than as a
// second move of its own.
#define GEN_POSITION_SETTLE_BEATS 2.0f

// Below this a travel is a pixel a minute — slower than anything the roster
// wants and slow enough to read as a standstill that quietly drifts.
#define GEN_STILL_PIXELS_PER_BEAT 0.05f

// Each pixel averages this many samples across its own width. Point-sampling
// at the pixel center aliases once a cell is only a pixel or two across: the
// shape strobes as it moves instead of fading out. Averaging makes detail
// finer than the strip can resolve wash out smoothly, which is what it
// should do.
#define GEN_SUBSAMPLES 4

static float genWidth = 0.3f;
static uint8_t genCount = 1;
static float genEdge = 0.15f;
static float genTail = 0.0f;
static float genPositionCells = 0.0f;
static float genSpeedPixels = 0.0f;
static float genFanPosition = 0.0f;
static float genFanPulse = 0.0f;
static float genFanRate = 0.0f;
static float genFanFreq = 0.125f;
static float genFanPhase = 0.0f;
static float genFanRandom = 0.0f;
static float genPulseBeats = 4.0f;
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

// One per strip, because the fan's rate amount gives each its own speed.
// Scaling a single shared phase five ways instead would jump every strip the
// moment that amount moved, which is the teleport the tracker exists to
// prevent.
static PhaseTracker travelPhase[NUMBER_OF_STRIPS];
static PhaseTracker pulsePhase = { 0.0f, 0.0f };

// One oscillator with one rate, reaching several places at once. A
// destination says how far the pulse pushes it and what wave does the
// pushing; what it can never say is how fast, because every rate in here
// feeds a running total and a pulse aimed at one would shift position
// permanently instead of returning. See TODO.md § "Give the pulse its
// destinations".
//
// Every destination is always connected and its amount may be zero: a morph
// interpolates an amount and cannot snap a connection on.
struct PulseSend {
  float amount;
  float shape;
  float skew;
};

// Shape at 1 is a sine, which is the wave that does least on its way to
// somewhere else; skew at 0 is an even rise and fall.
static PulseSend pulseSends[PULSE_TARGET_COUNT] = {
  { 0.0f, 1.0f, 0.0f },  // PULSE_TO_LIGHT
  { 0.0f, 1.0f, 0.0f },  // PULSE_TO_WIDTH
  { 0.0f, 1.0f, 0.0f },  // PULSE_TO_HUE
  { 0.0f, 1.0f, 0.0f },  // PULSE_TO_PAR_LEVEL
  { 0.0f, 1.0f, 0.0f },  // PULSE_TO_PAR_HUE
  { 0.0f, 1.0f, 0.0f },  // PULSE_TO_PAR_SAT
};

// The peak sits at mid-cycle, so the offset that lands a peak on a bar line
// is a half-integer rather than a whole one.
static inline float nearestAnchor(float offset) {
  return roundf(offset - 0.5f) + 0.5f;
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
static float anchoredPulsePhase(float beats, float rate) {
  static float lastBeats = 0.0f;
  const float elapsed = beats - lastBeats;
  lastBeats = beats;

  // The transport restarted, and beat zero is a bar line by definition.
  if (elapsed < 0.0f) {
    pulsePhase.offset = 0.5f;
    pulsePhase.rate = rate;
    return beats * rate + 0.5f;
  }

  const float phase = trackedPhase(pulsePhase, beats, rate);
  const float drift = pulsePhase.offset - nearestAnchor(pulsePhase.offset);
  if (fabsf(drift) < 0.0001f) return phase;

  float pull = elapsed * rate / GEN_PULSE_ANCHOR_CYCLES;
  if (pull > 1.0f) pull = 1.0f;
  pulsePhase.offset -= drift * pull;
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
static float settledTravel(PhaseTracker &tracker, float beats, float elapsed,
                           float rate) {
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
// position, so flipping the switch teleported the shape.
//
// One thing it cannot preserve: bounce cannot put a core within half its own
// width of a cell wall, because that is where it turns — a shape standing
// there snaps out to the wall, by at most half its width. Every strip is
// solved for separately, so a fanned wall keeps its stagger across the flip
// rather than only the strip the wave reads zero at.
static void reanchorTravel(PhaseTracker &tracker, float beats, bool bouncing,
                           float speedPixels, float coreCells,
                           float halfCore, float swingSpan, float direction,
                           float cellLength, float positionCells) {
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
  tracker.offset = wanted - beats * rate;
}

// Skew slides the peak through the cycle, so one side of the swell collapses
// into a snap and a ramp becomes reachable. It warps the phase rather than
// the output, which leaves the cycle's length alone: moving skew changes the
// swell's shape without changing how often it lands.
static float pulseWave(float phase, float shape, float skew) {
  if (skew < -1.0f) skew = -1.0f;
  else if (skew > 1.0f) skew = 1.0f;

  const float k = 0.5f + 0.48f * skew;  // 0 and 1 would divide by zero
  const float t = fract(phase);
  const float warped = (t < k) ? (0.5f * t / k)
                               : (0.5f + 0.5f * (t - k) / (1.0f - k));
  const float lfo = 0.5f - 0.5f * cosf(2.0f * (float)PI * warped);

  // Steepening the sine toward a square is what makes a strobe reachable;
  // no amount of depth on a sine ever produces an on/off edge. The sweep is
  // linear because the visible swelling tracks softness in proportion: spread
  // geometrically over the same range, half the fader's visible travel falls
  // in its top ten steps and everything below reads as one flat square.
  const float softness = 0.02f + 0.98f * shape;
  float shaped = (lfo - 0.5f) / softness + 0.5f;
  if (shaped < 0.0f) shaped = 0.0f;
  else if (shaped > 1.0f) shaped = 1.0f;
  return shaped;
}

// How hard this destination is being pushed right now: signed, and zero at
// the bottom of the swell so the dialed value is what the wall rests at.
static float pulsePush(uint8_t target, float phase) {
  const PulseSend &send = pulseSends[target];
  if (fabsf(send.amount) < 0.001f) return 0.0f;
  return send.amount * pulseWave(phase, send.shape, send.skew);
}

// A push is a fraction of the way from the dialed value to one of its two
// limits, and its sign picks which. Nothing can clip, and a control already
// sitting at a limit simply has nowhere to go that way — which is why the
// strips' brightness is the one destination with no sign: there is nothing
// above full light, so its only direction is down.
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
static float fanWave(uint8_t stripIndex) {
  const float u = fract(genFanPhase + genFanFreq * (float)stripIndex);
  const float ordered = (u < 0.5f) ? (4.0f * u - 1.0f) : (3.0f - 4.0f * u);
  if (genFanRandom < 0.0001f) return ordered;
  const float drawn = (float)hash8(stripIndex, 0, GEN_FAN_HASH_SALT) / 255.0f * 2.0f - 1.0f;
  return ordered + (drawn - ordered) * genFanRandom;
}

// ---------------------------------------------------------------------------
// The color layer
//
// A color is hue, whiteness and darkness. Everything else is a push on those
// three, and the pushes add. Three sources push:
//
//   the placed field  something aimed — a gradient across a ruler, or regions
//                     sitting on it
//   the wander        the wall never quite the same in two places, and where
//                     it differs keeps moving
//   the light level   color read off how lit the shape branch left a pixel
//
// The layer reads the SHAPE branch's light level and never its own. Feed its
// own darkness back in and color depends on color: pull the wall down for a
// quiet verse and the hue slides with it.
//
// Designed and dialed in tools/preview.js before any of it was flashed; the
// numbers here and there are meant to stay identical.
// ---------------------------------------------------------------------------

// How dark a full push pulls a pixel, as a fraction of what it would
// otherwise be. It stops short of zero because a WS2812 has eight linear bits
// and no gamma: at the bottom one step is a third of the light, so brightness
// quantizes into lurches and pixels crossing to zero pop out entirely.
#define DARK_FLOOR 0.02f

// Half the wheel each way. Past about half, the hue fader stops meaning
// anything and the wall becomes a spectrum rather than one color with depth
// in it; the bench put usable settings at a fifth to a half of the wheel.
#define PLACED_MAX_HUE 128.0f
#define WANDER_MAX_HUE 128.0f

// A quarter wheel each way. Red through to yellow is 64 units, which is the
// whole of the cooling ramp anyone is likely to want.
#define LIT_MAX_HUE 64.0f

#define WANDER_MAX_CYCLES_PER_BEAT 0.5f
#define PLACED_MAX_CELLS_PER_BEAT 1.0f

// Two terms whose rates sit at the golden ratio, so they never come back into
// step and the wall never repeats. Deliberately not a control: dialing how
// far apart the two speeds are is operating the mechanism rather than the
// look.
#define GOLD 0.6180339887f

enum ColorRuler : uint8_t {
  RULER_WALL = 0,   // which of the five strips a pixel is on
  RULER_STRIP = 1,  // how far along its strip a pixel is
  RULER_SHAPE = 2,  // leading tip of a shape through to the end of its tail
};

// Everything one placed field is, carried together so a second field is a
// second instance rather than a second set of file statics. Both rulers at
// once — a strip painted with a gradient and shapes crossing it carrying
// their own — is the look that wants one.
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
  PhaseTracker phase;
};

static PlacedField placed = {
  false, RULER_STRIP, 0.0f, 0.0f, 0.0f, 1, 0.5f, 0.5f, 0.0f, { 0.0f, 0.0f }
};

static float wanderHueReach = 0.0f;
static float wanderWhiteReach = 0.0f;
static float wanderDarkReach = 0.0f;
static float wanderCycles = 0.0f;
static float wanderScale = 0.0f;

static float litHueReach = 0.0f;
static float litWhiteReach = 0.0f;
static float litDarkReach = 0.0f;

static PhaseTracker wanderPhase = { 0.0f, 0.0f };

static bool placedActive(const PlacedField &field) {
  return fabsf(field.hueReach) > 0.5f
      || fabsf(field.whiteReach) > 0.001f
      || fabsf(field.darkReach) > 0.001f;
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

// The base color sits at zero, so two terms that rarely reach their ends cost
// nothing: a sum huddled around the middle is the wall sitting at the color
// that was dialed. There is no floor here for a color to fall off.
static float wanderAt(uint8_t stripIndex, float along01, float t) {
  // Measured from the middle strip, not the first. Fanned from the first,
  // strip one never moves and the last does all the traveling, which reads as
  // a one-sided ramp rather than the wall opening — see docs/bench-facts.md
  // § "A field built as along-plus-across".
  const float acrossFromCenter =
      ((float)stripIndex - (float)(NUMBER_OF_STRIPS - 1) * 0.5f)
      / (float)(NUMBER_OF_STRIPS - 1);
  const float cyclesAlong = 0.12f * powf(180.0f, wanderScale);
  float cyclesAcross = cyclesAlong * 0.3f;
  if (cyclesAcross > 1.4f) cyclesAcross = 1.4f;

  const float a = sinf(2.0f * (float)PI
      * (cyclesAlong * along01 + cyclesAcross * acrossFromCenter + t));
  const float b = sinf(2.0f * (float)PI
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

// Pushes arrive summed and normalized. Darkening rides a geometric taper
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

// 0 at one end of the ruler, 1 at the other. `alongPixels` is fractional
// because the field is read several times across one pixel, and the strip
// ruler's whole numbers are pixel centers: the shape branch's pixel runs from
// `pixelIndex` to `pixelIndex + 1`, this one is centered on `pixelIndex`.
static float rulerAt(const PlacedField &field, uint8_t stripIndex,
                     float alongPixels, float shapeU) {
  if (field.ruler == RULER_WALL) {
    return (float)stripIndex / (float)(NUMBER_OF_STRIPS - 1);
  }
  if (field.ruler == RULER_SHAPE) return shapeU;
  return alongPixels / (float)(PIXELS_PER_STRIP - 1);
}

// `pulseHue` arrives already summed rather than as a fourth source, because
// the pulse pushes the layer's output: one push after the three have added,
// which leaves the color layer's own design alone.
static CHSV colorAt(CHSV base, const PlacedField &field, float placedLevel,
                     uint8_t stripIndex, uint8_t pixelIndex,
                     float profile, float wanderT, float pulseHue,
                     bool wanderOn) {
  const float along01 = (float)pixelIndex / (float)(PIXELS_PER_STRIP - 1);
  const float wander = wanderOn ? wanderAt(stripIndex, along01, wanderT) : 0.0f;

  return applyPushes(base,
      placedLevel * field.hueReach + wander * wanderHueReach + profile * litHueReach + pulseHue,
      placedLevel * field.whiteReach + wander * wanderWhiteReach + profile * litWhiteReach,
      placedLevel * field.darkReach + wander * wanderDarkReach + profile * litDarkReach);
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

void Generator(CHSV color) {
  const float beats = tempo::beats();
  const float cellLength = (float)PIXELS_PER_STRIP / (float)genCount;
  const float countCells = (float)genCount;

  // Under bounce the core swings inside its own cell, turning where its own
  // edge meets the cell's boundary the way a ball meets a wall, so nothing
  // ever crosses into a neighboring cell. Taking the rate from the cell is
  // what keeps speed an absolute distance: adding shapes shrinks the cell and
  // quickens the turn, and the core still crosses the wall at the pixels per
  // beat on the dial. At full width the swing closes to nothing, which is
  // right — a shape filling its cell has nowhere to go.
  const float halfCore = genWidth * 0.5f;
  const float swingSpan = 1.0f - genWidth;

  // Speed is what the strip the wave reads zero at travels at, and the fan's
  // rate amount is measured from there. So a wall can be turning with Speed
  // at a standstill, and bounce has to ask the five rather than the one dial.
  float stripSpeeds[NUMBER_OF_STRIPS];
  bool anyMoving = false;
  for (uint8_t i = 0; i < NUMBER_OF_STRIPS; i++) {
    const float px = genSpeedPixels + genFanRate * fanWave(i);
    stripSpeeds[i] = (fabsf(px) < GEN_STILL_PIXELS_PER_BEAT) ? 0.0f : px;
    if (stripSpeeds[i] != 0.0f) anyMoving = true;
  }
  const bool bouncing = genBounce && anyMoving;

  static bool lastBouncing = false;
  static float lastCoreCells[NUMBER_OF_STRIPS] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };

  static float lastTravelBeats = 0.0f;
  const float travelElapsed = beats - lastTravelBeats;
  lastTravelBeats = beats;

  const float pulse = anchoredPulsePhase(beats, 1.0f / genPulseBeats);

  const bool placedOn = placedActive(placed);
  const bool wanderOn = wanderActive();
  const bool pulseHueOn = fabsf(pulseSends[PULSE_TO_HUE].amount) > 0.001f;
  const bool colorFlat = !placedOn && !wanderOn && !litActive() && !pulseHueOn;

  // Both color rates go through the tracker for the same reason travel and
  // the pulse do: beats only grows, so a small change of rate multiplied by a
  // large beat count is a large jump.
  const float wanderT = trackedPhase(wanderPhase, beats, wanderCycles);
  const float placedDrift = trackedPhase(placed.phase, beats, placed.cellsPerBeat);

  for (uint8_t stripIndex = 0; stripIndex < NUMBER_OF_STRIPS; stripIndex++) {
    const float wave = fanWave(stripIndex);
    const float stripOffset = genFanPosition * wave;

    const bool mirrored = genAlternate && (stripIndex & 1);

    // One wave, three amounts, so where a strip stands, how fast it runs and
    // where it is in the swell are dialed apart — a wall of staggered bars
    // can strobe in unison, which one shared offset could never do. The
    // washes take the unfanned phase whatever these say: a PAR is one
    // position with no strip to be offset from.
    const float stripPulse = pulse + genFanPulse * wave;

    // A switch belongs to the patch, so the one moment it moves is an arrival
    // the performer caused and is watching — see DESIGN.md § "Switches belong
    // to the patch". That is the moment a jump would be most visible, so the
    // phase is solved for rather than carried across.
    const float stripSpeed = stripSpeeds[stripIndex];
    const float direction = (stripSpeed >= 0.0f) ? 1.0f : -1.0f;
    PhaseTracker &tracker = travelPhase[stripIndex];
    if (bouncing != lastBouncing) {
      reanchorTravel(tracker, beats, bouncing, stripSpeed,
                     lastCoreCells[stripIndex], halfCore, swingSpan,
                     direction, cellLength, genPositionCells);
    }

    float travelCycles = 0.0f;
    float centerCells = 0.5f;
    if (bouncing) {
      const float rate = (swingSpan > 0.0001f)
          ? fabsf(stripSpeed) / (2.0f * swingSpan * cellLength) : 0.0f;
      travelCycles = trackedPhase(tracker, beats, rate);
    } else {
      // Half a cell puts a still shape in the middle of its cell rather than
      // straddling the boundary — which at count 1 is the strip's two ends.
      // Position slides it from there.
      centerCells = 0.5f + genPositionCells
          + settledTravel(tracker, beats, travelElapsed, stripSpeed / cellLength);
    }
    lastCoreCells[stripIndex] = bouncing
        ? (halfCore + triangleSwing(fract(travelCycles)) * swingSpan)
        : fract(centerCells);

    // Nothing sits above full light, so brightness is the one destination
    // with no sign: its amount is how far the trough digs below what the
    // shape branch already lit.
    const PulseSend &toLight = pulseSends[PULSE_TO_LIGHT];
    const float swell = 1.0f - toLight.amount
        * (1.0f - pulseWave(stripPulse, toLight.shape, toLight.skew));

    // A shape is anchored by its center, so growing it is a breath outward
    // rather than a wipe in from one end — which is what put this destination
    // out of reach the first time it was tried.
    const float width = pushToward(genWidth, pulsePush(PULSE_TO_WIDTH, stripPulse),
                                   0.0f, 1.0f);
    const float pulseHue = pulsePush(PULSE_TO_HUE, stripPulse) * GEN_PULSE_MAX_HUE;

    // The two sides of a shape are not the same length — a tail reaches much
    // further than an edge fade — so they are normalized separately. Halfway
    // between the two tips is not the core, and a region asked to sit at the
    // middle of a shape means the core every time.
    const float shapeGap = 1.0f - width;
    const float shapeLead = width * 0.5f + genEdge * shapeGap * 0.5f;
    const float shapeTrail = width * 0.5f
        + fmaxf(genEdge * shapeGap * 0.5f, genTail * shapeGap);

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

    for (uint8_t pixelIndex = 0; pixelIndex < PIXELS_PER_STRIP; pixelIndex++) {
      // The placed field is read at the same samples the shape is, and for
      // the same reason: read once at the pixel's center it aliases as soon
      // as its regions get down to a pixel or two across, which is the fault
      // docs/bench-facts.md § "Point-sampling a pattern aliases" records
      // against the shape branch. The wander needs none of this — it is
      // sines, and smooth by construction — and the light level reads the
      // averaged profile already.
      float accumulated = 0.0f;
      float placedAccumulated = 0.0f;
      for (uint8_t sampleIndex = 0; sampleIndex < GEN_SUBSAMPLES; sampleIndex++) {
        const float acrossPixel =
            ((float)sampleIndex + 0.5f) / (float)GEN_SUBSAMPLES - 0.5f;
        const float posCells =
            ((float)pixelIndex + 0.5f + acrossPixel) / cellLength;
        float shapeU = 0.5f;

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
          const bool onShape =
              nearestOffset(posCells, coreCenter, stripDirection, true, countCells, nearest);
          if (onShape) level = coreAt(nearest, width, genEdge);
          const float behind = trailBehind(journey, triangle, halfCore, swingSpan);
          const float trailing = tailAt(behind, width, genTail);
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
          if (nearestOffset(posCells, coreCenter, stripDirection, genBounce, countCells, nearest)) {
            accumulated += shapeAt(nearest, width, genEdge, genTail);
            const float reach = (nearest < 0.0f) ? shapeLead : shapeTrail;
            if (reach > 0.0001f) shapeU = 0.5f + 0.5f * nearest / reach;
          }
        }

        if (placedOn) {
          if (shapeU < 0.0f) shapeU = 0.0f;
          else if (shapeU > 1.0f) shapeU = 1.0f;
          const float u = rulerAt(placed, stripIndex,
                                  (float)pixelIndex + acrossPixel, shapeU);
          placedAccumulated += placedAt(placed, u, placedDrift);
        }
      }

      const float profile = accumulated / (float)GEN_SUBSAMPLES;
      const float brightness = profile * swell;
      if (brightness <= 0.002f) continue;

      // Scaling the RGB rather than handing a low value to CHSV keeps the hue
      // where it was set: converting at a low value lets a channel truncate to
      // zero before its neighbor, which is what turns a dim yellow red.
      CHSV tint = color;
      if (!colorFlat) {
        tint = colorAt(color, placed, placedAccumulated / (float)GEN_SUBSAMPLES,
                        stripIndex, pixelIndex, profile, wanderT, pulseHue, wanderOn);
      }
      CRGB lit = CHSV(tint.hue, tint.saturation, 255);
      strip[stripIndex][pixelIndex] =
          lit.nscale8_video((uint8_t)((float)tint.value * brightness));
    }
  }
  lastBouncing = bouncing;

  // The washes are not this frame's pixels, so the push is handed over
  // rather than applied here: dmx_out::tick() runs after every preset and
  // clears what it was given, which is what keeps a pulse dialed in here off
  // the washes while a hand-written preset is up.
  dmx_out::setPulsePush(pulsePush(PULSE_TO_PAR_LEVEL, pulse),
                        pulsePush(PULSE_TO_PAR_HUE, pulse) * GEN_PULSE_MAX_HUE,
                        pulsePush(PULSE_TO_PAR_SAT, pulse));
}

void setGeneratorWidth(uint8_t value) { genWidth = ccUnit(value); }
void setGeneratorEdge(uint8_t value) { genEdge = ccUnit(value); }
void setGeneratorTail(uint8_t value) { genTail = ccUnit(value); }
// The three amounts share one wave. Position and pulse are offsets into a
// cycle, so only the spread between strips is visible and a full amount
// spreads the five over exactly one cell or one swell. Rate is an absolute
// speed added to Speed's, so the strip the wave reads zero at travels at
// exactly what Speed says and the others are measured from it.
void setGeneratorFan(uint8_t value) { genFanPosition = ccBipolar(value) * 0.5f; }
void setGeneratorFanPulse(uint8_t value) { genFanPulse = ccBipolar(value) * 0.5f; }
// The same squared curve Speed runs on, so that mirroring one fader about its
// center against the other cancels *exactly*: a still strip at the wave's peak
// needs Speed to be the fan's opposite, and two controls on different curves
// can only ever nearly cancel.
void setGeneratorFanRate(uint8_t value) {
  const float x = ((float)value - 64.0f) / 63.0f;
  genFanRate = (x < 0.0f ? -1.0f : 1.0f) * x * x * GEN_MAX_SPEED_PIXELS_PER_BEAT;
}
void setGeneratorFanFreq(uint8_t value) {
  const long step = lroundf((float)value * GEN_FAN_FREQ_STEPS / 127.0f);
  genFanFreq = (float)step * (GEN_FAN_MAX_CYCLES_PER_STRIP / GEN_FAN_FREQ_STEPS);
}
void setGeneratorFanPhase(uint8_t value) { genFanPhase = (float)value / 128.0f; }
void setGeneratorFanRandom(uint8_t value) { genFanRandom = ccUnit(value); }

void setGeneratorCount(uint8_t value) { genCount = ccCount(value); }

// Half a cell each way covers every place a shape can stand, because the
// pattern repeats once per cell: a full cell of offset lands back where it
// started. Only read while the pattern is still — a traveling one is already
// everywhere in its cell.
void setGeneratorPosition(uint8_t value) { genPositionCells = ccBipolar(value) * 0.5f; }

// Bipolar around 64, squared so the slow end — where every pattern in the
// roster actually lives — gets most of the travel.
//
// One step either side of center is a crawl of a pixel a minute, which is not
// a speed anyone dials: it is a pattern that will not sit where Position puts
// it, since the settle runs only at a standstill. Snapping it to nothing
// costs the two steps that already read as still and makes still mean still.
void setGeneratorSpeed(uint8_t value) {
  const float x = ((float)value - 64.0f) / 63.0f;
  const float pixels = (x < 0.0f ? -1.0f : 1.0f) * x * x * GEN_MAX_SPEED_PIXELS_PER_BEAT;
  genSpeedPixels = (fabsf(pixels) < GEN_STILL_PIXELS_PER_BEAT) ? 0.0f : pixels;
}

// Stepped, not continuous: the phase is anchored to the musical grid, and a
// period the bar cannot hold a whole number of walks through the bar for ever
// whatever the phase is anchored to. See AURORA_PULSE_PERIODS.
void setGeneratorPulseRate(uint8_t value) {
  genPulseBeats = aurora_pulse_period(value);
}

void setGeneratorAlternate(uint8_t value) { genAlternate = aurora_cc_is_on(value); }
void setGeneratorBounce(uint8_t value)    { genBounce = aurora_cc_is_on(value); }


// Brightness has no room above full, so its amount is unipolar and its only
// direction is down. Every other destination has two sides and the sign of
// the amount picks one.
void setPulseAmount(uint8_t target, uint8_t value) {
  pulseSends[target].amount = (target == PULSE_TO_LIGHT) ? ccUnit(value)
                                                         : ccBipolar(value);
}

void setPulseShape(uint8_t target, uint8_t value) {
  pulseSends[target].shape = ccUnit(value);
}

void setPulseSkew(uint8_t target, uint8_t value) {
  pulseSends[target].skew = ccBipolar(value);
}

void setColorRegion(uint8_t value) { placed.isRegion = aurora_cc_is_on(value); }

void setColorRuler(uint8_t value) {
  const uint8_t ruler = aurora_cc_band3(value);
  placed.ruler = (ruler > RULER_SHAPE) ? RULER_SHAPE : ruler;
}

void setPlacedHue(uint8_t value)   { placed.hueReach = ccBipolar(value) * PLACED_MAX_HUE; }
void setPlacedWhite(uint8_t value) { placed.whiteReach = ccBipolar(value); }
void setPlacedDark(uint8_t value)  { placed.darkReach = ccBipolar(value); }
void setPlacedWidth(uint8_t value) { placed.width = ccUnit(value); }
void setPlacedEdge(uint8_t value)  { placed.edge = ccUnit(value); }

void setPlacedCount(uint8_t value) { placed.count = ccCount(value); }

// Bipolar and squared like the shape branch's travel, for the same reason:
// the slow end is where a color that reads as depth rather than as an effect
// actually lives.
void setPlacedSpeed(uint8_t value) {
  const float x = ((float)value - 64.0f) / 63.0f;
  placed.cellsPerBeat = (x < 0.0f ? -1.0f : 1.0f) * x * x * PLACED_MAX_CELLS_PER_BEAT;
}

void setWanderHue(uint8_t value)   { wanderHueReach = ccBipolar(value) * WANDER_MAX_HUE; }
void setWanderWhite(uint8_t value) { wanderWhiteReach = ccBipolar(value); }
void setWanderDark(uint8_t value)  { wanderDarkReach = ccBipolar(value); }
// Squared like the two travel speeds, for the same reason: the slow end is
// where a color reading as depth rather than as an effect lives. Unipolar
// unlike them, because the wander is symmetric interference with no anchor
// and reversing it gives the same look — measured in docs/generator.md
// § Open, item 11. Frozen therefore stays at the end of the throw, rather
// than at a center this taper makes hard to tell from a crawl.
void setWanderRate(uint8_t value) {
  const float x = ccUnit(value);
  wanderCycles = x * x * WANDER_MAX_CYCLES_PER_BEAT;
}
void setWanderScale(uint8_t value) { wanderScale = ccUnit(value); }

void setLitHue(uint8_t value)   { litHueReach = ccBipolar(value) * LIT_MAX_HUE; }
void setLitWhite(uint8_t value) { litWhiteReach = ccUnit(value); }
void setLitDark(uint8_t value)  { litDarkReach = ccBipolar(value); }
