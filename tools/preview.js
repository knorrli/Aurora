(function (global) {
  'use strict';

  const WALL_STRIP_ORDER = [5, 4, 3, 2, 1];

  const SCREEN_LEVEL_OF_STRIP_BYTE = Array.from({ length: 256 }, (_, byte) => {
    const light = byte / 255;
    const encoded = light <= 0.0031308 ? 12.92 * light : 1.055 * Math.pow(light, 1 / 2.4) - 0.055;
    return Math.round(255 * encoded);
  });

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
    const parHuesAt = renderer._aurora_par_hues();
    const parHues = renderer.HEAPU8.subarray(parHuesAt, parHuesAt + PARS);
    const fanAt = renderer._aurora_fan() >> 2;
    const fan = renderer.HEAPF32.subarray(fanAt, fanAt + STRIPS + CURVE_POINTS + 6);
    const bendAt = renderer._aurora_bend() >> 2;
    const bend = renderer.HEAPF32.subarray(bendAt, bendAt + renderer._aurora_bend_points());
    const centersAt = renderer._aurora_centers() >> 2;
    const centers = renderer.HEAPF32.subarray(centersAt, centersAt + STRIPS);

    function seenPars() {
      return Array.from({ length: PARS }, (_, par) => {
        const at = parColors + par * 4;
        return [0, 1, 2].map(i => Math.round(renderer.HEAPU8[at + i] * renderer.HEAPU8[at + 3] / 255));
      });
    }

    const PASS_MARKS = renderer._aurora_arp_pass_marks();
    const STRIP_SPOTS_SIZE = renderer._aurora_strip_spots_size();
    const fieldLevelsAt = renderer._aurora_field_levels() >> 2;
    const fieldLevels = renderer.HEAPF32.subarray(fieldLevelsAt, fieldLevelsAt + STRIPS * PIXELS);
    const fieldAcrossAt = renderer._aurora_field_across() >> 2;
    const fieldAcross = renderer.HEAPF32.subarray(fieldAcrossAt, fieldAcrossAt + renderer._aurora_field_across_points());
    const SPOT_MARK_FLOATS = 3;

    function readSpots() {
      const at = renderer._aurora_spots();
      return Array.from({ length: STRIPS }, (_, strip) => {
        const base = at + strip * STRIP_SPOTS_SIZE;
        const count = renderer.HEAPU8[base];
        const floats = renderer.HEAPF32.subarray((base + 4) >> 2, ((base + 4) >> 2) + count * SPOT_MARK_FLOATS);
        return Array.from({ length: count }, (_, i) => ({
          center: floats[i * SPOT_MARK_FLOATS], width: floats[i * SPOT_MARK_FLOATS + 1], life: floats[i * SPOT_MARK_FLOATS + 2],
        }));
      });
    }

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
      spotDestination: cc => !!renderer._aurora_spot_destination(cc),
      engineOf: cc => renderer._aurora_engine_of(cc),
      showsEngine: cc => !!renderer._aurora_shows_engine(cc),
      hiddenEngines(bytes) {
        renderer.HEAPU8.set(bytes, controls);
        return renderer._aurora_hidden_engines();
      },
      routeReach(cc) {
        const at = renderer._aurora_route_reach(cc);
        return at ? [renderer.HEAPF32[at >> 2], renderer.HEAPF32[(at >> 2) + 1]] : null;
      },

      render(bytes, quarterNotes, milliseconds, motion, wallState) {
        renderer.HEAPU8.set(bytes, controls);
        renderer._aurora_render(motion, wallState, quarterNotes, milliseconds);
        return {
          pixels, pars: seenPars(), fan: readFan(), bend: Array.from(bend), arp: readArpPass(), lfo: renderer._aurora_lfo(), spots: readSpots(),
          field: {
            horizontal: global.AuroraProtocol.threeWayPosition(bytes[global.AuroraProtocol.CC.fieldDirection])
              === global.AuroraProtocol.FIELD_DIRECTION.horizontal,
            levels: Array.from(fieldLevels),
            across: Array.from(fieldAcross),
          },
          centers: Array.from(centers),
          hues: {
            palette: bytes[global.AuroraProtocol.CC.palette],
            strips: renderer._aurora_strips_hue(),
            pars: Array.from(parHues),
          },
        };
      },
      draw,
    });
    return api;
  }

  function draw(context, glow, frame, order, flipped, width, height, overlays) {
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
        glowContext.fillStyle = `rgb(${SCREEN_LEVEL_OF_STRIP_BYTE[red]},${SCREEN_LEVEL_OF_STRIP_BYTE[green]},${SCREEN_LEVEL_OF_STRIP_BYTE[blue]})`;
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

    if (overlays.fan && frame.fan) drawFan(context, frame.fan, order, flipped, columnWidth, WALL_TOP, WALL_HEIGHT);
    if (overlays.bend && frame.bend) drawBend(context, frame.bend, flipped, columnWidth, WALL_TOP, WALL_HEIGHT);
    if (overlays.arp && frame.arp) drawArpPass(context, frame.arp, frame.pars.length, width, height - PAR_BAND, PAR_BAND);
    if (overlays.palette && frame.hues) drawPalette(context, frame.hues, width, height);
    if (overlays.field && frame.field) {
      if (frame.field.horizontal) drawFieldAcross(context, frame.field, order, columnWidth, WALL_TOP, WALL_HEIGHT);
      else drawField(context, frame.field.levels, order, flipped, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT);
    }
    if (overlays.spots && frame.spots) drawSpots(context, frame.spots, order, flipped, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT);
    if (overlays.centers) drawCenters(context, frame.centers, order, flipped, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT);
  }

  const PALETTE_MARK = 'rgba(176,162,236,0.95)';
  const SHAPE_MARK = 'rgba(240,168,96,0.95)';
  const SHAPE_MARK_SOFT = 'rgba(240,168,96,0.6)';
  const OUTLINE = 'rgba(10,11,14,0.85)';
  const SPOT_MARK = 'rgba(140,220,184,0.95)';
  const FIELD_MARK = 'rgba(140,190,235,0.95)';
  const FIELD_REACH = 7;

  function strokeOutlined(context, style, width) {
    context.strokeStyle = OUTLINE;
    context.lineWidth = width + 2.5;
    context.stroke();
    context.strokeStyle = style;
    context.lineWidth = width;
    context.stroke();
  }
  const paletteRibbons = new Map();

  function paletteRibbon(palette) {
    if (!paletteRibbons.has(palette)) {
      paletteRibbons.set(palette, Array.from({ length: 256 }, (_, hue) => `rgb(${api.paletteColor(palette, hue, 255).join(',')})`));
    }
    return paletteRibbons.get(palette);
  }

  function drawPalette(context, hues, width, height) {
    const left = width * 0.04;
    const span = width * 0.92;
    const top = height - 27;
    const ribbonHeight = 5;
    const xAt = hue => left + span * (hue + 0.5) / 256;
    const ribbon = paletteRibbon(hues.palette);

    context.save();
    context.fillStyle = 'rgba(10,11,14,0.7)';
    context.fillRect(left - 3, top - 9, span + 6, ribbonHeight + 23);
    ribbon.forEach((color, hue) => {
      context.fillStyle = color;
      context.fillRect(left + span * hue / 256, top, span / 256 + 0.6, ribbonHeight);
    });

    context.fillStyle = PALETTE_MARK;
    const strips = xAt(hues.strips);
    context.beginPath();
    context.moveTo(strips - 4, top - 7);
    context.lineTo(strips + 4, top - 7);
    context.lineTo(strips, top - 1);
    context.fill();

    context.font = '9px ui-monospace, monospace';
    context.textAlign = 'center';
    const parsAtHue = new Map();
    hues.pars.forEach((hue, par) => parsAtHue.set(hue, [...(parsAtHue.get(hue) || []), par + 1]));
    for (const [hue, pars] of parsAtHue) {
      const x = xAt(hue);
      context.fillRect(x - 0.75, top + ribbonHeight + 1, 1.5, 4);
      context.fillText(pars.join(''), x, top + ribbonHeight + 13);
    }
    context.restore();
  }

  function drawField(context, levels, order, flipped, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT) {
    const { PIXELS } = api;
    const yAt = pixel => WALL_TOP + (flipped ? pixel + 0.5 : PIXELS - pixel - 0.5) * WALL_HEIGHT / PIXELS;
    context.save();
    order.forEach((strip, column) => {
      const axis = columnWidth * (column + 1) - stripWidth / 2 - FIELD_REACH - 3;
      context.strokeStyle = 'rgba(255,255,255,0.12)';
      context.lineWidth = 1;
      context.beginPath();
      context.moveTo(axis, yAt(0));
      context.lineTo(axis, yAt(PIXELS - 1));
      context.stroke();
      context.beginPath();
      for (let pixel = 0; pixel < PIXELS; pixel++) {
        const x = axis + levels[strip * PIXELS + pixel] * FIELD_REACH;
        if (pixel === 0) context.moveTo(x, yAt(pixel)); else context.lineTo(x, yAt(pixel));
      }
      strokeOutlined(context, FIELD_MARK, 1.5);
    });
    context.restore();
  }

  function drawFieldAcross(context, field, order, columnWidth, WALL_TOP, WALL_HEIGHT) {
    const { STRIPS, PIXELS } = api;
    const step = order[1] - order[0];
    const monotonic = (step === 1 || step === -1) && order.every((strip, i) => i === 0 || strip - order[i - 1] === step);
    const xAtStrip = strip => columnWidth * (1 + (step === 1 ? strip - order[0] : order[0] - strip));
    const base = WALL_TOP + WALL_HEIGHT * 0.88;
    const reach = WALL_HEIGHT * 0.08;
    context.save();
    context.strokeStyle = 'rgba(255,255,255,0.14)';
    context.lineWidth = 1;
    context.setLineDash([3, 4]);
    context.beginPath();
    context.moveTo(columnWidth * 0.6, base);
    context.lineTo(columnWidth * (STRIPS + 0.4), base);
    context.stroke();
    context.setLineDash([]);
    if (monotonic) {
      context.beginPath();
      field.across.forEach((level, i) => {
        const x = xAtStrip(i / (field.across.length - 1) * (STRIPS - 1));
        if (i === 0) context.moveTo(x, base - level * reach); else context.lineTo(x, base - level * reach);
      });
      strokeOutlined(context, FIELD_MARK, 1.5);
    }
    order.forEach((strip, column) => {
      context.beginPath();
      context.arc(columnWidth * (column + 1), base - field.levels[strip * PIXELS] * reach, 3.2, 0, Math.PI * 2);
      context.fillStyle = FIELD_MARK;
      context.fill();
      context.strokeStyle = OUTLINE;
      context.lineWidth = 1.5;
      context.stroke();
    });
    context.restore();
  }

  function drawSpots(context, spots, order, flipped, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT) {
    const { PIXELS } = api;
    const yAt = pixel => WALL_TOP + (flipped ? pixel : PIXELS - pixel) * WALL_HEIGHT / PIXELS;
    context.save();
    context.beginPath();
    context.rect(0, WALL_TOP, columnWidth * (order.length + 1), WALL_HEIGHT);
    context.clip();
    order.forEach((strip, column) => {
      const x = columnWidth * (column + 1);
      const bracketX = x + stripWidth / 2 + 3;
      for (const { center, width, life } of spots[strip]) {
        const left = 1 - life;
        const top = yAt(center + width / 2), bottom = yAt(center - width / 2);
        context.globalAlpha = 0.35 + 0.6 * left;
        context.strokeStyle = SPOT_MARK;
        context.lineWidth = 1.5;
        context.beginPath();
        context.moveTo(bracketX - 3, top);
        context.lineTo(bracketX, top);
        context.lineTo(bracketX, bottom);
        context.lineTo(bracketX - 3, bottom);
        context.stroke();
        const y = yAt(center);
        context.beginPath();
        context.moveTo(x - stripWidth / 2 * left, y);
        context.lineTo(x + stripWidth / 2 * left, y);
        strokeOutlined(context, SPOT_MARK, 1.5);
      }
    });
    context.restore();
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
      tail.addColorStop(0, 'rgba(226,150,172,0.4)');
      tail.addColorStop(1, 'rgba(226,150,172,0)');
      context.fillStyle = tail;
      context.fillRect(xAt(start), y + 1, xAt(start + pass.length) - xAt(start), rowHeight - 2);
      context.save();
      context.translate(-span, 0);
      context.fillRect(xAt(start), y + 1, xAt(start + pass.length) - xAt(start), rowHeight - 2);
      context.restore();
      context.fillStyle = 'rgba(226,150,172,0.95)';
      context.fillRect(xAt(start) - 1, y, 2.5, rowHeight - 1);
    }

    const cursor = xAt(pass.at);
    context.strokeStyle = 'rgba(255,255,255,0.85)';
    context.lineWidth = 1.5;
    context.beginPath();
    context.moveTo(cursor, first - 3);
    context.lineTo(cursor, first + rowHeight * pars + 3);
    context.stroke();

    context.restore();
    context.save();
    context.fillStyle = 'rgba(226,150,172,0.8)';
    context.font = '10px ui-monospace, monospace';
    const cycles = Math.round(pass.turns * 100) / 100;
    context.fillText(cycles === 1 ? 'a pass every cycle' : `a pass every ${cycles} cycles`, left, first - 7);
    context.restore();
  }

  function drawCenters(context, centers, order, flipped, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT) {
    const { PIXELS } = api;
    const SEAM_PIXELS = 0.5;
    const xAt = column => columnWidth * (column + 1);
    const yAt = pixel => WALL_TOP + (flipped ? pixel : PIXELS - pixel) * WALL_HEIGHT / PIXELS;
    const nearest = (pixels, to) =>
      pixels.reduce((best, pixel) => (Math.abs(pixel - to) < Math.abs(best - to) ? pixel : best));

    const onWall = pixel => pixel >= -SEAM_PIXELS && pixel <= PIXELS + SEAM_PIXELS;
    const chainFrom = first => order.reduce((chain, strip, column) => {
      const center = centers[strip];
      const pixel = column === 0 ? first : nearest([center - PIXELS, center, center + PIXELS], chain[column - 1].pixel);
      return [...chain, { x: xAt(column), pixel }];
    }, []);
    const firstCenter = centers[order[0]];
    const points = [firstCenter, firstCenter + PIXELS, firstCenter - PIXELS]
      .map(chainFrom)
      .reduce((best, chain) =>
        (chain.filter(point => onWall(point.pixel)).length > best.filter(point => onWall(point.pixel)).length ? chain : best));

    context.save();
    context.beginPath();
    context.rect(0, WALL_TOP, xAt(order.length), WALL_HEIGHT);
    context.clip();
    context.beginPath();
    points.forEach(({ x, pixel }, column) => {
      if (column === 0) context.moveTo(x, yAt(pixel)); else context.lineTo(x, yAt(pixel));
    });
    strokeOutlined(context, SHAPE_MARK_SOFT, 1.5);
    context.restore();

    context.save();
    for (const { x, pixel } of points) {
      for (const seen of [pixel - PIXELS, pixel, pixel + PIXELS]) {
        if (!onWall(seen)) continue;
        const y = yAt(Math.min(PIXELS, Math.max(0, seen)));
        context.strokeStyle = 'rgba(10,11,14,0.9)';
        context.lineWidth = 4;
        context.beginPath();
        context.moveTo(x - stripWidth * 0.75, y);
        context.lineTo(x + stripWidth * 0.75, y);
        context.stroke();
        context.strokeStyle = SHAPE_MARK;
        context.lineWidth = 2;
        context.stroke();
      }
    }
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
      context.strokeStyle = SHAPE_MARK_SOFT;
      context.beginPath();
      context.moveTo(xAt(-0.4), y);
      context.lineTo(xAt(STRIPS - 0.6), y);
      context.stroke();
      context.fillStyle = SHAPE_MARK;
      context.font = '9px ui-monospace, monospace';
      context.fillText('still', xAt(-0.4) + 2, y - 3);
    }
    context.setLineDash([]);

    if (runs) {
      const columnOf = index => (step === 1 ? index - order[0] : order[0] - index);
      context.beginPath();
      fan.curve.forEach(([index, value], i) => {
        const x = xAt(columnOf(index));
        const y = middle - value * reach;
        if (i === 0) context.moveTo(x, y); else context.lineTo(x, y);
      });
      strokeOutlined(context, SHAPE_MARK_SOFT, 1.5);
    }

    for (let column = 0; column < STRIPS; column++) {
      const value = fan.values[order[column]];
      context.beginPath();
      context.arc(xAt(column), middle - value * reach, 3.2, 0, Math.PI * 2);
      context.fillStyle = SHAPE_MARK;
      context.fill();
      context.strokeStyle = OUTLINE;
      context.lineWidth = 1.5;
      context.stroke();
    }

    context.fillStyle = SHAPE_MARK;
    context.font = '10px ui-monospace, monospace';
    context.shadowColor = OUTLINE;
    context.shadowBlur = 3;
    const spent = fan.spent.length
      ? fan.spent.map(([where, amount]) =>
          where + ' ' + (amount > 0 ? '+' : '−') + Math.round(Math.abs(amount) * 100) + '%').join('  ')
      : 'spent nowhere';
    context.fillText(spent, 8, WALL_TOP + 12);
    if (fan.scrambled > 0.005) {
      context.fillText(Math.round(fan.scrambled * 100) + '% random', 8, WALL_TOP + 24);
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

    context.beginPath();
    bend.forEach((value, i) => {
      const x = axis + (value - middle) / middle * reach;
      if (i === 0) context.moveTo(x, yAt(i)); else context.lineTo(x, yAt(i));
    });
    strokeOutlined(context, SHAPE_MARK, 1.5);

    if (high - low < 0.01) {
      context.restore();
      return;
    }
    context.fillStyle = SHAPE_MARK;
    context.font = '10px ui-monospace, monospace';
    context.textAlign = 'right';
    context.shadowColor = OUTLINE;
    context.shadowBlur = 3;
    context.fillText(`travel ×${low.toFixed(low < 1 ? 2 : 1)}–${high.toFixed(1)}`, columnWidth * (STRIPS + 1) - 6, WALL_TOP + 12);
    context.restore();
  }

  global.AuroraPreview = api;
})(window);
