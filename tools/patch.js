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
  // Mirrors of the mappings in shared/render/generator.cpp, for labels only.
  // If those change these lie until they are changed to match.

  const unit = v => v / 127;
  const bip = v => (v < 64 ? (v - 64) / 64 : (v - 64) / 63);
  const ccCount = v => Math.min(20, Math.max(1, Math.round(Math.pow(20, v / 127))));
  const pct = v => (v / 127 * 100).toFixed(0) + '%';
  const signedPct = (v, up, down) => {
    const r = bip(v);
    if (Math.abs(r) < 0.01) return 'not reached';
    return (Math.abs(r) * 100).toFixed(0) + '% ' + (r > 0 ? up : down);
  };
  // Stepped, so that a strip the wave reads zero at is exactly still rather
  // than crawling. Eighths of a turn across the wall, 17 positions.
  function fanTurns(v) {
    const eighths = Math.round(v * 16 / 127);
    if (eighths === 0) return 'all five alike';
    if (eighths === 16) return 'every strip opposite its neighbors';
    const turns = (eighths / 8).toFixed(3).replace(/0+$/, '').replace(/\.$/, '');
    return turns + (turns === '1' ? ' turn' : ' turns') + ' across the wall';
  }

  const fanAmount = (v, unit) => {
    const r = Math.abs(bip(v));
    return r < 0.01 ? 'every strip together' : (r * 100).toFixed(0) + '% ' + unit;
  };
  const hueAmount = v => {
    const r = Math.round(bip(v) * 128);
    return r === 0 ? 'not reached' : (r > 0 ? '+' : '') + r + ' of 255 at the peak';
  };

  const PULSE_PERIODS = [16, 12, 8, 6, 4, 3, 2, 1.5, 1, 0.75, 0.5, 0.375, 0.25];
  const PULSE_PERIOD_NAMES = [
    '16 beats · four bars', '12 beats · three bars', '8 beats · two bars',
    '6 beats', '4 beats · one bar', '3 beats', '2 beats · half a bar',
    '1½ beats', '1 beat', '¾ beat', '½ beat', '⅜ beat', '¼ beat',
  ];
  const periodStep = v => Math.min(PULSE_PERIODS.length - 1,
    Math.floor((v * (PULSE_PERIODS.length - 1) + 63) / 127));
  const periodByte = step => Math.round(step * 127 / (PULSE_PERIODS.length - 1));

  // AuroraTempoDivision. The value is the enum index, not a 0-127 scale, which
  // is why this control is a list and not a fader.
  const DIVISIONS = [
    [0, 'quarter · one pulse a beat'], [1, 'bar'], [2, 'half'],
    [3, 'eighth'], [4, 'eighth triplet'], [5, 'sixteenth'],
  ];

  const DERIVED = {
    tempoDivision: v => (DIVISIONS.find(d => d[0] === v) || [0, 'quarter'])[1],

    genWidth: pct, genEdge: v => pct(v) + ' into the gap', genTail: v => pct(v) + ' of the gap',
    genCount: v => ccCount(v) + ' shapes',
    genPosition: v => Math.abs(bip(v)) < 0.02 ? 'center of the cell'
                 : (bip(v) * 50).toFixed(0) + '% of a cell off center',
    genSpeed: v => { const x = (v - 64) / 63; const s = Math.sign(x) * x * x * 60;
                  return Math.abs(s) < 0.05 ? 'still' : s.toFixed(1) + ' px/beat'; },
    // Both ends of a fan amount are the same wall with the wave turned over,
    // so these say how far apart the strips stand and not which way.
    genFan: v => fanAmount(v, 'of a cell apart'),
    genFanPulse: v => fanAmount(v, 'of a swell apart'),
    genFanRate: v => { const x = (v - 64) / 63; const r = x * x * 60;
                    return r < 0.05 ? 'every strip at Speed'
                         : '\u00b1' + r.toFixed(1) + ' px/beat either side of Speed'; },
    genFanFreq: v => fanTurns(v),
    genFanPhase: v => (v / 128 * 100).toFixed(0) + '% of a turn',
    genFanRandom: v => v === 0 ? 'the wave' : v > 125 ? 'a fixed draw per strip'
                    : pct(v) + ' scrambled',

    hue: v => Math.round(v / 127 * 250) + '/255',
    saturation: pct, value: pct,

    genPulseRate: v => PULSE_PERIOD_NAMES[periodStep(v)],
    routeDestination: v => v === 0 ? 'not aimed' : (A.NAME_BY_CC[v] || 'CC ' + v),
    routeAmount: v => Math.abs(bip(v)) < 0.01 ? 'nothing'
                    : signedPct(v, 'toward the top', 'toward the bottom'),
    routeRatio: v => '\u00d7' + A.routeRatio(v) + ' the clock',
    // One axis from a build to a stab, with the named shapes on values a
    // fader lands on exactly. See docs/modulation.md § "The fork, settled".
    routeWave: v => v === 0 ? 'builds, drops on the bar'
                  : v === 32 ? 'swell'
                  : v === 64 ? 'snaps, decays across the bar'
                  : v === 96 ? 'hard half-bar'
                  : v === 127 ? 'stab'
                  : v < 32 ? 'builds, ' + pct(v * 4) + ' decay'
                  : v < 64 ? 'swell, ' + pct((v - 32) * 4) + ' toward a snap'
                  : v < 96 ? 'snaps, ' + pct((v - 64) * 4) + ' toward square'
                  : 'hard, ' + pct((v - 96) * 4) + ' shorter',

    // The rate is squared in the renderer, so a linear readout here would be
    // wrong over most of the travel.
    scatterRate: v => { const r = unit(v) ** 2 * 4;
                        return r < 0.01 ? 'frozen'
                             : r >= 1 ? r.toFixed(1) + ' a beat'
                             : 'every ' + (1 / r).toFixed(1) + ' beats'; },
    scatterCount: v => { const n = ccCount(v);
                         return n + ' cells · ' + (45 / n).toFixed(1) + ' px each'; },
    scatterWidth: v => pct(v) + ' of its cell and its cycle',
    scatterEdge: v => v < 6 ? 'hard' : pct(v) + ' soft',
    scatterStagger: v => v === 0 ? 'every cell on one clock' : pct(v) + ' apart',
    scatterDrift: v => { const r = bip(v);
                         return Math.abs(r) < 0.01 ? 'stands still'
                              : (Math.abs(r) * 100).toFixed(0) + '% of its cell, '
                                + (r > 0 ? 'up the strip' : 'down the strip'); },
    scatterLight: v => signedPct(v, 'toward full', 'toward dark'),
    scatterHue: hueAmount,
    scatterWhite: v => signedPct(v, 'toward white', 'toward a pure hue'),

    placedHue: v => { const r = Math.round(bip(v) * 128);
                      return r === 0 ? 'flat' : (r > 0 ? '+' : '') + r + ' of 255 at one end'; },
    placedWhite: v => signedPct(v, 'to white', 'to pure'),
    placedDark: v => { const r = bip(v);
                       if (Math.abs(r) < 0.01) return 'flat';
                       return r < 0 ? 'down to ' + (100 * Math.pow(0.02, -r)).toFixed(1) + '%'
                                    : (r * 100).toFixed(0) + '% toward full'; },
    placedCount: v => ccCount(v) + ' regions',
    placedWidth: v => pct(v) + ' of a cell',
    placedEdge: v => v < 6 ? 'hard' : pct(v) + ' soft',
    placedSpeed: v => { const x = (v - 64) / 63; const r = Math.sign(x) * x * x;
                        return Math.abs(r) < 0.002 ? 'still'
                             : (1 / Math.abs(r)).toFixed(1) + ' beats per cell'
                               + (r < 0 ? ' back' : ''); },

    wanderHue: v => { const r = Math.round(bip(v) * 128);
                      return r === 0 ? 'off' : '±' + Math.abs(r) + ' of 255'; },
    wanderWhite: v => Math.abs(bip(v)) < 0.01 ? 'off'
                    : '±' + (Math.abs(bip(v)) * 100).toFixed(0) + '%',
    wanderDark: v => Math.abs(bip(v)) < 0.01 ? 'off'
                   : '±' + (Math.abs(bip(v)) * 100).toFixed(0) + '%',
    // Squared in the renderer since 2026-09-23.
    wanderRate: v => { const r = unit(v) ** 2 * 0.5;
                       return r < 0.004 ? 'frozen' : (1 / r).toFixed(0) + ' beats per cycle'; },
    wanderScale: v => { const c = 0.12 * Math.pow(180, unit(v));
                        return c < 0.35 ? 'the whole wall as one'
                             : (45 / c).toFixed(0) + ' px across'; },

    litHue: v => { const r = Math.round(bip(v) * 64);
                   return r === 0 ? 'off' : (r > 0 ? '+' : '') + r + ' at the core'; },
    litWhite: v => v === 0 ? 'off' : pct(v) + ' white at the core',
    litDark: v => { const r = bip(v);
                    if (Math.abs(r) < 0.01) return 'off';
                    return r < 0 ? 'core down to ' + (100 * Math.pow(0.02, -r)).toFixed(1) + '%'
                                 : 'core ' + (r * 100).toFixed(0) + '% toward full'; },

    washLevel: pct,
    washHueOffset: v => v === 0 ? 'matches the strips' : '+' + Math.round(v / 127 * 255) + ' of 255',
    washSaturation: v => v === 127 ? 'matches the strips' : v === 0 ? 'white'
                       : pct(v) + ' of theirs',
  };

  // Every route reads its four the same way.
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
    genWidth: 127, genCount: 0, genEdge: 0, genTail: 0, genPosition: 64, genSpeed: 64,
    genFan: 64, genFanPulse: 64, genFanRate: 64, genFanFreq: 32, genFanPhase: 0, genFanRandom: 0,
    genAlternate: OFF, genBounce: OFF,

    hue: 20, saturation: 100, value: 110,

    genPulseRate: 64,

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

  // Aimed nowhere, pushing nothing, at the clock's own rate, on the swell.
  for (let r = 0; r < A.ROUTES; r++) {
    NEUTRAL[routeName(r, 'destination')] = 0;
    NEUTRAL[routeName(r, 'amount')] = 64;
    NEUTRAL[routeName(r, 'ratio')] = 0;
    NEUTRAL[routeName(r, 'wave')] = A.GEN_WAVE_SWELL;
  }

  // A new patch is one shape traveling across a lit wall: something on the
  // screen the moment it exists, so the first thing you do is change it
  // rather than hunt for why the wall is dark.
  const DEFAULT = Object.assign({}, NEUTRAL, {
    genWidth: 40, genCount: 0, genEdge: 18, genSpeed: 80,
  });

  // ---- the surface -------------------------------------------------------
  //
  // Carrier, modulators, outputs. The two lanes hold only what the wall shows
  // with nothing pushing on it; everything that pushes is a source with its
  // amounts beside it, and every source is drawn the same way whatever it
  // reaches — the placed field, the wander and the light level all land on the
  // same three qualities and add.

  const LANES = [
    {
      key: 'shape', name: 'Shape',
      does: 'decides whether a pixel is lit, and how much',
      groups: [
        {
          key: 'form', title: 'Form',
          controls: define([
            C('genCount', 'Count', 'how many shapes along the strip, 1\u201320'),
            C('genWidth', 'Width', 'the solid core, as a proportion of one cell'),
            C('genEdge', 'Edge', 'how far the glow reaches into the gap, both sides'),
            C('genTail', 'Tail', 'how far the trail reaches behind, into the gap'),
          ]),
        },
        {
          key: 'travel', title: 'Travel',
          controls: define([
            C('genPosition', 'Position', 'where a still pattern stands in its cell'),
            C('genSpeed', 'Speed', 'center is still; either side travels'),
          ]),
          switches: define([
            C('genAlternate', 'Alternate', 'the odd strips run the journey backwards',
              { kind: 'two', options: [[OFF, 'together'], [ON, 'genAlternate']] }),
            C('genBounce', 'Bounce', 'turn at the cell\u2019s edge instead of wrapping',
              { kind: 'two', options: [[OFF, 'wrap'], [ON, 'genBounce']] }),
          ]),
          note: 'Bring Speed to a stop and the pattern walks home to Position over a beat or two, so a patch saved comes back to the same place. Under bounce the swing is anchored to the cell and Position does nothing.',
        },
        {
          key: 'genFan', title: 'Fan',
          controls: define([
            C('genFanFreq', 'Frequency', 'all five alike \u2192 every strip opposite its neighbors'),
            C('genFanPhase', 'Phase', 'where the wave sits on the strips: a staircase through a chevron'),
            C('genFanRandom', 'Randomize', 'the wave \u2192 a fixed draw per strip'),
            C('genFan', 'Position', 'how far apart the five strips stand in their cells'),
            C('genFanRate', 'Rate', 'how far apart their speeds stand, either side of Speed'),
            C('genFanPulse', 'Pulse', 'how far apart they stand in the swell'),
          ]),
          note: 'One wave running across the five strips, and three amounts aiming it at three places \u2014 so a wall of staggered bars can strobe in unison. Frequency at the top puts every strip opposite its neighbors, which is alternate; there the phase only scales how deep that is, and a quarter turn either side of it the fan goes quiet. Speed is what the strip the wave reads zero at travels at, and Rate is how far the others differ from it.',
        },
      ],
    },
    {
      key: 'color', name: 'Color',
      does: 'decides what color a lit pixel is',
      groups: [
        {
          key: 'base', title: 'The three faders',
          controls: define([
            C('hue', 'Hue', 'the center hue everything else is measured from'),
            C('saturation', 'Saturation', 'full is a pure hue, zero is white'),
            C('value', 'Brightness', 'the ceiling everything below scales against'),
          ]),
          note: 'A color is hue, whiteness and darkness, and every source below is a push on those three measured from here. With every amount at neutral the wall is exactly this color.',
        },
      ],
    },
  ];

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
  }));

  for (const route of ROUTES) {
    define([
      C(route.destination, 'Pushes', 'which control this route reaches'),
      C(route.amount, 'Amount', 'how far, as a share of the distance left'),
      C(route.ratio, 'Ratio', 'whole multiples of the clock'),
      C(route.wave, 'Wave', 'a build \u2192 swell \u2192 snap \u2192 hard half-bar \u2192 stab'),
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

  const MODULATORS = [
    {
      key: 'pulse', name: 'The clock', tone: 'pulse',
      when: 'regular in time, and nowhere on the wall',
      source: define([
        C('genPulseRate', 'Rate', 'how often the swell lands. Stepped, so it can sit on the bar'),
      ]),
      routes: ROUTES,
      note: 'One clock, and eight routes off it. A route says which control it reaches, how far, at what whole multiple of the clock, and what wave does the pushing. It cannot reach a rate: every rate feeds a running total, so a push on one would move the wall permanently instead of returning it. Two routes on one control add, and the sum stops at the limit.',
    },
    {
      key: 'scatter', name: 'The scatter', tone: 'scatter',
      when: 'random in space and in time',
      source: define([
        C('scatterRate', 'Rate', 'how often a cell relights'),
        C('scatterCount', 'Count', 'cells along a strip. The same unit as the shape lane\u2019s Count'),
        C('scatterWidth', 'Width', 'the spot\u2019s core on both axes at once: how much of its cell it covers, and how much of its cycle it is lit'),
        C('scatterEdge', 'Edge', 'hard through to a fade \u2014 in space and in time alike'),
        C('scatterStagger', 'Stagger', 'zero puts every cell on one clock and the whole wall flashes as one; full scatters their phases and rates'),
        C('scatterDrift', 'Drift', 'how far, and which way, a spot slides across its own cell over its life'),
      ]),
      amounts: define([
        C('scatterLight', 'Brightness', 'toward full light, or toward dark'),
        C('scatterHue', 'Hue', 'how far the hue departs where a spot is'),
        C('scatterWhite', 'To white', 'toward white, or toward a pure hue'),
      ]),
      note: 'A grid of cells along each strip, each with its own clock, each lighting a spot that appears, holds, fades, and may slide across its cell as it does. It pushes what the shape lane left, so it needs a gap to light and light to darken \u2014 which is also why a spot inside an already-full shape is invisible and the shape covers it with no occlusion rule anywhere. Moving Stagger re-keys every cell, so everything in flight jumps. It replaces jitter, which no longer has a control anywhere.',
    },
    {
      key: 'placed', name: 'The placed field', tone: 'color',
      when: 'aimed in space, still or drifting',
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
        C('placedSpeed', 'Speed', 'center is still; either side drifts the regions along the ruler', gradientInert),
      ]),
      amounts: define([
        C('placedHue', 'Hue', 'how far one end of the ruler departs from the base hue'),
        C('placedWhite', 'To white', 'which end departs toward white, and how far'),
        C('placedDark', 'Dark', 'which end departs toward dark, and how far'),
      ]),
      note: 'The only source you aim. A gradient runs one way across its ruler with the base color at the center, so an amount is how far one end departs and the two ends land twice that apart. A region is a bump: base, departure, back to base. These two switches and the four controls beside them shape this source and reach nothing else.',
    },
    {
      key: 'wander', name: 'The wander', tone: 'color',
      when: 'smooth in space and in time, and never the same twice',
      source: define([
        C('wanderRate', 'Rate', 'frozen, through a slow ocean swell, to a nervous flicker'),
        C('wanderScale', 'Density', 'the whole wall moving as one, down to individual pixels'),
      ]),
      amounts: define([
        C('wanderHue', 'Hue', 'how far the hue wanders either side of the base'),
        C('wanderWhite', 'To white', 'how far whiteness wanders'),
        C('wanderDark', 'Dark', 'how far darkness wanders'),
      ]),
      note: 'Two terms whose rates sit at the golden ratio, so they can never come back into step and the wall never repeats. That is built in rather than dialed \u2014 setting how far apart two speeds sit is operating the mechanism rather than the look. There is nothing to aim it at; that is what the placed field is for.',
    },
    {
      key: 'lit', name: 'The light level', tone: 'color',
      when: 'read off what the shape lane left',
      source: [],
      amounts: define([
        C('litHue', 'Hue', 'how far the brightest part rotates off the base hue'),
        C('litWhite', 'To white', 'how pale the brightest part goes'),
        C('litDark', 'Dark', 'how the brightest part sits against the base for brightness'),
      ]),
      note: 'The one source with no controls of its own: its value is how lit the shape lane left a pixel, so a comet\u2019s tail cools instead of only dimming. It is also the only source that reaches the pulse and the scatter, since neither of those has a position for a ruler to measure. It reads the shape\u2019s own profile and never the color lane\u2019s output \u2014 feed that back and pulling the wall down for a quiet verse would slide its hue.',
    },
  ];

  // ---- reading the matrix the other way ----------------------------------

  // The soldered amounts are fixed, so they are a table. What a route reaches
  // is whatever it happens to be aimed at, so it is a lookup — routesAimedAt
  // below. A destination carries `cc` when it is a control a route can name.
  const DESTINATIONS = [
    { key: 'light', name: 'the strips\u2019 brightness', lane: 'shape',
      cc: CC.value, from: { scatter: 'scatterLight' } },
    { key: 'genWidth', name: 'the shape\u2019s width', lane: 'shape',
      cc: CC.genWidth, from: {} },
    { key: 'hue', name: 'hue', lane: 'color',
      cc: CC.hue,
      from: { scatter: 'scatterHue', placed: 'placedHue',
              wander: 'wanderHue', lit: 'litHue' } },
    { key: 'white', name: 'whiteness', lane: 'color',
      from: { scatter: 'scatterWhite', placed: 'placedWhite',
              wander: 'wanderWhite', lit: 'litWhite' } },
    { key: 'dark', name: 'darkness', lane: 'color',
      from: { placed: 'placedDark', wander: 'wanderDark', lit: 'litDark' } },
    { key: 'parLevel', name: 'the PARs\u2019 level', lane: 'out',
      cc: CC.washLevel, from: {} },
    { key: 'parHue', name: 'the PARs\u2019 hue', lane: 'out',
      cc: CC.washHueOffset, from: {} },
    { key: 'parSat', name: 'the PARs\u2019 saturation', lane: 'out',
      cc: CC.washSaturation, from: {} },
  ];

  // Which routes are aimed at a control, for the mark beside its slider.
  function routesAimedAt(state, cc) {
    const aimed = [];
    if (cc === undefined) return aimed;
    for (let r = 0; r < A.ROUTES; r++) {
      if (state[routeName(r, 'destination')] !== cc) continue;
      const amount = state[routeName(r, 'amount')];
      if (amount === undefined || Math.abs(bip(amount)) < 0.01) continue;
      aimed.push({ route: r, amount, ratio: state[routeName(r, 'ratio')],
                   wave: state[routeName(r, 'wave')] });
    }
    return aimed;
  }

  const PARS = {
    controls: define([
      C('washLevel', 'Level', 'the PARs\u2019 master, independent of the strips'),
      C('washHueOffset', 'Hue offset', 'rotates the PARs off the strips\u2019 hue. Zero matches them'),
      C('washSaturation', 'Saturation', 'scales the PARs down from the strips\u2019 saturation. Full matches them, zero is white'),
    ]),
    note: 'A PAR is one position with no length, so the shape lane cannot reach it: count, width, edge, tail, speed and fan all describe places along a strip. What a patch holds for them is a relationship to the strips rather than a second look. A route can reach these three, and all four PARs take the clock\u2019s unfanned reading.',
  };

  const TIMING = {
    controls: define([
      C('tempoDivision', 'Tempo division', 'what one tempo pulse stands for. Every rate below scales with it',
        { kind: 'pick', options: DIVISIONS }),
    ]),
    note: 'Part of every parameter set, so a far end may sit at another division and a morph will step through the ones between. A half-time look is a real musical idea and a song that wants one wants it for every patch in that song.',
  };

;

  // ---- starting points ---------------------------------------------------

  // Carries the fan's wave as well as its amounts, so an anchor that fans
  // nothing cannot inherit a chevron from whatever was up before it. A
  // full-width look's strips cannot be seen to stand apart, which is why
  // Wave, Chase and Stutter below spend their amount on the swell.
  const SHAPE_FLAT = {
    genWidth: 127, genCount: 0, genEdge: 0, genTail: 0, genPosition: 64, genSpeed: 64,
    genFan: 64, genFanPulse: 64, genFanRate: 64, genFanFreq: 32, genFanPhase: 0, genFanRandom: 0,
    genAlternate: OFF, genBounce: OFF,
  };

  const ANCHORS = {
    Fill:       { genWidth: 127, genCount: 0, genEdge: 0, genTail: 0, genSpeed: 64, genFan: 64, pulseDepth: 0, genPulseRate: 64, pulseWave: 32 },
    Sweep:      { genWidth: 40, genCount: 0, genEdge: 18, genTail: 0, genSpeed: 80, genFan: 64, pulseDepth: 0, genPulseRate: 64, pulseWave: 32 },
    Rain:       { genWidth: 40, genCount: 0, genEdge: 18, genTail: 74, genSpeed: 80, genFan: 100, pulseDepth: 0, genPulseRate: 64, pulseWave: 32 },
    CrossSweep: { genWidth: 40, genCount: 0, genEdge: 18, genTail: 0, genSpeed: 80, genFan: 64, genAlternate: ON, pulseDepth: 0, genPulseRate: 64, pulseWave: 32 },
    Bars:       { genWidth: 25, genCount: 0, genEdge: 15, genTail: 0, genSpeed: 88, genFan: 64, genBounce: ON, pulseDepth: 0, genPulseRate: 64, pulseWave: 32 },
    Breathe:    { genWidth: 127, genCount: 0, genEdge: 0, genTail: 0, genSpeed: 64, genFan: 64, pulseDepth: 100, genPulseRate: 30, pulseWave: 32 },
    Wave:       { genWidth: 127, genCount: 0, genEdge: 0, genTail: 0, genSpeed: 64, genFan: 64, genFanPulse: 104, pulseDepth: 100, genPulseRate: 30, pulseWave: 32 },
    Chase:      { genWidth: 127, genCount: 0, genEdge: 0, genTail: 0, genSpeed: 64, genFan: 64, genFanPulse: 114, pulseDepth: 127, genPulseRate: 55, pulseWave: 88 },
    Comet:      { genWidth: 30, genCount: 0, genEdge: 30, genTail: 99, genSpeed: 80, genFan: 114, pulseDepth: 0, genPulseRate: 55, pulseWave: 32 },
    Strobe:     { genWidth: 127, genCount: 0, genEdge: 0, genTail: 0, genSpeed: 64, genFan: 64, pulseDepth: 127, genPulseRate: 100, pulseWave: 96 },
    Stutter:    { genWidth: 127, genCount: 0, genEdge: 0, genTail: 0, genSpeed: 64, genFan: 64, genFanPulse: 88, genAlternate: ON, pulseDepth: 127, genPulseRate: 100, pulseWave: 96 },
  };

  // The looks the fan rework was built against. The first is the patch that
  // could not be built before it: the five strips standing apart in their
  // cells while the swell lands on all of them together.
  //
  // Count 68 is five bars to a strip. Rate 96 is ±30 px/beat at the ends of
  // the wave, so Speed at 20 — about −29 px/beat — is what stands the outer
  // strips still in Hypno together while the center runs.
  const FAN_LOOKS = {
    'Bars, unison strobe': { genWidth: 40, genCount: 68, genEdge: 0, genTail: 0, genSpeed: 64, genFan: 100,
                             pulseDepth: 127, genPulseRate: 100, pulseWave: 96 },
    'Diagonal bars':  { genWidth: 25, genCount: 0, genEdge: 10, genTail: 0, genSpeed: 64, genFan: 114, genFanPhase: 0 },
    'Chevron \u2227':     { genWidth: 25, genCount: 0, genEdge: 10, genTail: 0, genSpeed: 64, genFan: 114, genFanPhase: 32 },
    'Chevron \u2228':     { genWidth: 25, genCount: 0, genEdge: 10, genTail: 0, genSpeed: 64, genFan: 14, genFanPhase: 32 },
    'Comets':         { genWidth: 12, genCount: 0, genEdge: 8, genTail: 99, genSpeed: 48, genFan: 127, genFanRandom: 127 },
    'Shooting stars': { genWidth: 10, genCount: 0, genEdge: 0, genTail: 90, genSpeed: 40, genFanRate: 105, genFanRandom: 127 },
    'Hypno outer':    { genWidth: 40, genCount: 68, genEdge: 0, genTail: 0, genSpeed: 64, genFanRate: 108, genFanPhase: 0 },
    'Hypno together': { genWidth: 40, genCount: 68, genEdge: 0, genTail: 0, genSpeed: 20, genFanRate: 108, genFanPhase: 32 },
    'Hypno center':   { genWidth: 40, genCount: 68, genEdge: 0, genTail: 0, genSpeed: 64, genFanRate: 108, genFanPhase: 32 },
    'Alternate by rate': { genWidth: 40, genCount: 68, genEdge: 0, genTail: 0, genSpeed: 64, genFanRate: 108,
                           genFanFreq: 127, genFanPhase: 64 },
  };

  const COLOR_FLAT = {
    colorRegion: GRADIENT, colorRuler: ON_STRIP,
    placedHue: 64, placedWhite: 64, placedDark: 64,
    placedCount: 0, placedWidth: 64, placedEdge: 64, placedSpeed: 64,
    wanderHue: 64, wanderWhite: 64, wanderDark: 64, wanderRate: 50, wanderScale: 20,
    litHue: 64, litWhite: 0, litDark: 64,
    scatterLight: 64, scatterHue: 64, scatterWhite: 64,
  };

  const COLOR_LOOKS = {
    Flat: {},
    Rainbow: { colorRegion: GRADIENT, colorRuler: ON_WALL, placedHue: 112 },
    Mirror: { colorRegion: REGION, colorRuler: ON_WALL, placedHue: 110, placedWidth: 30, placedEdge: 96 },
    'Strip gradient': { colorRegion: GRADIENT, colorRuler: ON_STRIP, placedHue: 110 },
    'Center cell': { colorRegion: REGION, colorRuler: ON_STRIP, placedHue: 104, placedWidth: 22, placedEdge: 0 },
    'Head and tail': { colorRegion: GRADIENT, colorRuler: IN_SHAPE, placedHue: 104 },
    'Comet tail': { colorRegion: GRADIENT, colorRuler: IN_SHAPE, placedHue: 108, litWhite: 127 },
    'Drifting bands': { colorRegion: REGION, colorRuler: ON_STRIP, placedHue: 90, placedCount: 54, placedWidth: 40, placedEdge: 110, placedSpeed: 78 },
    Alive: { wanderHue: 92, wanderRate: 60, wanderScale: 6 },
    Boiling: { wanderHue: 104, wanderDark: 40, wanderRate: 110, wanderScale: 74 },
    Ember: { litHue: 100, litWhite: 34 },
    Sprinkle: { scatterLight: 116, scatterRate: 72, scatterCount: 90, scatterWidth: 30, scatterEdge: 44, scatterStagger: 122, scatterDrift: 64 },
    Blitzgewitter: { scatterLight: 127, scatterRate: 58, scatterCount: 74, scatterWidth: 40, scatterEdge: 4, scatterStagger: 0, scatterDrift: 64 },
    Raindrops: { scatterLight: 120, scatterRate: 34, scatterCount: 10, scatterWidth: 16, scatterEdge: 70, scatterStagger: 108, scatterDrift: 8 },
  };
  for (const name of Object.keys(COLOR_LOOKS)) {
    COLOR_LOOKS[name] = Object.assign({}, COLOR_FLAT, COLOR_LOOKS[name]);
  }
  for (const name of Object.keys(FAN_LOOKS)) {
    FAN_LOOKS[name] = Object.assign({}, SHAPE_FLAT, { genPosition: 64 }, FAN_LOOKS[name]);
  }

  for (const name of Object.keys(ANCHORS)) {
    ANCHORS[name] = Object.assign({}, SHAPE_FLAT, { genPosition: 64 }, ANCHORS[name]);
  }

  global.AuroraPatch = {
    PATCH_FORMAT, CC_COUNT, NAME_LEN, SETS, KEYS, PATCH_MAX,
    SET_BASE, SET_COLOR, SET_EXTENT, SET_MOTION, SET_ACCENT, SET_NAMES, SET_BLURB,
    CC, NAMES, SWITCHES, CONTINUOUS, CONTROLS, DERIVED, NEUTRAL, DEFAULT,
    OFF, ON, isOn, band3, GRADIENT, REGION, ON_WALL, ON_STRIP, IN_SHAPE, clamp7,
    unit, bip, ccCount, PULSE_PERIODS, PULSE_PERIOD_NAMES, periodStep, periodByte,
    DIVISIONS,
    LANES, MODULATORS, ROUTES, DESTINATIONS, routesAimedAt, PARS, TIMING,
    ANCHORS, FAN_LOOKS, COLOR_LOOKS, SHAPE_FLAT, COLOR_FLAT,
  };
})(window);
