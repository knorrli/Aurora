import { Protocol } from './cc.js';
import { Preview } from './preview.js';


const LAYER_NAMES = ['Base', 'Color', 'Motion', 'Extent', 'Accent'];
const LAYERS_ABOVE_BASE = [Protocol.PATCH_LAYER_COLOR, Protocol.PATCH_LAYER_MOTION,
                 Protocol.PATCH_LAYER_EXTENT, Protocol.PATCH_LAYER_ACCENT];
const LAYERS = [Protocol.PATCH_LAYER_BASE, ...LAYERS_ABOVE_BASE];
const isAboveBase = layer => layer !== Protocol.PATCH_LAYER_BASE;

const CC = Object.assign({}, Protocol.CC);

const ROUTE_FIELDS = Object.keys(Protocol.ROUTE_FIELD);
const ROUTE_CONTROL_ORDER = ['amount', 'ratio', 'wave', 'smooth', 'phase', 'hold'];
const capitalized = word => word[0].toUpperCase() + word.slice(1);
const routeName = (route, field) => `route${route + 1}${capitalized(field)}`;

const ROUTES = Array.from({ length: Protocol.ROUTES }, (_, route) => {
  const names = {};
  for (const field of ROUTE_FIELDS) {
    names[field] = routeName(route, field);
    CC[names[field]] = Protocol.routeCC(route, Protocol.ROUTE_FIELD[field]);
  }
  return Object.assign({
    name: 'Route ' + (route + 1),
    fields: ROUTE_FIELDS.map(field => names[field]),
    controls: ROUTE_CONTROL_ORDER.map(field => names[field]),
  }, names);
});
const ROUTE_FIELD_NAMES = ROUTES.flatMap(route => route.fields);
const routeOf = name => ROUTES.find(route => route.fields.includes(name));

const NAMES = Protocol.tagged('patch').concat(Protocol.tagged('switch'), ROUTE_FIELD_NAMES)
  .sort((a, b) => CC[a] - CC[b]);
const SWITCHES = new Set(Protocol.tagged('switch')
  .concat(ROUTES.flatMap(route => [route.destination, route.hold, route.polarity])));
const isSwitch = name => SWITCHES.has(name);
const CONTINUOUS = NAMES.filter(name => !isSwitch(name));

const clampToSevenBits = value => (value < 0 ? 0 : value > 127 ? 127 : value | 0);

class Controls {
  constructor(bytes) { this.bytes = bytes; }
  copy() { return new Controls(this.bytes.slice()); }
}
for (const name of Object.keys(CC)) {
  Object.defineProperty(Controls.prototype, name, {
    get() { return this.bytes[CC[name]] | 0; },
    set(value) { this.bytes[CC[name]] = clampToSevenBits(value); },
  });
}
const controls = bytes => new Controls(bytes);

const real = (name, value) => Preview.convert(CC[name], value);

const percent = ratio => (ratio * 100).toFixed(0) + '%';
const sign = ratio => (ratio < 0 ? '−' : '+');
const signed = ratio => sign(ratio) + percent(Math.abs(ratio));

const degrees = hueSteps => Math.round(hueSteps * 360 / 256) + '°';
const signedDegrees = hueSteps => sign(hueSteps) + degrees(Math.abs(hueSteps));
const hueReach = name => value => signedDegrees(Math.round(real(name, value)));
const pixels = count => (count < 10 ? Number(count.toFixed(1)) : count.toFixed(0)) + ' px';
const counted = (count, one, many) => {
  const rounded = Math.round(count * 10) / 10;
  return rounded + ' ' + (rounded === 1 ? one : many);
};
const beatsText = beats => {
  const rounded = Math.round(beats * 100) / 100;
  return rounded + (rounded === 1 ? ' beat' : ' beats');
};

const PERIOD_NAMES = {
  0.0625: '1⁄16 beat', 0.125: '⅛ beat',
  16: '16 beats', 12: '12 beats', 8: '8 beats',
  6: '6 beats', 4: '4 beats', 3: '3 beats', 2: '2 beats',
  1.5: '1½ beats', 1: '1 beat', 0.75: '¾ beat', 0.5: '½ beat', 0.375: '⅜ beat', 0.25: '¼ beat',
};
const LFO_PERIOD_NAMES = Protocol.LFO_PERIODS.map(beats => PERIOD_NAMES[beats]);
const lapName = lapsPerBeat => PERIOD_NAMES[Math.round(1000 / Math.abs(lapsPerBeat)) / 1000];
const periodStep = value => Protocol.steps.lfoPeriod.stepOf(value);
const periodValue = step => Protocol.steps.lfoPeriod.valueOf(step);
const TRANSITION_NAMES = Array.from({ length: Protocol.steps.transition.count }, (_, step) => {
  const beats = Protocol.steps.transition.outputOf(step);
  return beats === 0 ? 'none' : PERIOD_NAMES[beats];
});
const transitionStep = value => Protocol.steps.transition.stepOf(value);
const transitionValue = step => Protocol.steps.transition.valueOf(step);

const TEMPO_DIVISION_NAMES = {
  bar: 'bar', half: 'half', quarter: 'quarter',
  eighth: 'eighth', eighthTriplet: 'eighth triplet', sixteenth: 'sixteenth',
};
const TEMPO_DIVISIONS = Object.entries(TEMPO_DIVISION_NAMES)
  .map(([key, name]) => [Protocol.TEMPO_DIVISION[key], name]);
const PULSE_UNITS = {
  [Protocol.TEMPO_DIVISION.bar]: ['bar', 'bars'],
  [Protocol.TEMPO_DIVISION.half]: ['half', 'halves'],
  [Protocol.TEMPO_DIVISION.quarter]: ['beat', 'beats'],
  [Protocol.TEMPO_DIVISION.eighth]: ['eighth', 'eighths'],
  [Protocol.TEMPO_DIVISION.eighthTriplet]: ['triplet', 'triplets'],
  [Protocol.TEMPO_DIVISION.sixteenth]: ['sixteenth', 'sixteenths'],
};
const inPulses = (text, live) => {
  const [one, many] = PULSE_UNITS[live ? live.tempoDivision : Protocol.TEMPO_DIVISION.quarter];
  return text.replace(/\bbeats\b/g, many).replace(/\bbeat\b/g, one);
};

const WAVE_NAMES = [
  [0, 'rise'], [Protocol.WAVE_SWELL, 'swell'], [Protocol.WAVE_FALL, 'fall'], [Protocol.WAVE_SQUARE, 'square'], [127, 'stab'],
];
const waveText = value => {
  const next = WAVE_NAMES.findIndex(([at]) => at >= value);
  const [at, name] = WAVE_NAMES[next];
  return at === value ? name : `${WAVE_NAMES[next - 1][1]} → ${name}`;
};

const perSpot = destination => Preview.spotDestination(Protocol.routeTarget(destination));

const cycleBeats = (live, route, ratioValue, oneshotBeats) => {
  const settings = live || DEFAULT;
  const perCycle = perSpot(settings[route.destination]) ? 1 / real('scatterRate', settings.scatterRate)
    : oneshotBeats || real('lfoRate', settings.lfoRate);
  return perCycle / Protocol.routeRatio(ratioValue);
};

const phaseText = (value, live, route, oneshotBeats) => {
  const step = Protocol.routePhaseStep(value);
  if (step === 0) return '0°';
  const degrees = Math.round(step * 360 / Protocol.ROUTE_PHASE_STEPS);
  const cycle = cycleBeats(live, route, (live || DEFAULT)[route.ratio], oneshotBeats);
  return `${degrees}° · ${beatsText(step / Protocol.ROUTE_PHASE_STEPS * cycle)}`;
};

const ROUTE_READOUTS = {
  amount: () => value => signed(Protocol.signedOf(value)),
  ratio: route => (value, live, oneshotBeats) =>
    `×${Protocol.routeRatio(value)} · ${beatsText(cycleBeats(live, route, value, oneshotBeats))}`,
  phase: route => (value, live, oneshotBeats) => phaseText(value, live, route, oneshotBeats),
  wave: () => waveText,
  smooth: () => value => (value === 0 ? 'straight' : percent(value / 127) + ' smooth'),
};

const isGradient = live => Protocol.threeWayPosition(live.fieldForm) === Protocol.FIELD_FORM.gradient;

const fieldSize = (fraction, live) => {
  const settings = live || DEFAULT;
  const count = real('fieldCount', settings.fieldCount);
  const direction = Protocol.fourWayPosition(settings.fieldDirection);
  if (direction === Protocol.DIRECTION.horizontal) {
    return (fraction * (Preview.STRIPS - 1) / count).toFixed(1) + ' strips';
  }
  if (direction === Protocol.DIRECTION.cell) return percent(fraction / count) + ' of cell';
  return pixels(fraction * Preview.PIXELS / count);
};

const upTheStrip = at =>
  (at < 0.005 ? 'at the bottom' : at > 0.995 ? 'at the top' : Math.round(at * 100) + '% up');
const backTheCell = at =>
  (at < 0.005 ? 'at the front' : at > 0.995 ? 'at the back' : Math.round(at * 100) + '% back');
const acrossTheWall = at => Math.round(at * 100) + '% across';
const beatsOrNone = (beats, none) => (beats === 0 ? none : PERIOD_NAMES[beats]);
const curveDirectionOf = live => Protocol.fourWayPosition((live || DEFAULT).cellDirection);

const DIRECTION_NAMES = { vertical: 'vertical', horizontal: 'horizontal', cell: 'cell', cellPosition: 'cell position' };
const directionSwitch = {
  kind: 'four',
  options: Object.entries(Protocol.DIRECTION).map(([key, position]) => [Protocol.fourWayValue(position), DIRECTION_NAMES[key]]),
};

const ENVELOPE_LABELS = { moves: 'Moves', attack: 'Attack', hold: 'Hold', decay: 'Decay', sustain: 'Sustain', release: 'Release' };
const envelopeName = (envelope, field) => `envelope${envelope + 1}${capitalized(field)}`;
const ENVELOPES = Array.from({ length: Protocol.ENVELOPES }, (_, envelope) => Object.fromEntries(
  ['moves', ...Object.keys(Protocol.ENVELOPE_STAGE)].map(field => [field, envelopeName(envelope, field)])));
const envelopeStages = (live, envelope) => Object.fromEntries(
  Object.entries(ENVELOPES[envelope]).map(([stage, name]) => [stage, live[name]]));

const READOUTS = {
  cellRate: value => {
    const beats = real('cellRate', value);
    return beats === 0 ? 'once' : 'every ' + PERIOD_NAMES[beats];
  },
  cellChord: value => counted(real('cellChord', value), 'cell', 'cells'),
  cellSpread: (value, live) => {
    const chord = real('cellChord', (live || DEFAULT).cellChord);
    const spread = real('cellSpread', value);
    return chord > 1 ? pixels(spread * Preview.PIXELS / chord) + ' apart' : percent(spread);
  },
  cellGate: value => PERIOD_NAMES[real('cellGate', value)],
  cellWidth: value => pixels(real('cellWidth', value)),
  cellEdge: value => pixels(real('cellEdge', value)) + ' glow',
  cellTail: value => beatsOrNone(real('cellTail', value), 'none'),
  cellPosition: value => upTheStrip(real('cellPosition', value)),
  cellDelay: (value, live) => {
    const steps = real('cellDelay', value);
    if (steps < 0.005) return 'none';
    const settings = live || DEFAULT;
    const rate = real('cellRate', settings.cellRate);
    const step = rate > 0 ? rate : real('lfoRate', settings.lfoRate);
    return `${percent(steps)} · ${beatsText(steps * step)}`;
  },
  cellBend: value => {
    const ratio = Preview.bendSpeedRatio(value);
    if (ratio < 1.01) return 'even';
    return ratio.toFixed(ratio < 10 ? 1 : 0) + '× ' + (real('cellBend', value) > 0 ? 'faster' : 'slower');
  },
  cellBendAt: value => upTheStrip(real('cellBendAt', value)),
  cellBrightness: value => percent(real('cellBrightness', value)),
  cellCurve: value => {
    const curve = real('cellCurve', value);
    if (Math.abs(curve) < 0.005) return 'flat';
    return curve > 0 ? percent(curve) + ' dimmer away' : percent(-curve) + ' dimmer at it';
  },
  cellCurveAt: (value, live) => {
    const at = real('cellCurveAt', value);
    const direction = curveDirectionOf(live);
    if (direction === Protocol.DIRECTION.horizontal) return acrossTheWall(at);
    if (direction === Protocol.DIRECTION.cell) return backTheCell(at);
    return upTheStrip(at);
  },
  hue: value => degrees(real('hue', value)),

  lfoRate: value => PERIOD_NAMES[real('lfoRate', value)],

  scatterRate: value => 'every ' + PERIOD_NAMES[Protocol.lfoPeriodBeats(value)],
  scatterCount: value => real('scatterCount', value).toFixed(1) + ' a strip',
  scatterWidth: (value, live) => pixels(Math.max(1, real('scatterWidth', value) * Preview.PIXELS / real('scatterCount', live.scatterCount))),
  scatterEdge: (value, live) => {
    const settings = live || DEFAULT;
    const reach = Preview.PIXELS / real('scatterCount', settings.scatterCount);
    const core = Math.max(real('scatterWidth', settings.scatterWidth), 1 / reach);
    return pixels(real('scatterEdge', value) * (1 - core) * 0.5 * reach) + ' soft';
  },
  scatterRandomize: value => percent(real('scatterRandomize', value)) + ' random',
  scatterSpeed: value => {
    const speed = real('scatterSpeed', value);
    return sign(speed) + Math.abs(speed).toFixed(1) + ' px/beat';
  },
  scatterPosition: value => {
    const reach = real('scatterPosition', value) * Preview.PIXELS * 0.5;
    return sign(reach) + pixels(Math.abs(reach));
  },
  scatterMix: value => percent(real('scatterMix', value)),
  scatterHue: hueReach('scatterHue'),

  fieldHue: hueReach('fieldHue'),
  fieldWhite: value => percent(real('fieldWhite', value)),
  fieldDark: value => percent(real('fieldDark', value)),
  fieldCount: (value, live) => (isGradient(live || DEFAULT)
    ? counted(real('fieldCount', value), 'ramp', 'ramps')
    : counted(real('fieldCount', value), 'region', 'regions')),
  fieldWidth: (value, live) => fieldSize(real('fieldWidth', value), live),
  fieldPosition: (value, live) => {
    const offset = real('fieldPosition', value);
    if (Math.abs(offset) < 0.005) return 'centered';
    const cells = isGradient(live || DEFAULT) ? 2 * Math.abs(offset) : Math.abs(offset);
    return sign(offset) + fieldSize(cells, live);
  },
  fieldEdge: (value, live) => {
    const settings = live || DEFAULT;
    const width = real('fieldWidth', settings.fieldWidth);
    return fieldSize(real('fieldEdge', value) * (1 - width) * 0.5, settings) + ' soft';
  },
  fieldSpeed: value => {
    const laps = real('fieldSpeed', value);
    return laps === 0 ? 'still' : sign(laps) + lapName(laps);
  },

  flowHue: value => '±' + degrees(Math.abs(Math.round(real('flowHue', value)))),
  flowWhite: value => percent(real('flowWhite', value)),
  flowDark: value => percent(real('flowDark', value)),
  flowRate: value => {
    const rate = real('flowRate', value);
    return rate === 0 ? 'frozen' : 'every ' + PERIOD_NAMES[Math.round(1000 / rate) / 1000];
  },
  flowDensity: value => {
    const cells = real('flowDensity', value);
    return cells <= 0 ? 'whole wall' : pixels(Preview.PIXELS / cells) + ' across';
  },

  coreHue: hueReach('coreHue'),
  coreWhite: value => percent(real('coreWhite', value)),
  coreDark: value => percent(real('coreDark', value)),

  parHueOffset: value => '+' + degrees(real('parHueOffset', value)),
  arpSpread: (value, live) => {
    const mode = (live || DEFAULT).arpMode;
    const pace = `${Math.round(Preview.arpPassLength(mode) / Preview.arpTurnsPerPass(mode, value) * 100) / 100} a cycle`;
    return Preview.arpReversed(mode, value) ? '← ' + pace : pace;
  },
  parHueRange: value => {
    const reach = Math.round(real('parHueRange', value));
    return reach === 0 ? 'one hue' : `±${degrees(Math.abs(reach))} ${reach < 0 ? 'high' : 'low'} first`;
  },
};

for (const route of ROUTES) {
  for (const field of ROUTE_FIELDS) {
    if (ROUTE_READOUTS[field]) READOUTS[route[field]] = ROUTE_READOUTS[field](route);
  }
}

for (const stages of ENVELOPES) {
  for (const [stage, name] of Object.entries(stages)) {
    if (stage === 'moves') continue;
    READOUTS[name] = stage === 'sustain'
      ? value => percent(real(name, value))
      : value => beatsOrNone(real(name, value), stage === 'attack' ? 'instant' : 'none');
  }
}

for (const [name, readout] of Object.entries(READOUTS)) {
  READOUTS[name] = (value, live, oneshotBeats) => inPulses(readout(value, live, oneshotBeats), live);
}

const DEFAULT = controls(new Array(Protocol.PATCH_CC_COUNT).fill(0));
for (const name of NAMES) DEFAULT[name] = Protocol.CONTROL_DEFAULTS[name] || 0;
for (const route of ROUTES) {
  for (const field of ROUTE_FIELDS) DEFAULT[route[field]] = Protocol.ROUTE_DEFAULTS[field];
}

const CONTROLS = {};
const control = (name, label, extra) => {
  CONTROLS[name] = Object.assign({ name, label, kind: 'fader' }, extra || {});
  return name;
};

const OFF = 0, ON = 127;
const threeWayOptions = (positions, labels) =>
  Object.entries(positions).map(([key, position]) => [Protocol.threeWayValue(position), labels[key]]);

const aimsAt = (live, aimed, phase) => ROUTES.filter(route =>
  live[route.destination] === Protocol.aimDestination(ROUTES.indexOf(aimed), phase)
  && live[route.amount] !== Protocol.ROUTE_DEFAULTS.amount);
const sounding = (live, route) => live[route.destination] !== 0
  && (live[route.amount] !== Protocol.ROUTE_DEFAULTS.amount || aimsAt(live, route, false).length > 0);
const soundingRoutes = live => ROUTES.filter(route => sounding(live, route));
const routedTo = (live, names) =>
  soundingRoutes(live).some(route => names.some(name => Protocol.routeTarget(live[route.destination]) === CC[name]));

const nothingTravels = live => !routedTo(live, ['cellPosition']);
const inertWhileStill = { inertWhen: nothingTravels };
const bendIsFlat = live => Math.abs(real('cellBend', live.cellBend)) < 0.005 && !routedTo(live, ['cellBend']);
const endsAs = (live, end) => Protocol.threeWayPosition(live.cellEnd) === Protocol.CELL_END[end];
const curveIsFlat = live => Math.abs(real('cellCurve', live.cellCurve)) < 0.005 && !routedTo(live, ['cellCurve']);
const envelopeUnused = (live, envelope) => !soundingRoutes(live).some(route =>
  Protocol.routeSource(live[route.ratio]) === Protocol.ROUTE_SOURCE[`envelope${envelope + 1}`]);

const ENVELOPE_CARDS = ENVELOPES.map((stages, envelope) => ({
  name: `Envelope ${envelope + 1}`, tone: 'lfo', envelope,
  names: Object.entries(stages).map(([field, name]) => control(name, ENVELOPE_LABELS[field], field === 'moves'
    ? { kind: 'two', options: [[OFF, 'glide'], [ON, 'step']], inertWhen: live => real('cellRate', live.cellRate) === 0 }
    : { inertWhen: live => field === 'release' && !endsAs(live, 'gone') })),
  unused: live => envelopeUnused(live, envelope),
}));

const CELLS = {
  name: 'Cells',
  groups: [
    {
      title: 'Place', tone: 'place',
      names: [
        control('cellEnds', 'Ends', { kind: 'two', options: [[OFF, 'inside'], [ON, 'outside']] }),
        control('cellChord', 'Chord'),
        control('cellSpread', 'Spread', { inertWhen: live => real('cellChord', live.cellChord) === 1 }),
        control('cellPosition', 'Position'),
        control('cellBend', 'Bend'),
        control('cellBendAt', 'Bend at', { inertWhen: bendIsFlat }),
      ],
    },
    {
      title: 'Form', tone: 'cells',
      names: [
        control('cellWidth', 'Width'),
        control('cellEdge', 'Edge'),
        control('cellTail', 'Tail', inertWhileStill),
      ],
    },
    {
      title: 'Life', tone: 'cells',
      names: [
        control('cellEnd', 'End',
          { kind: 'three', options: threeWayOptions(Protocol.CELL_END,
            { gone: 'gone', startOver: 'start over', stay: 'stay' }) }),
        control('cellRate', 'Rate'),
        control('cellDelay', 'Delay'),
        control('cellGate', 'Gate', { inertWhen: live => endsAs(live, 'stay') }),
      ],
    },
    {
      title: 'Brightness', tone: 'cells',
      names: [
        control('cellDirection', 'Direction', { ...directionSwitch, inertWhen: curveIsFlat }),
        control('cellBrightness', 'Brightness'),
        control('cellCurve', 'Curve'),
        control('cellCurveAt', 'Curve at', { inertWhen: curveIsFlat }),
      ],
    },
  ],
};

const fromLfo = (live, route) => Protocol.routeSource(live[route.ratio]) === Protocol.ROUTE_SOURCE.lfo;
const holdable = (live, route) => fromLfo(live, route)
  && Preview.cellControl(Protocol.routeTarget(live[route.destination]));
const fromGroup = (live, route) => [Protocol.ROUTE_SOURCE.strip, Protocol.ROUTE_SOURCE.note, Protocol.ROUTE_SOURCE.birth]
  .includes(Protocol.routeSource(live[route.ratio]));

for (const route of ROUTES) {
  const offLfo = { inertWhen: live => !fromLfo(live, route) };
  control(route.amount, 'Amount');
  control(route.ratio, 'Ratio', {
    ...offLfo,
    positions: {
      count: Protocol.ROUTE_MAX_RATIO,
      positionOf: value => Protocol.routeRatio(value) - 1,
      valueAt: (position, live) => (fromLfo(live, route)
        ? Protocol.routeRatioValue(position + 1, Protocol.routeOnce(live[route.ratio])) : live[route.ratio]),
    },
  });
  control(route.wave, 'Wave', offLfo);
  control(route.smooth, 'Smooth', offLfo);
  control(route.phase, 'Phase', offLfo);
  control(route.hold, 'Hold',
    { kind: 'two', options: [[OFF, 'live'], [ON, 'held']], inertWhen: live => !holdable(live, route) });
}

const gradientInert = { inertWhen: isGradient };

const ARP_MODE_NAMES = {
  sequence: 'sequence', bounce: 'bounce', evensOdds: 'evens / odds',
  pairs: 'pairs', mirror: 'mirror', random: 'random',
};

const arpRouted = live =>
  soundingRoutes(live).some(route => Protocol.routeArp(live[route.destination]) !== Protocol.ARP.unison);

const LFO = {
  name: 'LFO', tone: 'lfo',
  source: [control('lfoRate', 'Rate')],
  amounts: [],
  arpeggiator: [
    control('arpMode', 'Mode',
      { kind: 'steps', step: Protocol.arpMode,
        options: Object.entries(Protocol.ARP_MODE)
          .map(([key, mode]) => [Protocol.arpModeValue(mode), ARP_MODE_NAMES[key]]),
        inertWhen: live => !arpRouted(live) }),
    control('arpSpread', 'Spread',
      { inertWhen: live => !arpRouted(live) }),
  ],
};

const ROUTES_CARD = { name: 'Routes', tone: 'lfo', source: [], amounts: [] };

const ENGINES = [
  {
    name: 'Scatter', tone: 'scatter',
    source: [
      control('scatterCount', 'Count'),
      control('scatterWidth', 'Width'),
      control('scatterEdge', 'Edge'),
      control('scatterRate', 'Rate'),
      control('scatterSpeed', 'Speed'),
      control('scatterRandomize', 'Randomize'),
      control('scatterPosition', 'Position'),
    ],
    amounts: [
      control('scatterMix', 'Mix'),
      control('scatterHue', 'Hue'),
      control('scatterSaturation', 'Saturation'),
      control('scatterValue', 'Value'),
    ],
    shownBy: ['scatterMix'],
  },
  {
    name: 'Field', tone: 'color',
    source: [
      control('fieldForm', 'Form',
        { kind: 'three', options: threeWayOptions(Protocol.FIELD_FORM,
          { gradient: 'gradient', region: 'region', allButRegion: 'all but region' }) }),
      control('fieldDirection', 'Direction', directionSwitch),
      control('fieldCount', 'Count'),
      control('fieldWidth', 'Width', gradientInert),
      control('fieldEdge', 'Edge', gradientInert),
      control('fieldSpeed', 'Speed'),
      control('fieldPosition', 'Position'),
    ],
    amounts: [
      control('fieldHue', 'Hue'),
      control('fieldWhite', 'White'),
      control('fieldDark', 'Dark'),
    ],
  },
  {
    name: 'Flow', tone: 'color',
    source: [
      control('flowDensity', 'Density'),
      control('flowRate', 'Rate'),
    ],
    amounts: [
      control('flowHue', 'Hue'),
      control('flowWhite', 'White'),
      control('flowDark', 'Dark'),
    ],
  },
  {
    name: 'Core', tone: 'color',
    source: [],
    amounts: [
      control('coreHue', 'Hue'),
      control('coreWhite', 'White'),
      control('coreDark', 'Dark'),
    ],
  },
];

const FULL_SATURATION = 255;
const hueSwatch = (live, hue) => Preview.paletteColor(live.palette, hue & 255, FULL_SATURATION);

const SHARED_COLOR = {
  title: 'Strips and PARs',
  sections: [[null, [
    control('palette', 'Palette',
      { kind: 'pick', options: Preview.paletteNames().map((name, index) => [index, name]) }),
    control('hue', 'Hue',
      { swatch: live => hueSwatch(live, real('hue', live.hue)) }),
  ]]],
};

const STRIPS = {
  title: '5 strips',
  sections: [[null, [
    control('saturation', 'Saturation'),
  ]]],
};

const HUE_LAYOUT_NAMES = {
  gradient: 'gradient', evensOdds: 'evens / odds', pairs: 'pairs', mirror: 'mirror',
  random: 'random per pulse',
};

const PARS = {
  title: '4 PARs',
  sections: [
    ['Color', [
      control('parHueOffset', 'Hue offset',
        { swatch: live => hueSwatch(live, real('hue', live.hue) + real('parHueOffset', live.parHueOffset)) }),
      control('parSaturation', 'Saturation'),
      control('parValue', 'Value'),
    ]],
    ['Hue across them', [
      control('parHueLayout', 'Hue layout',
        { kind: 'steps', step: Protocol.hueLayout,
          options: Object.entries(Protocol.HUE_LAYOUT)
            .map(([key, layout]) => [Protocol.hueLayoutValue(layout), HUE_LAYOUT_NAMES[key]]),
          inertWhen: live => Math.round(real('parHueRange', live.parHueRange)) === 0 }),
      control('parHueRange', 'Hue range'),
    ]],
  ],
};

const OUTPUTS = [SHARED_COLOR, STRIPS, PARS];
const outputNames = output => output.sections.flatMap(([, names]) => names);
const cardNames = card => [...card.source, ...card.amounts, ...(card.arpeggiator || [])];

const PLACES = {};
for (const group of CELLS.groups) {
  for (const name of group.names) PLACES[name] = `${CELLS.name} · ${group.title}`;
}
for (const card of [LFO, ...ENGINES]) {
  for (const name of cardNames(card)) PLACES[name] = card.name;
}
for (const card of ENVELOPE_CARDS) {
  for (const name of card.names) PLACES[name] = card.name;
}
for (const output of OUTPUTS) {
  for (const name of outputNames(output)) PLACES[name] = output.title;
}

const TONES = {};
for (const group of CELLS.groups) {
  for (const name of group.names) TONES[name] = group.tone;
}
for (const card of [LFO, ...ENGINES]) {
  for (const name of cardNames(card)) TONES[name] = card.tone;
}
for (const card of ENVELOPE_CARDS) {
  for (const name of card.names) TONES[name] = card.tone;
}
for (const output of OUTPUTS) {
  for (const name of outputNames(output)) TONES[name] = 'out';
}
for (const route of ROUTES) {
  for (const name of route.fields) {
    TONES[name] = 'lfo';
    PLACES[name] = route.name;
  }
}

const ROUTABLE = new Set(Protocol.tagged('patch').filter(name => CONTROLS[name] && !Preview.routeRefused(CC[name]))
  .concat(ROUTES.flatMap(route => [route.amount, route.phase])));
const routable = name => ROUTABLE.has(name);
const targetName = destination => {
  const aimed = Protocol.aimedRoute(destination);
  if (aimed >= 0) return Protocol.aimsPhase(destination) ? ROUTES[aimed].phase : ROUTES[aimed].amount;
  return Protocol.NAME_BY_CC[Protocol.routeTarget(destination)] || null;
};
const destinationOf = name => {
  const route = routeOf(name);
  if (route) return Protocol.aimDestination(ROUTES.indexOf(route), name === route.phase);
  return CC[name];
};
const aimsRoute = destination => Protocol.aimedRoute(destination) >= 0;
const routableDestination = number => number === 0 || routable(targetName(number));
const arpCapable = name => Protocol.routeDestination(CC[name], Protocol.ARP.steps) !== CC[name];
const bipolarCapable = name => !routeOf(name) && Protocol.bipolarCapable(CC[name]);
const isCircular = name => Protocol.hasTag(name, 'circular');
const swings = name => Protocol.hasTag(name, 'rate');

function steps(valueOf) {
  const points = [];
  let start = 0;
  for (let value = 1; value <= 128; value++) {
    if (value < 128 && valueOf(value) === valueOf(start)) continue;
    if (start > 0 && value < 128) points.push(Math.round((start + value - 1) / 2));
    start = value;
  }
  return points;
}

function wholeCounts(valueOf) {
  const points = [];
  for (let count = Math.ceil(valueOf(0)); count <= Math.floor(valueOf(127)); count++) {
    let nearest = 0;
    for (let value = 1; value < 128; value++) {
      if (Math.abs(valueOf(value) - count) < Math.abs(valueOf(nearest) - count)) nearest = value;
    }
    points.push(nearest);
  }
  return points;
}

const STEPPED = new Set([
  'lfoRate', 'flowRate', 'scatterRate', 'fieldSpeed',
  'cellRate', 'cellChord', 'cellGate', 'cellTail',
  ...ENVELOPES.flatMap(stages => [stages.attack, stages.hold, stages.decay, stages.release]),
]);

function pointsFor(name) {
  const route = routeOf(name);
  if (route) {
    if (name === route.wave) return [Protocol.WAVE_SWELL, Protocol.WAVE_FALL, Protocol.WAVE_SQUARE];
    if (name === route.phase) return steps(Protocol.routePhaseStep);
    if (name === route.ratio) return steps(Protocol.routeRatio);
    if (name === route.smooth) return [];
    return [64];
  }
  const at = value => real(name, value);
  if (name === 'cellBendAt' || name === 'cellPosition' || name === 'cellCurveAt') return [64];
  if (name === 'fieldCount') return wholeCounts(at);
  if (STEPPED.has(name)) return steps(at);
  if (!Protocol.hasTag(name, 'patch')) return [];
  return at(56) < 0 && at(64) === 0 && at(72) > 0 ? [64] : [];
}

export {
  LAYER_NAMES, LAYERS, LAYERS_ABOVE_BASE, isAboveBase,
  CC, NAMES, CONTINUOUS, isSwitch, controls, CONTROLS, READOUTS, DEFAULT,
  clampToSevenBits,
  LFO_PERIOD_NAMES, periodStep, periodValue, TRANSITION_NAMES, transitionStep, transitionValue, TEMPO_DIVISIONS,
  CELLS, ENVELOPES, ENVELOPE_CARDS, envelopeStages, LFO, ROUTES_CARD, ENGINES, OUTPUTS, ROUTES, PLACES, TONES, cardNames,
  fromLfo, fromGroup, holdable, sounding, aimsAt, routable, routableDestination, targetName, destinationOf, aimsRoute, routeOf,
  arpCapable, bipolarCapable, perSpot, isCircular, swings, pointsFor,
};
