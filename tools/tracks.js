import { Protocol } from './cc.js';
import * as Rows from './control-rows.js';
import * as Patch from './patch.js';
import { Preview } from './preview.js';
import { session } from './session.js';


const HANDLE_RADIUS = 8;
const READ_AT_PARS = new Set(['parValue', 'parHueOffset', 'parSaturation']);

const rangeOf = input => [input.min === '' ? 0 : +input.min, input.max === '' ? 100 : +input.max];

function along(input, value) {
  const [min, max] = rangeOf(input);
  return `calc(${HANDLE_RADIUS}px + ${(value - min) / (max - min)} * (100% - ${2 * HANDLE_RADIUS}px))`;
}

function reaching(input, value) {
  const [min, max] = rangeOf(input);
  if (value <= min) return '0%';
  if (value >= max) return '100%';
  return along(input, value);
}

const stops = (...bands) => 'linear-gradient(90deg, '
  + bands.map(([color, from, to]) => `${color} ${from}, ${color} ${to}`).join(', ') + ')';

const offset = (at, pixels) => `calc(${at} + ${pixels}px)`;

const pointBackground = at => stops(['transparent', '0%', offset(at, -1)], ['var(--bg)', offset(at, -1), offset(at, 1)],
                               ['transparent', offset(at, 1), '100%']);

const markBackground = at => stops(['transparent', '0%', offset(at, -2)], ['var(--bg)', offset(at, -2), offset(at, -1)],
                              ['#fff', offset(at, -1), offset(at, 1)], ['var(--bg)', offset(at, 1), offset(at, 2)],
                              ['transparent', offset(at, 2), '100%']);

const hueStepsPerFaderStep = () => Preview.convert(Patch.CC.parHueOffset, 127) / 127;

function wrappedSpans(low, high) {
  const spans = [[Math.max(0, low), Math.min(127, high)]];
  if (low < 0) spans.push([low + 128, 127]);
  if (high > 127) spans.push([0, high - 128]);
  return spans;
}

function hueBandOf(input) {
  const live = session.liveControls();
  const reach = Math.abs(Preview.convert(Patch.CC.parHueRange, live.parHueRange)) / hueStepsPerFaderStep();
  if (reach < 0.5) return null;
  const center = +input.value;
  const offsets = Preview.controlAtPars(Patch.CC.parHueOffset);
  const places = Preview.parHuePlaces();
  return {
    spans: wrappedSpans(center - reach, center + reach),
    marks: offsets.map((offset, par) => (((offset + places[par] / hueStepsPerFaderStep()) % 128) + 128) % 128),
  };
}

const PHASE_VALUES_PER_TURN = 127 * Protocol.ROUTE_PHASE_STEPS / (Protocol.ROUTE_PHASE_STEPS - 1);
const amountValue = signed => (signed < 0 ? 64 + Math.max(signed, -1) * 64 : 64 + Math.min(signed, 1) * 63);

function aimMean(live, aim) {
  const source = Protocol.routeSource(live[aim.ratio]);
  if (source === Protocol.ROUTE_SOURCE.lfo) return Preview.waveMean(live[aim.wave], live[aim.smooth]);
  const envelope = source - Protocol.ROUTE_SOURCE.envelope1;
  return envelope >= 0 && envelope < Protocol.ENVELOPES ? 0 : 0.5;
}

function aimSwing(name) {
  const route = Patch.routeOf(name);
  if (!route || (name !== route.amount && name !== route.phase)) return null;
  const live = session.liveControls();
  const phase = name === route.phase;
  const aims = Patch.aimsAt(live, route, phase);
  if (!aims.length) return null;
  let low = 0, high = 0;
  for (const aim of aims) {
    const amount = Protocol.signedOf(live[aim.amount]) * (phase ? 1 : 2);
    const mean = aimMean(live, aim);
    low += Math.min(-amount * mean, amount * (1 - mean));
    high += Math.max(-amount * mean, amount * (1 - mean));
  }
  const index = Patch.ROUTES.indexOf(route);
  if (phase) {
    const dialed = live[name];
    return {
      spans: wrappedSpans(dialed - high * PHASE_VALUES_PER_TURN, dialed - low * PHASE_VALUES_PER_TURN),
      marks: Preview.routeShiftAtStrips(index).map(shift => (((dialed - shift * PHASE_VALUES_PER_TURN) % 128) + 128) % 128),
    };
  }
  const dialed = Protocol.signedOf(live[name]);
  return {
    spans: [[amountValue(dialed + low), amountValue(dialed + high)]],
    marks: Preview.routeAmountAtStrips(index).map(amountValue),
  };
}

function swingOf(name) {
  const aimed = aimSwing(name);
  if (aimed) return aimed;
  const cc = Patch.CC[name];
  const reach = Preview.routeReach(cc);
  if (!reach) return null;
  const [low, high] = reach;
  return {
    spans: wrappedSpans(low, high),
    marks: READ_AT_PARS.has(name) ? Preview.controlAtPars(cc) : Preview.controlAtStrips(cc),
  };
}

function paintTrack(input, swing, points, circular, band) {
  const backgrounds = [];
  const marks = band ? band.marks : swing ? swing.marks : [];
  for (const value of marks) backgrounds.push(markBackground(along(input, value)));
  for (const value of points) backgrounds.push(pointBackground(along(input, value)));
  if (swing) {
    for (const [low, high] of swing.spans) {
      const from = reaching(input, low), to = reaching(input, high);
      backgrounds.push(stops(['transparent', '0%', from], ['var(--lfo)', from, to], ['transparent', to, '100%']));
    }
  }
  if (band) {
    for (const [low, high] of band.spans) {
      const from = reaching(input, low), to = reaching(input, high);
      backgrounds.push(stops(['transparent', '0%', from], ['var(--color)', from, to], ['transparent', to, '100%']));
    }
  }
  const at = reaching(input, +input.value);
  backgrounds.push(circular ? stops(['var(--rest)', '0%', '100%'])
                       : stops(['var(--fill)', '0%', at], ['var(--rest)', at, '100%']));
  const track = backgrounds.join(', ');
  if (input.dataset.track !== track) {
    input.dataset.track = track;
    input.style.setProperty('--track', track);
  }
}

function paint() {
  for (const input of document.querySelectorAll('input[type=range]')) {
    const row = Rows.rows[input.closest('.row') && input.closest('.row').dataset.name];
    if (!row) {
      paintTrack(input, null, [], false, null);
      continue;
    }
    const name = row.control.name;
    paintTrack(input, swingOf(name), row.points, row.circular, name === 'parHueOffset' ? hueBandOf(input) : null);
  }
}

export { along, paint };
