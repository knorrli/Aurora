// Aurora wall preview — a port of brain/src/P_Generator.cpp and
// brain/src/dmx_out.cpp, so a setting can be judged without the rig.
//
// The shape branch is the firmware's. The color layer is not yet in the
// firmware at all — it is designed here first, and ported once it survives.
//
// Two deliberate divergences, both stated:
//
//   - sin8 is a float sine rather than FastLED's piecewise approximation.
//     They agree at the anchors that matter (0 -> 128, 64 -> 255,
//     192 -> 1) and never differ by more than about two of 255.
//   - A screen is not a WS2812. Desaturation, the dark floor and red's
//     resolution are the things docs/bench-facts.md caught a screen
//     getting wrong; settle those on the wall.
//
// Which physical end pixel 0 sits at, and the left-to-right order of the
// five strips, are rigging facts the firmware never states. PC 11 settles
// the order: it paints each strip one flat color, and that order is
// recorded in WALL_STRIP_ORDER below. It cannot settle which end pixel 0
// is — a flat color has no end to tell apart — so that stays a control.

(function (global) {
  'use strict';

  const STRIPS = 5;
  const PIXELS = 45;
  const SUBSAMPLES = 4;

  const GEN_MAX_COUNT = 20;
  const GEN_MAX_SPEED_PIXELS_PER_BEAT = 60;

  // Half the wheel each way, matching the placed field's reach.
  const GEN_PULSE_MAX_HUE = 128;

  // How long the pulse takes to walk back onto the musical grid after its
  // rate has been moved, in its own cycles.
  const GEN_PULSE_ANCHOR_CYCLES = 2;

  // How long a stopped pattern takes to walk home to Position, in beats.
  const GEN_POSITION_SETTLE_BEATS = 2;

  // Below this a travel is a pixel a minute: slower than anything the roster
  // wants, and a standstill that quietly drifts out from under Position.
  const GEN_STILL_PIXELS_PER_BEAT = 0.05;

  // AURORA_PULSE_PERIODS in shared/aurora_protocol.h. Stepped rather than
  // continuous: the phase is anchored to the musical grid, and only a period
  // a bar holds a whole number of stays there. Halves and their dotted
  // values, in animation beats.
  const PULSE_PERIODS = [16, 12, 8, 6, 4, 3, 2, 1.5, 1, 0.75, 0.5, 0.375, 0.25];
  const pulsePeriod = v => PULSE_PERIODS[
    Math.min(PULSE_PERIODS.length - 1,
             Math.floor((v * (PULSE_PERIODS.length - 1) + 63) / 127))];

  // How dark a full push pulls a pixel, as a fraction of what it would
  // otherwise be. It stops short of zero because a WS2812 has eight linear
  // bits and no gamma: at the bottom one step is a third of the light, so
  // brightness quantizes into lurches and pixels crossing to zero pop out.
  const DARK_FLOOR = 0.02;

  const LIT_MAX_HUE = 64;
  const WANDER_MAX_CYCLES_PER_BEAT = 0.5;
  const PLACED_MAX_CELLS_PER_BEAT = 1;

  // One switch per CC — see shared/aurora_protocol.h. Off below 64, on from
  // 64 up; the ruler is banded into thirds.
  const isOn = v => v >= 64;
  const band3 = v => (v < 43 ? 0 : v < 86 ? 1 : 2);

  // PRESET_STRIP_ORDER in shared/aurora_protocol.h.
  const PRESET_STRIP_ORDER = 11;

  // Data-chain strip numbers, left to right across the room. The chain runs
  // the opposite way to the wall, so strip 5 stands at the left-hand end —
  // read off PC 11, 2026-09-22. See docs/wiring.md § "Where they stand on
  // the wall", which is also where fan's zero end is worked out.
  const WALL_STRIP_ORDER = [5, 4, 3, 2, 1];

  // ---- FastLED byte maths, from brain/.pio/libdeps/.../lib8tion ----------
  // FASTLED_SCALE8_FIXED is 1 in fastled_config.h, which is what puts the
  // (1 + scale) in scale8 and takes the trailing +1 out of the callers.

  const clamp8 = v => (v < 0 ? 0 : v > 255 ? 255 : v | 0);
  const scale8 = (i, s) => ((i * (1 + s)) >> 8) & 255;
  const scale8v = (i, s) => (((i * s) >> 8) + (i && s ? 1 : 0)) & 255;
  const sin8 = t => clamp8(Math.round(128 + 127 * Math.sin(2 * Math.PI * (((t % 256) + 256) % 256) / 256)));

  // hsv2rgb_rainbow, with Y1 on and G2/Gscale off as FastLED ships them.
  // Not interchangeable with a textbook HSV conversion: this one widens and
  // brightens yellow, and every hue the wall has ever been dialed to was
  // chosen through it.
  function hsv2rgb(hue, sat, val) {
    hue &= 255; sat &= 255; val &= 255;

    const offset = hue & 0x1f;
    const offset8 = (offset << 3) & 255;
    const third = scale8(offset8, 85);
    const twothirds = scale8(offset8, 170);

    let r, g, b;
    if (!(hue & 0x80)) {
      if (!(hue & 0x40)) {
        if (!(hue & 0x20)) { r = 255 - third; g = third; b = 0; }
        else { r = 171; g = 85 + third; b = 0; }
      } else {
        if (!(hue & 0x20)) { r = 171 - twothirds; g = 170 + third; b = 0; }
        else { r = 0; g = 255 - third; b = third; }
      }
    } else {
      if (!(hue & 0x40)) {
        if (!(hue & 0x20)) { r = 0; g = 171 - twothirds; b = 85 + twothirds; }
        else { r = third; g = 0; b = 255 - third; }
      } else {
        if (!(hue & 0x20)) { r = 85 + third; g = 0; b = 171 - third; }
        else { r = 170 + third; g = 0; b = 85 - third; }
      }
    }

    if (sat !== 255) {
      if (sat === 0) { r = 255; g = 255; b = 255; }
      else {
        const desat = scale8v(255 - sat, 255 - sat);
        const satscale = 255 - desat;
        r = clamp8(scale8(r, satscale) + desat);
        g = clamp8(scale8(g, satscale) + desat);
        b = clamp8(scale8(b, satscale) + desat);
      }
    }

    if (val !== 255) {
      const v = scale8v(val, val);
      if (v === 0) { r = 0; g = 0; b = 0; }
      else { r = scale8(r, v); g = scale8(g, v); b = scale8(b, v); }
    }

    return [r, g, b];
  }

  // ---- parameter mapping, mirroring the setters at the foot of
  // P_Generator.cpp and the map() calls in midi_in.cpp -------------------

  const ccUnit = v => v / 127;
  const ccBipolar = v => (v < 64 ? (v - 64) / 64 : (v - 64) / 63);
  const stillBelowThreshold = px => Math.abs(px) < GEN_STILL_PIXELS_PER_BEAT ? 0 : px;
  const ccSquared = (v, max) => { const x = (v - 64) / 63; return Math.sign(x) * x * x * max; };
  const ccMap = (v, hi) => Math.floor(v * hi / 127);
  const ccCount = v => Math.min(GEN_MAX_COUNT, Math.max(1, Math.round(Math.pow(GEN_MAX_COUNT, v / 127))));

  function send(amount, shape, skew, unipolar) {
    return {
      amount: unipolar ? ccUnit(amount) : ccBipolar(amount),
      shape: ccUnit(shape),
      skew: ccBipolar(skew),
    };
  }

  function readParams(s) {
    return {
      width: ccUnit(s.width),
      count: ccCount(s.count),
      edge: ccUnit(s.edge),
      tail: ccUnit(s.tail),
      positionCells: ccBipolar(s.position) * 0.5,
      speedPixels: stillBelowThreshold(ccSquared(s.speed, GEN_MAX_SPEED_PIXELS_PER_BEAT)),
      fan: ccUnit(s.fan),
      jitter: ccUnit(s.jitter),
      pulseBeats: pulsePeriod(s.pulseRate),

      // One oscillator with one rate reaching six places, each with its own
      // amount and its own wave — which is what lets the washes breathe
      // while the strips strobe. Brightness is the only unipolar amount:
      // nothing sits above full light, so its only direction is down.
      sends: {
        light:    send(s.pulseDepth, s.pulseShape, s.pulseSkew, true),
        width:    send(s.pulseWidth, s.pulseWidthShape, s.pulseWidthSkew),
        hue:      send(s.pulseHue, s.pulseHueShape, s.pulseHueSkew),
        parLevel: send(s.pulseParLevel, s.pulseParLevelShape, s.pulseParLevelSkew),
        parHue:   send(s.pulseParHue, s.pulseParHueShape, s.pulseParHueSkew),
        parSat:   send(s.pulseParSat, s.pulseParSatShape, s.pulseParSatSkew),
      },

      alternate: isOn(s.alternate),
      bounce: isOn(s.bounce),


      litWhiteReach: ccUnit(s.litWhite),
      litHueReach: ccBipolar(s.litHue) * LIT_MAX_HUE,
      litDarkReach: ccBipolar(s.litDark),

      placedKind: isOn(s.colorRegion) ? KIND_REGION : KIND_GRADIENT,
      placedRuler: Math.min(RULER_SHAPE, band3(s.colorRuler)),
      placedHue: ccBipolar(s.placedHue) * PLACED_MAX_HUE,
      placedWhite: ccBipolar(s.placedWhite),
      placedDark: ccBipolar(s.placedDark),
      placedCount: ccCount(s.placedCount),
      placedWidth: ccUnit(s.placedWidth),
      placedEdge: ccUnit(s.placedEdge),
      placedSpeed: ccSquared(s.placedSpeed, PLACED_MAX_CELLS_PER_BEAT),

      wanderHue: ccBipolar(s.wanderHue) * WANDER_MAX_HUE,
      wanderWhite: ccBipolar(s.wanderWhite),
      wanderDark: ccBipolar(s.wanderDark),
      wanderRate: ccUnit(s.wanderRate) * WANDER_MAX_CYCLES_PER_BEAT,
      wanderScale: ccUnit(s.wanderScale),


      baseHue: ccMap(s.hue, 250),
      baseSat: ccMap(s.saturation, 255),
      baseVal: ccMap(s.value, 255),

      washLevel: ccMap(s.washLevel, 255),
      washHueOffset: ccMap(s.washHueOffset, 255),
    };
  }

  // ---- the shape branch's own maths -------------------------------------

  const fract = x => x - Math.floor(x);

  function hash8(a, b, c) {
    let h = (Math.imul(a, 73856093) ^ Math.imul(b, 19349663) ^ Math.imul(c, 83492791)) >>> 0;
    h ^= h >>> 13;
    h = Math.imul(h, 0x5bd1e995) >>> 0;
    h ^= h >>> 15;
    return h & 255;
  }

  // The shape repeats once per cell, so the only images that can reach a
  // sample are the two standing either side of it. Under bounce the strip is
  // a line and an image off its end is not there to be seen.
  function nearestOffset(posCells, coreCenter, stripDirection, bounce, countCells) {
    const firstImage = coreCenter + Math.floor(posCells - coreCenter);
    let nearest = 0;
    let lit = false;
    for (let image = 0; image < 2; image++) {
      const imagePos = firstImage + image;
      if (bounce && (imagePos < 0 || imagePos > countCells)) continue;
      const offset = -stripDirection * (posCells - imagePos);
      if (!lit || Math.abs(offset) < Math.abs(nearest)) { nearest = offset; lit = true; }
    }
    return lit ? nearest : null;
  }

  // The core and its edge fade are geometry: they sit around the core wherever
  // it stands, the same on both sides.
  function coreAt(offset, width, edge) {
    const halfCore = width * 0.5;
    const distance = Math.abs(offset);
    if (distance <= halfCore) return 1;

    const spread = edge * (1 - width) * 0.5;
    const beyond = distance - halfCore;
    if (spread > 0.0001 && beyond < spread) {
      const k = 1 - beyond / spread;
      return k * k * (3 - 2 * k);
    }
    return 0;
  }

  // The tail is not geometry. It is how far the core has traveled since it
  // was last at this point, so `behind` is a path length, never a straight
  // line.
  function tailAt(behind, width, tail) {
    if (tail <= 0.0001) return 0;
    const beyond = behind - width * 0.5;
    if (beyond <= 0) return 0;

    const tailLength = tail * (1 - width);
    if (tailLength <= 0.0001 || beyond >= tailLength) return 0;
    const k = 1 - beyond / tailLength;
    return k * k;
  }

  // While travel runs one way, how far behind the core a point lies and how
  // long ago the core was there are the same number, which is why a straight
  // offset serves for both. They come apart only where the core turns.
  function shapeAt(offset, width, edge, tail) {
    let brightness = coreAt(offset, width, edge);
    if (offset > 0) {
      const trailing = tailAt(offset, width, tail);
      if (trailing > brightness) brightness = trailing;
    }
    return brightness;
  }

  // Every cell runs the same journey, and an odd strip runs it backwards, so a
  // position on the strip becomes a position in that journey before the trail
  // can be measured against it.
  function journeyIn(posCells, mirrored) {
    const withinCell = posCells - Math.floor(posCells);
    return mirrored ? 1 - withinCell : withinCell;
  }

  // Under bounce the core's position is a triangle, so "when was the core last
  // here" has an answer in closed form: every point on the swing is crossed
  // exactly twice a cycle, going up and coming down, and the more recent of
  // the two is the one whose trail is still lying there. Distance is the path
  // the core walked in that time, which is what folds the trail back on itself
  // at a turn instead of moving it.
  //
  // Points inside half a core width of the cell's ends are never reached by
  // the center, only swept by the body at the turn, so they measure from the
  // turn and add the straight remainder.
  function trailBehind(journey, phase, halfCore, swingSpan) {
    if (swingSpan <= 0.0001) return Math.abs(journey - 0.5);

    const far = 1 - halfCore;
    const reachable = journey < halfCore ? halfCore : (journey > far ? far : journey);
    const rising = 0.5 * ((reachable - halfCore) / swingSpan);
    const up = fract(phase - rising);
    const down = fract(phase - (1 - rising));
    const elapsed = up < down ? up : down;
    return elapsed * 2 * swingSpan + Math.abs(journey - reachable);
  }

  // A phase derived as beats * rate teleports when the rate changes. Carrying
  // the offset keeps the phase where it was and alters only how fast it
  // advances, which is what makes a rate reachable with a fader.
  function makeTracker() { return { offset: 0, rate: 0 }; }
  function trackedPhase(tracker, beats, rate) {
    if (rate !== tracker.rate) {
      tracker.offset += beats * (tracker.rate - rate);
      tracker.rate = rate;
    }
    return beats * rate + tracker.offset;
  }

  // Where the page's beat zero sits. Reset when the panel starts the clock,
  // so the preview's bar lines and the brain's are the same bar lines — a
  // MIDI Start puts the brain's position back to zero at the same moment.
  // Without that the two agree on the period and not on the landing, which
  // is the whole of what anchoring is about.
  let startedAt = 0;
  function restart() { startedAt = global.performance.now(); }

  const travelPhase = makeTracker();
  const pulsePhase = makeTracker();
  const wanderPhase = makeTracker();
  const placedPhase = makeTracker();

  // The offset that stops a rate change teleporting is also what leaves the
  // cycle's zero wherever the rate was last touched — never a bar line, so a
  // deep slow swell peaks wherever it happens to. A whole cycle of offset is
  // invisible, so only the fraction has to go: easing it out over the next
  // couple of cycles walks the pulse back onto the grid without ever
  // jumping. The peak sits at mid-cycle, so the offset that lands one on a
  // bar line is a half-integer rather than a whole one.
  let lastPulseBeats = 0;
  function anchoredPulsePhase(beats, rate) {
    const elapsed = beats - lastPulseBeats;
    lastPulseBeats = beats;

    // The transport restarted, and beat zero is a bar line by definition.
    if (elapsed < 0) {
      pulsePhase.offset = 0.5;
      pulsePhase.rate = rate;
      return beats * rate + 0.5;
    }

    const phase = trackedPhase(pulsePhase, beats, rate);
    const drift = pulsePhase.offset - (Math.round(pulsePhase.offset - 0.5) + 0.5);
    if (Math.abs(drift) < 0.0001) return phase;

    const pull = Math.min(1, elapsed * rate / GEN_PULSE_ANCHOR_CYCLES);
    pulsePhase.offset -= drift * pull;
    return phase - drift * pull;
  }

  // The offset that stops a speed change teleporting the pattern is also what
  // leaves a stopped one standing wherever the last traveling pattern ran out,
  // so a patch saved still comes back somewhere else every time. A whole cell
  // of offset is invisible, since the shape repeats once per cell, so only the
  // fraction has to go and home is never further than half a cell away.
  // While travel is running the offset is where the pattern stands, so there
  // is nothing to settle and this leaves it alone.
  let lastTravelBeats = 0;
  function settledTravel(beats, rate) {
    const elapsed = beats - lastTravelBeats;
    lastTravelBeats = beats;

    const travel = trackedPhase(travelPhase, beats, rate);
    if (Math.abs(rate) > 0.0001 || elapsed <= 0) return travel;

    const drift = travelPhase.offset - Math.round(travelPhase.offset);
    if (Math.abs(drift) < 0.0001) return travel;

    const pull = Math.min(1, elapsed / GEN_POSITION_SETTLE_BEATS);
    travelPhase.offset -= drift * pull;
    return travel - drift * pull;
  }

  // Skew slides the peak through the cycle, so one side of the swell
  // collapses into a snap and a ramp becomes reachable. It warps the phase
  // rather than the output, which leaves the cycle's length untouched: moving
  // skew changes the swell's shape without changing how often it lands.
  //
  // Bipolar, so center is an exactly even rise and fall. Anything near but not
  // on 0.5 phase-shifts a square edge instead of leaving it alone, which reads
  // as the strobe sitting late rather than as a control doing nothing.
  function pulseWave(phase, shape, skew) {
    const k = 0.5 + 0.48 * Math.min(1, Math.max(-1, skew));  // 0 and 1 divide by zero
    const t = fract(phase);
    const warped = t < k ? 0.5 * t / k : 0.5 + 0.5 * (t - k) / (1 - k);
    const lfo = 0.5 - 0.5 * Math.cos(2 * Math.PI * warped);

    // Steepening the sine toward a square is what makes a strobe reachable;
    // no amount of depth on a sine ever produces an on/off edge. The sweep is
    // linear because the visible swelling tracks softness in proportion: spread
    // geometrically over the same range, half the fader's visible travel falls
    // in its top ten steps and everything below reads as one flat square.
    const softness = 0.02 + 0.98 * shape;
    const shaped = (lfo - 0.5) / softness + 0.5;
    return shaped < 0 ? 0 : shaped > 1 ? 1 : shaped;
  }

  // How hard a destination is being pushed right now: signed, and zero at the
  // bottom of the swell so the dialed value is what the wall rests at.
  function pulsePush(send, phase) {
    if (Math.abs(send.amount) < 0.001) return 0;
    return send.amount * pulseWave(phase, send.shape, send.skew);
  }

  // A push is a fraction of the way from the dialed value to one of its two
  // limits, and its sign picks which. Nothing can clip, and a control already
  // at a limit has nowhere to go that way — which is why brightness is the
  // one destination with no sign: there is nothing above full light.
  function pushToward(base, push, low, high) {
    return base + Math.abs(push) * ((push >= 0 ? high : low) - base);
  }

  // ---- the color layer, redesigned 2026-09-21 --------------------------
  //
  // A color is hue, whiteness and darkness. Everything else is a push on
  // those three, and the pushes add. Three sources push:
  //
  //   the placed field  something you aim — a gradient across a ruler, or
  //                     regions sitting on it
  //   the wander        the wall never quite the same in two places, and
  //                     where it differs keeps moving
  //   the light level   color read off how lit the shape left a pixel
  //
  // The layer reads the SHAPE branch's light level and never its own. Feed
  // its own darkness back in and color depends on color: pull the wall
  // down for a quiet verse and the hue slides with it.

  const RULER_WALL = 0, RULER_STRIP = 1, RULER_SHAPE = 2;
  const KIND_GRADIENT = 0, KIND_REGION = 1;

  const PLACED_MAX_HUE = 128;
  const WANDER_MAX_HUE = 128;

  // Two terms whose rates sit at the golden ratio, so they never come back
  // into step and the wall never repeats. This is not a control: dialing
  // "how far apart the two speeds are" is operating the mechanism.
  const GOLD = 0.6180339887;

  // The base color sits at zero, so two terms that rarely reach their ends
  // cost nothing: a sum huddled around the middle is the wall sitting at the
  // color that was dialed. There is no floor here for a color to fall off.
  function wanderAt(p, along01, stripIndex, t) {
    if (!p.wanderActive) return 0;
    // Measured from the middle strip, not the first. Fanned from the first,
    // strip one never moves and the last does all the traveling, which reads
    // as a one-sided ramp rather than the wall opening — see
    // docs/bench-facts.md § "A field built as along-plus-across".
    const acrossFromCenter = (stripIndex - (STRIPS - 1) * 0.5) / (STRIPS - 1);
    const cyclesAlong = 0.12 * Math.pow(180, p.wanderScale);
    const cyclesAcross = Math.min(1.4, cyclesAlong * 0.3);
    const a = Math.sin(2 * Math.PI * (cyclesAlong * along01 + cyclesAcross * acrossFromCenter + t));
    const b = Math.sin(2 * Math.PI * (cyclesAlong * GOLD * along01 - cyclesAcross * 1.37 * acrossFromCenter + t * GOLD));
    return (a + b) * 0.5;
  }

  // 0 at one end of the ruler, 1 at the other. The shape ruler runs from the
  // leading tip through to the end of the tail, so a gradient on it puts one
  // color at the head and the other behind.
  function rulerAt(p, stripIndex, pixelIndex, shapeU) {
    if (p.placedRuler === RULER_WALL) return STRIPS > 1 ? stripIndex / (STRIPS - 1) : 0.5;
    if (p.placedRuler === RULER_SHAPE) return shapeU;
    return PIXELS > 1 ? pixelIndex / (PIXELS - 1) : 0.5;
  }

  // A gradient is monotone with the base color at the ruler's center, so the
  // amount is how far ONE end departs and the two ends land twice that apart.
  // A region is a bump: base, departure, back to base — the shape branch's
  // own core-and-fades, which is what makes count, width and edge mean here
  // what they mean there.
  function placedAt(p, u, drift) {
    if (!p.placedActive) return 0;
    if (p.placedKind === KIND_GRADIENT) return (u - 0.5) * 2;
    const cell = u * p.placedCount + drift;
    const offset = fract(cell) - 0.5;
    return shapeAt(offset, p.placedWidth, p.placedEdge, 0);
  }

  // Pushes arrive summed and normalized. Darkening rides a geometric taper
  // because it is a ratio of light and the eye reads it as one; mapped
  // linearly, nearly the whole travel was imperceptible and everything worth
  // having sat in the last few steps. Brightening is a plain ride to full and
  // only has room when the brightness fader is left below the top. Both
  // measured on the wall — see docs/bench-facts.md.
  function applyPushes(base, hue, white, dark) {
    white = white < -1 ? -1 : white > 1 ? 1 : white;
    dark = dark < -1 ? -1 : dark > 1 ? 1 : dark;

    const satTarget = white >= 0 ? 0 : 255;
    const saturation = base.s + Math.abs(white) * (satTarget - base.s);

    let value;
    if (dark >= 0) value = base.v + dark * (255 - base.v);
    else value = base.v * Math.pow(DARK_FLOOR, -dark);

    return { h: (base.h + Math.trunc(hue)) & 255, s: saturation, v: value };
  }

  // `pulseHue` arrives already summed rather than as a fourth source: the
  // pulse pushes the layer's output, one push after the three have added,
  // which leaves the color layer's own design alone.
  function colorAt(p, base, stripIndex, pixelIndex, shapeU, profile, drift, wanderT, pulseHue) {
    const along01 = PIXELS > 1 ? pixelIndex / (PIXELS - 1) : 0.5;

    const placed = placedAt(p, rulerAt(p, stripIndex, pixelIndex, shapeU), drift);
    const wander = wanderAt(p, along01, stripIndex, wanderT);

    const hue = placed * p.placedHue + wander * p.wanderHue + profile * p.litHueReach + pulseHue;
    const white = placed * p.placedWhite + wander * p.wanderWhite + profile * p.litWhiteReach;
    const dark = placed * p.placedDark + wander * p.wanderDark + profile * p.litDarkReach;

    return applyPushes(base, hue, white, dark);
  }

  // ---- the wall ---------------------------------------------------------

  const wall = new Uint8Array(STRIPS * PIXELS * 3);

  function render(s, beats) {
    const p = readParams(s);
    wall.fill(0);

    const base = { h: p.baseHue, s: p.baseSat, v: p.baseVal };
    const cellLength = PIXELS / p.count;
    const countCells = p.count;

    // Under bounce the core swings inside its own cell, turning where its own
    // edge meets the cell's boundary the way a ball meets a wall, so nothing
    // ever crosses into a neighboring cell. Taking the rate from the cell is
    // what keeps speed an absolute distance: adding shapes shrinks the cell
    // and quickens the turn, and the core still crosses the wall at the pixels
    // per beat on the dial. At full width the swing closes to nothing, which
    // is right — a shape filling its cell has nowhere to go.
    const halfCore = p.width * 0.5;
    const swingSpan = 1 - p.width;
    const bouncing = p.bounce && Math.abs(p.speedPixels) > 0.0001;

    let travelCycles = 0;
    let centerCells = 0.5;
    const direction = p.speedPixels >= 0 ? 1 : -1;
    if (bouncing) {
      const rate = swingSpan > 0.0001
        ? Math.abs(p.speedPixels) / (2 * swingSpan * cellLength)
        : 0;
      travelCycles = trackedPhase(travelPhase, beats, rate);
    } else {
      centerCells = 0.5 + p.positionCells
          + settledTravel(beats, p.speedPixels / cellLength);
    }

    const pulse = anchoredPulsePhase(beats, 1 / p.pulseBeats);
    p.pulse = pulse;

    p.placedActive = Math.abs(p.placedHue) > 0.5
      || Math.abs(p.placedWhite) > 0.001
      || Math.abs(p.placedDark) > 0.001;
    p.wanderActive = Math.abs(p.wanderHue) > 0.5
      || Math.abs(p.wanderWhite) > 0.001
      || Math.abs(p.wanderDark) > 0.001;

    // The two sides of a shape are not the same length — a tail reaches far
    // further than an edge fade — so they are normalized separately. Halfway
    // between the two tips is not the core, and a region asked to sit at the
    // middle of a shape means the core every time.
    // Both color rates go through the tracker for the same reason travel and
    // the pulse do: beats only grows, so a small change of rate multiplied by
    // a large beat count is a large jump.
    const wanderT = trackedPhase(wanderPhase, beats, p.wanderRate);
    const placedDrift = trackedPhase(placedPhase, beats, p.placedSpeed);

    for (let stripIndex = 0; stripIndex < STRIPS; stripIndex++) {
      const stripPhase = p.fan * (stripIndex / STRIPS);

      const mirrored = p.alternate && (stripIndex & 1);

      // Fan is where a strip stands in the cycle, so every destination
      // landing on a strip inherits it and the three stay in step with each
      // other. The washes take the unfanned phase: a PAR is one position
      // with no strip to be offset from.
      const stripPulse = pulse + stripPhase;

      // Nothing sits above full light, so brightness is the one destination
      // with no sign: its amount is how far the trough digs below what the
      // shape branch already lit.
      const swell = 1 - p.sends.light.amount
        * (1 - pulseWave(stripPulse, p.sends.light.shape, p.sends.light.skew));

      // A shape is anchored by its center, so growing it is a breath outward
      // rather than a wipe in from one end — which is what put this
      // destination out of reach the first time it was tried.
      const width = pushToward(p.width, pulsePush(p.sends.width, stripPulse), 0, 1);
      const pulseHue = pulsePush(p.sends.hue, stripPulse) * GEN_PULSE_MAX_HUE;

      // The two sides of a shape are not the same length — a tail reaches far
      // further than an edge fade — so they are normalized separately.
      // Halfway between the two tips is not the core, and a region asked to
      // sit at the middle of a shape means the core every time.
      const shapeGap = 1 - width;
      const shapeLead = width * 0.5 + p.edge * shapeGap * 0.5;
      const shapeTrail = width * 0.5 + Math.max(p.edge * shapeGap * 0.5, p.tail * shapeGap);

      // Under bounce fan offsets where a strip stands in its own swing, so the
      // five turn at different moments. It cannot offset the core's position
      // instead: an image standing past the strip's end is clipped away by
      // nearestOffset, so displacing it there shortens a strip rather than
      // staggering it.
      let coreCenter, stripDirection, triangle = 0;
      if (bouncing) {
        triangle = fract(travelCycles + stripPhase);
        const rising = triangle < 0.5;
        const swing = rising ? triangle * 2 : (1 - triangle) * 2;
        const place = halfCore + swing * swingSpan;
        coreCenter = mirrored ? 1 - place : place;
        stripDirection = rising ? 1 : -1;
        if (mirrored) stripDirection = -stripDirection;
      } else {
        const centerHere = mirrored ? countCells - centerCells : centerCells;
        coreCenter = fract(centerHere + stripPhase);
        stripDirection = mirrored ? -direction : direction;
      }
      const jitterBucket = Math.floor(pulse + stripPhase) & 255;

      for (let pixelIndex = 0; pixelIndex < PIXELS; pixelIndex++) {
        let jitterOffset = 0;
        let jitterLevel = 1;
        if (p.jitter > 0.0001) {
          const offsetNoise = hash8(stripIndex, pixelIndex, jitterBucket);
          jitterOffset = p.jitter * (offsetNoise / 255 - 0.5);
          const levelNoise = hash8(pixelIndex, stripIndex, jitterBucket ^ 0x5a);
          jitterLevel = 1 - p.jitter * (levelNoise / 255);
        }

        let accumulated = 0;
        for (let sampleIndex = 0; sampleIndex < SUBSAMPLES; sampleIndex++) {
          const samplePosition = pixelIndex + (sampleIndex + 0.5) / SUBSAMPLES;
          const posCells = samplePosition / cellLength + jitterOffset;

          if (bouncing) {
            // The core stays inside its cell, so its trail does too: at a turn
            // the core walks back out through what it laid down rather than
            // the trail changing sides.
            const nearest = nearestOffset(posCells, coreCenter, stripDirection, true, countCells);
            const level = nearest === null ? 0 : coreAt(nearest, width, p.edge);
            const trailing = tailAt(
              trailBehind(journeyIn(posCells, mirrored), triangle, halfCore, swingSpan),
              width, p.tail);
            accumulated += trailing > level ? trailing : level;
          } else {
            const nearest = nearestOffset(posCells, coreCenter, stripDirection, p.bounce, countCells);
            if (nearest !== null) accumulated += shapeAt(nearest, width, p.edge, p.tail);
          }
        }

        const profile = accumulated / SUBSAMPLES;
        const brightness = profile * jitterLevel * swell;
        if (brightness <= 0.002) continue;

        const centerPos = (pixelIndex + 0.5) / cellLength + jitterOffset;
        const centerOffset = nearestOffset(centerPos, coreCenter, stripDirection,
                                           p.bounce, countCells);
        let shapeU = 0.5;
        if (bouncing) {
          // The ruler's trailing half has to be the same measure the tail is
          // drawn from, or color along a tail paints where the tail is not.
          const behind = trailBehind(journeyIn(centerPos, mirrored), triangle,
                                     halfCore, swingSpan);
          if (behind < shapeTrail && shapeTrail > 0.0001) {
            shapeU = 0.5 + 0.5 * behind / shapeTrail;
          } else if (centerOffset !== null && shapeLead > 0.0001) {
            shapeU = 0.5 - 0.5 * Math.abs(centerOffset) / shapeLead;
          }
        } else if (centerOffset !== null) {
          const reach = centerOffset < 0 ? shapeLead : shapeTrail;
          if (reach > 0.0001) shapeU = 0.5 + 0.5 * centerOffset / reach;
        }
        shapeU = Math.max(0, Math.min(1, shapeU));
        const tint = colorAt(p, base, stripIndex, pixelIndex, shapeU, profile,
                              placedDrift, wanderT, pulseHue);

        const rgb = hsv2rgb(tint.h & 255, clamp8(tint.s), 255);
        const level = clamp8(tint.v * brightness);
        const at = (stripIndex * PIXELS + pixelIndex) * 3;
        wall[at] = scale8v(rgb[0], level);
        wall[at + 1] = scale8v(rgb[1], level);
        wall[at + 2] = scale8v(rgb[2], level);
      }
    }

    return p;
  }

  // PC 11, a port of ShowStripOrder() in brain/src/helpers.cpp. Each strip one
  // flat color in data-chain order, which is how the left-to-right order and
  // which end pixel 0 sits at get worked out in the first place.
  const STRIP_ORDER_HUES = [0, 40, 96, 130, 165];

  function renderStripOrder() {
    wall.fill(0);
    for (let stripIndex = 0; stripIndex < STRIPS; stripIndex++) {
      const rgb = hsv2rgb(STRIP_ORDER_HUES[stripIndex], 255, 200);
      for (let pixelIndex = 0; pixelIndex < PIXELS; pixelIndex++) {
        const at = (stripIndex * PIXELS + pixelIndex) * 3;
        wall[at] = rgb[0];
        wall[at + 1] = rgb[1];
        wall[at + 2] = rgb[2];
      }
    }
  }

  // dmx_out::tick() takes presetColor — the three faders — converts it at
  // full value, and carries brightness on the fixture's own dimmer. All four
  // get the same color.
  //
  // The pulse is the one part of the shape branch that reaches them: a PAR is
  // one position with no length, so a swell, a strobe and a breathe all
  // render on it and a sweep does not. It takes the unfanned phase, and it
  // reaches them only while the generator is what is being drawn — `p.pulse`
  // is absent otherwise, which is the same thing the firmware does by
  // clearing the push it was handed every frame.
  function parColor(p) {
    const phase = p.pulse;
    const push = key => (phase === undefined ? 0 : pulsePush(p.sends[key], phase));

    const master = pushToward(p.washLevel, push('parLevel'), 0, 255);
    const level = scale8(p.baseVal, clamp8(master));
    const hue = (p.baseHue + p.washHueOffset
                 + Math.trunc(push('parHue') * GEN_PULSE_MAX_HUE)) & 255;
    const saturation = clamp8(pushToward(p.baseSat, push('parSat'), 0, 255));

    const rgb = hsv2rgb(hue, saturation, 255);
    return [scale8v(rgb[0], level), scale8v(rgb[1], level), scale8v(rgb[2], level)];
  }

  // ---- drawing ----------------------------------------------------------

  const W = 300, H = 480;
  const PAR_BAND = 96;
  const WALL_TOP = 12;
  const WALL_H = H - PAR_BAND - WALL_TOP - 12;

  function draw(ctx, glow, order, flipped, par) {
    const gctx = glow.getContext('2d');
    gctx.clearRect(0, 0, W, H);

    const pitch = WALL_H / PIXELS;
    const pixelH = Math.max(2, pitch - 1.4);
    const columnW = W / (STRIPS + 1);
    const stripW = Math.min(26, columnW * 0.62);

    for (let column = 0; column < STRIPS; column++) {
      const stripIndex = order[column];
      const x = columnW * (column + 1) - stripW / 2;
      for (let pixelIndex = 0; pixelIndex < PIXELS; pixelIndex++) {
        const at = (stripIndex * PIXELS + pixelIndex) * 3;
        const r = wall[at], g = wall[at + 1], b = wall[at + 2];
        if (r + g + b === 0) continue;
        const row = flipped ? pixelIndex : PIXELS - 1 - pixelIndex;
        gctx.fillStyle = `rgb(${r},${g},${b})`;
        gctx.fillRect(x, WALL_TOP + row * pitch, stripW, pixelH);
      }
    }

    const parY = H - PAR_BAND / 2;
    for (let i = 0; i < 4; i++) {
      const x = W * (i + 0.5) / 4;
      const grad = gctx.createRadialGradient(x, parY, 2, x, parY, PAR_BAND * 0.52);
      grad.addColorStop(0, `rgba(${par[0]},${par[1]},${par[2]},0.95)`);
      grad.addColorStop(1, `rgba(${par[0]},${par[1]},${par[2]},0)`);
      gctx.fillStyle = grad;
      gctx.beginPath();
      gctx.arc(x, parY, PAR_BAND * 0.52, 0, Math.PI * 2);
      gctx.fill();
    }

    ctx.fillStyle = '#0a0b0e';
    ctx.fillRect(0, 0, W, H);

    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    ctx.globalAlpha = 0.55;
    ctx.filter = 'blur(7px)';
    ctx.drawImage(glow, 0, 0);
    ctx.restore();

    ctx.drawImage(glow, 0, 0);

    ctx.strokeStyle = 'rgba(255,255,255,0.05)';
    ctx.beginPath();
    ctx.moveTo(0, H - PAR_BAND);
    ctx.lineTo(W, H - PAR_BAND);
    ctx.stroke();
  }

  // ---- panel ------------------------------------------------------------

  const PANEL_CSS = `
  #previewDock {
    position: fixed; top: 16px; right: 16px; width: 332px; z-index: 40;
    background: #1c1e26; border: 1px solid #2c2f3a; border-radius: 6px; padding: 12px;
  }
  #previewDock canvas { width: 100%; border-radius: 4px; display: block; background: #0a0b0e; }
  #previewDock .pvRow { display: flex; gap: 8px; align-items: center; margin-top: 9px; flex-wrap: wrap; }
  #previewDock input[type=text] {
    width: 86px; background: #262934; color: #e6e7ec; border: 1px solid #2c2f3a;
    border-radius: 4px; padding: 4px 6px; font: inherit;
  }
  #previewDock .pvNote { color: #8b8fa3; font-size: 11px; line-height: 1.45; margin-top: 8px; }
  #previewDock .pvWarn { color: #d98c3f; }
  body.hasPreview { padding-right: 368px; }
  @media (max-width: 1100px) {
    #previewDock { position: static; width: auto; margin-bottom: 16px; }
    body.hasPreview { padding-right: 18px; }
  }`;

  function start(getState, getBpm, getPreset) {
    const style = document.createElement('style');
    style.textContent = PANEL_CSS;
    document.head.appendChild(style);

    const dock = document.createElement('div');
    dock.id = 'previewDock';
    dock.innerHTML = `
      <h2 style="font-size:12px;margin:0 0 10px;letter-spacing:.1em;text-transform:uppercase;color:#8b8fa3">The wall</h2>
      <canvas id="pvCanvas"></canvas>
      <div class="pvRow">
        <button id="pvFlip">pixel 0 at bottom</button>
        <label style="color:#8b8fa3;font-size:11px">order
          <input type="text" id="pvOrder" value="${WALL_STRIP_ORDER.join(',')}">
        </label>
      </div>
      <div class="pvNote">Ported from the firmware, not written afresh. <em>Order</em> is this wall's, from PC 11, and it comes from the code — edit it here to try something, edit WALL_STRIP_ORDER to keep it. A flat color has no end to tell apart, so <em>pixel 0</em> needs a moving shape instead: one narrow shape, slow, no fan.</div>`;
    document.body.insertBefore(dock, document.body.firstChild);
    document.body.classList.add('hasPreview');

    const canvas = dock.querySelector('#pvCanvas');
    const dpr = Math.min(2, global.devicePixelRatio || 1);
    canvas.width = W * dpr;
    canvas.height = H * dpr;
    canvas.style.aspectRatio = `${W} / ${H}`;
    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);

    const glow = document.createElement('canvas');
    glow.width = W;
    glow.height = H;

    let flipped = false;
    const flip = dock.querySelector('#pvFlip');
    flip.addEventListener('click', () => {
      flipped = !flipped;
      flip.textContent = flipped ? 'pixel 0 at top' : 'pixel 0 at bottom';
    });

    const orderInput = dock.querySelector('#pvOrder');

    // Only the flip is remembered. The order is settled, so it lives in the
    // code, where a value left in one browser cannot quietly override it.
    try {
      const saved = JSON.parse(localStorage.getItem('aurora.preview') || '{}');
      if (saved.flipped) flip.click();
    } catch {}
    flip.addEventListener('click', () => {
      try {
        localStorage.setItem('aurora.preview', JSON.stringify({ flipped }));
      } catch {}
    });

    function order() {
      const parsed = orderInput.value.split(',')
        .map(n => parseInt(n, 10) - 1)
        .filter(n => Number.isInteger(n) && n >= 0 && n < STRIPS);
      return parsed.length === STRIPS ? parsed : WALL_STRIP_ORDER.map(n => n - 1);
    }

    restart();
    function frame() {
      const s = getState();
      const bpm = getBpm();
      const beats = ((performance.now() - startedAt) / 60000) * bpm;
      const preset = getPreset ? getPreset() : 10;

      // The PARs are not a pattern: dmx_out::tick() drives them from the
      // faders whatever the brain is running, which is why a blackout on
      // PC 0 alone leaves them lit.
      let p;
      if (preset === PRESET_STRIP_ORDER) {
        renderStripOrder();
        p = readParams(s);
      } else {
        p = render(s, beats);
      }
      draw(ctx, glow, order(), flipped, parColor(p));
      global.requestAnimationFrame(frame);
    }
    global.requestAnimationFrame(frame);
  }

  global.AuroraPreview = { start, restart, render, renderStripOrder, parColor, wall, pulseWave, pulsePeriod };
})(window);
