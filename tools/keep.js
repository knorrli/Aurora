import * as dom from './dom.js';
import * as Editor from './editor.js';
import * as Rail from './library-rail.js';
import * as Library from './library.js';
import * as Patch from './patch.js';
import * as Playback from './playback.js';
import { session } from './session.js';
import { say } from './status.js';
import * as Transition from './transition.js';

const { byId } = dom;

const NEW_PATCH = 'new';

const labelOf = name => {
  if (Patch.CONTROLS[name]) return Patch.CONTROLS[name].label;
  const route = Patch.ROUTES.find(candidate => candidate.fields.includes(name));
  return route ? route.name : name;
};

function report(layer, leftOnBase) {
  const kept = `kept as ${Patch.LAYER_NAMES[layer]}`;
  if (!leftOnBase.length) {
    say(kept, 'ok');
    return;
  }
  const labels = [...new Set(leftOnBase.map(labelOf))].join(', ');
  say(`${kept}; ${labels} stayed on the base — switches belong to the whole patch`, 'warn');
}

function keepAsLayer(layer) {
  const shown = Playback.shownControls();
  const patch = session.editing();
  const leftOnBase = Patch.isAboveBase(layer) ? Library.keepAsLayer(patch, layer, shown) : [];
  if (!Patch.isAboveBase(layer)) Library.keepAsBase(patch, shown);
  session.resetTransition();
  Transition.rebuild();
  session.changed();
  report(layer, leftOnBase);
}

function keepAsNewPatch() {
  const shown = Playback.shownControls();
  const current = session.patch();
  if (!Rail.leaveDraft()) return;
  const patch = Library.newPatch(current.name);
  Object.assign(patch, { transitionTime: current.transitionTime, accentTime: current.accentTime });
  Library.keepAsBase(patch, shown);
  Editor.open(null, patch);
  say('kept as a new patch — save it into a slot', 'ok');
}

function wire() {
  const select = byId('keepAs');
  dom.setOptions(select, [
    ['', 'keep as…'],
    ...Patch.LAYERS.map(layer => [layer, Patch.LAYER_NAMES[layer]]),
    [NEW_PATCH, 'new patch'],
  ], '');
  select.addEventListener('change', () => {
    const choice = select.value;
    select.value = '';
    if (choice === NEW_PATCH) keepAsNewPatch();
    else if (choice !== '') keepAsLayer(+choice);
  });
}

export { wire };
