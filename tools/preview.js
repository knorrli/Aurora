// Aurora wall preview — a port of brain/src/P_Generator.cpp and
// brain/src/dmx_out.cpp, so a setting can be judged without the rig.
//
// The shape branch is the firmware's. The colour layer is not yet in the
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
// five strips, are rigging facts the firmware never states — PC 11 exists
// to find them. Both are controls here rather than assumptions.

(function (global) {
  'use strict';

  const STRIPS = 5;
  const PIXELS = 45;
  const SUBSAMPLES = 4;

  const GEN_MAX_COUNT = 20;
  const GEN_MAX_SPEED_PIXELS_PER_BEAT = 60;
  const GEN_SLOWEST_PULSE_BEATS = 16;
  const GEN_PULSE_RATE_OCTAVES = 6;

  // How dark a full push pulls a pixel, as a fraction of what it would
  // otherwise be. It stops short of zero because a WS2812 has eight linear
  // bits and no gamma: at the bottom one step is a third of the light, so
  // brightness quantises into lurches and pixels crossing to zero pop out.
  const DARK_FLOOR = 0.02;

  const LIT_MAX_HUE = 64;
  const WANDER_MAX_CYCLES_PER_BEAT = 0.5;
  const PLACED_MAX_CELLS_PER_BEAT = 1;

  // CC_COLOUR_FLAGS, bit for bit — see shared/aurora_protocol.h.
  const COLOUR_FLAG_REGION = 1;
  const COLOUR_RULER_MASK = 3 << 1;

  const GEN_FLAG_ALTERNATE = 1;
  const GEN_FLAG_BOUNCE = 2;

  // ---- FastLED byte maths, from brain/.pio/libdeps/.../lib8tion ----------
  // FASTLED_SCALE8_FIXED is 1 in fastled_config.h, which is what puts the
  // (1 + scale) in scale8 and takes the trailing +1 out of the callers.

  const clamp8 = v => (v < 0 ? 0 : v > 255 ? 255 : v | 0);
  const scale8 = (i, s) => ((i * (1 + s)) >> 8) & 255;
  const scale8v = (i, s) => (((i * s) >> 8) + (i && s ? 1 : 0)) & 255;
  const sin8 = t => clamp8(Math.round(128 + 127 * Math.sin(2 * Math.PI * (((t % 256) + 256) % 256) / 256)));

  // hsv2rgb_rainbow, with Y1 on and G2/Gscale off as FastLED ships them.
  // Not interchangeable with a textbook HSV conversion: this one widens and
  // brightens yellow, and every hue the wall has ever been dialled to was
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
  const ccSquared = (v, max) => { const x = (v - 64) / 63; return Math.sign(x) * x * x * max; };
  const ccMap = (v, hi) => Math.floor(v * hi / 127);

  function readParams(s) {
    const flags = s.flags | 0;
    return {
      width: ccUnit(s.width),
      count: 1 + Math.floor(s.count * (GEN_MAX_COUNT - 1) / 127),
      edge: ccUnit(s.edge),
      tail: ccUnit(s.tail),
      speedPixels: ccSquared(s.speed, GEN_MAX_SPEED_PIXELS_PER_BEAT),
      fan: ccUnit(s.fan),
      jitter: ccUnit(s.jitter),
      pulseDepth: ccUnit(s.pulseDepth),
      pulseBeats: GEN_SLOWEST_PULSE_BEATS * Math.pow(0.5, ccUnit(s.pulseRate) * GEN_PULSE_RATE_OCTAVES),
      pulseShape: ccUnit(s.pulseShape),
      alternate: !!(flags & GEN_FLAG_ALTERNATE),
      bounce: !!(flags & GEN_FLAG_BOUNCE),


      litWhiteReach: ccUnit(s.litWhite),
      litHueReach: ccBipolar(s.litHue) * LIT_MAX_HUE,
      litDarkReach: ccBipolar(s.litDark),

      placedKind: (s.colourFlags & COLOUR_FLAG_REGION) ? KIND_REGION : KIND_SLIDE,
      placedRuler: Math.min(RULER_SHAPE, (s.colourFlags & COLOUR_RULER_MASK) >> 1),
      placedHue: ccBipolar(s.placedHue) * PLACED_MAX_HUE,
      placedWhite: ccBipolar(s.placedWhite),
      placedDark: ccBipolar(s.placedDark),
      placedCount: 1 + Math.floor(s.placedCount * (GEN_MAX_COUNT - 1) / 127),
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
  function nearestOffset(posCells, coreCentre, stripDirection, bounce, countCells) {
    const firstImage = coreCentre + Math.floor(posCells - coreCentre);
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

  function shapeAt(offset, width, edge, tail) {
    const halfCore = width * 0.5;
    const gap = 1 - width;
    const spread = edge * gap * 0.5;

    const distance = Math.abs(offset);
    if (distance <= halfCore) return 1;

    const beyond = distance - halfCore;

    let brightness = 0;
    if (spread > 0.0001 && beyond < spread) {
      const k = 1 - beyond / spread;
      brightness = k * k * (3 - 2 * k);
    }

    if (tail > 0.0001 && offset > 0) {
      const tailLength = tail * gap;
      if (beyond < tailLength) {
        const k = 1 - beyond / tailLength;
        const trailing = k * k;
        if (trailing > brightness) brightness = trailing;
      }
    }

    return brightness;
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

  const travelPhase = makeTracker();
  const pulsePhase = makeTracker();
  const wanderPhase = makeTracker();
  const placedPhase = makeTracker();

  // ---- the colour layer, redesigned 2026-09-21 --------------------------
  //
  // A colour is hue, whiteness and darkness. Everything else is a push on
  // those three, and the pushes add. Three sources push:
  //
  //   the placed field  something you aim — a slide across a ruler, or
  //                     regions sitting on it
  //   the wander        the wall never quite the same in two places, and
  //                     where it differs keeps moving
  //   the light level   colour read off how lit the shape left a pixel
  //
  // The layer reads the SHAPE branch's light level and never its own. Feed
  // its own darkness back in and colour depends on colour: pull the wall
  // down for a quiet verse and the hue slides with it.

  const RULER_WALL = 0, RULER_STRIP = 1, RULER_SHAPE = 2;
  const KIND_SLIDE = 0, KIND_REGION = 1;

  const PLACED_MAX_HUE = 128;
  const WANDER_MAX_HUE = 128;

  // Two terms whose rates sit at the golden ratio, so they never come back
  // into step and the wall never repeats. This is not a control: dialling
  // "how far apart the two speeds are" is operating the mechanism.
  const GOLD = 0.6180339887;

  // The base colour sits at zero, so two terms that rarely reach their ends
  // cost nothing: a sum huddled around the middle is the wall sitting at the
  // colour that was dialled. There is no floor here for a colour to fall off.
  function wanderAt(p, along01, stripIndex, t) {
    if (!p.wanderActive) return 0;
    // Measured from the middle strip, not the first. Fanned from the first,
    // strip one never moves and the last does all the travelling, which reads
    // as a one-sided ramp rather than the wall opening — see
    // docs/bench-facts.md § "A field built as along-plus-across".
    const acrossFromCentre = (stripIndex - (STRIPS - 1) * 0.5) / (STRIPS - 1);
    const cyclesAlong = 0.12 * Math.pow(180, p.wanderScale);
    const cyclesAcross = Math.min(1.4, cyclesAlong * 0.3);
    const a = Math.sin(2 * Math.PI * (cyclesAlong * along01 + cyclesAcross * acrossFromCentre + t));
    const b = Math.sin(2 * Math.PI * (cyclesAlong * GOLD * along01 - cyclesAcross * 1.37 * acrossFromCentre + t * GOLD));
    return (a + b) * 0.5;
  }

  // 0 at one end of the ruler, 1 at the other. The shape ruler runs from the
  // leading tip through to the end of the tail, so a slide on it puts one
  // colour at the head and the other behind.
  function rulerAt(p, stripIndex, pixelIndex, shapeU) {
    if (p.placedRuler === RULER_WALL) return STRIPS > 1 ? stripIndex / (STRIPS - 1) : 0.5;
    if (p.placedRuler === RULER_SHAPE) return shapeU;
    return PIXELS > 1 ? pixelIndex / (PIXELS - 1) : 0.5;
  }

  // A slide is monotone with the base colour at the ruler's centre, so the
  // amount is how far ONE end departs and the two ends land twice that apart.
  // A region is a bump: base, departure, back to base — the shape branch's
  // own core-and-fades, which is what makes count, width and edge mean here
  // what they mean there.
  function placedAt(p, u, drift) {
    if (!p.placedActive) return 0;
    if (p.placedKind === KIND_SLIDE) return (u - 0.5) * 2;
    const cell = u * p.placedCount + drift;
    const offset = fract(cell) - 0.5;
    return shapeAt(offset, p.placedWidth, p.placedEdge, 0);
  }

  // Pushes arrive summed and normalised. Darkening rides a geometric taper
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

  function colourAt(p, base, stripIndex, pixelIndex, shapeU, profile, drift, wanderT) {
    const along01 = PIXELS > 1 ? pixelIndex / (PIXELS - 1) : 0.5;

    const placed = placedAt(p, rulerAt(p, stripIndex, pixelIndex, shapeU), drift);
    const wander = wanderAt(p, along01, stripIndex, wanderT);

    const hue = placed * p.placedHue + wander * p.wanderHue + profile * p.litHueReach;
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

    let centreCells, direction;
    if (p.bounce && Math.abs(p.speedPixels) > 0.0001) {
      const rate = Math.abs(p.speedPixels) / (2 * PIXELS);
      const triangle = fract(trackedPhase(travelPhase, beats, rate));
      const rising = triangle < 0.5;
      const swing = rising ? triangle * 2 : (1 - triangle) * 2;
      const halfCore = p.width * 0.5;
      centreCells = halfCore + swing * (p.count - 2 * halfCore);
      direction = rising ? 1 : -1;
    } else {
      centreCells = 0.5 + trackedPhase(travelPhase, beats, p.speedPixels / cellLength);
      direction = p.speedPixels >= 0 ? 1 : -1;
    }

    const pulse = trackedPhase(pulsePhase, beats, 1 / p.pulseBeats);

    p.placedActive = Math.abs(p.placedHue) > 0.5
      || Math.abs(p.placedWhite) > 0.001
      || Math.abs(p.placedDark) > 0.001;
    p.wanderActive = Math.abs(p.wanderHue) > 0.5
      || Math.abs(p.wanderWhite) > 0.001
      || Math.abs(p.wanderDark) > 0.001;

    // The two sides of a shape are not the same length — a tail reaches far
    // further than an edge fade — so they are normalised separately. Halfway
    // between the two tips is not the core, and a region asked to sit at the
    // middle of a shape means the core every time.
    // Both colour rates go through the tracker for the same reason travel and
    // the pulse do: beats only grows, so a small change of rate multiplied by
    // a large beat count is a large jump.
    const wanderT = trackedPhase(wanderPhase, beats, p.wanderRate);
    const placedDrift = trackedPhase(placedPhase, beats, p.placedSpeed);

    const shapeGap = 1 - p.width;
    const shapeLead = p.width * 0.5 + p.edge * shapeGap * 0.5;
    const shapeTrail = p.width * 0.5 + Math.max(p.edge * shapeGap * 0.5, p.tail * shapeGap);

    for (let stripIndex = 0; stripIndex < STRIPS; stripIndex++) {
      const stripPhase = p.fan * (stripIndex / STRIPS);

      let stripDirection = direction;
      if (p.alternate && (stripIndex & 1)) stripDirection = -stripDirection;

      const lfo = 0.5 - 0.5 * Math.cos(2 * Math.PI * fract(pulse + stripPhase));
      const softness = 0.02 * Math.pow(50, p.pulseShape);
      let shaped = (lfo - 0.5) / softness + 0.5;
      if (shaped < 0) shaped = 0; else if (shaped > 1) shaped = 1;
      const swell = 1 - p.pulseDepth + p.pulseDepth * shaped;

      const centreHere = (p.alternate && (stripIndex & 1)) ? countCells - centreCells : centreCells;
      const coreCentre = fract(centreHere + stripPhase);
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

          const nearest = nearestOffset(posCells, coreCentre, stripDirection, p.bounce, countCells);
          if (nearest !== null) accumulated += shapeAt(nearest, p.width, p.edge, p.tail);
        }

        const profile = accumulated / SUBSAMPLES;
        const brightness = profile * jitterLevel * swell;
        if (brightness <= 0.002) continue;

        const centreOffset = nearestOffset(
          (pixelIndex + 0.5) / cellLength + jitterOffset,
          coreCentre, stripDirection, p.bounce, countCells);
        const reach = centreOffset < 0 ? shapeLead : shapeTrail;
        const shapeU = (centreOffset === null || reach < 0.0001)
          ? 0.5
          : Math.max(0, Math.min(1, 0.5 + 0.5 * centreOffset / reach));
        const tint = colourAt(p, base, stripIndex, pixelIndex, shapeU, profile,
                              placedDrift, wanderT);

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

  // The PARs never see the generator. dmx_out::tick() takes presetColor —
  // the three faders — converts it at full value, and carries brightness on
  // the fixture's own dimmer. All four get the same colour.
  function parColour(p) {
    const level = scale8(p.baseVal, p.washLevel);
    const rgb = hsv2rgb((p.baseHue + p.washHueOffset) & 255, p.baseSat, 255);
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

  function start(getState, getBpm) {
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
          <input type="text" id="pvOrder" value="1,2,3,4,5">
        </label>
      </div>
      <div class="pvNote">Ported from the firmware, not written afresh. Which end pixel 0 is and the strips' left-to-right order are rigging facts — set them here to match what PC 11 shows.</div>`;
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

    // Rigging facts, not patch data: they describe this wall, so they belong
    // with the browser rather than in a saved patch.
    try {
      const saved = JSON.parse(localStorage.getItem('aurora.preview') || '{}');
      if (saved.order) orderInput.value = saved.order;
      if (saved.flipped) flip.click();
    } catch {}
    const remember = () => {
      try {
        localStorage.setItem('aurora.preview',
          JSON.stringify({ order: orderInput.value, flipped }));
      } catch {}
    };
    orderInput.addEventListener('change', remember);
    flip.addEventListener('click', remember);

    function order() {
      const parsed = orderInput.value.split(',')
        .map(n => parseInt(n, 10) - 1)
        .filter(n => Number.isInteger(n) && n >= 0 && n < STRIPS);
      return parsed.length === STRIPS ? parsed : [0, 1, 2, 3, 4];
    }

    const startedAt = performance.now();
    function frame() {
      const s = getState();
      const bpm = getBpm();
      const beats = ((performance.now() - startedAt) / 60000) * bpm;
      const p = render(s, beats);
      draw(ctx, glow, order(), flipped, parColour(p));
      global.requestAnimationFrame(frame);
    }
    global.requestAnimationFrame(frame);
  }

  global.AuroraPreview = { start, render, parColour, wall };
})(window);
