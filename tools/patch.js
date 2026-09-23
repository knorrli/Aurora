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
  const CC = {
    tempoDivision: 10,

    hue: 20, saturation: 21, value: 22,

    placedHue: 24, placedWhite: 25, placedDark: 26,
    placedCount: 27, placedWidth: 28, placedEdge: 29, placedSpeed: 90,

    alternate: 45, bounce: 46, colorRegion: 47, colorRuler: 48,

    slotA: 50, slotB: 51, slotC: 52, slotD: 53, slotE: 54,
    slotF: 55, slotG: 56, slotH: 57, slotI: 58, slotJ: 59,

    washLevel: 60, washHueOffset: 61, washSaturation: 62,

    width: 70, count: 71, edge: 72, tail: 73, speed: 74, fan: 75,
    position: 115,

    pulseDepth: 77, pulseRate: 78, pulseSkew: 79, pulseShape: 80,
    pulseWidth: 100, pulseWidthShape: 101, pulseWidthSkew: 102,
    pulseHue: 103, pulseHueShape: 104, pulseHueSkew: 105,
    pulseParLevel: 106, pulseParLevelShape: 107, pulseParLevelSkew: 108,
    pulseParHue: 109, pulseParHueShape: 110, pulseParHueSkew: 111,
    pulseParSat: 112, pulseParSatShape: 113, pulseParSatSkew: 114,

    scatterRate: 81, scatterCount: 82, scatterWidth: 83, scatterEdge: 84,
    scatterStagger: 85, scatterDrift: 86,
    scatterLight: 87, scatterHue: 88, scatterWhite: 89,

    wanderHue: 91, wanderWhite: 92, wanderDark: 93,
    wanderRate: 94, wanderScale: 95,

    litHue: 96, litWhite: 97, litDark: 98,
  };

  const NAMES = Object.keys(CC);

  // The four with no middle. A morph never moves one, and within a patch the
  // far ends share the base's — DESIGN.md § "Switches belong to the patch"
  // states the authoring rule directly: a patch and its own morph target
  // share switches.
  const SWITCHES = ['alternate', 'bounce', 'colorRegion', 'colorRuler'];
  const CONTINUOUS = NAMES.filter(n => !SWITCHES.includes(n));

  const OFF = 0, ON = 127;
  const isOn = v => v >= 64;
  const band3 = v => (v < 43 ? 0 : v < 86 ? 1 : 2);
  const GRADIENT = OFF, REGION = ON;
  const ON_WALL = 0, ON_STRIP = 64, IN_SHAPE = 127;

  const clamp7 = v => (v < 0 ? 0 : v > 127 ? 127 : v | 0);

  // ---- readouts ----------------------------------------------------------
  //
  // Mirrors of the mappings in brain/src/P_Generator.cpp and tools/preview.js.
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

    width: pct, edge: v => pct(v) + ' into the gap', tail: v => pct(v) + ' of the gap',
    count: v => ccCount(v) + ' shapes',
    position: v => Math.abs(bip(v)) < 0.02 ? 'center of the cell'
                 : (bip(v) * 50).toFixed(0) + '% of a cell off center',
    speed: v => { const x = (v - 64) / 63; const s = Math.sign(x) * x * x * 60;
                  return Math.abs(s) < 0.05 ? 'still' : s.toFixed(1) + ' px/beat'; },
    fan: v => pct(v) + ' apart',

    hue: v => Math.round(v / 127 * 250) + '/255',
    saturation: pct, value: pct,

    pulseRate: v => PULSE_PERIOD_NAMES[periodStep(v)],
    pulseDepth: v => v === 0 ? 'not reached' : pct(v) + ' down at the trough',
    pulseSkew: v => { const k = 0.5 + 0.48 * bip(v);
                      return Math.abs(bip(v)) < 0.03 ? 'even rise and fall'
                           : (k * 100).toFixed(0) + '% of the cycle rising'; },
    pulseShape: v => v < 20 ? 'square' : v > 110 ? 'sine' : pct(v) + ' soft',
    pulseWidth: v => signedPct(v, 'toward full width', 'toward nothing'),
    pulseHue: hueAmount,
    pulseParLevel: v => signedPct(v, 'toward full', 'toward dark'),
    pulseParHue: hueAmount,
    pulseParSat: v => signedPct(v, 'toward a pure hue', 'toward white'),

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

  for (const n of ['pulseWidthShape', 'pulseHueShape', 'pulseParLevelShape',
                   'pulseParHueShape', 'pulseParSatShape']) DERIVED[n] = DERIVED.pulseShape;
  for (const n of ['pulseWidthSkew', 'pulseHueSkew', 'pulseParLevelSkew',
                   'pulseParHueSkew', 'pulseParSatSkew']) DERIVED[n] = DERIVED.pulseSkew;
  for (const n of ['slotA', 'slotB', 'slotC', 'slotD', 'slotE',
                   'slotF', 'slotG', 'slotH', 'slotI', 'slotJ']) DERIVED[n] = v => String(v);

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
    width: 127, count: 0, edge: 0, tail: 0, position: 64, speed: 64, fan: 0,
    alternate: OFF, bounce: OFF,

    hue: 20, saturation: 100, value: 110,

    pulseRate: 64, pulseDepth: 0, pulseShape: 127, pulseSkew: 64,
    pulseWidth: 64, pulseWidthShape: 127, pulseWidthSkew: 64,
    pulseHue: 64, pulseHueShape: 127, pulseHueSkew: 64,
    pulseParLevel: 64, pulseParLevelShape: 127, pulseParLevelSkew: 64,
    pulseParHue: 64, pulseParHueShape: 127, pulseParHueSkew: 64,
    pulseParSat: 64, pulseParSatShape: 127, pulseParSatSkew: 64,

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

  // A new patch is one shape traveling across a lit wall: something on the
  // screen the moment it exists, so the first thing you do is change it
  // rather than hunt for why the wall is dark.
  const DEFAULT = Object.assign({}, NEUTRAL, {
    width: 40, count: 0, edge: 18, speed: 80,
  });

  // ---- the surface -------------------------------------------------------
  //
  // Carrier, modulators, outputs. The two lanes hold only what the wall shows
  // with nothing pushing on it; everything that pushes is a source with its
  // amounts beside it, and every source is drawn the same way whatever it
  // reaches. That is what the old panel could not say: the placed field, the
  // wander and the light level were three unrelated boxes of sliders that all
  // land on the same three qualities and add.

  const LANES = [
    {
      key: 'shape', name: 'Shape',
      does: 'decides whether a pixel is lit, and how much',
      groups: [
        {
          key: 'form', title: 'Form',
          controls: define([
            C('count', 'Count', 'how many shapes along the strip, 1\u201320'),
            C('width', 'Width', 'the solid core, as a proportion of one cell'),
            C('edge', 'Edge', 'how far the glow reaches into the gap, both sides'),
            C('tail', 'Tail', 'how far the trail reaches behind, into the gap'),
          ]),
        },
        {
          key: 'travel', title: 'Travel',
          controls: define([
            C('position', 'Position', 'where a still pattern stands in its cell'),
            C('speed', 'Speed', 'center is still; either side travels'),
            C('fan', 'Fan', 'how far the five strips run out of step'),
          ]),
          switches: define([
            C('alternate', 'Alternate', 'the odd strips run the journey backwards',
              { kind: 'two', options: [[OFF, 'together'], [ON, 'alternate']] }),
            C('bounce', 'Bounce', 'turn at the cell\u2019s edge instead of wrapping',
              { kind: 'two', options: [[OFF, 'wrap'], [ON, 'bounce']] }),
          ]),
          note: 'Bring Speed to a stop and the pattern walks home to Position over a beat or two, so a patch saved comes back to the same place. Under bounce the swing is anchored to the cell and Position does nothing.',
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

  const PULSE_DESTS = [
    { key: 'light', name: 'Brightness', where: 'the strips\u2019 own light',
      amount: 'pulseDepth', shape: 'pulseShape', skew: 'pulseSkew',
      note: 'Nothing sits above full, so this is the one amount with no sign: it digs the trough below whatever the shape lane already lit.' },
    { key: 'width', name: 'Width', where: 'the shape\u2019s solid core',
      amount: 'pulseWidth', shape: 'pulseWidthShape', skew: 'pulseWidthSkew',
      note: 'A shape is anchored by its center, so this breathes outward instead of wiping in from one end.' },
    { key: 'hue', name: 'Hue', where: 'the strips, after the color lane',
      amount: 'pulseHue', shape: 'pulseHueShape', skew: 'pulseHueSkew',
      note: 'One push on what the other sources have already summed to \u2014 not a fourth source, so the color lane\u2019s own design is untouched.' },
    { key: 'parLevel', name: 'PAR level', where: 'the four washes',
      amount: 'pulseParLevel', shape: 'pulseParLevelShape', skew: 'pulseParLevelSkew',
      note: 'The washes flashing against still strips is the look that justifies the whole matrix. Pull Level down first: a push toward full needs somewhere to go.' },
    { key: 'parHue', name: 'PAR hue', where: 'the four washes',
      amount: 'pulseParHue', shape: 'pulseParHueShape', skew: 'pulseParHueSkew',
      note: 'Rotates the washes off the strips on the swell and back between them.' },
    { key: 'parSat', name: 'PAR saturation', where: 'the four washes',
      amount: 'pulseParSat', shape: 'pulseParSatShape', skew: 'pulseParSatSkew',
      note: 'Toward white is the flash between strip strobes. It measures from the PARs\u2019 own saturation, so pulling them pale leaves the flash less far to travel.' },
  ];

  for (const d of PULSE_DESTS) {
    define([
      C(d.amount, 'Amount', 'how far the pulse pushes ' + d.where),
      C(d.shape, 'Shape', 'hard on/off square \u2192 smooth sine'),
      C(d.skew, 'Skew', 'even rise and fall \u2192 a ramp that snaps back'),
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
      key: 'pulse', name: 'The pulse', tone: 'pulse',
      when: 'regular in time, and nowhere on the wall',
      source: define([
        C('pulseRate', 'Rate', 'how often the swell lands. Stepped, so it can sit on the bar'),
      ]),
      dests: PULSE_DESTS,
      note: 'One oscillator with one rate. A destination says how far the pulse pushes it and what wave does the pushing, never how fast \u2014 every rate here feeds a running total, so a pulse aimed at one would move the wall permanently instead of returning it. Every destination is always connected and its amount may be zero: a morph interpolates an amount and cannot snap a connection on.',
    },
    {
      key: 'scatter', name: 'The scatter', tone: 'scatter',
      when: 'random in space and in time',
      unbuilt: 'The firmware does not render this yet \u2014 the preview does. It replaces CC 76 jitter, which no longer has a control: a patch leaves that byte at zero, so nothing a patch does can reach the old mechanism.',
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
      note: 'A grid of cells along each strip, each with its own clock, each lighting a spot that appears, holds, fades, and may slide across its cell as it does. It pushes what the shape lane left, so it needs a gap to light and light to darken \u2014 which is also why a spot inside an already-full shape is invisible and the shape covers it with no occlusion rule anywhere. Moving Stagger re-keys every cell, so everything in flight jumps.',
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

  const DESTINATIONS = [
    { key: 'light', name: 'the strips\u2019 brightness', lane: 'shape',
      from: { pulse: 'pulseDepth', scatter: 'scatterLight' } },
    { key: 'width', name: 'the shape\u2019s width', lane: 'shape',
      from: { pulse: 'pulseWidth' } },
    { key: 'hue', name: 'hue', lane: 'color',
      from: { pulse: 'pulseHue', scatter: 'scatterHue', placed: 'placedHue',
              wander: 'wanderHue', lit: 'litHue' } },
    { key: 'white', name: 'whiteness', lane: 'color',
      from: { scatter: 'scatterWhite', placed: 'placedWhite',
              wander: 'wanderWhite', lit: 'litWhite' } },
    { key: 'dark', name: 'darkness', lane: 'color',
      from: { placed: 'placedDark', wander: 'wanderDark', lit: 'litDark' } },
    { key: 'parLevel', name: 'the PARs\u2019 level', lane: 'out',
      from: { pulse: 'pulseParLevel' } },
    { key: 'parHue', name: 'the PARs\u2019 hue', lane: 'out',
      from: { pulse: 'pulseParHue' } },
    { key: 'parSat', name: 'the PARs\u2019 saturation', lane: 'out',
      from: { pulse: 'pulseParSat' } },
  ];

  const PARS = {
    controls: define([
      C('washLevel', 'Level', 'the PARs\u2019 master, independent of the strips'),
      C('washHueOffset', 'Hue offset', 'rotates the PARs off the strips\u2019 hue. Zero matches them'),
      C('washSaturation', 'Saturation', 'scales the PARs down from the strips\u2019 saturation. Full matches them, zero is white'),
    ]),
    note: 'A PAR is one position with no length, so the shape lane cannot reach it: count, width, edge, tail, speed and fan all describe places along a strip. What a patch holds for them is a relationship to the strips rather than a second look. The pulse is the one part of the shape lane that does reach them, and all four take its unfanned phase.',
  };

  const TIMING = {
    controls: define([
      C('tempoDivision', 'Tempo division', 'what one tempo pulse stands for. Every rate below scales with it',
        { kind: 'pick', options: DIVISIONS }),
    ]),
    note: 'Part of every parameter set, so a far end may sit at another division and a morph will step through the ones between. A half-time look is a real musical idea and a song that wants one wants it for every patch in that song.',
  };

  const SLOTS = {
    controls: define([
      C('slotA', 'Slot A', 'per-pattern parameter, CC 50'),
      C('slotB', 'Slot B', 'per-pattern parameter, CC 51'),
      C('slotC', 'Slot C', 'per-pattern parameter, CC 52'),
      C('slotD', 'Slot D', 'per-pattern parameter, CC 53'),
      C('slotE', 'Slot E', 'per-pattern parameter, CC 54'),
      C('slotF', 'Slot F', 'per-pattern parameter, CC 55'),
      C('slotG', 'Slot G', 'per-pattern parameter, CC 56'),
      C('slotH', 'Slot H', 'per-pattern parameter, CC 57'),
      C('slotI', 'Slot I', 'per-pattern parameter, CC 58'),
      C('slotJ', 'Slot J', 'per-pattern parameter, CC 59'),
    ]),
    note: 'Each pattern decides what its own slots mean, so these are generic by design and say nothing under the generator. They are here because a patch carries them and an editor that could not set them could not build a complete patch.',
  };

  // ---- starting points ---------------------------------------------------

  const SHAPE_FLAT = {
    width: 127, count: 0, edge: 0, tail: 0, position: 64, speed: 64, fan: 0,
    alternate: OFF, bounce: OFF,
  };

  const ANCHORS = {
    Fill:       { width: 127, count: 0, edge: 0, tail: 0, speed: 64, fan: 0, pulseDepth: 0, pulseRate: 64, pulseShape: 127 },
    Sweep:      { width: 40, count: 0, edge: 18, tail: 0, speed: 80, fan: 0, pulseDepth: 0, pulseRate: 64, pulseShape: 127 },
    Rain:       { width: 40, count: 0, edge: 18, tail: 74, speed: 80, fan: 90, pulseDepth: 0, pulseRate: 64, pulseShape: 127 },
    CrossSweep: { width: 40, count: 0, edge: 18, tail: 0, speed: 80, fan: 0, alternate: ON, pulseDepth: 0, pulseRate: 64, pulseShape: 127 },
    Bars:       { width: 25, count: 0, edge: 15, tail: 0, speed: 88, fan: 0, bounce: ON, pulseDepth: 0, pulseRate: 64, pulseShape: 127 },
    Breathe:    { width: 127, count: 0, edge: 0, tail: 0, speed: 64, fan: 0, pulseDepth: 100, pulseRate: 30, pulseShape: 127 },
    Wave:       { width: 127, count: 0, edge: 0, tail: 0, speed: 64, fan: 100, pulseDepth: 100, pulseRate: 30, pulseShape: 127 },
    Chase:      { width: 127, count: 0, edge: 0, tail: 0, speed: 64, fan: 127, pulseDepth: 127, pulseRate: 55, pulseShape: 28 },
    Comet:      { width: 30, count: 0, edge: 30, tail: 99, speed: 80, fan: 127, pulseDepth: 0, pulseRate: 55, pulseShape: 127 },
    Strobe:     { width: 127, count: 0, edge: 0, tail: 0, speed: 64, fan: 0, pulseDepth: 127, pulseRate: 100, pulseShape: 0 },
    Stutter:    { width: 127, count: 0, edge: 0, tail: 0, speed: 64, fan: 60, alternate: ON, pulseDepth: 127, pulseRate: 100, pulseShape: 0 },
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
  for (const name of Object.keys(ANCHORS)) {
    ANCHORS[name] = Object.assign({}, SHAPE_FLAT, { position: 64 }, ANCHORS[name]);
  }

  global.AuroraPatch = {
    PATCH_FORMAT, CC_COUNT, NAME_LEN, SETS, KEYS, PATCH_MAX,
    SET_BASE, SET_COLOR, SET_EXTENT, SET_MOTION, SET_ACCENT, SET_NAMES, SET_BLURB,
    CC, NAMES, SWITCHES, CONTINUOUS, CONTROLS, DERIVED, NEUTRAL, DEFAULT,
    OFF, ON, isOn, band3, GRADIENT, REGION, ON_WALL, ON_STRIP, IN_SHAPE, clamp7,
    unit, bip, ccCount, PULSE_PERIODS, PULSE_PERIOD_NAMES, periodStep, periodByte,
    DIVISIONS,
    LANES, MODULATORS, PULSE_DESTS, DESTINATIONS, PARS, TIMING, SLOTS,
    ANCHORS, COLOR_LOOKS, SHAPE_FLAT, COLOR_FLAT,
  };
})(window);
