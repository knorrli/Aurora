// The wall on screen. What is lit comes from shared/render, the brain's own
// renderer compiled to WebAssembly in tools/render.js; this file only draws
// it.
//
// A screen is not a WS2812. Desaturation, the dark floor and red's resolution
// are the things docs/bench-facts.md caught a screen getting wrong; settle
// those on the wall.
//
// Which physical end pixel 0 sits at, and the left-to-right order of the
// five strips, are rigging facts the firmware never states. PC 11 settles
// the order: it paints each strip one flat color, and that order is
// recorded in WALL_STRIP_ORDER below. It cannot settle which end pixel 0
// is — a flat color has no end to tell apart — so that stays a control.

(function (global) {
  'use strict';

  // Data-chain strip numbers, left to right across the room. The chain runs
  // the opposite way to the wall, so strip 5 stands at the left-hand end —
  // read off PC 11, 2026-09-22. See docs/wiring.md § "Where they stand on
  // the wall", which is also where fan's zero end is worked out.
  const WALL_STRIP_ORDER = [5, 4, 3, 2, 1];

  const api = {
    WALL_STRIP_ORDER,
    ready: global.AuroraRenderModule().then(connect),
  };

  function connect(m) {
    const STRIPS = m._aurora_strips();
    const PIXELS = m._aurora_pixels_per_strip();
    const CURVE_POINTS = m._aurora_fan_curve_points();
    const controls = m._aurora_controls();
    const pixels = m.HEAPU8.subarray(m._aurora_pixels(), m._aurora_pixels() + STRIPS * PIXELS * 3);
    const wash = m._aurora_wash();
    const fan = m.HEAPF32.subarray(m._aurora_fan() >> 2,
                                   (m._aurora_fan() >> 2) + STRIPS + CURVE_POINTS + 6);
    const DARK = [0, 0, 0];

    // The fixture dims in its own hardware, so this is how the eye sees the
    // dimmer rather than anything the brain computes.
    function seenWash() {
      const level = m.HEAPU8[wash + 3];
      return [0, 1, 2].map(i => Math.round(m.HEAPU8[wash + i] * level / 255));
    }

    function readFan() {
      const after = STRIPS + CURVE_POINTS;
      const stepsPerStrip = (CURVE_POINTS - 1) / (STRIPS - 1);
      return {
        values: Array.from(fan.subarray(0, STRIPS)),
        curve: Array.from(fan.subarray(STRIPS, after), (v, i) => [i / stepsPerStrip, v]),
        turns: fan[after],
        stillAt: Math.abs(fan[after + 1]) <= 1 ? fan[after + 1] : null,
        spent: [
          ['position', fan[after + 2]],
          ['rate', fan[after + 3]],
          ['pulse', fan[after + 4]],
        ].filter(([, amount]) => Math.abs(amount) > 0.005),
        scrambled: fan[after + 5],
      };
    }

    // A frame is a view into the module's memory, good until the next render.
    Object.assign(api, {
      STRIPS, PIXELS,
      makeMotion: () => m._aurora_motion_new(),
      copyMotion: (to, from) => m._aurora_motion_copy(to, from),
      pulseWave: (phase, wave) => m._aurora_pulse_wave(phase, wave),
      convert: (cc, value) => m._aurora_convert(cc, value),
      pulsePeriodBeats: value => m._aurora_pulse_period_beats(value),

      // Both read the last render, so call them before the next one.
      stripValues: cc => Array.from({ length: STRIPS }, (_, i) => m._aurora_strip_value(cc, i)),
      routeRefused: cc => !!m._aurora_route_refused(cc),
      routeReach(cc) {
        const at = m._aurora_route_reach(cc);
        return at ? [m.HEAPF32[at >> 2], m.HEAPF32[(at >> 2) + 1]] : null;
      },

      render(bytes, beats, motion) {
        m.HEAPU8.set(bytes, controls);
        m._aurora_render(motion, beats);
        return { pixels, par: seenWash(), fan: readFan() };
      },
      renderStripOrder() {
        m._aurora_render_strip_order();
        return { pixels, par: DARK, fan: null };
      },
      blank() {
        pixels.fill(0);
        return { pixels, par: DARK, fan: null };
      },
      draw,
    });
    return api;
  }

  // The room's proportions are fixed and the picture scales inside them, so
  // a small copy of the wall beside a large one is the same wall.
  function draw(ctx, glow, frame, order, flipped, W, H, showFan) {
    const { STRIPS, PIXELS } = api;
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
        const r = frame.pixels[at], g = frame.pixels[at + 1], b = frame.pixels[at + 2];
        if (r + g + b === 0) continue;
        const row = flipped ? pixelIndex : PIXELS - 1 - pixelIndex;
        gctx.fillStyle = `rgb(${r},${g},${b})`;
        gctx.fillRect(x, WALL_TOP + row * pitch, stripW, pixelH);
      }
    }

    const par = frame.par;
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

    if (showFan && frame.fan) drawFan(ctx, frame.fan, order, flipped, columnW, WALL_TOP, WALL_H);
  }

  // Drawn against the wall's own left-to-right order, so it reads the way the
  // room does rather than the way the data chain runs. The curve between the
  // strips can only be drawn where that order is a straight run in one
  // direction; anywhere else the strips are not in the wave's order and the
  // dots alone are the truth.
  //
  // The still line is where a strip has to sit for the fan to cancel Speed
  // exactly. The zero line is not it: a strip there travels at Speed, which is
  // a standstill only while Speed is centered, so without it a dot plainly
  // above the zero line can be running backwards and the overlay looks like it
  // is lying.
  function drawFan(ctx, fan, order, flipped, columnW, WALL_TOP, WALL_H) {
    const { STRIPS } = api;
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
          where + ' ' + (amount > 0 ? '+' : '−') + Math.round(Math.abs(amount) * 100) + '%').join('  ')
      : 'spent nowhere';
    ctx.fillText(fan.turns.toFixed(2) + ' turns across the wall', 8, WALL_TOP + 12);
    ctx.fillText(spent, 8, WALL_TOP + 24);
    if (fan.scrambled > 0.005) {
      ctx.fillText(Math.round(fan.scrambled * 100) + '% scrambled', 8, WALL_TOP + 36);
    }
    ctx.restore();
  }

  global.AuroraPreview = api;
})(window);
