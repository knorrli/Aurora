#!/usr/bin/env node

import { readFileSync, writeFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const HEADER = join(ROOT, 'shared/aurora_protocol.h');
const ROUTES_SOURCE = join(ROOT, 'shared/render/routes.cpp');
const MORPH_SOURCE = join(ROOT, 'shared/render/morph.cpp');
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

const SENT_BY_DAWS = new Set([0, 1, 7, 10, 11, 32, 64]);

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

const routeNumbers = routeBase.flatMap(base => Array.from({ length: Object.keys(routeField).length }, (_, field) => base + field));
for (const [name, number] of [...Object.entries(cc), ...routeNumbers.map(number => ['a route', number])]) {
  if (SENT_BY_DAWS.has(number)) throw new Error(`${HEADER}: ${name} is on CC ${number}, which DAWs send on their own`);
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

const STEPPED = /step = \(uint8_t\)\(\(\(uint16_t\)value \* last \+ 63\) \/ 127\);/;
for (const name of ['aurora_route_ratio_step', 'aurora_route_delay', 'aurora_lfo_period', 'aurora_arp_mode', 'aurora_hue_layout']) {
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

const controlList = name => (header.match(new RegExp(`${name}\\[\\]\\s*=\\s*\\{([^}]*)\\}`)) || fail(name))[1]
  .split(',').map(text => text.trim()).filter(Boolean)
  .map(text => camel((text.match(/^CC_([A-Z0-9_]+)$/) || fail(`a CC in ${name}, not ${text}`))[1]));
const arpControls = controlList('AURORA_ARP_CONTROLS');
const bipolarControls = controlList('AURORA_BIPOLAR_CONTROLS');
const DESTINATION_PACKING = [
  ['aurora_route_arp', /ARP_STEPS \+ \(destination - AURORA_ARP_DESTINATION_BASE\) % 2/],
  ['aurora_route_bipolar', /destination >= AURORA_BIPOLAR_DESTINATION_BASE\s+&& destination < AURORA_BIPOLAR_DESTINATION_BASE \+ AURORA_BIPOLAR_CONTROL_COUNT/],
  ['aurora_route_target', /AURORA_BIPOLAR_CONTROLS\[destination - AURORA_BIPOLAR_DESTINATION_BASE\][\s\S]*\(destination - AURORA_ARP_DESTINATION_BASE\) \/ 2/],
  ['aurora_route_destination', /AURORA_BIPOLAR_DESTINATION_BASE \+ index\)[\s\S]*AURORA_ARP_DESTINATION_BASE \+ index \* 2 \+ \(arp - ARP_STEPS\)/],
];
for (const [name, shape] of DESTINATION_PACKING) {
  if (!shape.test(functionBody(name))) {
    throw new Error(`${HEADER}: ${name}() no longer packs destinations as routeTarget() in tools/cc.js does`);
  }
}
const arpModeEntries = enumEntries('AuroraArpMode');
const arpModeCount = (arpModeEntries.find(([key]) => key === 'ARP_MODES') || fail('ARP_MODES'))[1];
const hueLayoutCount = (enumEntries('AuroraHueLayout').find(([key]) => key === 'HUE_LAYOUTS') || fail('HUE_LAYOUTS'))[1];

const programs = Object.fromEntries(enumEntries('AuroraProgram'));
const notes = Object.fromEntries(enumEntries('AuroraNote'));
const patchLayers = Object.fromEntries(enumEntries('AuroraPatchLayer'));

const generated = {
  MIDI_CHANNEL: constant('AURORA_MIDI_CHANNEL'),
  PROGRAM_BLACKOUT: programs.PROGRAM_BLACKOUT,
  PROGRAM_SHOW: programs.PROGRAM_SHOW,
  NOTE_PATCH_ONESHOT_FIRST: notes.NOTE_PATCH_ONESHOT_FIRST,
  NOTE_PATCH_ONESHOT_SECOND: notes.NOTE_PATCH_ONESHOT_SECOND,
  NOTE_ONESHOT_FIRST: notes.NOTE_ONESHOT_FIRST,
  ONESHOTS: constant('AURORA_ONESHOTS'),
  TICKS_PER_BEAT: constant('AURORA_TICKS_PER_BEAT'),
  TEMPO_DIVISION: enumByPrefix('AuroraTempoDivision', 'TEMPO_DIVISION_'),
  FIELD_FORM: enumByPrefix('AuroraFieldForm', 'FIELD_FORM_'),
  FIELD_DIRECTION: enumByPrefix('AuroraFieldDirection', 'FIELD_DIRECTION_'),
  SWITCH_ON_AT: switchOnAt,
  THREE_WAY_STARTS: threeWayStarts,
  THREE_WAY_VALUES: threeWayValues,
  ARP: enumByPrefix('AuroraArp', 'ARP_'),
  ARP_MODE: enumByPrefix('AuroraArpMode', 'ARP_MODE_'),
  ARP_MODE_COUNT: arpModeCount,
  HUE_LAYOUT: enumByPrefix('AuroraHueLayout', 'HUE_LAYOUT_'),
  HUE_LAYOUT_COUNT: hueLayoutCount,
  ARP_DESTINATION_BASE: constant('AURORA_ARP_DESTINATION_BASE'),
  ARP_CONTROLS: arpControls,
  BIPOLAR_DESTINATION_BASE: constant('AURORA_BIPOLAR_DESTINATION_BASE'),
  BIPOLAR_CONTROLS: bipolarControls,
  WAVE_SWELL: constant('WAVE_SWELL'),
  WAVE_FALL: constant('WAVE_FALL'),
  WAVE_SQUARE: constant('WAVE_SQUARE'),
  LFO_PERIODS: lfoPeriods,
  ROUTES: routeCount,
  ROUTE_BASE: routeBase,
  ROUTE_FIELD: routeField,
  ROUTE_DEFAULTS: routeDefaults,
  ROUTE_MAX_RATIO: constant('AURORA_ROUTE_MAX_RATIO'),
  ROUTE_PHASE_STEPS: constant('AURORA_ROUTE_PHASE_STEPS'),
  PATCH_FORMAT: constant('AURORA_PATCH_FORMAT'),
  PATCH_MAX: constant('AURORA_PATCH_MAX'),
  PATCH_CC_COUNT: constant('AURORA_PATCH_CC_COUNT'),
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
const INTERNAL = new Set(['SWITCH_ON_AT', 'THREE_WAY_STARTS', 'ROUTE_BASE', 'ROUTE_MAX_RATIO',
  'ARP_DESTINATION_BASE', 'BIPOLAR_DESTINATION_BASE', 'ARP_MODE_COUNT', 'HUE_LAYOUT_COUNT']);
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
  const routeRatioStep = value => steppedIndex(value, 2 * ROUTE_MAX_RATIO);
  const routeRatio = value => 1 + routeRatioStep(value) % ROUTE_MAX_RATIO;
  const routeOnce = value => routeRatioStep(value) >= ROUTE_MAX_RATIO;
  const routeRatioValue = (ratio, once) =>
    Math.round(((once ? ROUTE_MAX_RATIO : 0) + ratio - 1) * 127 / (2 * ROUTE_MAX_RATIO - 1));
  const routePhaseStep = value => steppedIndex(value, ROUTE_PHASE_STEPS);

  const arpMode = value => steppedIndex(value, ARP_MODE_COUNT);
  const arpModeValue = mode => Math.round(mode * 127 / (ARP_MODE_COUNT - 1));
  const hueLayout = value => steppedIndex(value, HUE_LAYOUT_COUNT);
  const hueLayoutValue = layout => Math.round(layout * 127 / (HUE_LAYOUT_COUNT - 1));

  const routeArp = destination => (destination < ARP_DESTINATION_BASE ? ARP.unison
    : ARP.steps + (destination - ARP_DESTINATION_BASE) % 2);
  const routeBipolar = destination => destination >= BIPOLAR_DESTINATION_BASE
    && destination < BIPOLAR_DESTINATION_BASE + BIPOLAR_CONTROLS.length;
  const routeTarget = destination => {
    if (destination < BIPOLAR_DESTINATION_BASE) return destination;
    if (routeBipolar(destination)) return CC[BIPOLAR_CONTROLS[destination - BIPOLAR_DESTINATION_BASE]];
    if (destination < ARP_DESTINATION_BASE) return 0;
    const name = ARP_CONTROLS[Math.floor((destination - ARP_DESTINATION_BASE) / 2)];
    return name ? CC[name] : 0;
  };
  const routeDestination = (target, arp, bipolar) => {
    if (arp === ARP.unison) {
      const index = BIPOLAR_CONTROLS.indexOf(NAME_BY_CC[target]);
      return bipolar && index >= 0 ? BIPOLAR_DESTINATION_BASE + index : target;
    }
    const index = ARP_CONTROLS.indexOf(NAME_BY_CC[target]);
    return index < 0 ? target : ARP_DESTINATION_BASE + index * 2 + (arp - ARP.steps);
  };

  const isOn = value => value >= SWITCH_ON_AT;
  const threeWayPosition = value =>
    THREE_WAY_STARTS.filter(start => value >= start).length - 1;

  global.AuroraProtocol = {
    CC, CONTROL_DEFAULTS, NAME_BY_CC, tagged, hasTag,
${Object.keys(generated).filter(name => !INTERNAL.has(name)).map(name => `    ${name},`).join('\n')}
    steppedIndex, routeCC, routeRatio, routeOnce, routeRatioValue, routePhaseStep, isOn, threeWayPosition,
    arpMode, arpModeValue, hueLayout, hueLayoutValue, routeArp, routeBipolar, routeTarget, routeDestination,
  };
})(typeof window === 'undefined' ? globalThis : window);
`;

function checkCases(path, expected) {
  const source = readFileSync(path, 'utf8');
  const file = path.slice(ROOT.length + 1);
  const problems = [];
  for (const [name, want] of Object.entries(expected)) {
    const at = source.indexOf(`bool ${name}(uint8_t cc)`);
    if (at < 0) throw new Error(`${file}: no ${name}()`);
    const body = source.slice(at, source.indexOf('\n}', at));
    const got = new Set([...body.matchAll(/case CC_([A-Z0-9_]+):/g)].map(match => camel(match[1])));
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
      plainLfo: new Set(tagged('plain')),
    }),
    ...checkCases(MORPH_SOURCE, {
      performed: new Set([...tagged('ambient'), ...tagged('gesture')]),
      switched: new Set(tagged('switch')),
    }),
  ];
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
    + ` and routes.cpp and morph.cpp agree with the enum's tags`);
} else {
  writeFileSync(OUT, out);
  console.log(`tools/cc.js written — ${pairs.length} controls, ${routeCount} routes`);
}
