import { Protocol } from './cc.js';
import * as dom from './dom.js';
import * as Editor from './editor.js';
import * as Library from './library.js';
import * as Patch from './patch.js';
import * as Playback from './playback.js';
import { session } from './session.js';
import { say } from './status.js';

const { element, byId } = dom;

const SCRUB_STEPS = 1000;

const bar = { scrub: null, run: null, from: null, clear: null };
const mixBar = { sliders: {}, readouts: {} };

const percentOf = position => Math.round(position * 100) + '%';

function changesText(layer) {
  const count = Library.changedIn(session.patch(), layer).length;
  if (!count) return 'no changes';
  return `${count} change${count === 1 ? '' : 's'}`;
}

function showPosition() {
  if (bar.scrub) bar.scrub.value = Math.round(session.transition.position * SCRUB_STEPS);
}

function snap() {
  if (session.transition.position === 1) return;
  session.resetTransition();
  showPosition();
  Editor.refresh();
}

function run() {
  const from = session.transition.from;
  if (!session.comingFrom()) return;
  snap();
  Playback.run(from);
}

function paintRun() {
  if (bar.run) bar.run.disabled = !session.comingFrom();
}

function pushedFaders() {
  return Object.entries(Playback.faders).filter(([, position]) => position > 0);
}

function showingText() {
  if (session.editingOneshot()) return `over "${session.over().name}"`;
  const position = session.transition.position;
  if (session.isAboveBase()) return `${Patch.LAYER_NAMES[session.layerIndex]} ${percentOf(position)}`;
  if (session.transitioning()) return `from "${session.comingFrom().name}" ${percentOf(position)}`;
  const pushed = pushedFaders();
  if (pushed.length) {
    return pushed.map(([layer, position]) => `${Patch.LAYER_NAMES[layer]} ${percentOf(position)}`).join(' + ');
  }
  return 'the patch';
}

function clearLayer() {
  const layer = session.layerIndex;
  Library.clearLayer(session.editing(), layer);
  session.changed();
  say(`cleared ${Patch.LAYER_NAMES[layer]}`, 'ok');
}

function paintShowing() {
  byId('wallShowing').textContent = showingText();
  if (bar.clear) bar.clear.disabled = !Library.changedIn(session.patch(), session.layerIndex).length;
}

function labeledField(text, control) {
  const field = element('label', 'field');
  field.append(element('span', null, text), control);
  return field;
}

function patchOptions(slots) {
  return slots.map(slot => [slot, `${slot} · ${session.patchAt(slot).name}`]);
}

function refreshPatchChoices() {
  session.forgetMissingPatches();
  if (bar.from) {
    const from = session.transition.from;
    const others = Library.filledSlots(session.library).filter(slot => slot !== session.slot);
    dom.setOptions(bar.from, [['', '— nowhere —'], ...patchOptions(others)],
      from === null || from === session.slot ? '' : from);
  }
  paintRun();
}

function buildFrom() {
  const select = element('select');
  select.addEventListener('change', () => {
    session.transition.from = select.value === '' ? session.slot : +select.value;
    paintRun();
    Editor.refresh();
  });
  bar.from = select;
  return labeledField('from', select);
}

function build() {
  const host = byId('transitionBar');
  host.hidden = session.editingOneshot();
  const aboveBase = session.isAboveBase();
  Object.assign(bar, { run: null, from: null, clear: null });

  const scrub = element('div', 'scrub');
  bar.scrub = dom.rangeInput(SCRUB_STEPS);
  bar.scrub.id = 'transitionScrub';
  bar.scrub.addEventListener('input', () => {
    session.transition.position = +bar.scrub.value / SCRUB_STEPS;
    Editor.refresh();
  });
  scrub.appendChild(bar.scrub);

  const heading = element('h2', null, aboveBase ? `Base to ${Patch.LAYER_NAMES[session.layerIndex]}` : 'Transition');
  if (aboveBase) {
    bar.clear = element('button', 'tiny', 'clear');
    bar.clear.addEventListener('click', clearLayer);
    heading.appendChild(bar.clear);
  }
  const fields = element('div', 'fields');
  host.replaceChildren(heading, scrub);
  if (!aboveBase) {
    bar.run = element('button', 'tiny', 'run');
    bar.run.id = 'transitionRun';
    bar.run.addEventListener('click', run);
    scrub.appendChild(bar.run);
  }
  if (!aboveBase || session.layerIndex === Protocol.PATCH_LAYER_ACCENT) {
    fields.appendChild(buildFrom());
    host.appendChild(fields);
  }
  showPosition();
  refreshPatchChoices();
  paintShowing();
}

function buildMix() {
  const host = byId('mixBar');
  const hidden = session.isAboveBase() || session.editingOneshot();
  host.hidden = hidden;
  mixBar.sliders = {};
  mixBar.readouts = {};
  if (hidden) {
    host.replaceChildren();
    return;
  }

  const head = element('h2', null, 'Mix');
  const allDown = element('button', 'tiny', 'all down');
  allDown.addEventListener('click', () => {
    for (const layer of Object.keys(Playback.FADERS)) Playback.setFader(+layer, 0);
    paintMix();
    paintShowing();
  });
  head.appendChild(allDown);

  const rows = element('div', 'mix-rows');
  for (const [layer, fader] of Object.entries(Playback.FADERS)) {
    const row = element('div', 'row');
    const label = dom.ccLabeled('label', Patch.LAYER_NAMES[layer], Patch.CC[fader]);
    const track = element('div', 'track');
    const slider = dom.rangeInput(127);
    slider.id = 'mix' + layer;
    const readout = element('output');
    slider.addEventListener('input', () => {
      Playback.setFader(+layer, +slider.value / 127);
      paintMixReadout(layer);
      paintShowing();
    });
    track.append(slider);
    row.append(label, track, readout);
    rows.appendChild(row);
    mixBar.sliders[layer] = slider;
    mixBar.readouts[layer] = readout;
  }
  host.replaceChildren(head, rows);
  paintMix();
}

function paintMix() {
  for (const [layer, slider] of Object.entries(mixBar.sliders)) {
    slider.value = Math.round(Playback.faders[layer] * 127);
    paintMixReadout(layer);
  }
}

function paintMixReadout(layer) {
  mixBar.readouts[layer].textContent = percentOf(Playback.faders[layer]);
}

function rebuild() {
  build();
  buildMix();
}

export {
  snap, rebuild, refreshPatchChoices, paintShowing, paintMix, changesText,
};
