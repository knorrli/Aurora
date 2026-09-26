(function (global) {
  'use strict';

  const WALL_STRIP_ORDER = [5, 4, 3, 2, 1];

  const api = {
    WALL_STRIP_ORDER,
    ready: global.AuroraRenderModule().then(connect),
  };

  function connect(renderer) {
    const STRIPS = renderer._aurora_strip_count();
    const PARS = renderer._aurora_par_count();
    const PIXELS = renderer._aurora_pixels_per_strip();
    const CURVE_POINTS = renderer._aurora_fan_curve_points();
    const controls = renderer._aurora_controls();
    const pixelsAt = renderer._aurora_pixels();
    const pixels = renderer.HEAPU8.subarray(pixelsAt, pixelsAt + STRIPS * PIXELS * 3);
    const parColors = renderer._aurora_pars();
    const huePlacesAt = renderer._aurora_par_hue_places() >> 2;
    const huePlaces = renderer.HEAPF32.subarray(huePlacesAt, huePlacesAt + PARS);
    const fanAt = renderer._aurora_fan() >> 2;
    const fan = renderer.HEAPF32.subarray(fanAt, fanAt + STRIPS + CURVE_POINTS + 6);
    const bendAt = renderer._aurora_bend() >> 2;
    const bend = renderer.HEAPF32.subarray(bendAt, bendAt + renderer._aurora_bend_points());

    function seenPars() {
      return Array.from({ length: PARS }, (_, par) => {
        const at = parColors + par * 4;
        return [0, 1, 2].map(i => Math.round(renderer.HEAPU8[at + i] * renderer.HEAPU8[at + 3] / 255));
      });
    }

    const PASS_MARKS = renderer._aurora_arp_pass_marks();

    function readArpPass() {
      const at = renderer._aurora_arp_pass();
      if (!at) return null;
      const floats = renderer.HEAPF32.subarray(at >> 2, (at >> 2) + 4 + PASS_MARKS);
      const count = renderer.HEAPU8[at + 12];
      const parsAt = at + 4 * (4 + PASS_MARKS);
      return {
        turns: floats[0], at: floats[1], length: floats[2],
        marks: Array.from({ length: count }, (_, i) => ({ start: floats[4 + i], par: renderer.HEAPU8[parsAt + i] })),
      };
    }

    function readFan() {
      const after = STRIPS + CURVE_POINTS;
      const stepsPerStrip = (CURVE_POINTS - 1) / (STRIPS - 1);
      return {
        values: Array.from(fan.subarray(0, STRIPS)),
        curve: Array.from(fan.subarray(STRIPS, after), (value, i) => [i / stepsPerStrip, value]),
        turns: fan[after],
        stillAt: Math.abs(fan[after + 1]) <= 1 ? fan[after + 1] : null,
        spent: [
          ['position', fan[after + 2]],
          ['rate', fan[after + 3]],
          ['LFO', fan[after + 4]],
        ].filter(([, amount]) => Math.abs(amount) > 0.005),
        scrambled: fan[after + 5],
      };
    }

    Object.assign(api, {
      STRIPS, PIXELS, PARS,
      makeMotion: () => renderer._aurora_motion_new(),
      copyMotion: (to, from) => renderer._aurora_motion_copy(to, from),
      makeWallState: () => renderer._aurora_wall_new(),
      clearTails: wallState => renderer._aurora_wall_clear_tails(wallState),
      paletteNames: () => Array.from({ length: renderer._aurora_palette_count() }, (_, i) => {
        const at = renderer._aurora_palette_name(i);
        return String.fromCharCode(...renderer.HEAPU8.subarray(at, renderer.HEAPU8.indexOf(0, at)));
      }),
      paletteColor(palette, hue, saturation) {
        const packed = renderer._aurora_palette_color(palette, hue, saturation);
        return [packed >> 16 & 255, packed >> 8 & 255, packed & 255];
      },
      lfoWave: (phase, wave) => renderer._aurora_lfo_wave(phase, wave),
      waveMean: wave => renderer._aurora_wave_mean(wave),
      convert: (cc, value) => renderer._aurora_convert(cc, value),

      controlAtStrips: cc => Array.from({ length: STRIPS }, (_, i) => renderer._aurora_control_at_strip(cc, i)),
      parHuePlaces: () => Array.from(huePlaces),
      controlAtPars: cc => Array.from({ length: PARS }, (_, i) => renderer._aurora_control_at_par(cc, i)),
      routeRefused: cc => !!renderer._aurora_route_refused(cc),
      routeReach(cc) {
        const at = renderer._aurora_route_reach(cc);
        return at ? [renderer.HEAPF32[at >> 2], renderer.HEAPF32[(at >> 2) + 1]] : null;
      },

      render(bytes, quarterNotes, motion, wallState) {
        renderer.HEAPU8.set(bytes, controls);
        renderer._aurora_render(motion, wallState, quarterNotes);
        return { pixels, pars: seenPars(), fan: readFan(), bend: Array.from(bend), arp: readArpPass() };
      },
      draw,
    });
    return api;
  }

  function draw(context, glow, frame, order, flipped, width, height, showOverlays) {
    const { STRIPS, PIXELS } = api;
    const PAR_BAND = height * 0.2;
    const WALL_TOP = height * 0.025;
    const WALL_HEIGHT = height - PAR_BAND - WALL_TOP - height * 0.025;
    const glowContext = glow.getContext('2d');
    glowContext.clearRect(0, 0, width, height);

    const pitch = WALL_HEIGHT / PIXELS;
    const pixelHeight = Math.max(2, pitch - 1.4);
    const columnWidth = width / (STRIPS + 1);
    const stripWidth = Math.min(26, columnWidth * 0.62);

    for (let column = 0; column < STRIPS; column++) {
      const stripIndex = order[column];
      const x = columnWidth * (column + 1) - stripWidth / 2;
      for (let pixelIndex = 0; pixelIndex < PIXELS; pixelIndex++) {
        const at = (stripIndex * PIXELS + pixelIndex) * 3;
        const red = frame.pixels[at], green = frame.pixels[at + 1], blue = frame.pixels[at + 2];
        if (red + green + blue === 0) continue;
        const row = flipped ? pixelIndex : PIXELS - 1 - pixelIndex;
        glowContext.fillStyle = `rgb(${red},${green},${blue})`;
        glowContext.fillRect(x, WALL_TOP + row * pitch, stripWidth, pixelHeight);
      }
    }

    const parY = height - PAR_BAND / 2;
    for (let i = 0; i < frame.pars.length; i++) {
      const par = frame.pars[i];
      const x = width * (i + 0.5) / frame.pars.length;
      const gradient = glowContext.createRadialGradient(x, parY, 2, x, parY, PAR_BAND * 0.52);
      gradient.addColorStop(0, `rgba(${par[0]},${par[1]},${par[2]},0.95)`);
      gradient.addColorStop(1, `rgba(${par[0]},${par[1]},${par[2]},0)`);
      glowContext.fillStyle = gradient;
      glowContext.beginPath();
      glowContext.arc(x, parY, PAR_BAND * 0.52, 0, Math.PI * 2);
      glowContext.fill();
    }

    context.fillStyle = '#0a0b0e';
    context.fillRect(0, 0, width, height);

    context.save();
    context.globalCompositeOperation = 'lighter';
    context.globalAlpha = 0.55;
    context.filter = 'blur(7px)';
    context.drawImage(glow, 0, 0);
    context.restore();

    context.drawImage(glow, 0, 0);

    context.strokeStyle = 'rgba(255,255,255,0.05)';
    context.beginPath();
    context.moveTo(0, height - PAR_BAND);
    context.lineTo(width, height - PAR_BAND);
    context.stroke();

    if (showOverlays && frame.fan) drawFan(context, frame.fan, order, flipped, columnWidth, WALL_TOP, WALL_HEIGHT);
    if (showOverlays && frame.bend) drawBend(context, frame.bend, flipped, columnWidth, WALL_TOP, WALL_HEIGHT);
    if (showOverlays && frame.arp) drawArpPass(context, frame.arp, frame.pars.length, width, height - PAR_BAND, PAR_BAND);
  }

  function drawArpPass(context, pass, pars, width, top, bandHeight) {
    const left = width * 0.04;
    const span = width * 0.92;
    const rowHeight = bandHeight * 0.5 / pars;
    const first = top + bandHeight * 0.3;
    const xAt = turns => left + span * turns / pass.turns;

    context.save();
    context.fillStyle = 'rgba(10,11,14,0.55)';
    context.fillRect(left - 4, first - 4, span + 8, rowHeight * pars + 8);
    context.beginPath();
    context.rect(left, first - 4, span, rowHeight * pars + 8);
    context.clip();

    for (const { start, par } of pass.marks) {
      const y = first + par * rowHeight;
      const tail = context.createLinearGradient(xAt(start), 0, xAt(start + pass.length), 0);
      tail.addColorStop(0, 'rgba(150,215,255,0.4)');
      tail.addColorStop(1, 'rgba(150,215,255,0)');
      context.fillStyle = tail;
      context.fillRect(xAt(start), y + 1, xAt(start + pass.length) - xAt(start), rowHeight - 2);
      context.save();
      context.translate(-span, 0);
      context.fillRect(xAt(start), y + 1, xAt(start + pass.length) - xAt(start), rowHeight - 2);
      context.restore();
      context.fillStyle = 'rgba(150,215,255,0.95)';
      context.fillRect(xAt(start) - 1, y, 2.5, rowHeight - 1);
    }

    const cursor = xAt(pass.at);
    context.strokeStyle = 'rgba(232,168,90,0.9)';
    context.lineWidth = 1.5;
    context.beginPath();
    context.moveTo(cursor, first - 3);
    context.lineTo(cursor, first + rowHeight * pars + 3);
    context.stroke();

    context.restore();
    context.save();
    context.fillStyle = 'rgba(150,215,255,0.75)';
    context.font = '10px ui-monospace, monospace';
    const turns = Math.round(pass.turns * 100) / 100;
    context.fillText(`${turns} ${turns === 1 ? 'turn' : 'turns'} a pass`, left, first - 7);
    context.restore();
  }

  function drawFan(context, fan, order, flipped, columnWidth, WALL_TOP, WALL_HEIGHT) {
    const { STRIPS } = api;
    const middle = WALL_TOP + WALL_HEIGHT / 2;

    const reach = WALL_HEIGHT * 0.3 * (flipped ? -1 : 1);
    const xAt = column => columnWidth * (column + 1);

    const step = order[1] - order[0];
    const runs = (step === 1 || step === -1)
      && order.every((strip, i) => i === 0 || strip - order[i - 1] === step);

    context.save();
    context.lineWidth = 1;
    context.strokeStyle = 'rgba(255,255,255,0.14)';
    context.setLineDash([3, 4]);
    context.beginPath();
    context.moveTo(xAt(-0.4), middle);
    context.lineTo(xAt(STRIPS - 0.6), middle);
    context.stroke();

    if (fan.stillAt !== null) {
      const y = middle - fan.stillAt * reach;
      context.strokeStyle = 'rgba(232,168,90,0.6)';
      context.beginPath();
      context.moveTo(xAt(-0.4), y);
      context.lineTo(xAt(STRIPS - 0.6), y);
      context.stroke();
      context.fillStyle = 'rgba(232,168,90,0.8)';
      context.font = '9px ui-monospace, monospace';
      context.fillText('still', xAt(-0.4) + 2, y - 3);
    }
    context.setLineDash([]);

    if (runs) {
      const columnOf = index => (step === 1 ? index - order[0] : order[0] - index);
      context.strokeStyle = 'rgba(120,200,255,0.5)';
      context.lineWidth = 1.5;
      context.beginPath();
      fan.curve.forEach(([index, value], i) => {
        const x = xAt(columnOf(index));
        const y = middle - value * reach;
        if (i === 0) context.moveTo(x, y); else context.lineTo(x, y);
      });
      context.stroke();
    }

    for (let column = 0; column < STRIPS; column++) {
      const value = fan.values[order[column]];
      context.fillStyle = 'rgba(150,215,255,0.95)';
      context.beginPath();
      context.arc(xAt(column), middle - value * reach, 3.2, 0, Math.PI * 2);
      context.fill();
    }

    context.fillStyle = 'rgba(150,215,255,0.75)';
    context.font = '10px ui-monospace, monospace';
    const spent = fan.spent.length
      ? fan.spent.map(([where, amount]) =>
          where + ' ' + (amount > 0 ? '+' : '−') + Math.round(Math.abs(amount) * 100) + '%').join('  ')
      : 'spent nowhere';
    context.fillText(fan.turns.toFixed(2) + ' turns across the wall', 8, WALL_TOP + 12);
    context.fillText(spent, 8, WALL_TOP + 24);
    if (fan.scrambled > 0.005) {
      context.fillText(Math.round(fan.scrambled * 100) + '% scrambled', 8, WALL_TOP + 36);
    }
    context.restore();
  }

  function drawBend(context, bend, flipped, columnWidth, WALL_TOP, WALL_HEIGHT) {
    const { STRIPS, PIXELS } = api;
    const axis = columnWidth * (STRIPS + 0.5);
    const reach = columnWidth * 0.4;
    const low = Math.min(...bend), high = Math.max(...bend);
    const middle = (low + high) / 2;
    const yAt = i => WALL_TOP + (flipped ? i : PIXELS - i) * WALL_HEIGHT / PIXELS;

    context.save();
    context.strokeStyle = 'rgba(255,255,255,0.14)';
    context.setLineDash([3, 4]);
    context.beginPath();
    context.moveTo(axis, yAt(0));
    context.lineTo(axis, yAt(PIXELS));
    context.stroke();
    context.setLineDash([]);

    context.strokeStyle = 'rgba(232,168,90,0.8)';
    context.lineWidth = 1.5;
    context.beginPath();
    bend.forEach((value, i) => {
      const x = axis + (value - middle) / middle * reach;
      if (i === 0) context.moveTo(x, yAt(i)); else context.lineTo(x, yAt(i));
    });
    context.stroke();

    if (high - low < 0.01) {
      context.restore();
      return;
    }
    context.fillStyle = 'rgba(232,168,90,0.8)';
    context.font = '10px ui-monospace, monospace';
    context.textAlign = 'right';
    context.fillText(`travel ×${low.toFixed(low < 1 ? 2 : 1)}–${high.toFixed(1)}`, columnWidth * (STRIPS + 1) - 6, WALL_TOP + 12);
    context.restore();
  }

  global.AuroraPreview = api;
})(window);
