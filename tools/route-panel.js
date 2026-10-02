import { Protocol } from './cc.js';
import * as Rows from './control-rows.js';
import * as dom from './dom.js';
import * as Editor from './editor.js';
import * as LayerDrop from './layer-drop.js';
import * as Pad from './pad.js';
import * as Patch from './patch.js';
import { Preview } from './preview.js';
import { session } from './session.js';
import * as Transition from './transition.js';

const { element } = dom;

const WAVE_CYCLES = 2;
const PANEL_MIN_WIDTH = 420;
const WAVE_PADDING = 7;
const WAVE_OVERHANG = 10;

const panel = { root: null, blocks: [], add: null, free: null, target: null };
const list = { root: null, count: null, lines: [] };

const LAST_PHASE_DRAWN = 0.9999;
const wrapped = value => value - Math.floor(value);
const cycles = () => (session.editingOneshot() ? 1 : WAVE_CYCLES);

const turnsOf = (route, clock, live) => Preview.routeTurns(live[route.ratio], live[route.phase], clock);

const shownPhase = phase => (phase < 0 || phase > cycles()
  ? phase - Math.floor(phase / cycles()) * cycles() : Math.min(phase, cycles() * LAST_PHASE_DRAWN));

const periodWidth = width => width - 2 * WAVE_OVERHANG;
const xOfPhase = (phase, width) => WAVE_OVERHANG + phase / cycles() * periodWidth(width);
const phaseOfX = (x, width) => (x - WAVE_OVERHANG) / periodWidth(width) * cycles();

function departureAt(route, phase, live) {
  const wave = live[route.wave];
  const destination = Protocol.NAME_BY_CC[Protocol.routeTarget(live[route.destination])];
  const at = Preview.lfoWave(turnsOf(route, shownPhase(phase), live), wave);
  const centered = Patch.swings(destination) || Protocol.routeBipolar(live[route.destination]);
  return Protocol.signedOf(live[route.amount]) * (centered ? at - Preview.waveMean(wave) : at);
}

const SOURCE_NAMES = [['LFO', Protocol.ROUTE_SOURCE.lfo], ['pad X', Protocol.ROUTE_SOURCE.padX], ['pad Y', Protocol.ROUTE_SOURCE.padY]];
const sourceOf = (route, live) => Protocol.routeSource(live[route.ratio]);
const clockedPerSpot = (route, live) =>
  Patch.perSpot(live[route.destination]) && sourceOf(route, live) === Protocol.ROUTE_SOURCE.lfo;
const sourceName = source => SOURCE_NAMES.find(([, value]) => value === source)[0];

function padDeparture(route, level, live) {
  const bipolar = Protocol.routeBipolar(live[route.destination]);
  return Protocol.signedOf(live[route.amount]) * (bipolar ? level - 0.5 : level);
}

function drawWave(canvas, route, live) {
  const width = canvas.clientWidth, height = canvas.clientHeight;
  if (!width || !height) return;
  const scale = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = Math.round(width * scale);
  canvas.height = Math.round(height * scale);
  const context = canvas.getContext('2d');
  context.setTransform(scale, 0, 0, scale, 0, 0);
  context.clearRect(0, 0, width, height);

  const y = value => (height / 2) - value * (height / 2 - WAVE_PADDING);
  const rail = (value, style, dashed) => {
    context.setLineDash(dashed ? [3, 4] : []);
    context.strokeStyle = style;
    context.lineWidth = 1;
    context.beginPath();
    context.moveTo(0, y(value) + 0.5);
    context.lineTo(width, y(value) + 0.5);
    context.stroke();
    context.setLineDash([]);
  };
  rail(1, 'rgba(255,255,255,.06)');
  rail(-1, 'rgba(255,255,255,.06)');
  rail(0, 'rgba(255,255,255,.22)', true);
  const padSource = sourceOf(route, live) !== Protocol.ROUTE_SOURCE.lfo;
  context.strokeStyle = 'rgba(255,255,255,.22)';
  context.lineWidth = 1;
  for (const phase of padSource ? [] : [0, cycles()]) {
    const x = Math.round(xOfPhase(phase, width)) + 0.5;
    context.beginPath();
    context.moveTo(x, 0);
    context.lineTo(x, height);
    context.stroke();
  }

  context.strokeStyle = getComputedStyle(canvas).color;
  context.lineWidth = 2;
  context.lineJoin = 'round';
  context.beginPath();
  for (let x = 0; x <= width; x++) {
    const value = padSource ? padDeparture(route, x / width, live) : departureAt(route, phaseOfX(x, width), live);
    if (x === 0) context.moveTo(x, y(value));
    else context.lineTo(x, y(value));
  }
  context.stroke();
  canvas.wave = context.getImageData(0, 0, canvas.width, canvas.height);
}

const spotClockText = live => 'per spot, ' + Patch.READOUTS.scatterRate(live.scatterRate, live);

function drawPlayhead(canvas, route, lfo, live) {
  if (!canvas.wave || canvas.wave.width !== canvas.width || clockedPerSpot(route, live)) return;
  const width = canvas.clientWidth, height = canvas.clientHeight;
  const context = canvas.getContext('2d');
  context.putImageData(canvas.wave, 0, 0);
  const source = sourceOf(route, live);
  let x, value;
  if (source !== Protocol.ROUTE_SOURCE.lfo) {
    const level = Pad.fingerLevel(source);
    if (level === null) return;
    x = level * width;
    value = padDeparture(route, level, live);
  } else {
    if (lfo === null) return;
    const along = wrapped(lfo / cycles());
    x = xOfPhase(along * cycles(), width);
    value = departureAt(route, along * cycles(), live);
  }
  const y = (height / 2) - value * (height / 2 - WAVE_PADDING);
  context.strokeStyle = 'rgba(255,255,255,.35)';
  context.lineWidth = 1;
  context.beginPath();
  context.moveTo(x, 0);
  context.lineTo(x, height);
  context.stroke();
  context.fillStyle = '#fff';
  context.beginPath();
  context.arc(x, y, 3, 0, Math.PI * 2);
  context.fill();
}

function paintPlayheads(lfo, live) {
  if (list.root && list.root.open) {
    for (const { route, line, canvas } of list.lines) {
      if (!line.hidden) drawPlayhead(canvas, route, lfo, live);
    }
  }
  if (panel.target) {
    for (const { route, block, canvas } of panel.blocks) {
      if (!block.hidden) drawPlayhead(canvas, route, lfo, live);
    }
  }
}

function routeButtons(route) {
  const bypass = element('button', 'tiny', 'bypass');
  bypass.addEventListener('click', () => {
    session.toggleRouteBypass(route);
    Editor.refresh();
  });
  const remove = element('button', 'tiny', '×');
  remove.addEventListener('click', () => free(route));
  const root = element('span', 'route-buttons');
  root.append(bypass, remove);
  return { root, bypass, remove };
}

function paintButtons(buttons, route) {
  const bypassed = session.routeBypassed(route);
  buttons.bypass.classList.toggle('on', bypassed);
  buttons.remove.hidden = !session.routeRemovable(route);
  return bypassed;
}

function destinationRow(route, label, options, choose, name = route.destination) {
  const root = element('div', 'switch-row');
  root.append(dom.ccLabeled('label', label, Patch.CC[name]));
  const picks = element('div', 'picks');
  const buttons = options.map(([name, option]) => {
    const button = element('button', null, name);
    button.addEventListener('click', () => {
      if (root.classList.contains('locked') || root.classList.contains('inert')) return;
      Transition.snap();
      choose(route, option);
    });
    picks.appendChild(button);
    return [option, button];
  });
  root.appendChild(picks);
  return { root, buttons };
}

const setCycle = (route, once) =>
  session.setValue(route.ratio, Protocol.routeRatioValue(Protocol.routeRatio(session.liveControls()[route.ratio]), once));
const sourceRow = route => destinationRow(route, 'Source', SOURCE_NAMES, session.setRouteSource, route.ratio);
const cycleRow = route => destinationRow(route, 'Cycle', [['loop', false], ['once', true]], setCycle, route.ratio);
const arpRow = route => destinationRow(route, 'Arp', Object.entries(Protocol.ARP), session.setRouteArp);
const polarityRow = route => destinationRow(route, 'Polarity', [['unipolar', false], ['bipolar', true]], session.setRouteBipolar);

function paintDestinationRow(row, shown, chosen) {
  row.root.hidden = !shown;
  row.root.classList.toggle('locked', !!session.heldAt());
  for (const [option, button] of row.buttons) button.classList.toggle('on', option === chosen);
}

function paintArpRow(row, route, live) {
  const shown = Patch.arpCapable(panel.target) && sourceOf(route, live) === Protocol.ROUTE_SOURCE.lfo;
  paintDestinationRow(row, shown, Protocol.routeArp(live[route.destination]));
}

function paintCycleRow(row, route, live) {
  paintDestinationRow(row, true, Protocol.routeOnce(live[route.ratio]));
  row.root.classList.toggle('inert', Patch.fromPad(live, route));
}

function paintPolarityRow(row, route, live) {
  const destination = live[route.destination];
  const shown = Patch.bipolarCapable(panel.target) && Protocol.routeArp(destination) === Protocol.ARP.unison;
  paintDestinationRow(row, shown, Protocol.routeBipolar(destination));
}

function free(route) {
  Transition.snap();
  session.freeRoute(route);
  if (panel.target && !session.routesOn(panel.target, session.liveControls()).length) close();
}

function buildPanel() {
  const root = element('div', 'route-panel');
  root.hidden = true;
  for (const route of Patch.ROUTES) {
    const block = element('div', 'route-block');
    const head = element('div', 'route-head');
    const buttons = routeButtons(route);
    const clock = element('span', 'cc route-clock');
    head.append(element('span', 'route-name', route.name), clock, buttons.root);
    const source = sourceRow(route);
    const arp = arpRow(route);
    const polarity = polarityRow(route);
    const cycle = cycleRow(route);
    const body = element('div');
    body.append(source.root, arp.root, polarity.root, cycle.root);
    Rows.buildRows(body, route.controls);
    const canvas = element('canvas', 'route-wave');
    block.append(head, body, canvas);
    root.appendChild(block);
    panel.blocks.push({ route, block, canvas, buttons, source, arp, polarity, cycle, clock });
  }
  const foot = element('div', 'route-foot');
  const add = element('button', 'tiny', '+ add a route');
  add.addEventListener('click', () => {
    Transition.snap();
    session.addRoute(panel.target);
  });
  const freeCount = element('span', 'cc');
  foot.append(add, freeCount);
  root.append(foot);
  document.body.appendChild(root);
  Object.assign(panel, { root, add, free: freeCount });

  document.addEventListener('keydown', event => { if (event.key === 'Escape') close(); });
  document.addEventListener('pointerdown', event => {
    if (panel.target && !root.contains(event.target) && !event.target.closest('.routes-button')) close();
  });
  window.addEventListener('resize', place);
}

function buildList() {
  const root = element('details', 'route-list');
  root.open = true;
  const count = element('span', 'cc');
  const summary = element('summary', null, 'Routes ');
  summary.appendChild(count);
  root.appendChild(summary);
  root.addEventListener('toggle', () => Editor.paint());

  for (const route of Patch.ROUTES) {
    const line = element('div', 'route-line');
    const target = element('button', 'route-line-target');
    target.addEventListener('click', () => {
      const name = Protocol.NAME_BY_CC[Protocol.routeTarget(session.liveControls()[route.destination])];
      const row = Rows.rows[name];
      if (!row) return;
      row.root.scrollIntoView({ block: 'center' });
      if (panel.target !== name) toggle(name);
    });
    const values = element('span', 'cc route-line-values');
    const canvas = element('canvas', 'route-wave route-line-wave');
    const buttons = routeButtons(route);
    line.append(target, values, canvas, buttons.root);
    LayerDrop.source(line, () => ({
      label: `${route.name} (${destinationText(session.liveControls()[route.destination])})`,
      names: [route.amount],
    }));
    root.appendChild(line);
    list.lines.push({ route, line, target, values, canvas, buttons });
  }
  Object.assign(list, { root, count });
  return root;
}

function destinationText(destination) {
  const name = Protocol.NAME_BY_CC[Protocol.routeTarget(destination)];
  const control = Patch.CONTROLS[name];
  if (!control) return `CC ${destination}`;
  const arp = Object.keys(Protocol.ARP).find(key => Protocol.ARP[key] === Protocol.routeArp(destination));
  const form = arp !== 'unison' ? ` · ${arp}` : Protocol.routeBipolar(destination) ? ' · bipolar' : '';
  return `${Patch.PLACES[name]} · ${control.label}${form}`;
}

function paintList(live) {
  let aimed = 0;
  for (const { route, line, target, values, canvas, buttons } of list.lines) {
    const destination = live[route.destination];
    line.hidden = !destination;
    if (!destination) continue;
    aimed++;
    target.textContent = destinationText(destination);
    line.style.setProperty('--tone', `var(--${Patch.TONES[Protocol.NAME_BY_CC[Protocol.routeTarget(destination)]]})`);
    const source = sourceOf(route, live);
    const shownControls = source === Protocol.ROUTE_SOURCE.lfo ? route.controls : [route.amount];
    const readouts = shownControls.map(name => Patch.READOUTS[name](live[name], live, session.oneshotBeats()));
    if (source !== Protocol.ROUTE_SOURCE.lfo) readouts.push(sourceName(source));
    values.textContent = [...(clockedPerSpot(route, live) ? [spotClockText(live)] : []), ...readouts].join(' · ');
    line.classList.toggle('bypassed', paintButtons(buttons, route));
    if (list.root.open) drawWave(canvas, route, live);
  }
  list.count.textContent = aimed ? `${aimed} of ${Patch.ROUTES.length}` : 'none';
}

function paintMarkers(live) {
  const lfoBypassed = session.cardBypassed(Patch.LFO);
  for (const row of Object.values(Rows.rows)) {
    if (!row.routes) continue;
    const here = session.routesOn(row.control.name, live);
    const sounding = !lfoBypassed && here.some(route => !session.routeBypassed(route));
    row.routes.classList.toggle('aimed', here.length > 0);
    row.routes.classList.toggle('sounding', sounding);
    row.routes.classList.toggle('open', row.control.name === panel.target);
    row.routes.textContent = here.length > 1 ? '~' + here.length : '~';
  }
}

function paintPanel(live) {
  panel.root.hidden = !panel.target;
  if (!panel.target) return;
  const held = session.heldAt();
  const destination = Patch.CC[panel.target];
  for (const { route, block, canvas, buttons, source, arp, polarity, cycle, clock } of panel.blocks) {
    block.hidden = Protocol.routeTarget(live[route.destination]) !== destination;
    clock.textContent = clockedPerSpot(route, live) ? spotClockText(live) : '';
    block.classList.toggle('bypassed', paintButtons(buttons, route));
    paintArpRow(arp, route, live);
    paintPolarityRow(polarity, route, live);
    paintDestinationRow(source, true, sourceOf(route, live));
    paintCycleRow(cycle, route, live);
    if (!block.hidden) drawWave(canvas, route, live);
  }
  const free = session.freeRouteSlots(live).length;
  panel.add.disabled = free === 0 || !!held;
  panel.free.textContent = `${free} of ${Patch.ROUTES.length} free`;
  place();
}

function paint(live) {
  paintMarkers(live);
  paintPanel(live);
  paintList(live);
}

function place() {
  const row = panel.target && Rows.rows[panel.target];
  if (!row) return;
  const bounds = row.root.getBoundingClientRect();
  const root = panel.root;
  root.style.width = `${Math.max(PANEL_MIN_WIDTH, bounds.width)}px`;
  const height = root.offsetHeight;
  const below = bounds.bottom + 4;
  const above = bounds.top - 4 - height;
  const top = below + height > window.innerHeight && above > 0 ? above : below;
  root.style.left = `${bounds.left + window.scrollX}px`;
  root.style.top = `${top + window.scrollY}px`;
}

function toggle(name) {
  if (panel.target === name) {
    close();
    return;
  }
  panel.target = name;
  if (!session.heldAt() && !session.routesOn(name, session.liveControls()).length) {
    Transition.snap();
    session.addRoute(name);
  }
  Editor.paint();
}

function close() {
  panel.target = null;
  Editor.paint();
}

export { buildPanel, buildList, paint, toggle, paintPlayheads };
