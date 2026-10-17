import { Protocol } from './cc.js';
import * as Rows from './control-rows.js';
import * as dom from './dom.js';
import * as Editor from './editor.js';
import * as EnvelopeGraph from './envelope-graph.js';
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

const targetOf = (route, live) => Patch.targetName(live[route.destination]);
const aiming = (route, live) => Patch.aimsRoute(live[route.destination]);
const centered = (route, live) => aiming(route, live) || Patch.swings(targetOf(route, live))
  || (Patch.bipolarCapable(targetOf(route, live)) && Protocol.routeBipolar(live[route.polarity]));

const dialedCurve = (route, live) => ({ amount: Protocol.signedOf(live[route.amount]), shift: 0 });

const BIRTHS_SHOWN = 8;
const STRAIGHT_THROUGH = [Protocol.GROUP_PATTERN.evensOdds, Protocol.GROUP_PATTERN.random];

function birthsInCycle(live) {
  const rate = Preview.convert(Protocol.CC.cellRate, live.cellRate);
  return rate > 0 ? Math.floor(Preview.convert(Protocol.CC.lfoRate, live.lfoRate) / rate + 0.001) : 1;
}

function membersOf(route, live) {
  const source = sourceOf(route, live);
  if (source === Protocol.ROUTE_SOURCE.strip) return Preview.WALL_STRIP_ORDER.map(strip => strip - 1);
  const count = source === Protocol.ROUTE_SOURCE.note ? Preview.convert(Protocol.CC.cellChord, live.cellChord)
    : STRAIGHT_THROUGH.includes(Protocol.groupPattern(live[route.wave])) ? BIRTHS_SHOWN : birthsInCycle(live);
  return Array.from({ length: Math.max(1, count) }, (_, member) => member);
}

function memberLevel(route, member, live) {
  const pattern = Protocol.groupPattern(live[route.wave]);
  const source = sourceOf(route, live);
  if (source === Protocol.ROUTE_SOURCE.birth && pattern === Protocol.GROUP_PATTERN.evensOdds) return member % 2;
  const count = source === Protocol.ROUTE_SOURCE.strip ? Preview.STRIPS : membersOf(route, live).length;
  return Preview.groupLevel(pattern, member, count, Patch.ROUTES.indexOf(route));
}

function groupCurves(route, aims, grouping, live) {
  const source = sourceOf(grouping, live);
  return membersOf(grouping, live).map(member => {
    const curve = dialedCurve(route, live);
    for (const [aim, phase] of aims) {
      if (!fromGroup(aim, live) || sourceOf(aim, live) !== source) continue;
      let amount = Protocol.signedOf(live[aim.amount]);
      let level = memberLevel(aim, member, live);
      if (amount < 0) {
        amount = -amount;
        level = 1 - level;
      }
      if (phase) curve.shift += amount * (level - 0.5);
      else curve.amount += amount * (level - 0.5) * 2;
    }
    curve.amount = Math.max(-1, Math.min(1, curve.amount));
    return curve;
  });
}

function curvesOf(route, live) {
  const aims = [...Patch.aimsAt(live, route, false).map(aim => [aim, false]),
                ...Patch.aimsAt(live, route, true).map(aim => [aim, true])];
  if (!aims.length) return [dialedCurve(route, live)];
  const grouping = aims.map(([aim]) => aim).find(aim => fromGroup(aim, live)
    && sourceOf(aim, live) !== Protocol.ROUTE_SOURCE.strip);
  if (grouping) return groupCurves(route, aims, grouping, live);
  const index = Patch.ROUTES.indexOf(route);
  const amounts = Preview.routeAmountAtStrips(index);
  const shifts = Preview.routeShiftAtStrips(index);
  return Preview.WALL_STRIP_ORDER.map(strip => ({ amount: amounts[strip - 1], shift: shifts[strip - 1] }));
}

const curvesKey = curves => curves.map(curve => `${curve.amount.toFixed(3)} ${curve.shift.toFixed(3)}`).join(',');

function redrawIfMoved(canvas, route, live) {
  if (canvas.curves !== curvesKey(curvesOf(route, live))) drawWave(canvas, route, live);
}

function departureAt(route, phase, live, curve) {
  const wave = live[route.wave], smooth = live[route.smooth];
  const at = Preview.lfoWave(turnsOf(route, shownPhase(phase), live) + curve.shift, wave, smooth);
  return curve.amount * (centered(route, live) ? at - Preview.waveMean(wave, smooth) : at);
}

const SOURCE_NAMES = [
  ['LFO', Protocol.ROUTE_SOURCE.lfo], ['pad X', Protocol.ROUTE_SOURCE.padX], ['pad Y', Protocol.ROUTE_SOURCE.padY],
  ...Array.from({ length: Protocol.ENVELOPES }, (_, envelope) =>
    [`env ${envelope + 1}`, Protocol.ROUTE_SOURCE[`envelope${envelope + 1}`]]),
  ['strip', Protocol.ROUTE_SOURCE.strip], ['note', Protocol.ROUTE_SOURCE.note], ['birth', Protocol.ROUTE_SOURCE.birth],
];
const PATTERN_NAMES = [['sequence', 0], ['mirror', 1], ['evens / odds', 2], ['pairs', 3], ['random', 4]];
const patternName = (route, live) => PATTERN_NAMES.find(([, pattern]) => pattern === Protocol.groupPattern(live[route.wave]))[0];
const sourceOf = (route, live) => Protocol.routeSource(live[route.ratio]);
const envelopeOf = (route, live) => sourceOf(route, live) - Protocol.ROUTE_SOURCE.envelope1;
const fromEnvelope = (route, live) => envelopeOf(route, live) >= 0 && envelopeOf(route, live) < Protocol.ENVELOPES;
const fromGroup = (route, live) => Patch.fromGroup(live, route);
const heldText = (route, live) => (Patch.holdable(live, route) && Protocol.routeHeld(live[route.hold]) ? ['held'] : []);
const clockedPerSpot = (route, live) =>
  Patch.perSpot(live[route.destination]) && sourceOf(route, live) === Protocol.ROUTE_SOURCE.lfo;
const sourceName = source => SOURCE_NAMES.find(([, value]) => value === source)[0];

function padDeparture(route, level, live, curve) {
  return curve.amount * (centered(route, live) ? level - 0.5 : level);
}

function groupDeparture(route, along, live, curve) {
  const members = membersOf(route, live);
  const member = members[Math.min(members.length - 1, Math.floor(along * members.length))];
  let amount = curve.amount;
  let level = memberLevel(route, member, live);
  if (amount < 0) {
    amount = -amount;
    level = 1 - level;
  }
  return amount * (centered(route, live) ? level - 0.5 : level);
}

function envelopeDeparture(route, along, live, curve) {
  const envelope = envelopeOf(route, live);
  return curve.amount * EnvelopeGraph.levelAt(live, envelope, along * EnvelopeGraph.beatsShown(live, envelope));
}

function drawWave(canvas, route, live) {
  const width = canvas.clientWidth, height = canvas.clientHeight;
  if (!width || !height) return;
  const scale = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = Math.round(width * scale);
  canvas.height = Math.round(height * scale);
  const context = canvas.getContext('2d', { willReadFrequently: true });
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
  const envelopeSource = fromEnvelope(route, live);
  const groupSource = fromGroup(route, live);
  context.strokeStyle = 'rgba(255,255,255,.22)';
  context.lineWidth = 1;
  for (const phase of padSource ? [] : [0, cycles()]) {
    const x = Math.round(xOfPhase(phase, width)) + 0.5;
    context.beginPath();
    context.moveTo(x, 0);
    context.lineTo(x, height);
    context.stroke();
  }

  const curves = curvesOf(route, live);
  canvas.curves = curvesKey(curves);
  context.strokeStyle = getComputedStyle(canvas).color;
  context.lineWidth = curves.length > 1 ? 1.5 : 2;
  context.lineJoin = 'round';
  for (const curve of curves) {
    context.beginPath();
    for (let x = 0; x <= width; x++) {
      const value = envelopeSource ? envelopeDeparture(route, x / width, live, curve)
        : groupSource ? groupDeparture(route, x / width, live, curve)
        : padSource ? padDeparture(route, x / width, live, curve) : departureAt(route, phaseOfX(x, width), live, curve);
      if (x === 0) context.moveTo(x, y(value));
      else context.lineTo(x, y(value));
    }
    context.stroke();
  }
  canvas.wave = context.getImageData(0, 0, canvas.width, canvas.height);
}

const spotClockText = live => 'per spot, ' + Patch.READOUTS.scatterRate(live.scatterRate, live);

function drawPlayhead(canvas, route, lfo, live) {
  if (!canvas.wave || canvas.wave.width !== canvas.width || clockedPerSpot(route, live) || fromEnvelope(route, live)
    || fromGroup(route, live)) return;
  const width = canvas.clientWidth, height = canvas.clientHeight;
  const context = canvas.getContext('2d', { willReadFrequently: true });
  context.putImageData(canvas.wave, 0, 0);
  const source = sourceOf(route, live);
  let x, valueOf;
  if (source !== Protocol.ROUTE_SOURCE.lfo) {
    const level = Pad.fingerLevel(source);
    if (level === null) return;
    x = level * width;
    valueOf = curve => padDeparture(route, level, live, curve);
  } else {
    if (lfo === null) return;
    const along = wrapped(lfo / cycles());
    x = xOfPhase(along * cycles(), width);
    valueOf = curve => departureAt(route, along * cycles(), live, curve);
  }
  context.strokeStyle = 'rgba(255,255,255,.35)';
  context.lineWidth = 1;
  context.beginPath();
  context.moveTo(x, 0);
  context.lineTo(x, height);
  context.stroke();
  context.fillStyle = '#fff';
  for (const curve of curvesOf(route, live)) {
    const y = (height / 2) - valueOf(curve) * (height / 2 - WAVE_PADDING);
    context.beginPath();
    context.arc(x, y, 3, 0, Math.PI * 2);
    context.fill();
  }
}

function paintPlayheads(lfo, live) {
  if (list.root) {
    for (const { route, line, canvas } of list.lines) {
      if (line.hidden) continue;
      redrawIfMoved(canvas, route, live);
      drawPlayhead(canvas, route, lfo, live);
    }
  }
  if (panel.target) {
    for (const { route, block, canvas } of panel.blocks) {
      if (block.hidden) continue;
      redrawIfMoved(canvas, route, live);
      drawPlayhead(canvas, route, lfo, live);
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
const polarityRow = route => destinationRow(route, 'Polarity', [['unipolar', false], ['bipolar', true]], session.setRouteBipolar,
  route.polarity);
const patternRow = route => destinationRow(route, 'Pattern', PATTERN_NAMES, session.setRoutePattern, route.wave);

function paintDestinationRow(row, shown, chosen) {
  row.root.hidden = !shown;
  row.root.classList.toggle('locked', !!session.heldAt());
  for (const [option, button] of row.buttons) button.classList.toggle('on', option === chosen);
}

function paintArpRow(row, route, live) {
  const target = targetOf(route, live);
  const shown = !!target && Patch.arpCapable(target) && sourceOf(route, live) === Protocol.ROUTE_SOURCE.lfo;
  paintDestinationRow(row, shown, Protocol.routeArp(live[route.destination]));
}

function paintPatternRow(row, route, live) {
  paintDestinationRow(row, fromGroup(route, live), Protocol.groupPattern(live[route.wave]));
}

function paintCycleRow(row, route, live) {
  paintDestinationRow(row, true, Protocol.routeOnce(live[route.ratio]));
  row.root.classList.toggle('inert', !Patch.fromLfo(live, route));
}

function paintPolarityRow(row, route, live) {
  const target = targetOf(route, live);
  const shown = !!target && Patch.bipolarCapable(target) && Protocol.routeArp(live[route.destination]) === Protocol.ARP.unison;
  paintDestinationRow(row, shown, Protocol.routeBipolar(live[route.polarity]));
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
    const title = element('span', 'route-name', route.name);
    head.append(title, clock, buttons.root);
    const source = sourceRow(route);
    const pattern = patternRow(route);
    const arp = arpRow(route);
    const polarity = polarityRow(route);
    const cycle = cycleRow(route);
    const body = element('div');
    body.append(source.root, pattern.root, arp.root, polarity.root, cycle.root);
    Rows.buildRows(body, route.controls);
    const canvas = element('canvas', 'route-wave');
    block.append(head, body, canvas);
    root.appendChild(block);
    panel.blocks.push({ route, block, canvas, buttons, title, source, pattern, arp, polarity, cycle, clock });
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
  const root = element('div', 'route-list');
  const count = element('span', 'cc');

  for (const route of Patch.ROUTES) {
    const line = element('div', 'route-line');
    const target = element('button', 'route-line-target');
    target.addEventListener('click', () => {
      const name = panelTargetOf(route, session.liveControls());
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
  return { root, count };
}

function aimedRouteOf(route, live) {
  return Patch.ROUTES[Protocol.aimedRoute(live[route.destination])];
}

function panelTargetOf(route, live) {
  return aiming(route, live) ? targetOf(aimedRouteOf(route, live), live) : targetOf(route, live);
}

function destinationText(destination) {
  const name = Patch.targetName(destination);
  const control = Patch.CONTROLS[name];
  if (!control) return `CC ${destination}`;
  const arp = Object.keys(Protocol.ARP).find(key => Protocol.ARP[key] === Protocol.routeArp(destination));
  const form = arp !== 'unison' ? ` · ${arp}` : '';
  return `${Patch.PLACES[name]} · ${control.label}${form}`;
}

function amountRangeText(route, live) {
  const amounts = curvesOf(route, live).map(curve => Math.round(curve.amount * 100));
  const sign = value => (value < 0 ? '−' : '+') + Math.abs(value) + '%';
  return `${sign(Math.min(...amounts))} … ${sign(Math.max(...amounts))}`;
}

function paintList(live) {
  let aimed = 0;
  for (const { route, line, target, values, canvas, buttons } of list.lines) {
    const destination = live[route.destination];
    line.hidden = !destination;
    if (!destination) continue;
    aimed++;
    const bipolar = Patch.bipolarCapable(targetOf(route, live)) && Protocol.routeBipolar(live[route.polarity]);
    target.textContent = destinationText(destination) + (bipolar ? ' · bipolar' : '');
    line.style.setProperty('--tone', `var(--${Patch.TONES[targetOf(route, live)]})`);
    const source = sourceOf(route, live);
    const shownControls = source === Protocol.ROUTE_SOURCE.lfo ? route.controls.filter(name => name !== route.hold) : [route.amount];
    const readouts = shownControls.map(name => (name === route.amount && Patch.aimsAt(live, route, false).length
      ? amountRangeText(route, live) : Patch.READOUTS[name](live[name], live, session.oneshotBeats())));
    if (source !== Protocol.ROUTE_SOURCE.lfo) readouts.push(sourceName(source));
    if (fromGroup(route, live)) readouts.push(patternName(route, live));
    readouts.push(...heldText(route, live));
    values.textContent = [...(clockedPerSpot(route, live) ? [spotClockText(live)] : []), ...readouts].join(' · ');
    line.classList.toggle('bypassed', paintButtons(buttons, route));
    drawWave(canvas, route, live);
  }
  list.count.textContent = aimed ? `${aimed} of ${Patch.ROUTES.length}` : 'none';
}

function paintMarkers(live) {
  const routesBypassed = session.cardBypassed(Patch.ROUTES_CARD);
  for (const row of Object.values(Rows.rows)) {
    if (!row.routes) continue;
    const here = session.routesOn(row.control.name, live);
    const sounding = !routesBypassed && here.some(route => !session.routeBypassed(route));
    const owner = Patch.routeOf(row.control.name);
    row.routes.hidden = !!owner && aiming(owner, live);
    row.routes.classList.toggle('aimed', here.length > 0);
    row.routes.classList.toggle('sounding', sounding);
    row.routes.classList.toggle('open', row.control.name === panel.target);
    row.routes.textContent = here.length > 1 ? '~' + here.length : '~';
  }
}

function orderBlocks(live) {
  const blockOf = route => panel.blocks.find(entry => entry.route === route).block;
  const order = Patch.ROUTES.filter(route => !aiming(route, live)).flatMap(route => [route,
    ...Patch.ROUTES.filter(other => aiming(other, live) && aimedRouteOf(other, live) === route)]).map(blockOf);
  const shown = Array.from(panel.root.children).filter(child => child.classList.contains('route-block'));
  if (order.every((block, i) => shown[i] === block)) return;
  const foot = panel.add.parentNode;
  for (const block of order) panel.root.insertBefore(block, foot);
}

function paintPanel(live) {
  panel.root.hidden = !panel.target;
  if (!panel.target) return;
  const held = session.heldAt();
  for (const { route, block, canvas, buttons, title, source, pattern, arp, polarity, cycle, clock } of panel.blocks) {
    const aims = aiming(route, live);
    block.hidden = !live[route.destination] || panelTargetOf(route, live) !== panel.target;
    block.classList.toggle('aim', aims);
    title.textContent = aims
      ? `${route.name} → ${aimedRouteOf(route, live).name} ${Protocol.aimsPhase(live[route.destination]) ? 'phase' : 'amount'}`
      : route.name;
    paintPatternRow(pattern, route, live);
    clock.textContent = clockedPerSpot(route, live) ? spotClockText(live) : '';
    block.classList.toggle('bypassed', paintButtons(buttons, route));
    paintArpRow(arp, route, live);
    paintPolarityRow(polarity, route, live);
    paintDestinationRow(source, true, sourceOf(route, live));
    paintCycleRow(cycle, route, live);
    if (!block.hidden) drawWave(canvas, route, live);
  }
  orderBlocks(live);
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

function aim(name) {
  const live = session.liveControls();
  const owner = Patch.routeOf(name);
  if (aiming(owner, live) || session.heldAt() || session.routesOn(name, live).length) return;
  Transition.snap();
  session.addRoute(name);
}

function toggle(name) {
  if (Patch.routeOf(name)) {
    aim(name);
    return;
  }
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
