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

  // The fan's wave runs across the strips, and five of them cannot sample
  // anything faster than half a cycle each: at that setting every strip lands
  // on the opposite point of the wave from its neighbors, which is alternate,
  // and above it the wave folds back onto slower ones. So the fader stops
  // there.
  //
  // Stepped to eighths of a turn across the wall, with the phase on 128ths of
  // one, because the two together have to be able to read *exactly* zero on a
  // strip. Read near zero and a strip is not still, it crawls: a wave of 0.04
  // against a rate amount of ±12 px/beat is half a pixel a beat, which is a
  // quarter of the strip in a minute and five strip-lengths in a song. Still
  // has to mean still here for the same reason it does on Speed, and 0.125
  // cycles a strip is not a number 127 steps can land on.
  //
  // The phase divides by 128 rather than 127 because a whole turn is the same
  // wall as none, so the fader covers the turn and stops short of repeating
  // its own start.
  const GEN_FAN_FREQ_STEPS = 16;
  const GEN_FAN_MAX_CYCLES_PER_STRIP = 0.5;

  // Which salt the random fan draws its five offsets through. Of the 256,
  // this one puts the strips 0.094, 0.137, 0.200, 0.239 and 0.329 of a cycle
  // apart round the circle: none close enough to read as two strips in
  // unison, and no two gaps alike, which is what stops a random spread
  // arriving as just another pattern.
  const GEN_FAN_HASH_SALT = 118;

  // Where the named shapes sit on the wave byte. Whole numbers a fader lands
  // on exactly, which is why 32 and 96 rather than thirds of the range.
  const GEN_WAVE_SWELL = 32;
  const GEN_WAVE_SAW_DOWN = 64;
  const GEN_WAVE_SQUARE = 96;

  // The shortest stab the rig can draw, as a fraction of a cycle: about one
  // 7-8 ms frame at 120 bpm, and below it a stab lands between frames and
  // flickers instead of shortening. See docs/bench-facts.md § "Frame timing".
  const GEN_PULSE_MIN_WIDTH = 0.06;

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

  // The scatter's fastest clock. Four relights a beat is a sixteenth note,
  // which is where a flicker stops reading as separate events at stage
  // distance.
  const SCATTER_MAX_CYCLES_PER_BEAT = 4;
  const SCATTER_MAX_HUE = 128;

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

  const fanFrequency = v =>
    Math.round(v * GEN_FAN_FREQ_STEPS / 127)
      * (GEN_FAN_MAX_CYCLES_PER_STRIP / GEN_FAN_FREQ_STEPS);

  const or0 = v => (v === undefined ? 0 : v);
  const orMid = v => (v === undefined ? 64 : v);
  const orElse = (v, d) => (v === undefined ? d : v);

  // Where the wave sits when nothing has been dialed: one eighth of a cycle
  // per strip, which spreads the five across exactly one rising edge, and a
  // phase of zero, which starts them at its bottom. That is the staircase.
  const GEN_FAN_FREQ_DEFAULT = 32;

  const A = global.AuroraCC;
  const CC = A.CC;

  // Mirrors brain/src/routes.cpp. A rate feeds a running total, so a push on
  // one accumulates and the wall drifts instead of returning.
  const ROUTE_REFUSED = new Set([CC.tempoDivision, CC.placedSpeed, CC.wanderRate,
    CC.genSpeed, CC.genFanRate, CC.genPulseRate, CC.scatterRate]);
  // 0 and 127 are the same place, so there is no limit to travel toward.
  const ROUTE_CIRCULAR = new Set([CC.hue, CC.washHueOffset, CC.genPosition,
    CC.genFanPhase]);
  // The fan's own amounts spread the five strips, and count sets the cell
  // geometry the strip loop is built on, so both are read before that loop
  // opens. The washes have no strip to be offset from.
  const ROUTE_PLAIN = new Set([CC.washLevel, CC.washHueOffset, CC.washSaturation,
    CC.genCount, CC.genFan, CC.genFanPulse, CC.genFanFreq, CC.genFanPhase,
    CC.genFanRandom]);

  const routeByte = (s, r, field) => {
    const v = s['route' + r + field];
    return v === undefined ? (field === 'Amount' ? 64 : 0) : v;
  };

  function gatherRoutes(s, plainPhase, stripPhase) {
    const push = {};
    for (let r = 0; r < A.ROUTES; r++) {
      const dest = routeByte(s, r, 'Destination');
      if (dest === 0 || ROUTE_REFUSED.has(dest)) continue;
      const amount = ccBipolar(routeByte(s, r, 'Amount'));
      if (Math.abs(amount) < 0.001) continue;
      const ratio = A.routeRatio(routeByte(s, r, 'Ratio'));
      const wave = s['route' + r + 'Wave'];
      const phase = ROUTE_PLAIN.has(dest) ? plainPhase : stripPhase;
      push[dest] = (push[dest] || 0) + amount * pulseWave(phase * ratio, wave === undefined ? GEN_WAVE_SWELL : wave);
    }
    return push;
  }

  function routed(s, push, name, fallback) {
    let base = s[name];
    if (base === undefined) base = fallback;
    const cc = CC[name];
    let amount = (push && cc !== undefined && push[cc]) || 0;
    if (base === undefined || Math.abs(amount) < 0.001) return base;

    amount = Math.max(-1, Math.min(1, amount));
    if (ROUTE_CIRCULAR.has(cc)) {
      return ((Math.round(base + amount * 64) % 128) + 128) % 128;
    }
    const limit = amount >= 0 ? 127 : 0;
    return Math.max(0, Math.min(127, Math.round(base + Math.abs(amount) * (limit - base))));
  }

  function readParams(s, push) {
    const R = (name, fallback) => routed(s, push, name, fallback);
    return {
      width: ccUnit(R('genWidth')),
      count: ccCount(R('genCount')),
      edge: ccUnit(R('genEdge')),
      tail: ccUnit(R('genTail')),
      positionCells: ccBipolar(R('genPosition')) * 0.5,
      speedPixels: stillBelowThreshold(ccSquared(R('genSpeed'), GEN_MAX_SPEED_PIXELS_PER_BEAT)),
      // One wave across the five strips, with three amounts aiming it at three
      // places. Position and pulse are offsets into a cycle, so only the
      // spread between strips is visible and 100 % spreads them over exactly
      // one cell or one swell. Rate is an absolute speed added to Speed's, so
      // the strip the wave reads zero at travels at exactly what Speed says
      // and the others are measured from it.
      fanFreq: fanFrequency(R('genFanFreq', GEN_FAN_FREQ_DEFAULT)),
      fanPhase: R('genFanPhase', 0) / 128,
      fanRandom: ccUnit(R('genFanRandom', 0)),
      fanPosition: ccBipolar(R('genFan')) * 0.5,
      // The same squared curve Speed runs on, so that mirroring one fader
      // about its center against the other cancels *exactly*: a still strip
      // at the wave's peak needs Speed to be the fan's opposite, and two
      // controls on different curves can only ever nearly cancel.
      fanRate: ccSquared(R('genFanRate', 64), GEN_MAX_SPEED_PIXELS_PER_BEAT),
      fanPulse: ccBipolar(R('genFanPulse', 64)) * 0.5,
      pulseBeats: pulsePeriod(R('genPulseRate')),

      alternate: isOn(R('genAlternate')),
      bounce: isOn(R('genBounce')),


      litWhiteReach: ccUnit(R('litWhite')),
      litHueReach: ccBipolar(R('litHue')) * LIT_MAX_HUE,
      litDarkReach: ccBipolar(R('litDark')),

      placedKind: isOn(R('colorRegion')) ? KIND_REGION : KIND_GRADIENT,
      placedRuler: Math.min(RULER_SHAPE, band3(R('colorRuler'))),
      placedHue: ccBipolar(R('placedHue')) * PLACED_MAX_HUE,
      placedWhite: ccBipolar(R('placedWhite')),
      placedDark: ccBipolar(R('placedDark')),
      placedCount: ccCount(R('placedCount')),
      placedWidth: ccUnit(R('placedWidth')),
      placedEdge: ccUnit(R('placedEdge')),
      placedSpeed: ccSquared(R('placedSpeed'), PLACED_MAX_CELLS_PER_BEAT),

      wanderHue: ccBipolar(R('wanderHue')) * WANDER_MAX_HUE,
      wanderWhite: ccBipolar(R('wanderWhite')),
      wanderDark: ccBipolar(R('wanderDark')),
      wanderRate: ccUnit(R('wanderRate')) ** 2 * WANDER_MAX_CYCLES_PER_BEAT,
      wanderScale: ccUnit(R('wanderScale')),

      // Defaulted rather than read straight, because a caller that predates
      // the scatter sends none of these: undefined through ccBipolar is NaN,
      // and one NaN reaching the color sum turns every hue on the wall into
      // nothing at all.
      scatterRate: ccUnit(R('scatterRate', 0)) ** 2 * SCATTER_MAX_CYCLES_PER_BEAT,
      scatterCount: ccCount(R('scatterCount', 0)),
      scatterWidth: ccUnit(R('scatterWidth', 64)),
      scatterEdge: ccUnit(R('scatterEdge', 64)),
      scatterStagger: ccUnit(R('scatterStagger', 0)),
      scatterDrift: ccBipolar(R('scatterDrift', 64)),
      scatterLightReach: ccBipolar(R('scatterLight', 64)),
      scatterHueReach: ccBipolar(R('scatterHue', 64)) * SCATTER_MAX_HUE,
      scatterWhiteReach: ccBipolar(R('scatterWhite', 64)),


      baseHue: ccMap(R('hue'), 250),
      baseSat: ccMap(R('saturation'), 255),
      baseVal: ccMap(R('value'), 255),

      washLevel: ccMap(R('washLevel'), 255),
      washHueOffset: ccMap(R('washHueOffset'), 255),
      washSaturation: ccMap(R('washSaturation'), 255),
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

  // Everything that has to remember where it was between frames, in one
  // bundle. It is a bundle rather than five module-level variables because a
  // page showing more than one wall renders several different states inside a
  // single frame, and a tracker shared between them would be advanced at one
  // wall's rate and read at another's — which reads as the main picture
  // stuttering whenever a second one is on screen.
  function makeMotion() {
    return {
      // One per strip, because the fan's rate destination gives each its own
      // speed. Scaling a single shared phase five ways instead would jump
      // every strip the moment the fan amount moved, which is the teleport
      // the tracker exists to prevent.
      travelPhase: Array.from({ length: STRIPS }, makeTracker),
      pulsePhase: makeTracker(),
      wanderPhase: makeTracker(),
      placedPhase: makeTracker(),
      scatterPhase: makeTracker(),
      lastBouncing: false,
      lastCoreCells: new Array(STRIPS).fill(0.5),
      lastPulseBeats: 0,
      lastTravelBeats: 0,
    };
  }

  const cloneMotion = m => ({
    travelPhase: m.travelPhase.map(t => ({ ...t })),
    pulsePhase: { ...m.pulsePhase },
    wanderPhase: { ...m.wanderPhase },
    placedPhase: { ...m.placedPhase },
    scatterPhase: { ...m.scatterPhase },
    lastBouncing: m.lastBouncing,
    lastCoreCells: m.lastCoreCells.slice(),
    lastPulseBeats: m.lastPulseBeats,
    lastTravelBeats: m.lastTravelBeats,
  });

  let M = makeMotion();

  // The offset that stops a rate change teleporting is also what leaves the
  // cycle's zero wherever the rate was last touched — never a bar line, so a
  // deep slow swell peaks wherever it happens to. A whole cycle of offset is
  // invisible, so only the fraction has to go: easing it out over the next
  // couple of cycles walks the pulse back onto the grid without ever
  // jumping. Every wave peaks at its cycle's zero, so the offset that lands
  // one on a bar line is a whole number.
  function anchoredPulsePhase(beats, rate) {
    const elapsed = beats - M.lastPulseBeats;
    M.lastPulseBeats = beats;

    // The transport restarted, and beat zero is a bar line by definition.
    if (elapsed < 0) {
      M.pulsePhase.offset = 0;
      M.pulsePhase.rate = rate;
      return beats * rate;
    }

    const phase = trackedPhase(M.pulsePhase, beats, rate);
    const drift = M.pulsePhase.offset - Math.round(M.pulsePhase.offset);
    if (Math.abs(drift) < 0.0001) return phase;

    const pull = Math.min(1, elapsed * rate / GEN_PULSE_ANCHOR_CYCLES);
    M.pulsePhase.offset -= drift * pull;
    return phase - drift * pull;
  }

  // The offset that stops a speed change teleporting the pattern is also what
  // leaves a stopped one standing wherever the last traveling pattern ran out,
  // so a patch saved still comes back somewhere else every time. A whole cell
  // of offset is invisible, since the shape repeats once per cell, so only the
  // fraction has to go and home is never further than half a cell away.
  // While travel is running the offset is where the pattern stands, so there
  // is nothing to settle and this leaves it alone.
  function settledTravel(tracker, beats, elapsed, rate) {
    const travel = trackedPhase(tracker, beats, rate);
    if (Math.abs(rate) > 0.0001 || elapsed <= 0) return travel;

    const drift = tracker.offset - Math.round(tracker.offset);
    if (Math.abs(drift) < 0.0001) return travel;

    const pull = Math.min(1, elapsed / GEN_POSITION_SETTLE_BEATS);
    tracker.offset -= drift * pull;
    return travel - drift * pull;
  }

  // Under bounce the core's position is a triangle: out to one wall of its
  // cell and back, once per cycle.
  const triangleSwing = phase => (phase < 0.5 ? phase * 2 : (1 - phase) * 2);

  // The fan's wave, running across the strips rather than through time. A
  // triangle rather than a sine because it is what stands five strips at
  // evenly spaced offsets; a sine bunches the middle three and a staircase
  // stops looking straight.
  //
  // At the top of the frequency range neighbors sit half a cycle apart, so
  // the wave reads the same two points whatever the phase — moving Phase
  // there only scales how deep the alternation is, and at a quarter and three
  // quarters it reads zero on every strip and the fan goes quiet.
  //
  // Randomize crossfades each strip toward a fixed draw. It is the one
  // arrangement no frequency reaches: every setting of a wave is orderly, and
  // what the wall asked for was comets that do not look placed.
  function fanWave(p, stripIndex) {
    const u = fract(p.fanPhase + p.fanFreq * stripIndex);
    const ordered = u < 0.5 ? 4 * u - 1 : 3 - 4 * u;
    if (p.fanRandom < 0.0001) return ordered;
    const drawn = hash8(stripIndex, 0, GEN_FAN_HASH_SALT) / 255 * 2 - 1;
    return ordered + (drawn - ordered) * p.fanRandom;
  }

  // Which phase stands the core where it already stands, for the mode being
  // entered. Position is read out of the travel phase differently in each — a
  // fraction of a cell under wrap, a triangle between the cell's two walls
  // under bounce — and the tracker keeps the phase continuous rather than the
  // position, so flipping the switch teleported the shape.
  //
  // One thing it cannot preserve: bounce cannot put a core within half its
  // own width of a cell wall, because that is where it turns — a shape
  // standing there snaps out to the wall, by at most half its width. Every
  // strip is solved for separately, so a fanned wall keeps its stagger across
  // the flip rather than only the strip the wave reads zero at.
  function reanchorTravel(tracker, beats, bouncing, p, speedPixels, coreCells,
                          halfCore, swingSpan, direction, cellLength) {
    let rate, wanted;
    if (bouncing) {
      rate = swingSpan > 0.0001
        ? Math.abs(speedPixels) / (2 * swingSpan * cellLength) : 0;
      const swing = swingSpan > 0.0001
        ? Math.max(0, Math.min(1, (coreCells - halfCore) / swingSpan)) : 0;

      // The half of the swing already traveling the way the shape is, so it
      // carries on and turns at the end it was heading for.
      wanted = direction >= 0 ? swing * 0.5 : 1 - swing * 0.5;
    } else {
      rate = speedPixels / cellLength;
      wanted = coreCells - 0.5 - p.positionCells;
    }
    tracker.rate = rate;
    tracker.offset = wanted - beats * rate;
  }

  // Zero at 0 and one at 1, easing at both ends.
  const raisedCosine = x =>
    0.5 - 0.5 * Math.cos(Math.PI * Math.min(1, Math.max(0, x)));

  // One byte, one axis: the peak never leaves the bar line, and what moves is
  // how the bar fills around it. Below the swell the attack shrinks as the
  // decay grows; above it the attack is gone and the decay both shortens and
  // flattens. Every named shape lands on a value a fader can reach — see
  // GEN_WAVE_* and docs/modulation.md § "The fork, settled".
  //
  // Saw down has to come before square. The other order leaves a crossfade
  // between two shapes that blends into neither; this way it is one decay
  // getting shorter and harder, and every value between is a wave worth
  // dialing.
  function pulseWave(phase, wave) {
    let attack, decay, hard;
    if (wave <= GEN_WAVE_SAW_DOWN) {
      decay = wave / GEN_WAVE_SAW_DOWN;
      attack = 1 - decay;
      hard = 0;
    } else {
      attack = 0;
      const toSquare = (wave - GEN_WAVE_SAW_DOWN) / (GEN_WAVE_SQUARE - GEN_WAVE_SAW_DOWN);
      hard = Math.min(1, toSquare);
      decay = wave <= GEN_WAVE_SQUARE
        ? 1 - 0.5 * toSquare
        : 0.5 * Math.pow(GEN_PULSE_MIN_WIDTH / 0.5,
                         (wave - GEN_WAVE_SQUARE) / (127 - GEN_WAVE_SQUARE));
    }

    const u = fract(phase);
    if (decay > 0 && u <= decay) {
      // Dividing by what is left of softness is what turns a decay into a
      // cliff: at the hard end every point above the floor saturates, which
      // is the flat top a square needs.
      const soft = 1 - hard < 0.001 ? 0.001 : 1 - hard;
      return Math.min(1, raisedCosine(1 - u / decay) / soft);
    }
    if (attack > 0 && u >= 1 - attack) {
      return raisedCosine((u - (1 - attack)) / attack);
    }
    return 0;
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
  //
  // `alongPixels` is fractional because the field is read several times
  // across one pixel, and the strip ruler's whole numbers are pixel centers:
  // the shape branch's pixel runs from `pixelIndex` to `pixelIndex + 1`, this
  // one is centered on `pixelIndex`.
  function rulerAt(p, stripIndex, alongPixels, shapeU) {
    if (p.placedRuler === RULER_WALL) return STRIPS > 1 ? stripIndex / (STRIPS - 1) : 0.5;
    if (p.placedRuler === RULER_SHAPE) return shapeU;
    return PIXELS > 1 ? alongPixels / (PIXELS - 1) : 0.5;
  }

  // A gradient is monotone with the base color at the ruler's center, so the
  // amount is how far ONE end departs and the two ends land twice that apart.
  // A region is a bump: base, departure, back to base — the shape branch's
  // own core-and-fades, which is what makes count, width and edge mean here
  // what they mean there.
  function placedAt(p, u, drift) {
    if (p.placedKind === KIND_GRADIENT) return (u - 0.5) * 2;
    const cell = u * p.placedCount + drift;
    const offset = fract(cell) - 0.5;
    return shapeAt(offset, p.placedWidth, p.placedEdge, 0);
  }

  // The third source, and the first with a position. The pulse is a value
  // over time with nowhere on the wall; the wander is smooth over both. This
  // one is random over both — a grid of cells, each with its own clock, each
  // lighting a spot that appears, holds, fades, and may slide across its own
  // cell while it does.
  //
  // Stateless: a cell's clock is its hash, so there is no particle list and
  // nothing to advance. Width is the spot's core on both axes at once — how
  // much of its cell it covers, and how much of its cycle it is lit — and
  // Edge softens both the same way, which is what keeps this to one word per
  // idea rather than two.
  //
  // Moving Stagger re-keys every cell, so everything in flight jumps. That is
  // the price of having no state and it is confined to that one control: Rate
  // runs through the phase tracker like every other rate here.
  function scatterAt(p, stripIndex, alongPixels, t) {
    const cellF = (alongPixels / PIXELS) * p.scatterCount;
    const cell = Math.floor(cellF);
    const u = cellF - cell;

    const rateSpread = hash8(stripIndex, cell, 17) / 255 - 0.5;
    const phaseOffset = hash8(stripIndex, cell, 43) / 255;
    const age = fract(t * (1 + p.scatterStagger * rateSpread)
                      + p.scatterStagger * phaseOffset);

    // Centered on the middle of the cycle, so a cell runs dark, lights, holds
    // and fades rather than being cut off at the wrap.
    const alive = coreAt(age - 0.5, p.scatterWidth, p.scatterEdge);
    if (alive <= 0.0001) return 0;

    const center = 0.5 + p.scatterDrift * (age - 0.5);
    return alive * coreAt(u - center, p.scatterWidth, p.scatterEdge);
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

  function colorAt(p, base, stripIndex, pixelIndex, placed, profile, wanderT,
                   scatter) {
    const along01 = PIXELS > 1 ? pixelIndex / (PIXELS - 1) : 0.5;

    const wander = wanderAt(p, along01, stripIndex, wanderT);

    const hue = placed * p.placedHue + wander * p.wanderHue + profile * p.litHueReach
              + scatter * p.scatterHueReach;
    const white = placed * p.placedWhite + wander * p.wanderWhite + profile * p.litWhiteReach
              + scatter * p.scatterWhiteReach;
    const dark = placed * p.placedDark + wander * p.wanderDark + profile * p.litDarkReach;

    return applyPushes(base, hue, white, dark);
  }

  // ---- the wall ---------------------------------------------------------

  const wall = new Uint8Array(STRIPS * PIXELS * 3);

  function render(s, beats, motion) {
    const previous = M;
    if (motion) M = motion;
    try {
      return renderInto(s, beats);
    } finally {
      M = previous;
    }
  }

  function renderInto(s, beats) {
    // Read twice: once with no pushes, which is what the clock's own rate
    // needs since a rate is a destination routes refuse, then again on the
    // plain reading of the clock for everything the strip loop is built on.
    const p = readParams(s, null);
    wall.fill(0);


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

    // Speed is what the strip the wave reads zero at travels at; the fan's
    // rate amount is measured from there. So a wall can be turning with
    // Speed at a standstill, and bounce has to ask the five rather than the
    // one dial.
    const stripSpeeds = [];
    for (let i = 0; i < STRIPS; i++) {
      stripSpeeds.push(stillBelowThreshold(p.speedPixels + p.fanRate * fanWave(p, i)));
    }
    const bouncing = p.bounce && stripSpeeds.some(v => v !== 0);

    const travelElapsed = beats - M.lastTravelBeats;
    M.lastTravelBeats = beats;

    const pulse = anchoredPulsePhase(beats, 1 / p.pulseBeats);

    // The washes read the dialed base colour, not the pushed one: a push
    // reaches one fixture family, and CC 38-40 are the strips'. Their own
    // three do take their routes, which the plain pass below applies.
    const parBase = { h: p.baseHue, s: p.baseSat, v: p.baseVal };
    Object.assign(p, readParams(s, gatherRoutes(s, pulse, pulse)));
    p.parBase = parBase;
    p.pulse = pulse;

    p.placedActive = Math.abs(p.placedHue) > 0.5
      || Math.abs(p.placedWhite) > 0.001
      || Math.abs(p.placedDark) > 0.001;
    p.wanderActive = Math.abs(p.wanderHue) > 0.5
      || Math.abs(p.wanderWhite) > 0.001
      || Math.abs(p.wanderDark) > 0.001;
    p.scatterActive = Math.abs(p.scatterLightReach) > 0.001
      || Math.abs(p.scatterHueReach) > 0.5
      || Math.abs(p.scatterWhiteReach) > 0.001;

    // The two sides of a shape are not the same length — a tail reaches far
    // further than an edge fade — so they are normalized separately. Halfway
    // between the two tips is not the core, and a region asked to sit at the
    // middle of a shape means the core every time.
    // Both color rates go through the tracker for the same reason travel and
    // the pulse do: beats only grows, so a small change of rate multiplied by
    // a large beat count is a large jump.
    const wanderT = trackedPhase(M.wanderPhase, beats, p.wanderRate);
    const placedDrift = trackedPhase(M.placedPhase, beats, p.placedSpeed);
    const scatterT = trackedPhase(M.scatterPhase, beats, p.scatterRate);

    for (let stripIndex = 0; stripIndex < STRIPS; stripIndex++) {
      const wave = fanWave(p, stripIndex);
      const stripOffset = p.fanPosition * wave;

      const mirrored = p.alternate && (stripIndex & 1);

      // One wave, three amounts, so where a strip stands, how fast it runs
      // and where it is in the swell are dialed apart — a wall of staggered
      // bars can strobe in unison, which one shared offset could never do.
      // The washes take the unfanned phase whatever these say: a PAR is one
      // position with no strip to be offset from.
      const stripPulse = pulse + p.fanPulse * wave;

      // Now this strip's own reading, so a push rolls across the wall instead
      // of landing on all five at once.
      Object.assign(p, readParams(s, gatherRoutes(s, pulse, stripPulse)));
      const base = { h: p.baseHue, s: p.baseSat, v: p.baseVal };

      // A switch belongs to the patch, so the one moment it moves is an
      // arrival the performer caused and is watching — see DESIGN.md
      // § "Switches belong to the patch". That is the moment a jump would be
      // most visible, so the phase is solved for rather than carried across.
      const stripSpeed = stripSpeeds[stripIndex];
      const direction = stripSpeed >= 0 ? 1 : -1;
      const tracker = M.travelPhase[stripIndex];
      if (bouncing !== M.lastBouncing) {
        reanchorTravel(tracker, beats, bouncing, p, stripSpeed,
                       M.lastCoreCells[stripIndex], halfCore, swingSpan,
                       direction, cellLength);
      }

      let travelCycles = 0;
      let centerCells = 0.5;
      if (bouncing) {
        const rate = swingSpan > 0.0001
          ? Math.abs(stripSpeed) / (2 * swingSpan * cellLength) : 0;
        travelCycles = trackedPhase(tracker, beats, rate);
      } else {
        centerCells = 0.5 + p.positionCells
          + settledTravel(tracker, beats, travelElapsed, stripSpeed / cellLength);
      }
      M.lastCoreCells[stripIndex] = bouncing
        ? halfCore + triangleSwing(fract(travelCycles)) * swingSpan
        : fract(centerCells);

      // Nothing sits above full light, so brightness is the one destination
      // with no sign: its amount is how far the trough digs below what the
      // shape branch already lit.


      // A shape is anchored by its center, so growing it is a breath outward
      // rather than a wipe in from one end — which is what put this
      // destination out of reach the first time it was tried.
      const width = p.width;

      // The two sides of a shape are not the same length — a tail reaches far
      // further than an edge fade — so they are normalized separately.
      // Halfway between the two tips is not the core, and a region asked to
      // sit at the middle of a shape means the core every time.
      const shapeGap = 1 - width;
      const shapeLead = width * 0.5 + p.edge * shapeGap * 0.5;
      const shapeTrail = width * 0.5 + Math.max(p.edge * shapeGap * 0.5, p.tail * shapeGap);

      // Under bounce the position amount offsets where a strip stands in its
      // own swing, so the five turn at different moments. It cannot offset
      // the core's position instead: an image standing past the strip's end
      // is clipped away by nearestOffset, so displacing it there shortens a
      // strip rather than staggering it.
      let coreCenter, stripDirection, triangle = 0;
      if (bouncing) {
        triangle = fract(travelCycles + stripOffset);
        const rising = triangle < 0.5;
        const place = halfCore + triangleSwing(triangle) * swingSpan;
        coreCenter = mirrored ? 1 - place : place;
        stripDirection = rising ? 1 : -1;
        if (mirrored) stripDirection = -stripDirection;
      } else {
        const centerHere = mirrored ? countCells - centerCells : centerCells;
        coreCenter = fract(centerHere + stripOffset);
        stripDirection = mirrored ? -direction : direction;
      }

      for (let pixelIndex = 0; pixelIndex < PIXELS; pixelIndex++) {
        // The placed field is read at the same samples the shape is, and for
        // the same reason: read once at the pixel's center it aliases as soon
        // as its regions get down to a pixel or two across, which is the
        // fault docs/bench-facts.md § "Point-sampling a pattern aliases"
        // records against the shape branch. The wander needs none of this —
        // it is sines, and smooth by construction — and the light level reads
        // the averaged profile already.
        let accumulated = 0;
        let placedAccumulated = 0;
        let scatterAccumulated = 0;
        for (let sampleIndex = 0; sampleIndex < SUBSAMPLES; sampleIndex++) {
          const acrossPixel = (sampleIndex + 0.5) / SUBSAMPLES - 0.5;
          const posCells = (pixelIndex + 0.5 + acrossPixel) / cellLength;
          let shapeU = 0.5;

          if (bouncing) {
            // The core stays inside its cell, so its trail does too: at a turn
            // the core walks back out through what it laid down rather than
            // the trail changing sides.
            const nearest = nearestOffset(posCells, coreCenter, stripDirection, true, countCells);
            const level = nearest === null ? 0 : coreAt(nearest, width, p.edge);
            const behind = trailBehind(journeyIn(posCells, mirrored), triangle,
                                       halfCore, swingSpan);
            const trailing = tailAt(behind, width, p.tail);
            accumulated += trailing > level ? trailing : level;

            // The ruler's trailing half has to be the same measure the tail is
            // drawn from, or color along a tail paints where the tail is not.
            if (behind < shapeTrail && shapeTrail > 0.0001) {
              shapeU = 0.5 + 0.5 * behind / shapeTrail;
            } else if (nearest !== null && shapeLead > 0.0001) {
              shapeU = 0.5 - 0.5 * Math.abs(nearest) / shapeLead;
            }
          } else {
            const nearest = nearestOffset(posCells, coreCenter, stripDirection, p.bounce, countCells);
            if (nearest !== null) {
              accumulated += shapeAt(nearest, width, p.edge, p.tail);
              const reach = nearest < 0 ? shapeLead : shapeTrail;
              if (reach > 0.0001) shapeU = 0.5 + 0.5 * nearest / reach;
            }
          }

          if (p.placedActive) {
            const u = rulerAt(p, stripIndex, pixelIndex + acrossPixel,
                              Math.max(0, Math.min(1, shapeU)));
            placedAccumulated += placedAt(p, u, placedDrift);
          }

          // Read at the same four samples the shape and the placed field are,
          // and for the same reason: at twenty cells a cell is 2.2 pixels, and
          // a spot that size aliases into a flicker if it is read once.
          if (p.scatterActive) {
            scatterAccumulated +=
              scatterAt(p, stripIndex, pixelIndex + 0.5 + acrossPixel, scatterT);
          }
        }

        const profile = accumulated / SUBSAMPLES;
        const scatter = scatterAccumulated / SUBSAMPLES;

        // The push runs from what the shape branch left toward one of the two
        // limits, which is what makes a spot invisible inside a fully lit
        // shape and visible in the gap beside it — no occlusion rule
        // anywhere. It therefore has to be applied before an unlit pixel is
        // culled, or the one place a spot has the furthest to travel is the
        // one place it could never appear.
        let brightness = profile;
        if (p.scatterActive) {
          brightness = pushToward(brightness, scatter * p.scatterLightReach, 0, 1);
        }
        if (brightness <= 0.002) continue;

        const tint = colorAt(p, base, stripIndex, pixelIndex,
                              placedAccumulated / SUBSAMPLES, profile, wanderT,
                              scatter);

        const rgb = hsv2rgb(tint.h & 255, clamp8(tint.s), 255);
        const level = clamp8(tint.v * brightness);
        const at = (stripIndex * PIXELS + pixelIndex) * 3;
        wall[at] = scale8v(rgb[0], level);
        wall[at + 1] = scale8v(rgb[1], level);
        wall[at + 2] = scale8v(rgb[2], level);
      }
    }
    M.lastBouncing = bouncing;

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
    const b = p.parBase || { h: p.baseHue, s: p.baseSat, v: p.baseVal };
    const level = scale8(b.v, clamp8(p.washLevel));
    const hue = (b.h + p.washHueOffset) & 255;
    // A scale down from the strips' saturation rather than a setting of its own.
    const saturation = clamp8(scale8(b.s, p.washSaturation));

    const rgb = hsv2rgb(hue, saturation, 255);
    return [scale8v(rgb[0], level), scale8v(rgb[1], level), scale8v(rgb[2], level)];
  }

  // ---- drawing ----------------------------------------------------------

  const W = 300, H = 480;
  const PAR_BAND = 96;
  const WALL_TOP = 12;
  const WALL_H = H - PAR_BAND - WALL_TOP - 12;

  // The room's proportions are fixed and the picture scales inside them, so
  // a small copy of the wall beside a large one is the same wall.
  function draw(ctx, glow, order, flipped, par, W = 300, H = 480, fan = null) {
    const PAR_BAND = H * 0.2;
    const WALL_TOP = H * 0.025;
    const WALL_H = H - PAR_BAND - WALL_TOP - H * 0.025;
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

    if (fan) drawFan(ctx, fan, order, flipped, columnW, WALL_TOP, WALL_H);
  }

  // Drawn against the wall's own left-to-right order, so it reads the way the
  // room does rather than the way the data chain runs. The curve between the
  // strips can only be drawn where that order is a straight run in one
  // direction; anywhere else the strips are not in the wave's order and the
  // dots alone are the truth.
  function drawFan(ctx, fan, order, flipped, columnW, WALL_TOP, WALL_H) {
    const mid = WALL_TOP + WALL_H / 2;

    // Follows the flip, so a positive amount aimed at position always draws
    // the curve through the bars it puts there rather than through their
    // mirror image.
    const reach = WALL_H * 0.3 * (flipped ? -1 : 1);
    const xAt = column => columnW * (column + 1);

    const step = order[1] - order[0];
    const runs = (step === 1 || step === -1)
      && order.every((s, i) => i === 0 || s - order[i - 1] === step);

    ctx.save();
    ctx.lineWidth = 1;
    ctx.strokeStyle = 'rgba(255,255,255,0.14)';
    ctx.setLineDash([3, 4]);
    ctx.beginPath();
    ctx.moveTo(xAt(-0.4), mid);
    ctx.lineTo(xAt(STRIPS - 0.6), mid);
    ctx.stroke();

    if (fan.stillAt !== null) {
      const y = mid - fan.stillAt * reach;
      ctx.strokeStyle = 'rgba(232,168,90,0.6)';
      ctx.beginPath();
      ctx.moveTo(xAt(-0.4), y);
      ctx.lineTo(xAt(STRIPS - 0.6), y);
      ctx.stroke();
      ctx.fillStyle = 'rgba(232,168,90,0.8)';
      ctx.font = '9px ui-monospace, monospace';
      ctx.fillText('still', xAt(-0.4) + 2, y - 3);
    }
    ctx.setLineDash([]);

    if (runs) {
      const columnOf = index => (step === 1 ? index - order[0] : order[0] - index);
      ctx.strokeStyle = 'rgba(120,200,255,0.5)';
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      fan.curve.forEach(([index, value], i) => {
        const x = xAt(columnOf(index));
        const y = mid - value * reach;
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      });
      ctx.stroke();
    }

    for (let column = 0; column < STRIPS; column++) {
      const value = fan.values[order[column]];
      ctx.fillStyle = 'rgba(150,215,255,0.95)';
      ctx.beginPath();
      ctx.arc(xAt(column), mid - value * reach, 3.2, 0, Math.PI * 2);
      ctx.fill();
    }

    ctx.fillStyle = 'rgba(150,215,255,0.75)';
    ctx.font = '10px ui-monospace, monospace';
    const spent = fan.spent.length
      ? fan.spent.map(([where, amount]) =>
          where + ' ' + (amount > 0 ? '+' : '\u2212') + Math.round(Math.abs(amount) * 100) + '%').join('  ')
      : 'spent nowhere';
    ctx.fillText(fan.turns.toFixed(2) + ' turns across the wall', 8, WALL_TOP + 12);
    ctx.fillText(spent, 8, WALL_TOP + 24);
    if (fan.scrambled > 0.005) {
      ctx.fillText(Math.round(fan.scrambled * 100) + '% scrambled', 8, WALL_TOP + 36);
    }
  }

  // What the fan is doing, for the overlay to draw. The curve is the wave
  // between the strips, which nothing on the wall can show: five strips read
  // five points off it, and a wave that turns between two of them is a wave
  // whose shape the wall cannot report. Seeing where the dots sit on it is
  // the whole of why the frequency fader has a usable end and a mushy one.
  function fanReading(p) {
    const values = [];
    for (let i = 0; i < STRIPS; i++) values.push(fanWave(p, i));

    // Sampled finely enough to show a turn that falls between two strips.
    const curve = [];
    for (let i = 0; i <= (STRIPS - 1) * 24; i++) {
      const at = i / 24;
      const u = fract(p.fanPhase + p.fanFreq * at);
      curve.push([at, u < 0.5 ? 4 * u - 1 : 3 - 4 * u]);
    }
    // Where a strip has to sit for the fan to cancel Speed exactly. The zero
    // line is not it: a strip there travels at Speed, which is a standstill
    // only while Speed is centered. Without this, a dot plainly above the
    // zero line can be running backwards and the overlay looks like it is
    // lying.
    let stillAt = null;
    if (Math.abs(p.fanRate) > 0.0001) {
      const at = -p.speedPixels / p.fanRate;
      if (Math.abs(at) <= 1) stillAt = at;
    }

    return {
      values,
      curve,
      stillAt,
      scrambled: p.fanRandom,
      turns: p.fanFreq * (STRIPS - 1),
      spent: [
        ['position', p.fanPosition * 2],
        ['rate', p.fanRate / GEN_MAX_SPEED_PIXELS_PER_BEAT],
        ['pulse', p.fanPulse * 2],
      ].filter(([, amount]) => Math.abs(amount) > 0.005),
    };
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
        <button id="pvFan">fan wave</button>
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

    // On unless it has been turned off, because it explains the two fan
    // controls that have nothing on the wall to read them from.
    let showFan = true;
    const fanToggle = dock.querySelector('#pvFan');
    const paintFanToggle = () => {
      fanToggle.style.color = showFan ? '#96d7ff' : '';
    };
    fanToggle.addEventListener('click', () => { showFan = !showFan; paintFanToggle(); });

    const orderInput = dock.querySelector('#pvOrder');

    // Only the flip is remembered. The order is settled, so it lives in the
    // code, where a value left in one browser cannot quietly override it.
    const remember = () => {
      try {
        localStorage.setItem('aurora.preview', JSON.stringify({ flipped, showFan }));
      } catch {}
    };
    try {
      const saved = JSON.parse(localStorage.getItem('aurora.preview') || '{}');
      if (saved.flipped) flip.click();
      if (saved.showFan === false) showFan = false;
    } catch {}
    paintFanToggle();
    flip.addEventListener('click', remember);
    fanToggle.addEventListener('click', remember);

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
      draw(ctx, glow, order(), flipped, parColor(p), W, H,
           showFan ? fanReading(p) : null);
      global.requestAnimationFrame(frame);
    }
    global.requestAnimationFrame(frame);
  }

  global.AuroraPreview = {
    start, restart, render, renderStripOrder, parColor, wall, pulseWave, pulsePeriod,
    draw, fanReading, hsv2rgb, makeMotion, cloneMotion, STRIPS, PIXELS, WALL_STRIP_ORDER,
  };
})(window);
