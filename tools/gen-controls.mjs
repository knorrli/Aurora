#!/usr/bin/env node

import { readFileSync, writeFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const HEADER = join(ROOT, 'shared/aurora_protocol.h');
const ROUTES_SOURCE = join(ROOT, 'shared/render/routes.cpp');
const MORPH_SOURCE = join(ROOT, 'shared/render/morph.cpp');
const OUT = join(ROOT, 'tools/controls.js');

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

const cc = {};
for (const match of enumBody('AuroraCC').matchAll(/^\s*CC_([A-Z0-9_]+)\s*=\s*(\d+),/gm)) {
  cc[camel(match[1])] = Number(match[2]);
}
const number = {};
const tags = {};
for (const match of enumBody('AuroraControl').matchAll(/^\s*CONTROL_([A-Z0-9_]+)\s*=\s*(\d+),(?: *\/\/ *([^\n]*))?/gm)) {
  const key = camel(match[1]);
  number[key] = Number(match[2]);
  tags[key] = [...(match[3] || '').matchAll(/\[(\w+)\]/g)].map(tag => tag[1]);
}
const tagged = tag => Object.keys(tags).filter(name => tags[name].includes(tag));

const SENT_BY_DAWS = new Set([0, 1, 7, 10, 11, 32, 64]);

const defaultsBody = header.match(/AURORA_CONTROL_DEFAULTS\[\]\s*=\s*\{([\s\S]*?)\n\};/);
if (!defaultsBody) fail('AURORA_CONTROL_DEFAULTS');
const controlDefaults = {};
for (const match of defaultsBody[1].matchAll(/\{\s*CONTROL_([A-Z0-9_]+),\s*(\d+)\s*\}/g)) {
  controlDefaults[camel(match[1])] = Number(match[2]);
}

const routeCount = constant('AURORA_ROUTES');
const routeField = Object.fromEntries(enumEntries('AuroraRouteField')
  .filter(([key]) => key !== 'ROUTE_FIELDS')
  .map(([key, value]) => [camel(key.replace(/^ROUTE_/, '')), value]));

for (const [name, value] of Object.entries(cc)) {
  if (SENT_BY_DAWS.has(value)) throw new Error(`${HEADER}: ${name} is on CC ${value}, which DAWs send on their own`);
}

const routeDefaultValues = (header.match(/AURORA_ROUTE_DEFAULTS\[ROUTE_FIELDS\]\s*=\s*\{([^}]*)\}/) || fail('AURORA_ROUTE_DEFAULTS'))[1]
  .split(',').map(text => text.trim()).filter(Boolean)
  .map(text => (/^\d+$/.test(text) ? Number(text) : constant(text)));
if (routeDefaultValues.length !== Object.keys(routeField).length) {
  throw new Error('AURORA_ROUTE_DEFAULTS does not match AuroraRouteField');
}
const routeDefaults = Object.fromEntries(Object.entries(routeField)
  .map(([field, index]) => [field, routeDefaultValues[index]]));

const lfoPeriods = (header.match(/AURORA_LFO_PERIODS\[\]\s*=\s*\{([^}]*)\}/) || fail('AURORA_LFO_PERIODS'))[1]
  .split(',').map(text => text.trim().replace(/f$/, '')).filter(Boolean).map(Number);

const programs = Object.fromEntries(enumEntries('AuroraProgram'));
const notes = Object.fromEntries(enumEntries('AuroraNote'));
const patchLayers = Object.fromEntries(enumEntries('AuroraPatchLayer'));

const generated = {
  MIDI_CHANNEL: constant('AURORA_MIDI_CHANNEL'),
  PROGRAM_BLACKOUT: programs.PROGRAM_BLACKOUT,
  PROGRAM_SONG_FIRST: programs.PROGRAM_SONG_FIRST,
  SONGS: constant('AURORA_SONGS'),
  NOTE_KEY_HELD: notes.NOTE_KEY_HELD,
  NOTE_PATCH_ONESHOT_FIRST: notes.NOTE_PATCH_ONESHOT_FIRST,
  NOTE_PATCH_ONESHOT_SECOND: notes.NOTE_PATCH_ONESHOT_SECOND,
  NOTE_ONESHOT_FIRST: notes.NOTE_ONESHOT_FIRST,
  ONESHOTS: constant('AURORA_ONESHOTS'),
  TICKS_PER_BEAT: constant('AURORA_TICKS_PER_BEAT'),
  TEMPO_DIVISION: enumByPrefix('AuroraTempoDivision', 'TEMPO_DIVISION_'),
  CELL_END: enumByPrefix('AuroraCellEnd', 'CELL_END_'),
  DIRECTION: enumByPrefix('AuroraDirection', 'DIRECTION_'),
  ENVELOPES: constant('AURORA_ENVELOPES'),
  ENVELOPE_STAGE: Object.fromEntries(Object.entries(enumByPrefix('AuroraEnvelopeStage', 'ENVELOPE_'))
    .filter(([key]) => key !== 'stages')),
  FIELD_FORM: enumByPrefix('AuroraFieldForm', 'FIELD_FORM_'),
  ARP: enumByPrefix('AuroraArp', 'ARP_'),
  ARP_MODE: enumByPrefix('AuroraArpMode', 'ARP_MODE_'),
  GROUP_PATTERN: enumByPrefix('AuroraGroupPattern', 'GROUP_'),
  HUE_LAYOUT: enumByPrefix('AuroraHueLayout', 'HUE_LAYOUT_'),
  PAD_MODE: enumByPrefix('AuroraPadMode', 'PAD_MODE_'),
  CORNER: enumByPrefix('AuroraCorner', 'CORNER_'),
  KIT_PLACES: constant('AURORA_KIT_PLACES'),
  ROUTE_SOURCE: enumByPrefix('AuroraRouteSource', 'ROUTE_SOURCE_'),
  PAD_WIDTH: enumByPrefix('AuroraPadWidth', 'PAD_WIDTH_'),
  WAVE_SWELL: constant('WAVE_SWELL'),
  WAVE_FALL: constant('WAVE_FALL'),
  WAVE_SQUARE: constant('WAVE_SQUARE'),
  LFO_PERIODS: lfoPeriods,
  ROUTES: routeCount,
  FIRST_ROUTE_CONTROL: constant('AURORA_FIRST_ROUTE_CONTROL'),
  ROUTE_FIELD: routeField,
  ROUTE_DEFAULTS: routeDefaults,
  ROUTE_MAX_RATIO: constant('AURORA_ROUTE_MAX_RATIO'),
  ROUTE_PHASE_STEPS: constant('AURORA_ROUTE_PHASE_STEPS'),
  PATCH_FORMAT: constant('AURORA_PATCH_FORMAT'),
  LAST_PATCH_SLOT: constant('AURORA_LAST_PATCH_SLOT'),
  PATCH_CONTROLS: constant('AURORA_PATCH_CONTROLS'),
  PATCH_NAME_LENGTH: constant('AURORA_PATCH_NAME_LENGTH'),
  PATCH_HEAD_LENGTH: constant('AURORA_PATCH_HEAD_LENGTH'),
  PATCH_LAYER_BASE: patchLayers.PATCH_LAYER_BASE,
  PATCH_LAYER_COLOR: patchLayers.PATCH_LAYER_COLOR,
  PATCH_LAYER_EXTENT: patchLayers.PATCH_LAYER_EXTENT,
  PATCH_LAYER_MOTION: patchLayers.PATCH_LAYER_MOTION,
  PATCH_LAYER_ACCENT: patchLayers.PATCH_LAYER_ACCENT,
  PATCH_LAYERS: patchLayers.AURORA_PATCH_LAYERS,
  KEYPAD_KEYS: constant('AURORA_KEYPAD_KEYS'),
  BANKS: constant('AURORA_BANKS'),
  SLOT_MAP_LENGTH: constant('AURORA_SLOT_MAP_LENGTH'),
  KIT_MAP_LENGTH: constant('AURORA_KIT_MAP_LENGTH'),
  MARK_MAP_LENGTH: constant('AURORA_MARK_MAP_LENGTH'),
  NO_ONESHOT: constant('AURORA_NO_ONESHOT'),
  PIN_POSITION_STEPS: constant('AURORA_PIN_POSITION_STEPS'),
  SONG_SECTIONS: constant('AURORA_SONG_SECTIONS'),
  SONG_HEAD_LENGTH: constant('AURORA_SONG_HEAD_LENGTH'),
  LIBRARY_HEAD_LENGTH: constant('AURORA_LIBRARY_HEAD_LENGTH'),
  SYSEX_ID: constant('AURORA_SYSEX_ID'),
  SYSEX_SIGNATURE_A: constant('AURORA_SYSEX_SIGNATURE_A'),
  SYSEX_SIGNATURE_B: constant('AURORA_SYSEX_SIGNATURE_B'),
  SYSEX_HEADER_LENGTH: constant('AURORA_SYSEX_HEADER_LENGTH'),
  SYSEX_TYPE: enumByPrefix('AuroraSysEx', 'SYSEX_'),
  SYSEX_STATUS: enumByPrefix('AuroraSysExStatus', 'SYSEX_'),
  LIBRARY_STATE: enumByPrefix('AuroraLibraryState', 'LIBRARY_'),
};

const listed = map => {
  const pairs = Object.entries(map).sort((a, b) => a[1] - b[1]);
  const width = Math.max(...pairs.map(([name]) => name.length));
  return pairs.map(([name, value]) => `  ${name}:${' '.repeat(width - name.length)} ${value},`).join('\n');
};
const constantLines = Object.entries(generated)
  .map(([name, value]) => `const ${name} = ${JSON.stringify(value)};`).join('\n');

const out = `const CC = {
${listed(cc)}
};

const NUMBER = {
${listed(number)}
};

const TAGS = ${JSON.stringify(tags)};
const CONTROL_DEFAULTS = ${JSON.stringify(controlDefaults)};

${constantLines}

const tagged = tag => Object.keys(TAGS).filter(name => TAGS[name].includes(tag));
const hasTag = (name, tag) => (TAGS[name] || []).includes(tag);

const NAME_BY_NUMBER = {};
for (const [name, value] of Object.entries(NUMBER)) NAME_BY_NUMBER[value] = name;

const routeNumber = (route, field) => FIRST_ROUTE_CONTROL + field * ROUTES + route;

export const Protocol = {
  CC, NUMBER, CONTROL_DEFAULTS, NAME_BY_NUMBER, tagged, hasTag,
${Object.keys(generated).map(name => `  ${name},`).join('\n')}
  routeNumber,
};
`;

function checkCases(path, expected) {
  const source = readFileSync(path, 'utf8');
  const file = path.slice(ROOT.length + 1);
  const problems = [];
  for (const [name, want] of Object.entries(expected)) {
    const at = source.indexOf(`bool ${name}(uint8_t control)`);
    if (at < 0) throw new Error(`${file}: no ${name}()`);
    const body = source.slice(at, source.indexOf('\n}', at));
    const got = new Set([...body.matchAll(/case CONTROL_([A-Z0-9_]+):/g)].map(match => camel(match[1])));
    for (const control of want) if (!got.has(control)) problems.push(`${file}: ${name}() is missing ${control}`);
    for (const control of got) if (!want.has(control)) problems.push(`${file}: ${name}() has ${control}, which the enum does not tag`);
  }
  return problems;
}

function checkRenderer() {
  const problems = [
    ...checkCases(ROUTES_SOURCE, {
      refused: new Set([...tagged('switch'), 'lfoRate']),
      swings: new Set(tagged('rate')),
      circular: new Set(tagged('circular')),
    }),
    ...checkCases(MORPH_SOURCE, {
      switched: new Set(tagged('switch')),
    }),
  ];
  for (const name of Object.keys(controlDefaults)) {
    if (!(name in number)) problems.push(`shared/aurora_protocol.h: AURORA_CONTROL_DEFAULTS names ${name}, which is not a control`);
  }
  return problems;
}

if (process.argv.includes('--check')) {
  const problems = checkRenderer();
  let current = '';
  try { current = readFileSync(OUT, 'utf8'); } catch { }
  if (current !== out) problems.push('tools/controls.js is out of date — run: node tools/gen-controls.mjs');
  if (problems.length) {
    for (const line of problems) console.error(line);
    process.exit(1);
  }
  console.log(`tools/controls.js up to date — ${Object.keys(number).length} controls, ${routeCount} routes,`
    + ` and routes.cpp and morph.cpp agree with the enum's tags`);
} else {
  writeFileSync(OUT, out);
  console.log(`tools/controls.js written — ${Object.keys(number).length} controls, ${routeCount} routes`);
}
