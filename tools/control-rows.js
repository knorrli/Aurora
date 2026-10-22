import { Protocol } from './controls.js';
import * as dom from './dom.js';
import * as Editor from './editor.js';
import * as EnvelopeGraph from './envelope-graph.js';
import * as LayerDrop from './layer-drop.js';
import * as Library from './library.js';
import * as Patch from './patch.js';
import * as Routes from './route-panel.js';
import { session } from './session.js';
import * as Tracks from './tracks.js';
import * as Transition from './transition.js';

const { element, byId } = dom;

const rows = {};
const cards = [];

const controlLabel = name => dom.numberLabeled('label', Patch.CONTROLS[name].label, Patch.NUMBER[name]);

function faderRow(host, name) {
  const control = Patch.CONTROLS[name];
  const root = element('div', 'row');
  root.dataset.name = name;

  const label = controlLabel(name);
  label.addEventListener('click', () => {
    Transition.snap();
    if (session.editingOneshot()) session.toggleMark(name);
    else session.resetNames([name]);
  });
  const swatch = control.swatch ? element('i', 'swatch') : null;
  if (swatch) label.firstChild.appendChild(swatch);

  const track = element('div', 'track');
  const positions = control.positions;
  const slider = dom.rangeInput(positions ? positions.count - 1 : 127);
  const ghost = element('i', 'ghost');
  track.append(slider, ghost);
  slider.addEventListener('pointerdown', () => Transition.snap());
  slider.addEventListener('keydown', () => Transition.snap());
  slider.addEventListener('input', () => session.setValue(name,
    positions ? positions.valueAt(+slider.value, session.liveControls()) : +slider.value));

  const readout = element('output');
  const now = dom.labeled('span', '', '');
  now.className = 'now';
  const room = dom.labeled('span', '127', longestReadout(name));
  room.className = 'room';
  room.setAttribute('aria-hidden', 'true');
  readout.append(now, room);

  const routes = Patch.routable(name) ? element('button', 'routes-button', '~') : null;
  if (routes) routes.addEventListener('click', () => Routes.toggle(name));

  root.append(label, track, readout, routes || element('span'));
  host.appendChild(root);
  rows[name] = {
    kind: 'fader', control, root, label, slider, ghost, now, routes, swatch,
    points: positions ? Array.from({ length: positions.count - 1 }, (_, step) => step + 0.5) : Patch.pointsFor(name),
    circular: Patch.isCircular(name),
  };
}

function longestReadout(name) {
  const readout = Patch.READOUTS[name];
  let longest = '';
  for (let value = 0; value < 128 && readout; value++) {
    const text = readout(value, Patch.DEFAULT);
    if (text.length > longest.length) longest = text;
  }
  return longest;
}

function pickRow(host, name) {
  const control = Patch.CONTROLS[name];
  const root = element('div', 'switch-row');
  root.dataset.name = name;
  const label = controlLabel(name);
  label.addEventListener('click', () => { if (session.editingOneshot()) session.toggleMark(name); });
  const picks = element('div', 'picks');
  const pick = value => {
    if (root.classList.contains('locked')) return;
    Transition.snap();
    session.setValue(name, value);
  };

  let select = null;
  let buttons = [];
  if (control.kind === 'pick') {
    select = element('select');
    dom.setOptions(select, control.options, control.options[0][0]);
    select.addEventListener('change', () => pick(+select.value));
    picks.appendChild(select);
  } else {
    buttons = control.options.map(([value, text]) => {
      const button = element('button', null, text);
      button.addEventListener('click', () => pick(value));
      picks.appendChild(button);
      return [value, button];
    });
  }
  const from = element('span', 'from');
  picks.appendChild(from);
  root.append(label, picks);
  host.appendChild(root);

  const positionOf = control.kind === 'three' ? Protocol.threeWayPosition
    : control.kind === 'four' ? Protocol.fourWayPosition
    : control.kind === 'steps' ? control.step : Protocol.isOn;
  const lit = (value, live) => positionOf(live) === positionOf(value);
  rows[name] = { kind: control.kind, control, root, select, buttons, from, lit };
}

function buildRows(host, names) {
  for (const name of names) {
    if (Patch.CONTROLS[name].kind === 'fader') faderRow(host, name);
    else pickRow(host, name);
  }
}

function resetButton(names) {
  const button = element('button', 'tiny', 'reset');
  button.addEventListener('click', () => session.resetNames(names));
  return button;
}

function withRouteAmounts(names) {
  const live = session.liveControls();
  return [...names, ...names.flatMap(name => session.routesOn(name, live).map(route => route.amount))];
}

const movable = (handle, label, names) =>
  LayerDrop.source(handle, () => ({ label, names: withRouteAmounts(names) }));

function buildGroup(title, sections, label = title, columns = false) {
  const box = element('div');
  const heading = element('h3', null, title);
  const groupNames = sections.flatMap(([, names]) => names);
  const reset = resetButton(groupNames);
  reset.classList.add('push-right');
  heading.appendChild(reset);
  movable(heading, label, groupNames);
  const body = element('div', columns ? 'card-body' : null);
  for (const [subheading, names] of sections) {
    const section = columns ? element('div') : body;
    if (subheading) {
      const handle = element('h4', 'subhead', subheading);
      movable(handle, `${label} · ${subheading}`, names);
      section.appendChild(handle);
    }
    buildRows(section, names);
    if (columns) body.appendChild(section);
  }
  box.append(heading, body);
  return box;
}

function bypassButton(card) {
  const bypass = element('button', 'tiny', 'bypass');
  bypass.addEventListener('click', () => {
    session.toggleCardBypass(card);
    Editor.refresh();
  });
  return bypass;
}

function buildCard(card) {
  const root = element('article', 'card');
  root.dataset.tone = card.tone;

  const head = element('div', 'card-head');
  const state = element('span', 'card-state');
  const bypass = card === Patch.LFO || card === Patch.ARP ? null : bypassButton(card);
  head.append(element('span', 'card-name', card.name), state, ...(bypass ? [bypass] : []),
              resetButton(Patch.cardNames(card)));
  movable(head, card.name, Patch.cardNames(card));
  root.appendChild(head);

  const body = element('div', 'card-body');
  const sections = [['Source', card.source], ['Amounts', card.amounts]].filter(([, names]) => names.length);
  for (const [title, names] of sections) {
    const box = element('div');
    if (sections.length > 1) {
      const handle = element('h4', null, title);
      movable(handle, `${card.name} · ${title}`, names);
      box.appendChild(handle);
    }
    buildRows(box, names);
    body.appendChild(box);
  }
  if (body.children.length < 2) body.classList.add('single');
  root.appendChild(body);
  cards.push({ card, root, state, bypass });
  return root;
}

function buildRoutesCard() {
  const card = Patch.ROUTES_CARD;
  const root = element('article', 'card');
  root.dataset.tone = card.tone;
  const head = element('div', 'card-head');
  const state = element('span', 'card-state');
  const bypass = bypassButton(card);
  const list = Routes.buildList();
  head.append(element('span', 'card-name', card.name), list.count, state, bypass);
  root.append(head, list.root);
  cards.push({ card, root, state, bypass });
  return root;
}

const envelopes = { root: null, state: null, tabs: [], shown: [], selected: 0 };

function buildEnvelopeBody(card) {
  const stages = element('div');
  buildRows(stages, card.names);
  const graphBox = element('div', 'envelope-graph');
  const graph = element('canvas');
  graphBox.appendChild(graph);
  const body = element('div', 'card-body');
  body.append(stages, graphBox);
  return { card, body, graph, drawn: null };
}

function buildEnvelopesCard() {
  const root = element('article', 'card envelope-card');
  root.dataset.tone = 'lfo';
  const head = element('div', 'card-head');
  const state = element('span', 'card-state');
  const tabs = Patch.ENVELOPE_CARDS.map((card, envelope) => {
    const tab = element('button', 'tiny envelope-tab', String(envelope + 1));
    tab.addEventListener('click', () => {
      envelopes.selected = envelope;
      Editor.paint();
    });
    return tab;
  });
  const reset = element('button', 'tiny', 'reset');
  reset.addEventListener('click', () => session.resetNames(Patch.ENVELOPE_CARDS[envelopes.selected].names));
  head.append(element('span', 'card-name', 'Envelopes'), ...tabs, state, reset);
  LayerDrop.source(head, () => {
    const card = Patch.ENVELOPE_CARDS[envelopes.selected];
    return { label: card.name, names: withRouteAmounts(card.names) };
  });
  const shown = Patch.ENVELOPE_CARDS.map(buildEnvelopeBody);
  root.append(head, ...shown.map(({ body }) => body));
  Object.assign(envelopes, { root, state, tabs, shown });
  return root;
}

function paintEnvelopesCard(live) {
  envelopes.shown.forEach(({ card, body }, envelope) => {
    const selected = envelope === envelopes.selected;
    body.hidden = !selected;
    envelopes.tabs[envelope].classList.toggle('on', selected);
    envelopes.tabs[envelope].classList.toggle('unused', card.unused(live));
  });
  const unused = Patch.ENVELOPE_CARDS[envelopes.selected].unused(live);
  envelopes.root.classList.toggle('no-effect', unused);
  envelopes.state.textContent = unused ? 'no effect' : '';
  paintEnvelopeGraphs(live);
}

function paintEnvelopeGraphs(live) {
  const shown = envelopes.shown[envelopes.selected];
  if (!shown) return;
  const key = [...shown.card.names, 'cellGate', 'cellEnd', 'cellRate'].map(name => live[name]).join(',')
    + `@${shown.graph.clientWidth}x${shown.graph.clientHeight}`;
  if (key === shown.drawn) return;
  shown.drawn = key;
  EnvelopeGraph.draw(shown.graph, live, shown.card.envelope);
}

function build() {
  byId('outputs').replaceChildren(...Patch.OUTPUTS.map(output => buildGroup(output.title, output.sections)));
  const clocks = element('div', 'card-pair');
  clocks.append(buildCard(Patch.LFO), buildCard(Patch.ARP));
  byId('lfo').replaceChildren(clocks, buildEnvelopesCard(), buildRoutesCard());
  const groups = element('div', 'groups');
  groups.append(...Patch.CELLS.groups.map(group => {
    const box = buildGroup(group.title, group.sections || [[null, group.names]], `${Patch.CELLS.name} · ${group.title}`,
                           !!group.sections);
    box.classList.add(`tone-${group.tone}`);
    return box;
  }));
  byId('cells').replaceChildren(element('h2', 'cells-name', Patch.CELLS.name), groups);
  byId('engines').replaceChildren(...Patch.ENGINES.map(buildCard));
}

const sliderPosition = (control, value) => (control.positions ? control.positions.positionOf(value) : value);

function paintFaderValue(row, live) {
  const value = live[row.control.name];
  const position = sliderPosition(row.control, value);
  if (+row.slider.value !== position) row.slider.value = position;
  const readout = Patch.READOUTS[row.control.name];
  dom.fillLabeled(row.now, String(value), readout ? readout(value, live, session.oneshotBeats()) : '');
}

function paintPickValue(row, live) {
  const value = live[row.control.name];
  for (const [option, button] of row.buttons) button.classList.toggle('on', row.lit(option, value));
  if (row.select && +row.select.value !== value) row.select.value = value;
}

const paintInert = (row, live) =>
  row.root.classList.toggle('inert', !!(row.control.inertWhen && row.control.inertWhen(live)));

function paintValues(live) {
  for (const row of Object.values(rows)) {
    if (row.kind === 'fader') paintFaderValue(row, live);
    else paintPickValue(row, live);
    paintInert(row, live);
  }
  paintCards(live);
  paintEnvelopesCard(live);
}

function paintFader(row, live, base, overrides) {
  const name = row.control.name;
  paintFaderValue(row, live);
  const overridden = session.isAboveBase() && name in overrides;
  row.root.classList.toggle('changed', overridden);
  if (overridden) {
    row.ghost.style.left = `calc(${Tracks.along(row.slider, sliderPosition(row.control, base[name]))} - 1px)`;
  }
  if (row.swatch) row.swatch.style.background = `rgb(${row.control.swatch(live).join(',')})`;
  paintInert(row, live);
}

function paintPick(row, live) {
  const held = session.heldAt();
  paintPickValue(row, live);
  if (row.select) row.select.disabled = !!held;
  row.root.classList.toggle('locked', !!held);
  row.from.textContent = !session.isAboveBase() ? '' : held ? `held at "${held.name}"` : 'from the patch';
  paintInert(row, live);
}

function paintCards(live) {
  for (const { card, root, state, bypass } of cards) {
    const bypassed = session.cardBypassed(card);
    const noEffect = session.hasNoEffect(card, live);
    root.classList.toggle('bypassed', bypassed);
    root.classList.toggle('no-effect', noEffect && !bypassed);
    if (bypass) bypass.classList.toggle('on', bypassed);
    state.textContent = bypassed ? 'bypassed' : noEffect ? 'no effect' : '';
  }
}

function paint(live) {
  const base = Patch.controls(session.patch().base);
  const overrides = session.overrides();
  const oneshot = session.editingOneshot();
  for (const row of Object.values(rows)) {
    if (row.kind === 'fader') paintFader(row, live, base, overrides);
    else paintPick(row, live);
    const name = row.control.name;
    row.root.classList.toggle('unmarked', oneshot && Library.isMarkable(name) && !session.marked(name));
  }
  paintCards(live);
  paintEnvelopesCard(live);
}

export { rows, buildRows, build, paint, paintValues };
