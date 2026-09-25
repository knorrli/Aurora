#!/usr/bin/env node

import { readFileSync, writeFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const HEADER = join(ROOT, 'shared/aurora_protocol.h');
const ROUTES_SOURCE = join(ROOT, 'shared/render/routes.cpp');
const OUT = join(ROOT, 'tools/cc.js');

const header = readFileSync(HEADER, 'utf8');

const camel = name => name.toLowerCase().replace(/_(.)/g, (_, c) => c.toUpperCase());

function fail(what) {
  throw new Error(`${HEADER}: could not find ${what}`);
}

function enumBody(name) {
  const at = header.indexOf(`enum ${name}`);
  if (at < 0) fail(`enum ${name}`);
  return header.slice(header.indexOf('{', at) + 1, header.indexOf('};', at));
}

function enumEntries(name) {
  const entries = [...enumBody(name).matchAll(/^\s*([A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+),/gm)]
    .map(match => [match[1], Number(match[2])]);
  if (!entries.length) fail(`entries in enum ${name}`);
  return entries;
}

function enumByPrefix(name, prefix) {
  return Object.fromEntries(enumEntries(name)
    .filter(([key]) => key.startsWith(prefix))
    .map(([key, value]) => [camel(key.slice(prefix.length)), value]));
}

const constants = {};
for (const match of header.matchAll(/^static const uint(?:8|16)_t (\w+)\s*=\s*([^;{]+);/gm)) {
  const expression = match[2]
    .replace(/\(uint16_t\)/g, '')
    .replace(/0x[0-9A-Fa-f]+/g, hex => String(Number(hex)))
    .replace(/\b[A-Z_][A-Z0-9_]*\b/g, name => {
      if (name in constants) return String(constants[name]);
      const entry = header.match(new RegExp(`\\b${name}\\s*=\\s*(\\d+),`));
      return entry ? entry[1] : name;
    });
  if (!/^[\d\s+\-*/()]+$/.test(expression)) continue;
  constants[match[1]] = Math.floor(Function(`return (${expression});`)());
}

function constant(name) {
  if (!(name in constants)) fail(name);
  return constants[name];
}

function functionBody(name) {
  const at = header.indexOf(` ${name}(`);
  if (at < 0) fail(`${name}()`);
  return header.slice(at, header.indexOf('\n}', at));
}

const cc = {};
const tags = {};
for (const match of enumBody('AuroraCC').matchAll(/^\s*CC_([A-Z0-9_]+)\s*=\s*(\d+),(?: *\/\/ *([^\n]*))?/gm)) {
  const key = camel(match[1]);
  cc[key] = Number(match[2]);
  tags[key] = [...(match[3] || '').matchAll(/\[(\w+)\]/g)].map(tag => tag[1]);
}
const tagged = tag => Object.keys(tags).filter(name => tags[name].includes(tag));

const defaultsBody = header.match(/AURORA_CONTROL_DEFAULTS\[\]\s*=\s*\{([\s\S]*?)\n\};/);
if (!defaultsBody) fail('AURORA_CONTROL_DEFAULTS');
const controlDefaults = {};
for (const match of defaultsBody[1].matchAll(/\{\s*CC_([A-Z0-9_]+),\s*(\d+)\s*\}/g)) {
  controlDefaults[camel(match[1])] = Number(match[2]);
}

const routeCount = constant('AURORA_ROUTES');
const routeBase = (header.match(/AURORA_ROUTE_BASE\[AURORA_ROUTES\]\s*=\s*\{([^}]*)\}/) || fail('AURORA_ROUTE_BASE'))[1]
  .split(',').map(text => text.trim()).filter(Boolean).map(Number);
if (routeBase.length !== routeCount) throw new Error('AURORA_ROUTE_BASE does not match AURORA_ROUTES');
const routeField = Object.fromEntries(enumEntries('AuroraRouteField')
  .filter(([key]) => key !== 'ROUTE_FIELDS')
  .map(([key, value]) => [camel(key.replace(/^ROUTE_/, '')), value]));

const lfoPeriods = (header.match(/AURORA_LFO_PERIODS\[\]\s*=\s*\{([^}]*)\}/) || fail('AURORA_LFO_PERIODS'))[1]
  .split(',').map(text => text.trim().replace(/f$/, '')).filter(Boolean).map(Number);

const STEPPED = /step = \(uint8_t\)\(\(\(uint16_t\)value \* last \+ 63\) \/ 127\);/;
for (const name of ['aurora_route_ratio', 'aurora_lfo_period']) {
  if (!STEPPED.test(functionBody(name))) {
    throw new Error(`${HEADER}: ${name}() no longer steps as steppedIndex() in tools/cc.js does`);
  }
}

const switchOnAt = Number((functionBody('aurora_switch_is_on').match(/value >= (\d+)/) || fail('the switch threshold'))[1]);
const threeWayStarts = [0, ...[...functionBody('aurora_three_way_position').matchAll(/value < (\d+)/g)].map(match => Number(match[1]))];
if (threeWayStarts.length !== 3) fail('the three-way bands');
const threeWayValues = threeWayStarts.map((start, position) =>
  position === 0 ? 0 : position === threeWayStarts.length - 1 ? 127
    : Math.round((start + threeWayStarts[position + 1] - 1) / 2));

const programs = Object.fromEntries(enumEntries('AuroraProgram'));
const patchParts = Object.fromEntries(enumEntries('AuroraPatchPart'));

const generated = {
  MIDI_CHANNEL: constant('AURORA_MIDI_CHANNEL'),
  PROGRAM_BLACKOUT: programs.PROGRAM_BLACKOUT,
  PROGRAM_SHOW: programs.PROGRAM_SHOW,
  TICKS_PER_BEAT: constant('AURORA_TICKS_PER_BEAT'),
  TEMPO_DIVISION: enumByPrefix('AuroraTempoDivision', 'TEMPO_DIVISION_'),
  FIELD_FORM: enumByPrefix('AuroraFieldForm', 'FIELD_FORM_'),
  FIELD_DIRECTION: enumByPrefix('AuroraFieldDirection', 'FIELD_DIRECTION_'),
  SWITCH_ON_AT: switchOnAt,
  THREE_WAY_STARTS: threeWayStarts,
  THREE_WAY_VALUES: threeWayValues,
  WAVE_SWELL: constant('WAVE_SWELL'),
  WAVE_SNAP: constant('WAVE_SNAP'),
  WAVE_SQUARE: constant('WAVE_SQUARE'),
  LFO_PERIODS: lfoPeriods,
  ROUTES: routeCount,
  ROUTE_BASE: routeBase,
  ROUTE_FIELD: routeField,
  ROUTE_MAX_RATIO: constant('AURORA_ROUTE_MAX_RATIO'),
  PATCH_FORMAT: constant('AURORA_PATCH_FORMAT'),
  PATCH_MAX: constant('AURORA_PATCH_MAX'),
  PATCH_CC_COUNT: constant('AURORA_PATCH_CC_COUNT'),
  PATCH_NAME_LENGTH: constant('AURORA_PATCH_NAME_LENGTH'),
  PATCH_HEAD_LENGTH: constant('AURORA_PATCH_HEAD_LENGTH'),
  PATCH_BASE: patchParts.PATCH_BASE,
  PATCH_TARGET_COLOR: patchParts.PATCH_TARGET_COLOR,
  PATCH_TARGET_EXTENT: patchParts.PATCH_TARGET_EXTENT,
  PATCH_TARGET_MOTION: patchParts.PATCH_TARGET_MOTION,
  PATCH_TARGET_ACCENT: patchParts.PATCH_TARGET_ACCENT,
  PATCH_PARTS: patchParts.AURORA_PATCH_PARTS,
  KEYPAD_KEYS: constant('AURORA_KEYPAD_KEYS'),
  SLOT_MAP_LENGTH: constant('AURORA_SLOT_MAP_LENGTH'),
  SYSEX_ID: constant('AURORA_SYSEX_ID'),
  SYSEX_SIGNATURE_A: constant('AURORA_SYSEX_SIGNATURE_A'),
  SYSEX_SIGNATURE_B: constant('AURORA_SYSEX_SIGNATURE_B'),
  SYSEX_HEADER_LENGTH: constant('AURORA_SYSEX_HEADER_LENGTH'),
  SYSEX_TYPE: enumByPrefix('AuroraSysEx', 'SYSEX_'),
  SYSEX_STATUS: enumByPrefix('AuroraSysExStatus', 'SYSEX_'),
  LIBRARY_STATE: enumByPrefix('AuroraLibraryState', 'LIBRARY_'),
};

const pairs = Object.entries(cc).sort((a, b) => a[1] - b[1]);
const width = Math.max(...pairs.map(([name]) => name.length));
const ccBody = pairs.map(([name, number]) => `    ${name}:${' '.repeat(width - name.length)} ${number},`).join('\n');
const INTERNAL = new Set(['SWITCH_ON_AT', 'THREE_WAY_STARTS', 'ROUTE_BASE', 'ROUTE_MAX_RATIO']);
const constantLines = Object.entries(generated)
  .map(([name, value]) => `  const ${name} = ${JSON.stringify(value)};`).join('\n');

const out = `(function (global) {
  'use strict';

  const CC = {
${ccBody}
  };

  const TAGS = ${JSON.stringify(tags)};
  const CONTROL_DEFAULTS = ${JSON.stringify(controlDefaults)};

${constantLines}

  const tagged = tag => Object.keys(TAGS).filter(name => TAGS[name].includes(tag));
  const hasTag = (name, tag) => (TAGS[name] || []).includes(tag);

  const NAME_BY_CC = {};
  for (const [name, number] of Object.entries(CC)) NAME_BY_CC[number] = name;

  const steppedIndex = (value, count) =>
    Math.min(count - 1, Math.floor((value * (count - 1) + 63) / 127));

  const routeCC = (route, field) => ROUTE_BASE[route] + field;
  const routeRatio = value => 1 + steppedIndex(value, ROUTE_MAX_RATIO);

  const isOn = value => value >= SWITCH_ON_AT;
  const threeWayPosition = value =>
    THREE_WAY_STARTS.filter(start => value >= start).length - 1;

  global.AuroraProtocol = {
    CC, CONTROL_DEFAULTS, NAME_BY_CC, tagged, hasTag,
${Object.keys(generated).filter(name => !INTERNAL.has(name)).map(name => `    ${name},`).join('\n')}
    steppedIndex, routeCC, routeRatio, isOn, threeWayPosition,
  };
})(typeof window === 'undefined' ? globalThis : window);
`;

function checkRenderer() {
  const source = readFileSync(ROUTES_SOURCE, 'utf8');
  const cases = name => {
    const at = source.indexOf(`static bool ${name}(uint8_t cc)`);
    if (at < 0) throw new Error(`routes.cpp: no ${name}()`);
    const body = source.slice(at, source.indexOf('\n}', at));
    return new Set([...body.matchAll(/case CC_([A-Z0-9_]+):/g)].map(match => camel(match[1])));
  };
  const expected = {
    refused: new Set([...tagged('switch'), 'lfoRate', 'parHueShuffleEvery']),
    swings: new Set(tagged('rate')),
    circular: new Set(tagged('circular')),
    plainLfo: new Set(tagged('plain')),
  };
  const problems = [];
  for (const [name, want] of Object.entries(expected)) {
    const got = cases(name);
    for (const control of want) if (!got.has(control)) problems.push(`shared/render/routes.cpp: ${name}() is missing ${control}`);
    for (const control of got) if (!want.has(control)) problems.push(`shared/render/routes.cpp: ${name}() has ${control}, which the enum does not tag`);
  }
  for (const name of Object.keys(controlDefaults)) {
    if (!(name in cc)) problems.push(`shared/aurora_protocol.h: AURORA_CONTROL_DEFAULTS names ${name}, which is not a CC`);
  }
  return problems;
}

if (process.argv.includes('--check')) {
  const problems = checkRenderer();
  let current = '';
  try { current = readFileSync(OUT, 'utf8'); } catch { }
  if (current !== out) problems.push('tools/cc.js is out of date — run: node tools/gen-cc.mjs');
  if (problems.length) {
    for (const line of problems) console.error(line);
    process.exit(1);
  }
  console.log(`tools/cc.js up to date — ${pairs.length} controls, ${routeCount} routes,`
    + ` and routes.cpp agrees with the enum's tags`);
} else {
  writeFileSync(OUT, out);
  console.log(`tools/cc.js written — ${pairs.length} controls, ${routeCount} routes`);
}
