#!/usr/bin/env node
//
// Drive the same function out of the firmware and out of the preview over the
// same grid of inputs, and report where they disagree.
//
//   node tools/crosscheck.mjs              every check
//   node tools/crosscheck.mjs scatterAt    one or more by name
//
// The two renderers are the same maths written twice, in C++ and in JavaScript,
// and nothing but discipline keeps them identical. A check names the symbols to
// lift out of each file, a driver in each language that sweeps them, and the
// shape of the sweep so a mismatch can be reported as coordinates rather than
// as an index.
//
// Symbols are lifted from the real sources, never copied into this file, so a
// check cannot quietly go on testing a version of the code that no longer
// exists. They are emitted in the order the check lists them, because C++ wants
// its declarations first.

import { readFileSync, writeFileSync, mkdtempSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { tmpdir } from 'node:os';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const CPP_SOURCES = ['brain/src/P_Generator.cpp', 'brain/src/Aurora.h',
                     'shared/aurora_protocol.h'];
const JS_SOURCE = 'tools/preview.js';

// Loose, deliberately. The firmware computes in 32-bit floats and the preview
// in 64-bit doubles, so the two track each other to about a part in ten
// million and no closer — and a step that divides by a small number, as
// pulseWave does by softness, multiplies that gap. What a check is looking for
// is a wrong constant or a missing term, which lands orders of magnitude above
// this. Every run prints its worst difference, so a drift toward the limit is
// visible before it trips.
const TOLERANCE = 1e-4;
const SHOW_MISMATCHES = 8;

// ---- lifting a symbol out of a source -------------------------------------

// Walks source positions while skipping comments and string literals, so a
// brace inside either never counts toward nesting.
function* significant(src, from) {
  let i = from;
  while (i < src.length) {
    const c = src[i];
    if (c === '/' && src[i + 1] === '/') {
      const end = src.indexOf('\n', i);
      i = end < 0 ? src.length : end + 1;
      continue;
    }
    if (c === '/' && src[i + 1] === '*') {
      const end = src.indexOf('*/', i + 2);
      i = end < 0 ? src.length : end + 2;
      continue;
    }
    if (c === '"' || c === "'") {
      let j = i + 1;
      while (j < src.length && src[j] !== c) j += src[j] === '\\' ? 2 : 1;
      i = j + 1;
      continue;
    }
    yield [i, c];
    i++;
  }
}

function endOfBlock(src, openBrace) {
  let depth = 0;
  for (const [i, c] of significant(src, openBrace)) {
    if (c === '{') depth++;
    else if (c === '}' && --depth === 0) return i + 1;
  }
  throw new Error('unterminated block');
}

function endOfStatement(src, from) {
  let depth = 0;
  for (const [i, c] of significant(src, from)) {
    if ('{(['.includes(c)) depth++;
    else if ('})]'.includes(c)) depth--;
    else if (c === ';' && depth === 0) return i + 1;
  }
  throw new Error('unterminated statement');
}

// A definition whose parameter list is followed by `;` is a forward
// declaration; the real one is further down the file.
function definitionAfter(src, headStart) {
  const paren = src.indexOf('(', headStart);
  if (paren < 0) return null;
  let depth = 0;
  for (const [i, c] of significant(src, paren)) {
    if (c === '(') depth++;
    else if (c === ')' && --depth === 0) {
      const rest = src.slice(i + 1);
      const gap = rest.match(/^\s*/)[0].length;
      if (rest[gap] === ';') return null;
      if (rest[gap] !== '{') return null;
      return src.slice(headStart, endOfBlock(src, i + 1 + gap));
    }
  }
  return null;
}

function liftCpp(sources, name) {
  const escaped = name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  for (const [path, src] of sources) {
    const define = src.match(new RegExp(`^#define\\s+${escaped}\\b.*$`, 'm'));
    if (define) return define[0];

    const head = new RegExp(`^[A-Za-z_][\\w\\s:<>,*&]*?\\b${escaped}\\s*\\(`, 'gm');
    for (const match of src.matchAll(head)) {
      const found = definitionAfter(src, match.index);
      if (found) return found;
    }

    const variable = new RegExp(`^static\\s+[\\w\\s*]+\\b${escaped}\\s*(=|\\[)`, 'm');
    const at = src.search(variable);
    if (at >= 0) return src.slice(at, endOfStatement(src, at));

    void path;
  }
  throw new Error(`no C++ definition of ${name}`);
}

function liftJs(src, name) {
  const escaped = name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const fn = src.search(new RegExp(`^\\s*function\\s+${escaped}\\s*\\(`, 'm'));
  if (fn >= 0) {
    const open = src.indexOf('{', src.indexOf('(', fn));
    return src.slice(fn, endOfBlock(src, open)).trim();
  }
  const con = src.search(new RegExp(`^\\s*const\\s+${escaped}\\s*=`, 'm'));
  if (con >= 0) return src.slice(con, endOfStatement(src, con)).trim();
  throw new Error(`no JavaScript definition of ${name}`);
}

// ---- running one side ------------------------------------------------------

const CPP_PREAMBLE = `#include <cstdint>
#include <cmath>
#include <cstdio>
#define PI 3.1415926535897932384626433832795
#define EMIT(value) printf("%.9g\\n", (double)(value))
`;

function runCpp(check, sources, workDir) {
  const body = check.cpp.map(name => liftCpp(sources, name)).join('\n\n');
  const source = `${CPP_PREAMBLE}\n${body}\n\nint main() {\n${check.cppDriver}\n  return 0;\n}\n`;
  const file = join(workDir, `${check.name}.cpp`);
  const binary = join(workDir, check.name);
  writeFileSync(file, source);
  execFileSync('c++', ['-O2', '-o', binary, file], { stdio: 'pipe' });
  return execFileSync(binary, { encoding: 'utf8' }).trim().split('\n').map(Number);
}

function runJs(check, src) {
  const body = check.js.map(name => liftJs(src, name)).join('\n\n');
  const out = [];
  const emit = v => out.push(Number(v));
  new Function('emit', `${body}\n${check.jsDriver}`)(emit);
  return out;
}

// ---- the checks ------------------------------------------------------------
//
// Each one lists the symbols to lift, a driver per language that sweeps the
// same grid in the same order, and the grid's shape so a mismatch reads as
// coordinates. A driver calls EMIT / emit exactly once per sample.
//
// `known` excuses a sample that differs for a reason someone has established,
// keyed by its coordinates. A check with an unexplained difference fails; one
// whose known difference has gone away says so, because that means something
// changed under it.

const SETTINGS = 8;

const CHECKS = [
  // The conversions first: they turn the byte a fader sent into what every
  // check below consumes, and since a control is stored as that byte and
  // converted where it is drawn, a wrong curve here moves the whole wall.
  // These sweep all 128 values, so they are exhaustive rather than sampled.
  {
    name: 'pulsePeriod',
    cpp: ['AURORA_PULSE_PERIODS', 'AURORA_PULSE_PERIOD_COUNT', 'aurora_pulse_period'],
    js: ['PULSE_PERIODS', 'pulsePeriod'],
    dims: [['value', 128]],
    cppDriver: `
  for (int v = 0; v < 128; v++) EMIT(aurora_pulse_period((uint8_t)v));`,
    jsDriver: `
  for (let v = 0; v < 128; v++) emit(pulsePeriod(v));`,
  },
  {
    name: 'ccUnit',
    cpp: ['ccUnit'],
    js: ['ccUnit'],
    dims: [['value', 128]],
    cppDriver: `
  for (int v = 0; v < 128; v++) EMIT(ccUnit((uint8_t)v));`,
    jsDriver: `
  for (let v = 0; v < 128; v++) emit(ccUnit(v));`,
  },
  {
    name: 'ccBipolar',
    cpp: ['ccBipolar'],
    js: ['ccBipolar'],
    dims: [['value', 128]],
    cppDriver: `
  for (int v = 0; v < 128; v++) EMIT(ccBipolar((uint8_t)v));`,
    jsDriver: `
  for (let v = 0; v < 128; v++) emit(ccBipolar(v));`,
  },
  {
    name: 'ccCount',
    cpp: ['GEN_MAX_COUNT', 'ccUnit', 'ccCount'],
    js: ['GEN_MAX_COUNT', 'ccCount'],
    dims: [['value', 128]],
    cppDriver: `
  for (int v = 0; v < 128; v++) EMIT((float)ccCount((uint8_t)v));`,
    jsDriver: `
  for (let v = 0; v < 128; v++) emit(ccCount(v));`,
  },
  {
    name: 'speedPixels',
    cpp: ['GEN_MAX_SPEED_PIXELS_PER_BEAT', 'GEN_STILL_PIXELS_PER_BEAT',
          'speedPixelsFrom'],
    js: ['GEN_MAX_SPEED_PIXELS_PER_BEAT', 'GEN_STILL_PIXELS_PER_BEAT',
         'ccSquared', 'stillBelowThreshold'],
    dims: [['value', 128]],
    cppDriver: `
  for (int v = 0; v < 128; v++) EMIT(speedPixelsFrom((uint8_t)v));`,
    jsDriver: `
  for (let v = 0; v < 128; v++)
    emit(stillBelowThreshold(ccSquared(v, GEN_MAX_SPEED_PIXELS_PER_BEAT)));`,
  },
  {
    name: 'fanRate',
    cpp: ['GEN_MAX_SPEED_PIXELS_PER_BEAT', 'fanRateFrom'],
    js: ['GEN_MAX_SPEED_PIXELS_PER_BEAT', 'ccSquared'],
    dims: [['value', 128]],
    cppDriver: `
  for (int v = 0; v < 128; v++) EMIT(fanRateFrom((uint8_t)v));`,
    jsDriver: `
  for (let v = 0; v < 128; v++) emit(ccSquared(v, GEN_MAX_SPEED_PIXELS_PER_BEAT));`,
  },
  {
    name: 'fanFreq',
    cpp: ['GEN_FAN_FREQ_STEPS', 'GEN_FAN_MAX_CYCLES_PER_STRIP', 'fanFreqFrom'],
    js: ['GEN_FAN_FREQ_STEPS', 'GEN_FAN_MAX_CYCLES_PER_STRIP', 'fanFrequency'],
    dims: [['value', 128]],
    cppDriver: `
  for (int v = 0; v < 128; v++) EMIT(fanFreqFrom((uint8_t)v));`,
    jsDriver: `
  for (let v = 0; v < 128; v++) emit(fanFrequency(v));`,
  },
  {
    name: 'placedSpeed',
    cpp: ['PLACED_MAX_CELLS_PER_BEAT', 'placedCellsPerBeatFrom'],
    js: ['PLACED_MAX_CELLS_PER_BEAT', 'ccSquared'],
    dims: [['value', 128]],
    cppDriver: `
  for (int v = 0; v < 128; v++) EMIT(placedCellsPerBeatFrom((uint8_t)v));`,
    jsDriver: `
  for (let v = 0; v < 128; v++) emit(ccSquared(v, PLACED_MAX_CELLS_PER_BEAT));`,
  },
  {
    name: 'wanderCycles',
    cpp: ['WANDER_MAX_CYCLES_PER_BEAT', 'ccUnit', 'wanderCyclesFrom'],
    js: ['WANDER_MAX_CYCLES_PER_BEAT', 'ccUnit'],
    dims: [['value', 128]],
    cppDriver: `
  for (int v = 0; v < 128; v++) EMIT(wanderCyclesFrom((uint8_t)v));`,
    jsDriver: `
  for (let v = 0; v < 128; v++)
    emit(ccUnit(v) ** 2 * WANDER_MAX_CYCLES_PER_BEAT);`,
  },
  {
    name: 'scatterRate',
    cpp: ['SCATTER_MAX_CYCLES_PER_BEAT', 'ccUnit', 'scatterRateFrom'],
    js: ['SCATTER_MAX_CYCLES_PER_BEAT', 'ccUnit'],
    dims: [['value', 128]],
    cppDriver: `
  for (int v = 0; v < 128; v++) EMIT(scatterRateFrom((uint8_t)v));`,
    jsDriver: `
  for (let v = 0; v < 128; v++)
    emit(ccUnit(v) ** 2 * SCATTER_MAX_CYCLES_PER_BEAT);`,
  },
  {
    name: 'coreAt',
    cpp: ['coreAt'],
    js: ['coreAt'],
    dims: [['offset', 65], ['width', 9], ['edge', 9]],
    cppDriver: `
  for (int o = 0; o < 65; o++)
    for (int w = 0; w < 9; w++)
      for (int e = 0; e < 9; e++)
        EMIT(coreAt((float)o / 32.0f - 1.0f, (float)w / 8.0f, (float)e / 8.0f));`,
    jsDriver: `
  for (let o = 0; o < 65; o++)
    for (let w = 0; w < 9; w++)
      for (let e = 0; e < 9; e++)
        emit(coreAt(o / 32 - 1, w / 8, e / 8));`,
  },
  {
    name: 'shapeAt',
    cpp: ['coreAt', 'tailAt', 'shapeAt'],
    js: ['coreAt', 'tailAt', 'shapeAt'],
    dims: [['offset', 65], ['width', 9], ['edge', 5], ['tail', 5]],
    cppDriver: `
  for (int o = 0; o < 65; o++)
    for (int w = 0; w < 9; w++)
      for (int e = 0; e < 5; e++)
        for (int t = 0; t < 5; t++)
          EMIT(shapeAt((float)o / 32.0f - 1.0f, (float)w / 8.0f,
                       (float)e / 4.0f, (float)t / 4.0f));`,
    jsDriver: `
  for (let o = 0; o < 65; o++)
    for (let w = 0; w < 9; w++)
      for (let e = 0; e < 5; e++)
        for (let t = 0; t < 5; t++)
          emit(shapeAt(o / 32 - 1, w / 8, e / 4, t / 4));`,
  },
  {
    // Every wave byte against a phase grid of 256 — exhaustive in the byte,
    // because a wrong landmark would show at one value and nowhere else.
    name: 'pulseWave',
    cpp: ['GEN_WAVE_SAW_DOWN', 'GEN_WAVE_SQUARE', 'GEN_PULSE_MIN_WIDTH',
          'fract', 'raisedCosine', 'pulseWave'],
    js: ['GEN_WAVE_SAW_DOWN', 'GEN_WAVE_SQUARE', 'GEN_PULSE_MIN_WIDTH',
         'fract', 'raisedCosine', 'pulseWave'],
    dims: [['wave', 128], ['phase', 256]],
    cppDriver: `
  for (int w = 0; w < 128; w++)
    for (int p = 0; p < 256; p++)
      EMIT(pulseWave((float)p / 256.0f, (uint8_t)w));`,
    jsDriver: `
  for (let w = 0; w < 128; w++)
    for (let p = 0; p < 256; p++)
      emit(pulseWave(p / 256, w));`,
  },
  {
    name: 'fanWave',
    cpp: ['fract', 'hash8', 'GEN_FAN_HASH_SALT', 'genFanPhase', 'genFanFreq',
          'genFanRandom', 'fanWave'],
    js: ['fract', 'hash8', 'GEN_FAN_HASH_SALT', 'fanWave'],
    dims: [['phase', 33], ['freq', 17], ['random', 9], ['strip', 5]],
    cppDriver: `
  for (int p = 0; p < 33; p++)
    for (int f = 0; f < 17; f++)
      for (int r = 0; r < 9; r++) {
        genFanPhase = (float)p / 32.0f;
        genFanFreq = (float)f / 32.0f;
        genFanRandom = (float)r / 8.0f;
        for (int s = 0; s < 5; s++) EMIT(fanWave((uint8_t)s));
      }`,
    jsDriver: `
  for (let p = 0; p < 33; p++)
    for (let f = 0; f < 17; f++)
      for (let r = 0; r < 9; r++) {
        const q = { fanPhase: p / 32, fanFreq: f / 32, fanRandom: r / 8 };
        for (let s = 0; s < 5; s++) emit(fanWave(q, s));
      }`,
  },
  {
    name: 'wanderAt',
    cpp: ['NUMBER_OF_STRIPS', 'GOLD', 'wanderScale', 'wanderAt'],
    js: ['STRIPS', 'GOLD', 'wanderAt'],
    dims: [['scale', 9], ['strip', 5], ['along', 33], ['t', 5]],
    cppDriver: `
  for (int c = 0; c < 9; c++) {
    wanderScale = (float)c / 8.0f;
    for (int s = 0; s < 5; s++)
      for (int a = 0; a < 33; a++)
        for (int i = 0; i < 5; i++)
          EMIT(wanderAt((uint8_t)s, (float)a / 32.0f, (float)i / 8.0f));
  }`,
    jsDriver: `
  for (let c = 0; c < 9; c++) {
    const q = { wanderScale: c / 8, wanderActive: true };
    for (let s = 0; s < 5; s++)
      for (let a = 0; a < 33; a++)
        for (let i = 0; i < 5; i++)
          emit(wanderAt(q, a / 32, s, i / 8));
  }`,
  },
  {
    name: 'scatterAt',
    cpp: ['PIXELS_PER_STRIP', 'GEN_MAX_COUNT', 'SCATTER_MAX_CYCLES_PER_BEAT',
          'fract', 'ccUnit', 'ccCount', 'ccBipolar', 'hash8', 'coreAt',
          'scatterRate', 'scatterCount', 'scatterWidth', 'scatterEdge',
          'scatterStagger', 'scatterDrift', 'scatterAt'],
    js: ['PIXELS', 'GEN_MAX_COUNT', 'SCATTER_MAX_CYCLES_PER_BEAT',
         'fract', 'ccUnit', 'ccCount', 'ccBipolar', 'hash8', 'coreAt', 'scatterAt'],
    dims: [['setting', SETTINGS], ['strip', 5], ['pixel', 45], ['sub', 4], ['t', 5]],
    known: {
      'setting=3 strip=2 pixel=31 sub=3 t=0':
        'a tie at full width, where the whole cell is core: the sample sits ' +
        '1.2e-15 inside the boundary and a 32-bit float rounds it onto the ' +
        'far side of the <= in coreAt',
    },
    // Rate, count, width, edge, stagger, drift, as the CC bytes a patch holds.
    settings: `
  { 0, 0, 64, 64, 0, 64 }, { 30, 90, 20, 0, 0, 64 },
  { 64, 64, 64, 64, 64, 64 }, { 127, 127, 127, 127, 127, 127 },
  { 40, 50, 10, 127, 90, 10 }, { 90, 20, 100, 30, 33, 110 },
  { 12, 110, 45, 70, 127, 64 }, { 100, 5, 127, 1, 5, 0 },`,
    cppDriver: `
  const uint8_t settings[][6] = {
%SETTINGS%
  };
  for (int c = 0; c < %SETTINGS_COUNT%; c++) {
    const float x = ccUnit(settings[c][0]);
    scatterRate = x * x * SCATTER_MAX_CYCLES_PER_BEAT;
    scatterCount = ccCount(settings[c][1]);
    scatterWidth = ccUnit(settings[c][2]);
    scatterEdge = ccUnit(settings[c][3]);
    scatterStagger = ccUnit(settings[c][4]);
    scatterDrift = ccBipolar(settings[c][5]);
    for (int s = 0; s < 5; s++)
      for (int p = 0; p < 45; p++)
        for (int k = 0; k < 4; k++) {
          const float acrossPixel = ((float)k + 0.5f) / 4.0f - 0.5f;
          for (int i = 0; i < 5; i++)
            EMIT(scatterAt((uint8_t)s, (float)p + 0.5f + acrossPixel, (float)i / 8.0f));
        }
  }`,
    jsDriver: `
  const settings = [
%SETTINGS%
  ];
  for (const c of settings) {
    const q = {
      scatterRate: ccUnit(c[0]) ** 2 * SCATTER_MAX_CYCLES_PER_BEAT,
      scatterCount: ccCount(c[1]),
      scatterWidth: ccUnit(c[2]),
      scatterEdge: ccUnit(c[3]),
      scatterStagger: ccUnit(c[4]),
      scatterDrift: ccBipolar(c[5]),
    };
    for (let s = 0; s < 5; s++)
      for (let p = 0; p < 45; p++)
        for (let k = 0; k < 4; k++) {
          const acrossPixel = (k + 0.5) / 4 - 0.5;
          for (let i = 0; i < 5; i++)
            emit(scatterAt(q, s, p + 0.5 + acrossPixel, i / 8));
        }
  }`,
  },
];

// A settings table has to read the same in both languages; braces are C++'s and
// brackets are JavaScript's, and nothing else about the numbers differs.
function fillSettings(check) {
  if (!check.settings) return check;
  const count = check.settings.match(/\{/g).length;
  const cpp = check.settings;
  const js = check.settings.replace(/\{/g, '[').replace(/\}/g, ']');
  return {
    ...check,
    cppDriver: check.cppDriver.replace('%SETTINGS%', cpp)
                              .replace('%SETTINGS_COUNT%', String(count)),
    jsDriver: check.jsDriver.replace('%SETTINGS%', js),
  };
}

// ---- comparing -------------------------------------------------------------

function coordinates(dims, index) {
  const parts = [];
  let rest = index;
  for (let i = dims.length - 1; i >= 0; i--) {
    const [name, size] = dims[i];
    parts.unshift(`${name}=${rest % size}`);
    rest = Math.floor(rest / size);
  }
  return parts.join(' ');
}

function main() {
  const wanted = process.argv.slice(2);
  const chosen = wanted.length
    ? CHECKS.filter(c => wanted.includes(c.name))
    : CHECKS;

  const missing = wanted.filter(n => !CHECKS.some(c => c.name === n));
  if (missing.length) {
    console.error(`no such check: ${missing.join(', ')}`);
    console.error(`have: ${CHECKS.map(c => c.name).join(', ')}`);
    process.exit(2);
  }

  const cppSources = CPP_SOURCES.map(p => [p, readFileSync(join(ROOT, p), 'utf8')]);
  const jsSource = readFileSync(join(ROOT, JS_SOURCE), 'utf8');
  const workDir = mkdtempSync(join(tmpdir(), 'aurora-crosscheck-'));

  let failed = 0;
  for (const raw of chosen) {
    const check = fillSettings(raw);
    const expected = check.dims.reduce((n, [, size]) => n * size, 1);

    let cpp, js;
    try {
      cpp = runCpp(check, cppSources, workDir);
      js = runJs(check, jsSource);
    } catch (error) {
      console.log(`${check.name.padEnd(12)} ERROR  ${error.message.split('\n')[0]}`);
      console.log(`             build files in ${workDir}`);
      failed++;
      continue;
    }

    if (cpp.length !== expected || js.length !== expected) {
      console.log(`${check.name.padEnd(12)} ERROR  sweep says ${expected} samples, ` +
                  `C++ emitted ${cpp.length}, JavaScript emitted ${js.length}`);
      failed++;
      continue;
    }

    const known = check.known || {};
    const off = [];
    const excused = new Set();
    let worst = 0;
    for (let i = 0; i < expected; i++) {
      const gap = Math.abs(cpp[i] - js[i]);
      if (gap <= TOLERANCE) continue;
      const where = coordinates(check.dims, i);
      if (known[where]) excused.add(where);
      else off.push(i);
      if (gap > worst) worst = gap;
    }
    if (!off.length) {
      for (let i = 0; i < expected; i++) {
        const gap = Math.abs(cpp[i] - js[i]);
        if (gap <= TOLERANCE && gap > worst) worst = gap;
      }
    }

    const stale = Object.keys(known).filter(where => !excused.has(where));
    const size = `${expected} samples`.padEnd(16);

    if (!off.length && !stale.length) {
      const tail = excused.size ? `, ${excused.size} known` : '';
      console.log(`${check.name.padEnd(12)} ok     ${size}worst ${worst.toExponential(1)}${tail}`);
      continue;
    }

    failed++;
    if (off.length) {
      console.log(`${check.name.padEnd(12)} DIFFER ${size}${off.length} unexplained`);
      for (const i of off.slice(0, SHOW_MISMATCHES)) {
        console.log(`             ${coordinates(check.dims, i)}` +
                    `  firmware ${cpp[i]}  preview ${js[i]}`);
      }
      if (off.length > SHOW_MISMATCHES) {
        console.log(`             and ${off.length - SHOW_MISMATCHES} more`);
      }
    }
    for (const where of stale) {
      console.log(`${check.name.padEnd(12)} STALE  a known difference has gone: ${where}`);
      console.log(`             it was excused because ${known[where]}`);
    }
  }

  process.exit(failed ? 1 : 0);
}

main();
