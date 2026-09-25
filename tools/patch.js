// tools/patch.js — what a patch is, in the editor.
//
// One place that knows the CC map, what every control means, how its value
// reads in words, and how a patch turns into the bytes the wire carries.
// The page above it holds no numbers of its own.
//
// A patch is five parameter sets of 128 bytes plus a head — DESIGN.md
// § "Patch storage". The editor owns the [patch] and [switch] CCs and writes
// zero to every other byte: arriving at a patch writes those CCs through the
// brain's own handlers, and a byte no handler claims is never read.

(function (global) {
  'use strict';

  const PATCH_FORMAT = 1;
  const CC_COUNT = 128;
  const NAME_LEN = 16;
  const SETS = 5;
  const KEYS = 9;
  const PATCH_MAX = 128;

  // PATCH_SET_* in shared/aurora_protocol.h. The order is part of the format.
  const SET_BASE = 0, SET_COLOR = 1, SET_EXTENT = 2, SET_MOTION = 3, SET_ACCENT = 4;

  const SET_NAMES = ['Base', 'Color', 'Extent', 'Motion', 'Accent'];
  const SET_BLURB = [
    'the look itself — what the wall shows with every fader down',
    'the Color fader’s far end. More means hotter, toward white',
    'the Extent fader’s far end. More means more of the wall lit',
    'the Motion fader’s far end. More means faster, harder, more agitated',
    'where holding this patch’s own key pushes. Belongs to no fader',
  ];

  // Every [patch] and [switch] CC in shared/aurora_protocol.h, and nothing
  // else. A name here is the only handle the rest of the editor uses.
  const A = global.AuroraCC;
  const CC = Object.assign({}, A.CC);

  // The route CCs have no enum names of their own — the block is regular, so
  // AURORA_ROUTE_BASE and the field offsets are the whole of it. Their control
  // names are built to match: route3Amount is route 3's amount.
  const ROUTE_FIELDS = Object.keys(A.ROUTE_FIELD);
  const routeName = (r, field) => `route${r}${field[0].toUpperCase()}${field.slice(1)}`;
  const ROUTE_NAMES = [];
  for (let r = 0; r < A.ROUTES; r++) {
    for (const field of ROUTE_FIELDS) {
      const name = routeName(r, field);
      CC[name] = A.routeCC(r, A.ROUTE_FIELD[field]);
      ROUTE_NAMES.push(name);
    }
  }

  // What a patch holds: the enum's [patch] and [switch], plus the routes. The
  // controller's own reports are [ambient] and [gesture] — a fader position is
  // where a hand left it, not something a look holds.
  const NAMES = A.tagged('patch').concat(A.tagged('switch'), ROUTE_NAMES)
    .sort((a, b) => CC[a] - CC[b]);

  // The four with no middle. A morph never moves one, and within a patch the
  // far ends share the base's — DESIGN.md § "Switches belong to the patch"
  // states the authoring rule directly: a patch and its own morph target
  // share switches.
  // A route's destination has no middle either: halfway between two controls
  // is not a control.
  const SWITCHES = A.tagged('switch')
    .concat(Array.from({ length: A.ROUTES }, (_, r) => routeName(r, 'destination')));
  const CONTINUOUS = NAMES.filter(n => !SWITCHES.includes(n));

  const OFF = 0, ON = 127;
  const isOn = v => v >= 64;
  const band3 = v => (v < 43 ? 0 : v < 86 ? 1 : 2);
  const GRADIENT = OFF, REGION = ON;
  const ON_WALL = 0, ON_STRIP = 64, IN_SHAPE = 127;

  const clamp7 = v => (v < 0 ? 0 : v > 127 ? 127 : v | 0);

  // ---- readouts ----------------------------------------------------------
  //
  // The number in every label is the renderer's own, from convert() in
  // shared/render/generator.cpp, so a label says what the wall is doing. What
  // stays here is only the wording. Labels are drawn after tools/preview.js has
  // its module, which is why they reach it at call time.

  const V = () => global.AuroraPreview;
  const real = (name, v) => V().convert(A.CC[name], v);

  const unit = v => v / 127;
  const bip = v => (v < 64 ? (v - 64) / 64 : (v - 64) / 63);
  const pct = v => (v / 127 * 100).toFixed(0) + '%';
  const percent = r => (r * 100).toFixed(0) + '%';
  const ofByte = r => percent(r / 255);
  const sign = r => (r < 0 ? '−' : '+');
  const signed = r => sign(r) + percent(Math.abs(r));
  const signedInt = r => sign(r) + Math.abs(r);
  const beatsPer = rate => (Math.abs(rate) < 0.004 ? '∞' : (1 / Math.abs(rate)).toFixed(1));

  function fanTurns(v) {
    const turns = real('genFanFreq', v) * (V().STRIPS - 1);
    return turns.toFixed(2) + ' turns across the wall';
  }

  // Both ends of a fan amount are the same wall with the wave turned over,
  // so these say how far apart the strips stand and not which way.
  const fanAmount = (name, what) => v => percent(Math.abs(real(name, v)) * 2) + ' ' + what;
  const hueReach = (name, suffix) => v => signedInt(Math.round(real(name, v))) + suffix;
  const swing = name => v => '±' + percent(Math.abs(real(name, v)));

  const LFO_PERIODS = A.LFO_PERIODS;
  const PERIOD_NAMES = {
    16: '16 beats · four bars', 12: '12 beats · three bars', 8: '8 beats · two bars',
    6: '6 beats', 4: '4 beats · one bar', 3: '3 beats', 2: '2 beats · half a bar',
    1.5: '1½ beats', 1: '1 beat', 0.75: '¾ beat', 0.5: '½ beat', 0.375: '⅜ beat', 0.25: '¼ beat',
  };
  const LFO_PERIOD_NAMES = LFO_PERIODS.map(beats => PERIOD_NAMES[beats]);
  const periodStep = v => LFO_PERIODS.indexOf(V().lfoPeriodBeats(v));
  const periodByte = step => Math.round(step * 127 / (LFO_PERIODS.length - 1));

  // AuroraTempoDivision. The value is the enum index, not a 0-127 scale, which
  // is why this control is a list and not a fader.
  const DIVISIONS = [
    [0, 'quarter · one pulse a beat'], [1, 'bar'], [2, 'half'],
    [3, 'eighth'], [4, 'eighth triplet'], [5, 'sixteenth'],
  ];

  // One pattern per control, with only the number moving: a readout that
  // changes shape as the fader moves reflows the rows below it.
  const DERIVED = {
    tempoDivision: v => (DIVISIONS.find(d => d[0] === v) || [0, 'quarter'])[1],

    genWidth: v => percent(real('genWidth', v)),
    genEdge: v => percent(real('genEdge', v)) + ' into the gap',
    genTail: v => { const beats = real('genTail', v);
                    return beats < 0.001 ? 'none' : beats.toFixed(2) + ' beats of afterglow'; },
    genCount: v => real('genCount', v) + ' shapes',
    genPosition: v => signed(real('genPosition', v)) + ' of a cell off center',
    genSpeed: v => { const s = real('genSpeed', v);
                     return sign(s) + Math.abs(s).toFixed(1) + ' px/beat'; },
    genBend: v => signed(real('genBend', v)) + ' bent',
    genBendAt: v => { const at = real('genBendAt', v);
                      return at < 0.005 ? 'at the bottom' : at > 0.995 ? 'at the top'
                           : Math.round(at * 100) + '% up'; },
    genFan: fanAmount('genFan', 'of a cell apart'),
    genFanLfo: fanAmount('genFanLfo', 'of a swell apart'),
    genFanRate: v => '±' + Math.abs(real('genFanRate', v)).toFixed(1)
                     + ' px/beat either side of Speed',
    genFanFreq: fanTurns,
    genFanPhase: v => percent(real('genFanPhase', v)) + ' of a turn',
    genFanRandom: v => percent(real('genFanRandom', v)) + ' scrambled',

    hue: v => real('hue', v) + '/255',
    saturation: v => ofByte(real('saturation', v)),
    value: v => ofByte(real('value', v)),

    genLfoRate: v => PERIOD_NAMES[real('genLfoRate', v)].split(' · ')[0],
    routeAmount: v => signed(bip(v)),
    routeRatio: v => '×' + A.routeRatio(v) + ' the LFO',
    routePhase: v => Math.round(v / 128 * 360) + '° into its cycle',
    // One axis from a build to a stab; the named shapes are notched on the
    // track. See docs/modulation.md § "The fork, settled".
    routeWave: v => v < 32 ? 'builds, ' + pct(v * 4) + ' decay'
                  : v < 64 ? 'swell, ' + pct((v - 32) * 4) + ' toward a snap'
                  : v < 96 ? 'snaps, ' + pct((v - 64) * 4) + ' toward square'
                  : 'hard, ' + pct((v - 96) * 4) + ' shorter',

    scatterRate: v => 'every ' + beatsPer(real('scatterRate', v)) + ' beats',
    scatterCount: v => { const n = real('scatterCount', v);
                         return n + ' cells · ' + (V().PIXELS / n).toFixed(1) + ' px each'; },
    scatterWidth: v => percent(real('scatterWidth', v)) + ' of its cell and its cycle',
    scatterEdge: v => percent(real('scatterEdge', v)) + ' soft',
    scatterStagger: v => percent(real('scatterStagger', v)) + ' apart',
    scatterDrift: v => signed(real('scatterDrift', v)) + ' of its cell',
    scatterLight: v => signed(real('scatterLight', v)),
    scatterHue: hueReach('scatterHue', ' of 255 at the peak'),
    scatterWhite: v => signed(real('scatterWhite', v)),

    placedHue: hueReach('placedHue', ' of 255 at one end'),
    placedWhite: v => signed(real('placedWhite', v)),
    placedDark: v => signed(real('placedDark', v)),
    placedCount: v => real('placedCount', v) + ' regions',
    placedWidth: v => percent(real('placedWidth', v)) + ' of a cell',
    placedEdge: v => percent(real('placedEdge', v)) + ' soft',
    placedSpeed: v => { const r = real('placedSpeed', v);
                        return sign(r) + beatsPer(r) + ' beats per cell'; },

    wanderHue: v => '±' + Math.abs(Math.round(real('wanderHue', v))) + ' of 255',
    wanderWhite: swing('wanderWhite'),
    wanderDark: swing('wanderDark'),
    wanderRate: v => { const r = real('wanderRate', v);
                       return (r < 0.004 ? '∞' : (1 / r).toFixed(0)) + ' beats per cycle'; },
    wanderScale: v => { const c = real('wanderScale', v);
                        return (c <= 0 ? '∞' : (V().PIXELS / c).toFixed(0)) + ' px across'; },

    litHue: hueReach('litHue', ' at the core'),
    litWhite: v => percent(real('litWhite', v)) + ' white at the core',
    litDark: v => signed(real('litDark', v)) + ' at the core',

    washLevel: v => ofByte(real('washLevel', v)),
    washHueOffset: v => '+' + real('washHueOffset', v) + ' of 255',
    washSaturation: v => ofByte(real('washSaturation', v)) + ' of theirs',
  };

  // Every route reads its fields the same way.
  for (let r = 0; r < A.ROUTES; r++) {
    for (const field of ROUTE_FIELDS) {
      DERIVED[routeName(r, field)] =
          DERIVED['route' + field[0].toUpperCase() + field.slice(1)];
    }
  }

  // ---- what a control is -------------------------------------------------
  //
  // kind: 'fader' unless stated. 'pick' carries its own options and is for the
  // handful of values that are an index rather than a position.

  const C = (name, label, hint, extra) =>
    Object.assign({ name, label, hint, kind: 'fader' }, extra || {});

  const CONTROLS = {};
  const define = list => { for (const c of list) CONTROLS[c.name] = c; return list.map(c => c.name); };

  // ---- neutral and default ----------------------------------------------
  //
  // NEUTRAL is what a reset gives: no push. DEFAULT is where a new patch
  // starts, which for a source is a setting you can hear rather than a dead
  // one — a scatter at rate zero is not neutral, it is switched off.

  const NEUTRAL = {
    tempoDivision: 0,
    genWidth: 127, genCount: 0, genEdge: 0, genTail: 0, genPosition: 64, genSpeed: 64, genBend: 64, genBendAt: 64,
    genFan: 64, genFanLfo: 64, genFanRate: 64, genFanFreq: 32, genFanPhase: 0, genFanRandom: 0,
    genBounce: OFF,

    hue: 20, saturation: 100, value: 110,

    genLfoRate: 64,

    scatterRate: 60, scatterCount: 80, scatterWidth: 34, scatterEdge: 40,
    scatterStagger: 110, scatterDrift: 64,
    scatterLight: 64, scatterHue: 64, scatterWhite: 64,

    colorRegion: GRADIENT, colorRuler: ON_STRIP,
    placedHue: 64, placedWhite: 64, placedDark: 64,
    placedCount: 0, placedWidth: 64, placedEdge: 64, placedSpeed: 64,

    wanderHue: 64, wanderWhite: 64, wanderDark: 64, wanderRate: 50, wanderScale: 20,

    litHue: 64, litWhite: 0, litDark: 64,

    washLevel: 127, washHueOffset: 0, washSaturation: 127,
  };
  for (const n of ['slotA', 'slotB', 'slotC', 'slotD', 'slotE',
                   'slotF', 'slotG', 'slotH', 'slotI', 'slotJ']) NEUTRAL[n] = 0;

  // Aimed nowhere, pushing nothing, at the LFO's own rate, on the swell,
  // starting on the bar line.
  for (let r = 0; r < A.ROUTES; r++) {
    NEUTRAL[routeName(r, 'destination')] = 0;
    NEUTRAL[routeName(r, 'amount')] = 64;
    NEUTRAL[routeName(r, 'ratio')] = 0;
    NEUTRAL[routeName(r, 'wave')] = A.GEN_WAVE_SWELL;
    NEUTRAL[routeName(r, 'phase')] = 0;
  }

  // A new patch is one shape traveling across a lit wall: something on the
  // screen the moment it exists, so the first thing you do is change it
  // rather than hunt for why the wall is dark.
  const DEFAULT = Object.assign({}, NEUTRAL, {
    genWidth: 40, genCount: 0, genEdge: 18, genSpeed: 80,
  });

  // ---- the surface -------------------------------------------------------
  //
  // Carrier, modulators, outputs. The shape lane and the outputs hold only
  // what the wall shows with nothing pushing on it; everything that pushes is
  // a source with its amounts beside it, and every source is drawn the same
  // way whatever it reaches — the placed field, the wander and the light level
  // all land on the same three qualities and add.

  const SHAPE = {
    name: 'Shape',
    groups: [
      {
        key: 'form', title: 'Form',
        controls: define([
          C('genCount', 'Count', 'how many shapes along the strip, 1\u201320'),
          C('genWidth', 'Width', 'the solid core, as a proportion of one cell'),
          C('genEdge', 'Edge', 'how far the glow reaches into the gap, both sides'),
          C('genTail', 'Tail', 'how long a pixel glows after a moving shape passes it'),
        ]),
      },
      {
        key: 'travel', title: 'Travel',
        controls: define([
          C('genPosition', 'Position', 'where a still pattern stands in its cell'),
          C('genSpeed', 'Speed', 'center is still; either side travels'),
          C('genBend', 'Bend', 'travel slowed and sped by where a shape is; plus is fastest where Bend at points, minus slowest there'),
          C('genBendAt', 'Bend at', 'where along the strip the bend peaks, bottom to top; bouncing, along each shape\u2019s own cell'),
        ]),
        switches: define([
          C('genBounce', 'Bounce', 'turn at the cell\u2019s edge instead of wrapping',
            { kind: 'two', options: [[OFF, 'wrap'], [ON, 'bounce']] }),
        ]),
      },
      {
        key: 'genFan', title: 'Fan',
        controls: define([
          C('genFanFreq', 'Frequency', 'all five alike \u2192 every strip opposite its neighbors'),
          C('genFanPhase', 'Phase', 'where the wave sits on the strips: a staircase through a chevron'),
          C('genFanRandom', 'Randomize', 'the wave \u2192 a fixed draw per strip'),
          C('genFan', 'Position', 'how far apart the five strips stand in their cells'),
          C('genFanRate', 'Rate', 'how far apart their speeds stand, either side of Speed'),
          C('genFanLfo', 'LFO', 'how far apart they stand in the LFO\u2019s cycle'),
        ]),
      },
    ],
  };

  // ---- the modulators ----------------------------------------------------
  //
  // Amounts live at the source rather than at the target, settled in
  // docs/generator.md § Modulation: most of these destinations are not
  // controls at all — how lit a pixel is, what color it is — so an amount
  // beside the target would mean inventing rows for things that are not
  // controls. What that costs is the view from the target’s end, and the
  // destination table below is what buys it back.

  const ROUTES = Array.from({ length: A.ROUTES }, (_, r) => ({
    key: 'route' + r,
    name: 'Route ' + (r + 1),
    destination: routeName(r, 'destination'),
    amount: routeName(r, 'amount'),
    ratio: routeName(r, 'ratio'),
    wave: routeName(r, 'wave'),
    phase: routeName(r, 'phase'),
  }));

  for (const route of ROUTES) {
    define([
      C(route.amount, 'Amount', 'how far, as a share of the distance left; plus is toward the top, minus toward the bottom. On a rate, how wide the swing either side, and which half comes first'),
      C(route.ratio, 'Ratio', 'whole multiples of the LFO'),
      C(route.wave, 'Wave', 'a build \u2192 swell \u2192 snap \u2192 hard half-bar \u2192 stab'),
      C(route.phase, 'Phase', 'how far into its own cycle the wave starts after the bar line'),
    ]);
  }

  // Under a gradient placedAt returns on its first line, so these four reach
  // nothing at all — and gradient is where the switch starts. They are shown
  // and dimmed rather than hidden: a control that vanishes reads as a bug, and
  // the reason is short enough to say.
  const gradientInert = {
    inertWhen: s => !isOn(s.colorRegion),
    inertWhy: 'a gradient spans its ruler once, so there is nothing here to repeat, size or move',
  };

  const LFO = {
    key: 'lfo', name: 'LFO', tone: 'lfo',
    source: define([
      C('genLfoRate', 'Rate', 'how often the swell lands. Stepped, so it can sit on the bar'),
    ]),
  };

  const MODULATORS = [
    {
      key: 'scatter', name: 'The scatter', tone: 'scatter',
      source: define([
        C('scatterRate', 'Rate', 'how often a cell relights'),
        C('scatterCount', 'Count', 'cells along a strip. The same unit as the shape lane\u2019s Count'),
        C('scatterWidth', 'Width', 'the spot\u2019s core on both axes at once: how much of its cell it covers, and how much of its cycle it is lit'),
        C('scatterEdge', 'Edge', 'hard through to a fade \u2014 in space and in time alike'),
        C('scatterStagger', 'Stagger', 'zero puts every cell on one clock and the whole wall flashes as one; full scatters their phases and rates'),
        C('scatterDrift', 'Drift', 'how far a spot slides across its own cell over its life; plus is up the strip, minus down'),
      ]),
      amounts: define([
        C('scatterLight', 'Brightness', 'plus is toward full light, minus toward dark'),
        C('scatterHue', 'Hue', 'how far the hue departs where a spot is'),
        C('scatterWhite', 'To white', 'plus is toward white, minus toward a pure hue'),
      ]),
    },
    {
      key: 'placed', name: 'The placed field', tone: 'color',
      switches: define([
        C('colorRegion', 'Primitive', 'one ramp across the ruler, or a bump sitting on it',
          { kind: 'two', options: [[GRADIENT, 'gradient'], [REGION, 'region']] }),
        C('colorRuler', 'Ruler', 'what the position is measured against',
          { kind: 'three',
            options: [[ON_WALL, 'across the strips'], [ON_STRIP, 'along a strip'],
                      [IN_SHAPE, 'within a shape']] }),
      ]),
      source: define([
        C('placedCount', 'Count', 'how many regions along the ruler', gradientInert),
        C('placedWidth', 'Width', 'a region\u2019s solid core, as a proportion of one cell', gradientInert),
        C('placedEdge', 'Edge', 'hard-edged cell through to a smooth fade', gradientInert),
        C('placedSpeed', 'Speed', 'center is still; plus drifts the regions along the ruler, minus back', gradientInert),
      ]),
      amounts: define([
        C('placedHue', 'Hue', 'how far one end of the ruler departs from the base hue'),
        C('placedWhite', 'To white', 'how far one end departs; plus is toward white, minus toward a pure hue'),
        C('placedDark', 'Dark', 'how far one end departs; minus is toward dark, plus toward full light'),
      ]),
    },
    {
      key: 'wander', name: 'The wander', tone: 'color',
      source: define([
        C('wanderRate', 'Rate', 'frozen, through a slow ocean swell, to a nervous flicker'),
        C('wanderScale', 'Density', 'the whole wall moving as one, down to individual pixels'),
      ]),
      amounts: define([
        C('wanderHue', 'Hue', 'how far the hue wanders either side of the base'),
        C('wanderWhite', 'To white', 'how far whiteness wanders'),
        C('wanderDark', 'Dark', 'how far darkness wanders'),
      ]),
    },
    {
      key: 'lit', name: 'The light level', tone: 'color',
      source: [],
      amounts: define([
        C('litHue', 'Hue', 'how far the brightest part rotates off the base hue'),
        C('litWhite', 'To white', 'how pale the brightest part goes'),
        C('litDark', 'Dark', 'how the brightest part sits against the base for brightness; minus is toward dark, plus toward full light'),
      ]),
    },
  ];

  const STRIPS = {
    controls: define([
      C('hue', 'Hue', 'the center hue everything else is measured from'),
      C('saturation', 'Saturation', 'full is a pure hue, zero is white'),
      C('value', 'Brightness', 'the ceiling everything below scales against'),
    ]),
  };

  const PARS = {
    controls: define([
      C('washLevel', 'Level', 'the PARs\u2019 master, independent of the strips'),
      C('washHueOffset', 'Hue offset', 'rotates the PARs off the strips\u2019 hue. Zero matches them'),
      C('washSaturation', 'Saturation', 'scales the PARs down from the strips\u2019 saturation. Full matches them, zero is white'),
    ]),
  };

  const TIMING = {
    controls: define([
      C('tempoDivision', 'Tempo division', 'what one tempo pulse stands for. Every rate below scales with it',
        { kind: 'pick', options: DIVISIONS }),
    ]),
  };

  global.AuroraPatch = {
    PATCH_FORMAT, CC_COUNT, NAME_LEN, SETS, KEYS, PATCH_MAX,
    SET_BASE, SET_COLOR, SET_EXTENT, SET_MOTION, SET_ACCENT, SET_NAMES, SET_BLURB,
    CC, NAMES, SWITCHES, CONTINUOUS, CONTROLS, DERIVED, NEUTRAL, DEFAULT,
    OFF, ON, isOn, band3, GRADIENT, REGION, ON_WALL, ON_STRIP, IN_SHAPE, clamp7,
    unit, bip, LFO_PERIODS, LFO_PERIOD_NAMES, periodStep, periodByte,
    DIVISIONS,
    SHAPE, LFO, MODULATORS, ROUTES, STRIPS, PARS, TIMING,
  };
})(window);
