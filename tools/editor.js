import { Protocol } from './cc.js';
import * as Rows from './control-rows.js';
import * as dom from './dom.js';
import * as Keep from './keep.js';
import * as LayerDrop from './layer-drop.js';
import * as LibraryFile from './library-file.js';
import * as Rail from './library-rail.js';
import * as Library from './library.js';
import * as Midi from './midi-out.js';
import * as Oneshots from './oneshots.js';
import * as Patch from './patch.js';
import * as Playback from './playback.js';
import * as Routes from './route-panel.js';
import { session } from './session.js';
import * as Songs from './songs.js';
import { say } from './status.js';
import * as Transition from './transition.js';
import * as Wall from './wall.js';

const { element, byId } = dom;

const tabs = [];

function buildTabs() {
  const buttons = Patch.LAYERS.map(layer => {
    const button = element('button', 'tab');
    const badge = element('span', 'badge');
    button.append(element('span', null, Patch.LAYER_NAMES[layer]), badge);
    button.addEventListener('click', () => selectLayer(layer));
    if (Patch.isAboveBase(layer)) LayerDrop.target(button, layer);
    tabs.push({ layer, button, badge });
    return button;
  });
  byId('layerTabs').replaceChildren(...buttons);
}

function paintTabs() {
  document.body.classList.toggle('above-base', session.isAboveBase());
  for (const { layer, button, badge } of tabs) {
    button.classList.toggle('on', layer === session.layerIndex);
    button.classList.toggle('unsaved', session.layerUnsaved(layer));
    badge.textContent = Patch.isAboveBase(layer)
      ? Transition.changesText(layer) : '';
  }
}

function selectLayer(layer) {
  session.selectLayer(layer);
  Transition.rebuild();
  paint();
  Rail.paintList();
  Playback.changed();
}

const TIME_FIELDS = {
  transitionTime: { names: Patch.TRANSITION_NAMES, step: Patch.transitionStep, value: Patch.transitionValue },
  accentTime: { names: Patch.LFO_PERIOD_NAMES, step: Patch.periodStep, value: Patch.periodValue },
};

function buildHead() {
  for (const [field, steps] of Object.entries(TIME_FIELDS)) {
    const select = byId(field);
    dom.setOptions(select, steps.names.map((text, step) => [steps.value(step), text]), '');
    select.addEventListener('change', () => {
      session.editing()[field] = +select.value;
      session.changed();
    });
  }

  const tempo = byId('tempoDivision');
  dom.setOptions(tempo, Patch.TEMPO_DIVISIONS, Protocol.TEMPO_DIVISION.quarter);
  tempo.addEventListener('change', () => {
    Transition.snap();
    session.setValue('tempoDivision', +tempo.value);
  });

  const name = byId('patchName');
  name.maxLength = Protocol.PATCH_NAME_LENGTH;
  name.addEventListener('input', () => {
    const printable = LibraryFile.printableName(name.value);
    if (printable !== name.value) name.value = printable;
    session.editing().name = printable;
    session.changed();
    Rail.paintList();
  });
}

function paintHead() {
  const patch = session.patch();
  byId('subjectKind').textContent = session.editingOneshot() ? 'Oneshot' : 'Patch';
  if (byId('patchName').value !== patch.name) byId('patchName').value = patch.name;
  if (session.editingOneshot()) {
    Oneshots.paintHead();
    return;
  }
  Oneshots.paintPicks();
  for (const [field, steps] of Object.entries(TIME_FIELDS)) {
    byId(field).value = String(steps.value(steps.step(patch[field])));
  }
  byId('tempoDivision').value = String(Patch.controls(patch.base).tempoDivision);
  const accentReachesNothing = !Library.changedIn(patch, Protocol.PATCH_LAYER_ACCENT).length;
  byId('accentTime').closest('.field').classList.toggle('inert', accentReachesNothing);
}

function paint() {
  document.body.classList.toggle('oneshot-mode', session.editingOneshot());
  Rows.paint(Playback.shownControls());
  paintTabs();
  Routes.paint(session.liveControls());
  paintHead();
  Transition.paintShowing();
  Transition.paintMix();
}

function refresh() {
  Playback.changed();
  paint();
}

function open(slot, draft) {
  session.select(slot, draft);
  Transition.rebuild();
  Playback.cut();
  paint();
  Rail.paintList();
}

function follow(slot) {
  session.layerIndex = Protocol.PATCH_LAYER_BASE;
  session.select(slot, null);
  Transition.rebuild();
  Playback.follow(slot);
  paint();
  Rail.paintList();
}

function sendPatchToWall() {
  Playback.dial();
  say(`sent "${session.patch().name}"`);
}

const STORE_SHOW_MIDI = 'aurora.editor.showMidi';

function showMidi(shown) {
  document.body.classList.toggle('show-midi', shown);
  byId('midiToggle').classList.toggle('on', shown);
  try {
    localStorage.setItem(STORE_SHOW_MIDI, JSON.stringify(shown));
  } catch {
    return;
  }
}

function wireMidiToggle() {
  let stored = false;
  try {
    stored = JSON.parse(localStorage.getItem(STORE_SHOW_MIDI)) === true;
  } catch {
    stored = false;
  }
  showMidi(stored);
  byId('midiToggle').addEventListener('click', () => showMidi(!document.body.classList.contains('show-midi')));
  byId('tempoDivisionCC').textContent = `CC ${Patch.CC.tempoDivision}`;
}

function measureTopbar() {
  const topbar = byId('topbar');
  const measure = () => document.documentElement.style
    .setProperty('--topbar', Math.round(topbar.getBoundingClientRect().height) + 'px');
  new ResizeObserver(measure).observe(topbar);
  measure();
}

export { paint, refresh, open, follow, selectLayer };

session.load();
session.transition.from = session.slot;
session.onChange = refresh;
session.onDraftStarted = () => Rail.paintList();
session.onLibrarySaved = () => Playback.syncLibrary();
window.addEventListener('pagehide', session.flush);
document.addEventListener('visibilitychange', session.flush);

Rows.build();
Routes.buildPanel();
buildTabs();
buildHead();
Rail.wire();
Oneshots.wire();
Songs.wire();
Keep.wire();
Midi.wire();
byId('sendPatch').addEventListener('click', sendPatchToWall);
wireMidiToggle();
measureTopbar();

Playback.start();
Wall.start();
Transition.rebuild();
paint();
Rail.paintList();
Midi.open();
say('ready — ask the brain what it holds to check the link');
