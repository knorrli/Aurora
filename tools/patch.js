(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const preview = () => global.AuroraPreview;

  const PART_NAMES = ['Base', 'Color', 'Extent', 'Motion', 'Accent'];
  const PART_BLURBS = [
    'the look itself — what the wall shows with every fader down',
    'where the Color fader morphs to. More means hotter, toward white',
    'where the Extent fader morphs to. More means more of the wall lit',
    'where the Motion fader morphs to. More means faster, harder, more agitated',
    'where holding this patch’s own key pushes. Belongs to no fader',
  ];
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
  const ofByte = ratio => percent(ratio / 255);
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
    16: '16 beats · four bars', 12: '12 beats · three bars', 8: '8 beats · two bars',
    6: '6 beats', 4: '4 beats · one bar', 3: '3 beats', 2: '2 beats · half a bar',
    1.5: '1½ beats', 1: '1 beat', 0.75: '¾ beat', 0.5: '½ beat', 0.375: '⅜ beat', 0.25: '¼ beat',
  };
  const LFO_PERIOD_NAMES = Protocol.LFO_PERIODS.map(beats => PERIOD_NAMES[beats]);
  const shortPeriodName = beats => PERIOD_NAMES[beats].split(' · ')[0];
  const lapName = lapsPerBeat => shortPeriodName(Math.round(1000 / Math.abs(lapsPerBeat)) / 1000);
  const periodStep = value => Protocol.steppedIndex(value, Protocol.LFO_PERIODS.length);
  const periodValue = step => Math.round(step * 127 / (Protocol.LFO_PERIODS.length - 1));

  const TEMPO_DIVISION_NAMES = {
    quarter: 'quarter · one pulse a beat', bar: 'bar', half: 'half',
    eighth: 'eighth', eighthTriplet: 'eighth triplet', sixteenth: 'sixteenth',
  };
  const TEMPO_DIVISIONS = Object.entries(Protocol.TEMPO_DIVISION)
    .map(([key, value]) => [value, TEMPO_DIVISION_NAMES[key]]);

  const waveText = value => {
    const stages = ['build', 'swell', 'snap', 'square', 'stab'];
    const stage = Math.min(3, value >> 5);
    return `${stages[stage]} ${percent((value - stage * 32) * 4 / 127)}→${stages[stage + 1]}`;
  };

  const ROUTE_READOUTS = {
    amount: value => signed(bipolar(value)),
    ratio: value => '×' + Protocol.routeRatio(value) + ' the LFO',
    phase: value => Math.round(value / 128 * 360) + '°',
    wave: waveText,
  };

  const cellPixels = live => preview().PIXELS / real('shapeCount', live.shapeCount);
  const shapeGap = live => 1 - real('shapeWidth', live.shapeWidth);

  const BARS = '▁▂▃▄▅▆▇█';
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
      return beats < 0.001 ? 'none' : beats.toFixed(2) + ' beats';
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
    fanFrequency: (value, live) => fanPicture(Object.assign({}, live, { fanFrequency: value })),
    fanPhase: (value, live) => fanPicture(Object.assign({}, live, { fanPhase: value })),
    fanRandomize: value => percent(real('fanRandomize', value)) + ' random',

    hue: value => degrees(real('hue', value)),
    saturation: value => ofByte(real('saturation', value)),
    value: value => ofByte(real('value', value)),

    lfoRate: value => shortPeriodName(real('lfoRate', value)),

    scatterRate: value => 'every ' + shortPeriodName(Protocol.LFO_PERIODS[periodStep(value)]),
    scatterCount: value => real('scatterCount', value).toFixed(1) + ' spots',
    scatterWidth: (value, live) => pixels(Math.max(1, real('scatterWidth', value) * preview().PIXELS / real('scatterCount', live.scatterCount))),
    scatterEdge: value => percent(real('scatterEdge', value)) + ' soft',
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
    scatterSaturation: value => percent(real('scatterSaturation', value)),
    scatterValue: value => percent(real('scatterValue', value)),

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

    parValue: value => ofByte(real('parValue', value)),
    parHueOffset: value => '+' + degrees(real('parHueOffset', value)),
    parSaturation: value => ofByte(real('parSaturation', value)),
    arpSpread: value => {
      const spread = real('arpSpread', value);
      return Math.abs(spread) < 0.005 ? 'together' : `${percent(Math.abs(spread))} ${spread < 0 ? 'reverse' : 'forward'}`;
    },
    parHueRange: value => {
      const reach = Math.round(real('parHueRange', value));
      return reach === 0 ? 'one hue' : `±${degrees(Math.abs(reach))} ${reach < 0 ? 'high' : 'low'} first`;
    },
  };

  for (const route of ROUTES) {
    for (const field of ROUTE_FIELDS) {
      if (ROUTE_READOUTS[field]) READOUTS[route[field]] = ROUTE_READOUTS[field];
    }
  }

  const DEFAULT = {};
  for (const name of NAMES) DEFAULT[name] = Protocol.CONTROL_DEFAULTS[name] || 0;
  for (const route of ROUTES) {
    for (const field of ROUTE_FIELDS) DEFAULT[route[field]] = Protocol.ROUTE_DEFAULTS[field];
  }

  const NEUTRAL = Object.assign({}, DEFAULT, { shapeWidth: 127, shapeEdge: 0, shapeSpeed: 64 });

  const CONTROLS = {};
  const control = (name, label, hint, extra) => {
    CONTROLS[name] = Object.assign({ name, label, hint, kind: 'fader' }, extra || {});
    return name;
  };

  const OFF = 0, ON = 127;
  const threeWayOptions = (positions, labels) =>
    Object.entries(positions).map(([key, position]) => [Protocol.THREE_WAY_VALUES[position], labels[key]]);

  const SHAPE = {
    name: 'Shape',
    groups: [
      {
        title: 'Form',
        names: [
          control('shapeCount', 'Count', 'how many shapes along the strip, 1–20'),
          control('shapeWidth', 'Width', 'the solid core of each shape. Each shape has the strip divided by Count to itself'),
          control('shapeEdge', 'Edge', 'how far the glow reaches toward the next shape, each side'),
          control('shapeTail', 'Tail', 'how long a pixel glows after a moving shape passes it'),
        ],
      },
      {
        title: 'Travel',
        names: [
          control('shapeBounce', 'Bounce', 'turn back at the end of each shape’s own stretch of strip instead of wrapping round',
            { kind: 'two', options: [[OFF, 'wrap'], [ON, 'bounce']] }),
          control('shapeSpeed', 'Speed', 'center is still; either side travels, slowest nearest center. Stepped by how long the pattern takes to repeat, whatever Count, Width and Bounce: ↑ up, ↓ down, ↕ there and back'),
          control('shapePosition', 'Position', 'where a still shape rests in its own stretch of strip, from the bottom end to the top'),
          control('shapeBend', 'Bend', 'travel slowed and sped by where a shape is; plus is fastest at Bend at, minus slowest there. The readout compares fastest to slowest'),
          control('shapeBendAt', 'Bend at', 'where along the strip the bend peaks, bottom to top; bouncing, along each shape’s own stretch'),
        ],
      },
      {
        title: 'Fan',
        names: [
          control('fanSpread', 'Spread', 'how far apart the strips stand, following the fan’s shape. The readout is the furthest strip: in pixels when still, in beats when moving'),
          control('fanSpeed', 'Speed', 'how far apart their speeds stand, either side of Speed'),
          control('fanLfo', 'LFO', 'how far apart the strips’ LFOs swing, following the fan’s shape. The readout is the furthest strip, in beats'),
          control('fanFrequency', 'Frequency', 'the shape of the fan across the five strips, as the readout draws it: all alike, a slope, a peak, or every strip opposite its neighbors'),
          control('fanPhase', 'Phase', 'slides the fan’s shape sideways across the strips, as the readout draws it'),
          control('fanRandomize', 'Randomize', 'from the fan’s shape to a fixed random amount per strip'),
        ],
      },
    ],
  };

  for (const route of ROUTES) {
    control(route.amount, 'Amount', 'how far, as a share of the distance left; plus is toward the top, minus toward the bottom. On a rate, how wide the swing either side, and which half comes first');
    control(route.ratio, 'Ratio', 'whole multiples of the LFO');
    control(route.wave, 'Wave', 'a build → swell → snap → hard half-bar → stab');
    control(route.phase, 'Phase', 'how far into its own cycle the wave starts after the bar line');
  }

  const gradientInert = {
    inertWhen: live => Protocol.threeWayPosition(live.fieldForm) === Protocol.FIELD_FORM.gradient,
    inertWhy: 'a gradient spans its direction once, so there is nothing here to repeat, size or move',
  };

  const LFO = {
    name: 'LFO', tone: 'lfo',
    source: [control('lfoRate', 'Rate', 'how often the swell lands. Stepped, so it can sit on the bar')],
    amounts: [],
  };

  const MODULATORS = [
    {
      name: 'Scatter', tone: 'scatter',
      source: [
        control('scatterCount', 'Count', 'how many spots each strip has, 1–20. Raising it adds spots and keeps the ones already there'),
        control('scatterWidth', 'Width', 'how wide each spot is, from one pixel up to touching the next spot'),
        control('scatterEdge', 'Edge', 'hard through to a fade at the spot’s sides'),
        control('scatterRate', 'Rate', 'how often a spot lands somewhere new, stepped so it sits on the beat. A route onto Mix, Hue, Saturation, Value, Width or Position runs once per spot on this clock'),
        control('scatterSpeed', 'Speed', 'center is still; either side moves each spot from where it landed, steadily up or down the strip'),
        control('scatterRandomize', 'Randomize', 'zero puts every spot on one clock and the whole wall moves as one; full scatters their phases and rates'),
        control('scatterPosition', 'Position', 'how far a spot sits from where it landed, up to half the strip either way; plus is up. Route a build here to move each spot over its life'),
      ],
      amounts: [
        control('scatterMix', 'Mix', 'how much the spots paint over what is below; zero is no scatter. Route a pulse here to flash each spot'),
        control('scatterHue', 'Hue', 'the spot’s hue, measured from the patch’s hue'),
        control('scatterSaturation', 'Saturation', 'the spot’s own saturation, whatever is below it'),
        control('scatterValue', 'Value', 'the spot’s own brightness, whatever is below it; zero paints a dark hole'),
      ],
      shownBy: ['scatterMix'],
    },
    {
      name: 'Field', tone: 'color',
      source: [
        control('fieldForm', 'Form', 'one ramp along the direction, a region sitting on it, or everything but the region departing',
          { kind: 'three', options: threeWayOptions(Protocol.FIELD_FORM,
            { gradient: 'gradient', region: 'region', allButRegion: 'all but region' }) }),
        control('fieldDirection', 'Direction', 'which way the field runs: across the five strips, up a strip, or along a shape from its tip to the end of its tail',
          { kind: 'three', options: threeWayOptions(Protocol.FIELD_DIRECTION,
            { horizontal: 'horizontal', vertical: 'vertical', shape: 'shape' }) }),
        control('fieldCount', 'Count', 'how many regions along the direction', gradientInert),
        control('fieldWidth', 'Width', 'a region’s solid core; each region has the direction divided by Count to itself', gradientInert),
        control('fieldEdge', 'Edge', 'hard-edged regions through to a smooth fade', gradientInert),
        control('fieldSpeed', 'Speed', 'center is still; plus drifts the regions along the direction, minus back, slowest nearest center. Stepped by how long the regions take to move one region along', gradientInert),
      ],
      amounts: [
        control('fieldHue', 'Hue', 'how far the hue moves round the palette, opposite ways at the two ends of a gradient'),
        control('fieldWhite', 'White', 'how far it whitens: both ends of a gradient, the region, or all but the region'),
        control('fieldDark', 'Dark', 'how far it darkens: both ends of a gradient, the region, or all but the region'),
      ],
    },
    {
      name: 'Flow', tone: 'color',
      source: [
        control('flowDensity', 'Density', 'the whole wall moving as one, down to individual pixels'),
        control('flowRate', 'Rate', 'frozen, through a slow ocean swell, to a nervous flicker'),
      ],
      amounts: [
        control('flowHue', 'Hue', 'how far the hue wanders either side of the base'),
        control('flowWhite', 'White', 'how far it whitens where it swings high'),
        control('flowDark', 'Dark', 'how far it darkens where it swings high'),
      ],
    },
    {
      name: 'Light', tone: 'color',
      source: [],
      amounts: [
        control('lightHue', 'Hue', 'how far the brightest part rotates off the base hue'),
        control('lightWhite', 'White', 'how pale the brightest part goes'),
        control('lightDark', 'Dark', 'how far the brightest part darkens'),
      ],
    },
  ];

  const FULL_SATURATION = 255;
  const hueSwatch = (live, hue) => preview().paletteColor(live.palette, hue & 255, FULL_SATURATION);

  const STRIPS = {
    title: '5 strips',
    sections: [[null, [
      control('palette', 'Palette', 'what the hue walks through: the rainbow, or a set of colors that loops',
        { kind: 'pick', options: () => preview().paletteNames().map((name, index) => [index, name]) }),
      control('hue', 'Hue', 'the center hue everything else is measured from, around the palette’s loop',
        { swatch: live => hueSwatch(live, real('hue', live.hue)) }),
      control('saturation', 'Saturation', 'full is a pure hue, zero is white'),
      control('value', 'Value', 'the ceiling everything below scales against'),
    ]]],
  };

  const ARP_MODE_NAMES = {
    together: 'together', sequence: 'sequence', bounce: 'bounce', evensOdds: 'evens / odds',
    pairs: 'pairs', mirror: 'mirror', random: 'random',
  };

  const HUE_LAYOUT_NAMES = {
    gradient: 'gradient', evensOdds: 'evens / odds', pairs: 'pairs', mirror: 'mirror',
    random: 'random per pulse',
  };

  const PARS = {
    title: '4 PARs',
    sections: [
      ['Color', [
        control('parHueOffset', 'Hue offset', 'rotates the PARs off the strips’ hue, as routes move it. Zero matches them',
          { swatch: live => hueSwatch(live, real('hue', live.hue) + real('parHueOffset', live.parHueOffset)) }),
        control('parSaturation', 'Saturation', 'independent of the strips. Zero is white'),
        control('parValue', 'Value', 'the PARs’ master, independent of the strips'),
      ]],
      ['Hue across them', [
        control('parHueLayout', 'Hue layout', 'how the band is laid across the PARs: groups from one end to the other, or a new hue from anywhere in it every time a PAR’s turn comes',
          { kind: 'steps', step: Protocol.hueLayout,
            options: Object.entries(Protocol.HUE_LAYOUT)
              .map(([key, layout]) => [Protocol.hueLayoutValue(layout), HUE_LAYOUT_NAMES[key]]) }),
        control('parHueRange', 'Hue range', 'a band of hue either side of Hue offset. Plus puts the first group at the low end, minus at the high end'),
      ]],
      ['Arpeggiator', [
        control('arpMode', 'Mode', 'how the PARs are grouped, and the order the groups take their turns in',
          { kind: 'steps', step: Protocol.arpMode,
            options: Object.entries(Protocol.ARP_MODE)
              .map(([key, mode]) => [Protocol.arpModeValue(mode), ARP_MODE_NAMES[key]]) }),
        control('arpSpread', 'Spread', 'how far apart the groups take their turns within a pass. Center is all at once; plus runs first to last, minus last to first'),
      ]],
    ],
  };

  const OUTPUTS = [STRIPS, PARS];
  const outputNames = output => output.sections.flatMap(([, names]) => names);
  const cardNames = card => [...card.source, ...card.amounts];

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
      if (name === route.wave) return [Protocol.WAVE_SWELL, Protocol.WAVE_SNAP, Protocol.WAVE_SQUARE];
      if (name === route.phase) return [32, 64, 96];
      if (name === route.ratio) return steps(Protocol.routeRatio);
      return [64];
    }
    const at = value => real(name, value);
    if (name === 'shapeBendAt' || name === 'shapePosition') return [64];
    if (name === 'lfoRate' || name === 'scatterRate' || name === 'fanFrequency' || name === 'shapeSpeed' || name === 'fieldSpeed') return steps(at);
    if (!Protocol.hasTag(name, 'patch')) return [];
    return at(56) < 0 && at(64) === 0 && at(72) > 0 ? [64] : [];
  }

  global.AuroraPatch = {
    PART_NAMES, PART_BLURBS, PARTS, TARGETS, isTarget,
    CC, NAMES, CONTINUOUS, isSwitch, CONTROLS, READOUTS, DEFAULT, NEUTRAL,
    clampToSevenBits, bipolar,
    LFO_PERIOD_NAMES, periodStep, periodValue, TEMPO_DIVISIONS,
    SHAPE, LFO, MODULATORS, OUTPUTS, ROUTES, PLACES, cardNames,
    routable, routableDestination, arpCapable, isCircular, swings, pointsFor,
  };
})(window);
