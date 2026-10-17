import { Protocol } from './cc.js';
import AuroraRenderModule from './render.js';

const WALL_STRIP_ORDER = [5, 4, 3, 2, 1];
const WALL_PAR_ORDER = [4, 3, 2, 1];
const PAR_BAND_SHARE = 0.2;

const SCREEN_LEVEL_OF_STRIP_BYTE = Array.from({ length: 256 }, (_, byte) => {
  const light = byte / 255;
  const encoded = light <= 0.0031308 ? 12.92 * light : 1.055 * Math.pow(light, 1 / 2.4) - 0.055;
  return Math.round(255 * encoded);
});

const api = {
  WALL_STRIP_ORDER,
  WALL_PAR_ORDER,
  PAR_BAND_SHARE,
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

  function seenPars() {
    return Array.from({ length: PARS }, (_, par) => {
      const at = parColors + par * 4;
      return [0, 1, 2].map(i => Math.round(renderer.HEAPU8[at + i] * renderer.HEAPU8[at + 3] / 255));
    });
  }

  const PASS_MARKS = renderer._aurora_arp_pass_marks();
  const STRIP_MARKS_SIZE = renderer._aurora_strip_marks_size();
  const BEND_POINTS = renderer._aurora_bend_points();
  const curveAt = renderer._aurora_curve() >> 2;
  const curve = renderer.HEAPF32.subarray(curveAt, curveAt + STRIPS * BEND_POINTS);
  const curveAcrossAt = renderer._aurora_curve_across() >> 2;
  const curveAcross = renderer.HEAPF32.subarray(curveAcrossAt, curveAcrossAt + renderer._aurora_field_across_points());
  const fieldLevelsAt = renderer._aurora_field_levels() >> 2;
  const fieldLevels = renderer.HEAPF32.subarray(fieldLevelsAt, fieldLevelsAt + STRIPS * PIXELS);
  const fieldAcrossAt = renderer._aurora_field_across() >> 2;
  const fieldAcross = renderer.HEAPF32.subarray(fieldAcrossAt, fieldAcrossAt + renderer._aurora_field_across_points());
  const MARK_FLOATS = 3;

  function readMarks(at) {
    return Array.from({ length: STRIPS }, (_, strip) => {
      const base = at + strip * STRIP_MARKS_SIZE;
      const count = renderer.HEAPU8[base];
      const floats = renderer.HEAPF32.subarray((base + 4) >> 2, ((base + 4) >> 2) + count * MARK_FLOATS);
      return Array.from({ length: count }, (_, i) => ({
        center: floats[i * MARK_FLOATS], width: floats[i * MARK_FLOATS + 1], life: floats[i * MARK_FLOATS + 2],
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

  const morphed = at => Array.from(renderer.HEAPU8.subarray(at, at + Protocol.PATCH_CC_COUNT));

  function readFan() {
    const after = STRIPS + CURVE_POINTS;
    const stepsPerStrip = (CURVE_POINTS - 1) / (STRIPS - 1);
    return {
      values: Array.from(fan.subarray(0, STRIPS)),
      curve: Array.from(fan.subarray(STRIPS, after), (value, i) => [i / stepsPerStrip, value]),
      stillAt: Math.abs(fan[after + 1]) <= 1 ? fan[after + 1] : null,
      used: [fan[after + 2], fan[after + 3], fan[after + 4]].some(amount => Math.abs(amount) > 0.005),
    };
  }

  function stepsOf(reader) {
    const outputs = Array.from({ length: 128 }, (_, value) => reader(value));
    const starts = outputs.flatMap((output, value) => (value === 0 || output !== outputs[value - 1] ? [value] : []));
    const last = starts.length - 1;
    return {
      count: starts.length,
      stepOf: value => starts.filter(start => start <= value).length - 1,
      valueOf: step => (step <= 0 ? 0 : step >= last ? 127 : Math.round((starts[step] + starts[step + 1] - 1) / 2)),
      outputOf: step => outputs[starts[step]],
    };
  }

  const steps = {
    lfoPeriod: stepsOf(value => renderer._aurora_lfo_period_beats(value)),
    transition: stepsOf(value => renderer._aurora_transition_length(value)),
    ratio: stepsOf(value => renderer._aurora_ratio_step(value)),
    arpMode: stepsOf(value => renderer._aurora_arp_mode_of(value)),
    hueLayout: stepsOf(value => renderer._aurora_hue_layout_of(value)),
    threeWay: stepsOf(value => renderer._aurora_three_way(value)),
    fourWay: stepsOf(value => renderer._aurora_four_way(value)),
  };

  Object.assign(Protocol, {
    steps,
    signedOf: value => renderer._aurora_signed(value),
    lfoPeriodBeats: value => renderer._aurora_lfo_period_beats(value),
    transitionBeats: value => renderer._aurora_transition_length(value),
    routeRatio: value => renderer._aurora_ratio(value),
    routeOnce: value => !!renderer._aurora_once(value),
    routeSource: value => renderer._aurora_source(value),
    routeSourceValue: source => steps.ratio.valueOf(2 * Protocol.ROUTE_MAX_RATIO + source - 1),
    routeRatioValue: (ratio, once) => steps.ratio.valueOf((once ? Protocol.ROUTE_MAX_RATIO : 0) + ratio - 1),
    routePhaseStep: value => renderer._aurora_phase_step(value),
    arpMode: value => renderer._aurora_arp_mode_of(value),
    arpModeValue: mode => steps.arpMode.valueOf(mode),
    hueLayout: value => renderer._aurora_hue_layout_of(value),
    hueLayoutValue: layout => steps.hueLayout.valueOf(layout),
    isOn: value => !!renderer._aurora_switch_on(value),
    routeHeld: value => !!renderer._aurora_route_held(value),
    threeWayPosition: value => renderer._aurora_three_way(value),
    threeWayValue: position => steps.threeWay.valueOf(position),
    fourWayPosition: value => renderer._aurora_four_way(value),
    fourWayValue: position => steps.fourWay.valueOf(position),
    routeArp: destination => renderer._aurora_destination_arp(destination),
    routeBipolar: destination => !!renderer._aurora_destination_bipolar(destination),
    routeTarget: destination => renderer._aurora_destination_target(destination),
    routeDestination: (target, arp, bipolar) => renderer._aurora_destination(target, arp, bipolar ? 1 : 0),
  });

  const PATCH_BYTES = 4 + Protocol.PATCH_LAYERS * Protocol.PATCH_CC_COUNT;
  const NO_ONESHOT = 127;
  const drawnAt = renderer._aurora_drawn_controls();

  function writePatch(patch) {
    const at = renderer._aurora_patch_buffer();
    const head = [patch.transitionTime, patch.accentTime,
      ...patch.oneshots.map(pick => (pick === null ? NO_ONESHOT : pick))];
    renderer.HEAPU8.set(head, at);
    patch.layers.forEach((bytes, layer) => renderer.HEAPU8.set(bytes, at + 4 + layer * bytes.length));
  }

  function readFrame() {
    const drawn = renderer.HEAPU8.subarray(drawnAt, drawnAt + Protocol.PATCH_CC_COUNT);
    return {
      pixels, pars: seenPars(), fan: readFan(), bend: Array.from(bend), arp: readArpPass(), lfo: renderer._aurora_lfo(),
      spots: readMarks(renderer._aurora_spots()), cells: readMarks(renderer._aurora_cells()),
      field: {
        horizontal: Protocol.threeWayPosition(drawn[Protocol.CC.fieldDirection])
          === Protocol.FIELD_DIRECTION.horizontal,
        levels: Array.from(fieldLevels),
        across: Array.from(fieldAcross),
      },
      curve: renderer._aurora_curve_shown() && {
        acrossWall: !!renderer._aurora_curve_across_wall(),
        strips: Array.from({ length: STRIPS }, (_, strip) =>
          Array.from(curve.subarray(strip * BEND_POINTS, (strip + 1) * BEND_POINTS))),
        across: Array.from(curveAcross),
      },
      hues: {
        palette: drawn[Protocol.CC.palette],
        strips: renderer._aurora_strips_hue(),
        pars: Array.from(parHues),
      },
      drawn: Array.from(drawn),
    };
  }

  Object.assign(api, {
    STRIPS, PIXELS, PARS, PATCH_BYTES,
    paletteNames: () => Array.from({ length: renderer._aurora_palette_count() }, (_, i) => {
      const at = renderer._aurora_palette_name(i);
      return String.fromCharCode(...renderer.HEAPU8.subarray(at, renderer.HEAPU8.indexOf(0, at)));
    }),
    paletteColor(palette, hue, saturation) {
      const packed = renderer._aurora_palette_color(palette, hue, saturation);
      return [packed >> 16 & 255, packed >> 8 & 255, packed & 255];
    },
    lfoWave: (phase, wave, smooth) => renderer._aurora_lfo_wave(phase, wave, smooth),
    waveMean: (wave, smooth) => renderer._aurora_wave_mean(wave, smooth),
    convert: (cc, value) => renderer._aurora_convert(cc, value),
    fanWaveAt: (phase, frequency, strip) => renderer._aurora_fan_wave_at(phase, frequency, strip),
    arpPassLength: mode => renderer._aurora_arp_pass_length(mode),
    arpTurnsPerPass: (mode, spread) => renderer._aurora_arp_turns_per_pass(mode, spread),
    arpReversed: (mode, spread) => !!renderer._aurora_arp_reversed(mode, spread),
    routeTurns: (ratio, phase, clock) => renderer._aurora_route_turns(ratio, phase, clock),
    bendSpeedRatio: bend => renderer._aurora_bend_speed_ratio(bend),

    controlAtStrips: cc => Array.from({ length: STRIPS }, (_, i) => renderer._aurora_control_at_strip(cc, i)),
    parHuePlaces: () => Array.from(huePlaces),
    controlAtPars: cc => Array.from({ length: PARS }, (_, i) => renderer._aurora_control_at_par(cc, i)),
    routeRefused: cc => !!renderer._aurora_route_refused(cc),
    cellControl: cc => !!renderer._aurora_cell_control(cc),
    envelopeLevel: (stages, gate, end, rate, age) =>
      renderer._aurora_envelope_level(stages.attack, stages.hold, stages.decay, stages.sustain, stages.release,
                                      stages.moves, gate, end, rate, age),
    spotDestination: cc => !!renderer._aurora_spot_destination(cc),
    blend(fromBytes, toBytes, position, switchBytes, switchesFromStart) {
      renderer.HEAPU8.set(fromBytes, renderer._aurora_morph_from());
      renderer.HEAPU8.set(toBytes, renderer._aurora_morph_to());
      renderer.HEAPU8.set(switchBytes, renderer._aurora_morph_switches());
      return morphed(renderer._aurora_blend(position, switchesFromStart ? 1 : 0));
    },
    routeReach(cc) {
      const at = renderer._aurora_route_reach(cc);
      return at ? [renderer.HEAPF32[at >> 2], renderer.HEAPF32[(at >> 2) + 1]] : null;
    },

    playback: {
      begin: micros => renderer._aurora_playback_begin(micros),
      storePatch(slot, patch) {
        writePatch(patch);
        renderer._aurora_library_store_patch(slot);
      },
      clearPatch: slot => renderer._aurora_library_clear_patch(slot),
      storeOneshot(index, oneshot) {
        const at = renderer._aurora_oneshot_buffer();
        renderer.HEAPU8[at] = oneshot.length;
        renderer.HEAPU8.set(oneshot.marks, at + 1);
        renderer.HEAPU8.set(oneshot.controls, at + 1 + oneshot.marks.length);
        renderer._aurora_library_store_oneshot(index);
      },
      clearOneshot: index => renderer._aurora_library_clear_oneshot(index),
      defaultOneshots: picks => renderer._aurora_library_default_oneshots(
        ...picks.map(pick => (pick === null ? NO_ONESHOT : pick))),
      message: (status, first, second, micros) => renderer._aurora_playback_message(status, first, second, micros),
      cut(slot, patch) {
        writePatch(patch);
        renderer._aurora_playback_cut(slot);
      },
      patchChanged: slot => renderer._aurora_playback_patch_changed(slot),
      pin(bytes) {
        renderer.HEAPU8.set(bytes, controls);
        renderer._aurora_playback_pin();
      },
      unpin: () => renderer._aurora_playback_unpin(),
      frame(micros) {
        renderer._aurora_playback_frame(micros);
        return readFrame();
      },
      beats: () => renderer._aurora_playback_beats(),
      slot: () => renderer._aurora_playback_slot(),
      oneshotProgress: () => renderer._aurora_playback_oneshot_progress(),
      oneshotIndex: () => {
        const index = renderer._aurora_playback_oneshot_index();
        return index === NO_ONESHOT ? null : index;
      },
    },
    draw,
  });
}

function draw(context, glow, frame, order, parOrder, width, height, overlays) {
  const { STRIPS, PIXELS } = api;
  const PAR_BAND = height * PAR_BAND_SHARE;
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
      const row = PIXELS - 1 - pixelIndex;
      glowContext.fillStyle = `rgb(${SCREEN_LEVEL_OF_STRIP_BYTE[red]},${SCREEN_LEVEL_OF_STRIP_BYTE[green]},${SCREEN_LEVEL_OF_STRIP_BYTE[blue]})`;
      glowContext.fillRect(x, WALL_TOP + row * pitch, stripWidth, pixelHeight);
    }
  }

  const parY = height - PAR_BAND / 2;
  for (let column = 0; column < frame.pars.length; column++) {
    const par = frame.pars[parOrder[column]];
    const x = width * (column + 0.5) / frame.pars.length;
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

  if (overlays.fan && frame.fan) drawFan(context, frame.fan, order, columnWidth, WALL_TOP, WALL_HEIGHT);
  if (overlays.bend && frame.bend) drawBend(context, frame.bend, columnWidth, WALL_TOP, WALL_HEIGHT);
  if (overlays.arp && frame.arp) drawArpPass(context, frame.arp, frame.pars.length, width, height - PAR_BAND, PAR_BAND);
  if (overlays.palette && frame.hues) drawPalette(context, frame.hues, width, height);
  if (overlays.field && frame.field) {
    if (frame.field.horizontal) {
      drawAcross(context, frame.field.across, Array.from({ length: api.STRIPS }, (_, strip) => frame.field.levels[strip * api.PIXELS]),
                 FIELD_MARK, FIELD_BASE, order, columnWidth, WALL_TOP, WALL_HEIGHT);
    }
    else drawField(context, frame.field.levels, order, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT);
  }
  if (overlays.curve && frame.curve) {
    if (frame.curve.acrossWall) {
      drawAcross(context, frame.curve.across, frame.curve.strips.map(levels => levels[0]), FORM_MARK, CURVE_BASE, order, columnWidth, WALL_TOP, WALL_HEIGHT);
    } else {
      drawCurve(context, frame.curve.strips, order, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT);
    }
  }
  if (overlays.spots && frame.spots) drawMarks(context, frame.spots, SPOT_MARK, 1, order, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT);
  if (overlays.cells && frame.cells) drawMarks(context, frame.cells, FORM_MARK, -1, order, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT);
}

const PALETTE_MARK = 'rgba(176,162,236,0.95)';
const FORM_MARK = 'rgba(240,206,100,0.95)';
const TRAVEL_MARK = 'rgba(240,150,80,0.95)';
const FAN_MARK = 'rgba(236,96,84,0.95)';
const FAN_MARK_SOFT = 'rgba(236,96,84,0.6)';
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
  const top = height - 20;
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

const CURVE_REACH = 7;

function drawCurve(context, curves, order, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT) {
  const { PIXELS } = api;
  const yAt = point => WALL_TOP + (PIXELS - point) * WALL_HEIGHT / PIXELS;
  context.save();
  order.forEach((strip, column) => {
    const axis = columnWidth * (column + 1) + stripWidth / 2 + 6;
    context.strokeStyle = 'rgba(255,255,255,0.12)';
    context.lineWidth = 1;
    context.beginPath();
    context.moveTo(axis, yAt(0));
    context.lineTo(axis, yAt(PIXELS));
    context.stroke();
    context.beginPath();
    curves[strip].forEach((level, point) => {
      const x = axis + level * CURVE_REACH;
      if (point === 0) context.moveTo(x, yAt(point)); else context.lineTo(x, yAt(point));
    });
    strokeOutlined(context, FORM_MARK, 1.5);
  });
  context.restore();
}

function drawField(context, levels, order, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT) {
  const { PIXELS } = api;
  const yAt = pixel => WALL_TOP + (PIXELS - pixel - 0.5) * WALL_HEIGHT / PIXELS;
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

const FIELD_BASE = 0.88;
const CURVE_BASE = 0.76;

function drawAcross(context, line, dots, style, baseShare, order, columnWidth, WALL_TOP, WALL_HEIGHT) {
  const { STRIPS } = api;
  const step = order[1] - order[0];
  const monotonic = (step === 1 || step === -1) && order.every((strip, i) => i === 0 || strip - order[i - 1] === step);
  const xAtStrip = strip => columnWidth * (1 + (step === 1 ? strip - order[0] : order[0] - strip));
  const base = WALL_TOP + WALL_HEIGHT * baseShare;
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
    line.forEach((level, i) => {
      const x = xAtStrip(i / (line.length - 1) * (STRIPS - 1));
      if (i === 0) context.moveTo(x, base - level * reach); else context.lineTo(x, base - level * reach);
    });
    strokeOutlined(context, style, 1.5);
  }
  order.forEach((strip, column) => {
    context.beginPath();
    context.arc(columnWidth * (column + 1), base - dots[strip] * reach, 3.2, 0, Math.PI * 2);
    context.fillStyle = style;
    context.fill();
    context.strokeStyle = OUTLINE;
    context.lineWidth = 1.5;
    context.stroke();
  });
  context.restore();
}

function drawMarks(context, marks, style, side, order, columnWidth, stripWidth, WALL_TOP, WALL_HEIGHT) {
  const { PIXELS } = api;
  const yAt = pixel => WALL_TOP + (PIXELS - pixel) * WALL_HEIGHT / PIXELS;
  context.save();
  context.beginPath();
  context.rect(0, WALL_TOP, columnWidth * (order.length + 1), WALL_HEIGHT);
  context.clip();
  order.forEach((strip, column) => {
    const x = columnWidth * (column + 1);
    const bracketX = x + side * (stripWidth / 2 + 3);
    for (const { center, width, life } of marks[strip]) {
      const left = 1 - life;
      const top = yAt(center + width / 2), bottom = yAt(center - width / 2);
      context.globalAlpha = 0.35 + 0.6 * left;
      context.strokeStyle = style;
      context.lineWidth = 1.5;
      context.beginPath();
      context.moveTo(bracketX - side * 3, top);
      context.lineTo(bracketX, top);
      context.lineTo(bracketX, bottom);
      context.lineTo(bracketX - side * 3, bottom);
      context.stroke();
      const y = yAt(center);
      context.beginPath();
      context.moveTo(x - stripWidth / 2 * left, y);
      context.lineTo(x + stripWidth / 2 * left, y);
      strokeOutlined(context, style, 1.5);
    }
  });
  context.restore();
}

function drawArpPass(context, pass, pars, width, top, bandHeight) {
  const left = width * 0.04;
  const span = width * 0.92;
  const rowHeight = bandHeight * 0.5 / pars;
  const first = top + bandHeight * 0.1;
  const xAt = turns => left + span * turns / pass.turns;

  context.save();
  context.fillStyle = 'rgba(10,11,14,0.25)';
  context.fillRect(left - 4, first - 4, span + 8, rowHeight * pars + 8);
  context.beginPath();
  context.rect(left, first - 4, span, rowHeight * pars + 8);
  context.clip();

  for (const { start, par } of pass.marks) {
    const y = first + par * rowHeight;
    const tail = context.createLinearGradient(xAt(start), 0, xAt(start + pass.length), 0);
    tail.addColorStop(0, 'rgba(220,120,222,0.4)');
    tail.addColorStop(1, 'rgba(220,120,222,0)');
    context.fillStyle = tail;
    context.fillRect(xAt(start), y + 1, xAt(start + pass.length) - xAt(start), rowHeight - 2);
    context.save();
    context.translate(-span, 0);
    context.fillRect(xAt(start), y + 1, xAt(start + pass.length) - xAt(start), rowHeight - 2);
    context.restore();
    context.fillStyle = 'rgba(220,120,222,0.95)';
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
}

function drawFan(context, fan, order, columnWidth, WALL_TOP, WALL_HEIGHT) {
  const { STRIPS } = api;
  const middle = WALL_TOP + WALL_HEIGHT / 2;

  const reach = WALL_HEIGHT * 0.3;
  const xAt = column => columnWidth * (column + 1);

  const step = order[1] - order[0];
  const runs = (step === 1 || step === -1)
    && order.every((strip, i) => i === 0 || strip - order[i - 1] === step);

  context.save();
  if (!fan.used) context.globalAlpha = 0.3;
  context.lineWidth = 1;
  context.strokeStyle = 'rgba(255,255,255,0.14)';
  context.setLineDash([3, 4]);
  context.beginPath();
  context.moveTo(xAt(-0.4), middle);
  context.lineTo(xAt(STRIPS - 0.6), middle);
  context.stroke();

  if (fan.stillAt !== null) {
    const y = middle - fan.stillAt * reach;
    context.strokeStyle = FAN_MARK_SOFT;
    context.beginPath();
    context.moveTo(xAt(-0.4), y);
    context.lineTo(xAt(STRIPS - 0.6), y);
    context.stroke();
    context.fillStyle = FAN_MARK;
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
    strokeOutlined(context, FAN_MARK_SOFT, 1.5);
  }

  for (let column = 0; column < STRIPS; column++) {
    const value = fan.values[order[column]];
    context.beginPath();
    context.arc(xAt(column), middle - value * reach, 3.2, 0, Math.PI * 2);
    context.fillStyle = FAN_MARK;
    context.fill();
    context.strokeStyle = OUTLINE;
    context.lineWidth = 1.5;
    context.stroke();
  }

  context.restore();
}

function drawBend(context, bend, columnWidth, WALL_TOP, WALL_HEIGHT) {
  const { STRIPS, PIXELS } = api;
  const axis = columnWidth * (STRIPS + 0.5);
  const reach = columnWidth * 0.4;
  const low = Math.min(...bend), high = Math.max(...bend);
  const middle = (low + high) / 2;
  const yAt = i => WALL_TOP + (PIXELS - i) * WALL_HEIGHT / PIXELS;

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
  strokeOutlined(context, TRAVEL_MARK, 1.5);
  context.restore();
}

connect(await AuroraRenderModule());

export { api as Preview };
