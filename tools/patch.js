(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const preview = () => global.AuroraPreview;

  const PART_NAMES = ['Base', 'Color', 'Extent', 'Motion', 'Accent'];
  const TARGETS = [Protocol.PATCH_TARGET_COLOR, Protocol.PATCH_TARGET_MOTION,
                   Protocol.PATCH_TARGET_EXTENT, Protocol.PATCH_TARGET_ACCENT];
  const PARTS = [Protocol.PATCH_BASE, ...TARGETS];
  const isTarget = part => part !== Protocol.PATCH_BASE;

  const CC = Object.assign({}, Protocol.CC);

  const ROUTE_FIELDS = Object.keys(Protocol.ROUTE_FIELD);
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
      controls: ROUTE_FIELDS.filter(field => field !== 'destination').map(field => names[field]),
    }, names);
  });
  const ROUTE_FIELD_NAMES = ROUTES.flatMap(route => route.fields);
  const routeOf = name => ROUTES.find(route => route.fields.includes(name));

  const NAMES = Protocol.tagged('patch').concat(Protocol.tagged('switch'), ROUTE_FIELD_NAMES)
    .sort((a, b) => CC[a] - CC[b]);
  const SWITCHES = new Set(Protocol.tagged('switch').concat(ROUTES.map(route => route.destination)));
  const isSwitch = name => SWITCHES.has(name);
  const CONTINUOUS = NAMES.filter(name => !isSwitch(name));

  const clampToSevenBits = value => (value < 0 ? 0 : value > 127 ? 127 : value | 0);
  const bipolar = value => (value < 64 ? (value - 64) / 64 : (value - 64) / 63);

  const real = (name, value) => preview().convert(CC[name], value);

  const percent = ratio => (ratio * 100).toFixed(0) + '%';
  const sign = ratio => (ratio < 0 ? '−' : '+');
  const signed = ratio => sign(ratio) + percent(Math.abs(ratio));

  const degrees = hueSteps => Math.round(hueSteps * 360 / 256) + '°';
  const signedDegrees = hueSteps => sign(hueSteps) + degrees(Math.abs(hueSteps));
  const hueReach = name => value => signedDegrees(Math.round(real(name, value)));
  const pixels = count => (count < 10 ? Number(count.toFixed(1)) : count.toFixed(0)) + ' px';
  const counted = (count, one, many) => count + ' ' + (count === 1 ? one : many);
  const beatsText = beats => {
    const rounded = Math.round(beats * 100) / 100;
    return rounded + (rounded === 1 ? ' beat' : ' beats');
  };

  const PERIOD_NAMES = {
    16: '16 beats', 12: '12 beats', 8: '8 beats',
    6: '6 beats', 4: '4 beats', 3: '3 beats', 2: '2 beats',
    1.5: '1½ beats', 1: '1 beat', 0.75: '¾ beat', 0.5: '½ beat', 0.375: '⅜ beat', 0.25: '¼ beat',
  };
  const LFO_PERIOD_NAMES = Protocol.LFO_PERIODS.map(beats => PERIOD_NAMES[beats]);
  const lapName = lapsPerBeat => PERIOD_NAMES[Math.round(1000 / Math.abs(lapsPerBeat)) / 1000];
  const periodStep = value => Protocol.steppedIndex(value, Protocol.LFO_PERIODS.length);
  const periodValue = step => Math.round(step * 127 / (Protocol.LFO_PERIODS.length - 1));

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

  const perSpot = destination => preview().spotDestination(Protocol.routeTarget(destination));

  const phaseText = (value, live, route) => {
    const step = Protocol.routePhaseStep(value);
    if (step === 0) return '0°';
    const degrees = Math.round(step * 360 / Protocol.ROUTE_PHASE_STEPS);
    const settings = live || DEFAULT;
    const perCycle = perSpot(settings[route.destination])
      ? 1 / real('scatterRate', settings.scatterRate) : real('lfoRate', settings.lfoRate);
    const cycle = perCycle / Protocol.routeRatio(settings[route.ratio]);
    return `${degrees}° · ${beatsText(step / Protocol.ROUTE_PHASE_STEPS * cycle)}`;
  };

  const ROUTE_READOUTS = {
    amount: () => value => signed(bipolar(value)),
    ratio: () => value => '×' + Protocol.routeRatio(value),
    phase: route => (value, live) => phaseText(value, live, route),
    wave: () => waveText,
  };

  const cellPixels = live => preview().PIXELS / real('shapeCount', live.shapeCount);
  const shapeGap = live => 1 - real('shapeWidth', live.shapeWidth);

  const BARS = '▁▂▃▄▅▆▇█';
  const FAN_LANDMARKS = { 0: 'alike', 0.125: 'slope', 0.25: 'peak', 0.5: 'alternate' };
  const fanTriangle = u => (u < 0.5 ? 4 * u - 1 : 3 - 4 * u);
  const fract = x => x - Math.floor(x);
  const fanPicture = live => preview().WALL_STRIP_ORDER.map(strip => {
    const wave = fanTriangle(fract(real('fanPhase', live.fanPhase) + real('fanFrequency', live.fanFrequency) * (strip - 1)));
    return BARS[Math.round((wave + 1) / 2 * (BARS.length - 1))];
  }).join('');

  const fanShift = (live, reach) => {
    const laps = Math.abs(real('shapeSpeed', live.shapeSpeed));
    if (laps > 0) return '±' + beatsText(reach / laps);
    const room = Protocol.isOn(live.shapeBounce) ? shapeGap(live) : 1;
    return '±' + pixels(reach * room * cellPixels(live));
  };

  const upTheStrip = at =>
    (at < 0.005 ? 'at the bottom' : at > 0.995 ? 'at the top' : Math.round(at * 100) + '% up');

  const READOUTS = {
    shapeWidth: (value, live) => pixels(real('shapeWidth', value) * cellPixels(live)),
    shapeEdge: (value, live) => pixels(real('shapeEdge', value) * shapeGap(live) * 0.5 * cellPixels(live)) + ' glow',
    shapeTail: value => {
      const beats = real('shapeTail', value);
      return beats === 0 ? 'none' : PERIOD_NAMES[beats];
    },
    shapeCount: value => counted(real('shapeCount', value), 'shape', 'shapes'),
    shapePosition: value => upTheStrip(real('shapePosition', value)),
    shapeSpeed: (value, live) => {
      const laps = real('shapeSpeed', value);
      if (laps === 0) return 'still';
      const arrow = Protocol.isOn(live.shapeBounce) ? '↕' : laps > 0 ? '↑' : '↓';
      return arrow + ' ' + lapName(laps);
    },
    shapeBend: value => {
      const bend = real('shapeBend', value) * 0.95;
      if (Math.abs(bend) < 0.005) return 'even';
      const ratio = (1 + Math.abs(bend)) / (1 - Math.abs(bend));
      return ratio.toFixed(ratio < 10 ? 1 : 0) + '× ' + (bend > 0 ? 'faster' : 'slower');
    },
    shapeBendAt: value => upTheStrip(real('shapeBendAt', value)),
    fanSpread: (value, live) => fanShift(live, Math.abs(real('fanSpread', value))),
    fanLfo: (value, live) => '±' + beatsText(Math.abs(real('fanLfo', value)) * real('lfoRate', live.lfoRate)),
    fanSpeed: value => '±' + Math.abs(real('fanSpeed', value)).toFixed(1) + ' px/beat',
    fanFrequency: (value, live) => {
      const picture = fanPicture(Object.assign({}, live, { fanFrequency: value }));
      const landmark = FAN_LANDMARKS[real('fanFrequency', value)];
      return landmark ? `${picture} ${landmark}` : picture;
    },
    fanPhase: (value, live) => fanPicture(Object.assign({}, live, { fanPhase: value })),
    fanRandomize: value => percent(real('fanRandomize', value)) + ' random',

    hue: value => degrees(real('hue', value)),

    lfoRate: value => PERIOD_NAMES[real('lfoRate', value)],

    scatterRate: value => 'every ' + PERIOD_NAMES[Protocol.LFO_PERIODS[periodStep(value)]],
    scatterCount: value => real('scatterCount', value).toFixed(1) + ' a strip',
    scatterWidth: (value, live) => pixels(Math.max(1, real('scatterWidth', value) * preview().PIXELS / real('scatterCount', live.scatterCount))),
    scatterEdge: (value, live) => {
      const settings = live || DEFAULT;
      const reach = preview().PIXELS / real('scatterCount', settings.scatterCount);
      const core = Math.max(real('scatterWidth', settings.scatterWidth), 1 / reach);
      return pixels(real('scatterEdge', value) * (1 - core) * 0.5 * reach) + ' soft';
    },
    scatterRandomize: value => percent(real('scatterRandomize', value)) + ' random',
    scatterSpeed: value => {
      const speed = real('scatterSpeed', value);
      return sign(speed) + Math.abs(speed).toFixed(1) + ' px/beat';
    },
    scatterPosition: value => {
      const reach = real('scatterPosition', value) * preview().PIXELS * 0.5;
      return sign(reach) + pixels(Math.abs(reach));
    },
    scatterMix: value => percent(real('scatterMix', value)),
    scatterHue: hueReach('scatterHue'),

    fieldHue: hueReach('fieldHue'),
    fieldWhite: value => percent(real('fieldWhite', value)) + ' white',
    fieldDark: value => percent(real('fieldDark', value)) + ' dark',
    fieldCount: value => counted(real('fieldCount', value), 'region', 'regions'),
    fieldWidth: (value, live) => {
      const width = real('fieldWidth', value);
      const count = real('fieldCount', live.fieldCount);
      const direction = Protocol.threeWayPosition(live.fieldDirection);
      if (direction === Protocol.FIELD_DIRECTION.horizontal) {
        return (width * (preview().STRIPS - 1) / count).toFixed(1) + ' strips';
      }
      if (direction === Protocol.FIELD_DIRECTION.vertical) return pixels(width * preview().PIXELS / count);
      return percent(width / count) + ' of shape';
    },
    fieldEdge: value => percent(real('fieldEdge', value)) + ' soft',
    fieldSpeed: value => {
      const laps = real('fieldSpeed', value);
      return laps === 0 ? 'still' : sign(laps) + lapName(laps);
    },

    flowHue: value => '±' + degrees(Math.abs(Math.round(real('flowHue', value)))),
    flowWhite: value => percent(real('flowWhite', value)) + ' white',
    flowDark: value => percent(real('flowDark', value)) + ' dark',
    flowRate: value => {
      const rate = real('flowRate', value);
      return rate < 0.004 ? 'frozen' : 'every ' + beatsText(Math.round(1 / rate));
    },
    flowDensity: value => {
      const cells = real('flowDensity', value);
      return cells <= 0 ? 'whole wall' : pixels(preview().PIXELS / cells) + ' across';
    },

    lightHue: hueReach('lightHue'),
    lightWhite: value => percent(real('lightWhite', value)) + ' white',
    lightDark: value => percent(real('lightDark', value)) + ' dark',

    parHueOffset: value => '+' + degrees(real('parHueOffset', value)),
    arpSpread: (value, live) => {
      const spread = real('arpSpread', value);
      const mode = Protocol.arpMode((live || DEFAULT).arpMode);
      const steps = passLength(mode);
      const cycles = Math.max(1, Math.round(Math.abs(spread) * steps));
      const pace = `${Math.round(steps / cycles * 100) / 100} a cycle`;
      const hasDirection = mode !== Protocol.ARP_MODE.random;
      return spread < 0 && hasDirection ? '← ' + pace : pace;
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

  for (const [name, readout] of Object.entries(READOUTS)) {
    READOUTS[name] = (value, live) => inPulses(readout(value, live), live);
  }

  const DEFAULT = {};
  for (const name of NAMES) DEFAULT[name] = Protocol.CONTROL_DEFAULTS[name] || 0;
  for (const route of ROUTES) {
    for (const field of ROUTE_FIELDS) DEFAULT[route[field]] = Protocol.ROUTE_DEFAULTS[field];
  }

  const NEUTRAL = Object.assign({}, DEFAULT, { shapeWidth: 127, shapeEdge: 0, shapeSpeed: 64 });

  const CONTROLS = {};
  const control = (name, label, extra) => {
    CONTROLS[name] = Object.assign({ name, label, kind: 'fader' }, extra || {});
    return name;
  };

  const OFF = 0, ON = 127;
  const threeWayOptions = (positions, labels) =>
    Object.entries(positions).map(([key, position]) => [Protocol.THREE_WAY_VALUES[position], labels[key]]);

  const soundingRoutes = live => ROUTES.filter(route =>
    Protocol.routeTarget(live[route.destination]) !== 0 && live[route.amount] !== Protocol.ROUTE_DEFAULTS.amount);
  const routedTo = (live, names) =>
    soundingRoutes(live).some(route => names.some(name => Protocol.routeTarget(live[route.destination]) === CC[name]));
  const atZero = (live, names) => names.every(name => real(name, live[name]) === 0);

  const nothingTravels = live =>
    atZero(live, ['shapeSpeed', 'fanSpeed']) && !routedTo(live, ['shapeSpeed', 'shapePosition', 'fanSpread', 'fanSpeed']);
  const FAN_AMOUNTS = ['fanSpread', 'fanSpeed', 'fanLfo'];
  const fanIsFlat = live => atZero(live, FAN_AMOUNTS) && !routedTo(live, FAN_AMOUNTS);
  const inertWhileStill = { inertWhen: nothingTravels };
  const inertWhileFlat = { inertWhen: fanIsFlat };

  const SHAPE = {
    name: 'Shape',
    groups: [
      {
        title: 'Form',
        names: [
          control('shapeCount', 'Count'),
          control('shapeWidth', 'Width'),
          control('shapeEdge', 'Edge'),
          control('shapeTail', 'Tail', inertWhileStill),
        ],
      },
      {
        title: 'Travel',
        names: [
          control('shapeBounce', 'Bounce',
            { kind: 'two', options: [[OFF, 'wrap'], [ON, 'bounce']] }),
          control('shapeSpeed', 'Speed'),
          control('shapePosition', 'Position'),
          control('shapeBend', 'Bend', inertWhileStill),
          control('shapeBendAt', 'Bend at',
            { inertWhen: live => nothingTravels(live) || Math.abs(real('shapeBend', live.shapeBend)) < 0.005 }),
        ],
      },
      {
        title: 'Fan',
        names: [
          control('fanSpread', 'Spread'),
          control('fanSpeed', 'Speed'),
          control('fanLfo', 'LFO', { inertWhen: live => !soundingRoutes(live).length }),
          control('fanFrequency', 'Frequency', inertWhileFlat),
          control('fanPhase', 'Phase', inertWhileFlat),
          control('fanRandomize', 'Randomize', inertWhileFlat),
        ],
      },
    ],
  };

  for (const route of ROUTES) {
    control(route.amount, 'Amount');
    control(route.ratio, 'Ratio');
    control(route.wave, 'Wave');
    control(route.phase, 'Phase');
  }

  const gradientInert = {
    inertWhen: live => Protocol.threeWayPosition(live.fieldForm) === Protocol.FIELD_FORM.gradient,
  };

  const ARP_MODE_NAMES = {
    sequence: 'sequence', bounce: 'bounce', evensOdds: 'evens / odds',
    pairs: 'pairs', mirror: 'mirror', random: 'random',
  };

  const PARS_COUNT = 4;
  const passLength = mode => ({
    [Protocol.ARP_MODE.bounce]: 2 * PARS_COUNT - 2,
    [Protocol.ARP_MODE.evensOdds]: 2,
    [Protocol.ARP_MODE.pairs]: PARS_COUNT / 2,
    [Protocol.ARP_MODE.mirror]: PARS_COUNT / 2,
  }[mode] || PARS_COUNT);

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
      control('arpSpread', 'Steps',
        { inertWhen: live => !arpRouted(live) }),
    ],
  };

  const MODULATORS = [
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
        control('fieldDirection', 'Direction',
          { kind: 'three', options: threeWayOptions(Protocol.FIELD_DIRECTION,
            { horizontal: 'horizontal', vertical: 'vertical', shape: 'shape' }) }),
        control('fieldCount', 'Count', gradientInert),
        control('fieldWidth', 'Width', gradientInert),
        control('fieldEdge', 'Edge', gradientInert),
        control('fieldSpeed', 'Speed', gradientInert),
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
      name: 'Light', tone: 'color',
      source: [],
      amounts: [
        control('lightHue', 'Hue'),
        control('lightWhite', 'White'),
        control('lightDark', 'Dark'),
      ],
    },
  ];

  const FULL_SATURATION = 255;
  const hueSwatch = (live, hue) => preview().paletteColor(live.palette, hue & 255, FULL_SATURATION);

  const SHARED_COLOR = {
    title: 'Strips and PARs',
    sections: [[null, [
      control('palette', 'Palette',
        { kind: 'pick', options: () => preview().paletteNames().map((name, index) => [index, name]) }),
      control('hue', 'Hue',
        { swatch: live => hueSwatch(live, real('hue', live.hue)) }),
    ]]],
  };

  const STRIPS = {
    title: '5 strips',
    sections: [[null, [
      control('saturation', 'Saturation'),
      control('value', 'Value'),
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
  for (const group of SHAPE.groups) {
    for (const name of group.names) PLACES[name] = `${SHAPE.name} · ${group.title}`;
  }
  for (const card of [LFO, ...MODULATORS]) {
    for (const name of cardNames(card)) PLACES[name] = card.name;
  }
  for (const output of OUTPUTS) {
    for (const name of outputNames(output)) PLACES[name] = output.title;
  }

  let routableNames = null;
  const routable = name => {
    if (!routableNames) {
      routableNames = new Set(Protocol.tagged('patch').filter(candidate =>
        CONTROLS[candidate] && !preview().routeRefused(CC[candidate])));
    }
    return routableNames.has(name);
  };
  const routableDestination = number => {
    const target = Protocol.routeTarget(number);
    return number === 0 || (target !== 0 && routable(Protocol.NAME_BY_CC[target]));
  };
  const arpCapable = name => Protocol.ARP_CONTROLS.includes(name);
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

  function pointsFor(name) {
    const route = routeOf(name);
    if (route) {
      if (name === route.wave) return [Protocol.WAVE_SWELL, Protocol.WAVE_FALL, Protocol.WAVE_SQUARE];
      if (name === route.phase) return steps(Protocol.routePhaseStep);
      if (name === route.ratio) return steps(Protocol.routeRatio);
      return [64];
    }
    const at = value => real(name, value);
    if (name === 'shapeBendAt' || name === 'shapePosition') return [64];
    if (name === 'lfoRate' || name === 'shapeTail' || name === 'scatterRate' || name === 'fanFrequency' || name === 'shapeSpeed' || name === 'fieldSpeed') return steps(at);
    if (!Protocol.hasTag(name, 'patch')) return [];
    return at(56) < 0 && at(64) === 0 && at(72) > 0 ? [64] : [];
  }

  global.AuroraPatch = {
    PART_NAMES, PARTS, TARGETS, isTarget,
    CC, NAMES, CONTINUOUS, isSwitch, CONTROLS, READOUTS, DEFAULT, NEUTRAL,
    clampToSevenBits, bipolar,
    LFO_PERIOD_NAMES, periodStep, periodValue, TEMPO_DIVISIONS,
    SHAPE, LFO, MODULATORS, OUTPUTS, ROUTES, PLACES, cardNames,
    routable, routableDestination, arpCapable, perSpot, isCircular, swings, pointsFor,
  };
})(window);
