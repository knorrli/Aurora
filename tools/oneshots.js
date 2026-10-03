import { Protocol } from './cc.js';
import * as dom from './dom.js';
import * as Editor from './editor.js';
import * as Rail from './library-rail.js';
import * as Library from './library.js';
import * as Patch from './patch.js';
import * as Playback from './playback.js';
import { session } from './session.js';
import { say } from './status.js';
import * as Transition from './transition.js';

const { element, byId } = dom;

const PICK_FIELDS = ['oneshotPick0', 'oneshotPick1'];
const FIRE_BUTTONS = ['oneshotFire0', 'oneshotFire1'];
const DEFAULT_FIELDS = ['defaultOneshot0', 'defaultOneshot1'];

const noteOf = index => Protocol.NOTE_ONESHOT_FIRST + index;
const CORNER_NAMES = {
  [Protocol.CORNER.bottomLeft]: 'bottom left', [Protocol.CORNER.bottomRight]: 'bottom right',
  [Protocol.CORNER.topLeft]: 'top left', [Protocol.CORNER.topRight]: 'top right',
};
const CORNERS_AS_ON_THE_PAD = [Protocol.CORNER.topLeft, Protocol.CORNER.topRight,
  Protocol.CORNER.bottomLeft, Protocol.CORNER.bottomRight];
const placeText = index =>
  (index < Protocol.ONESHOTS ? `note ${noteOf(index)}` : `${CORNER_NAMES[index - Protocol.ONESHOTS]} corner`);
const kitOneshot = session.kitOneshot;
let repeat = true;
let dragged = null;

const leaveOneshotDraft = () =>
  !session.oneshotDraft || confirm(`Discard your changes to the oneshot "${session.oneshotDraft.name}"?`);

function kitOptions(empty) {
  return [['', empty], ...Library.filledKit(session.library)
    .map(index => [index, `${noteOf(index)} · ${session.library.kit[index].name}`])];
}

const pickValue = pick => (pick === null ? '' : String(pick));
const pickFrom = value => (value === '' ? null : +value);

function paintPickSelects(ids, picks, emptyOf) {
  ids.forEach((id, place) => {
    dom.setOptions(byId(id), kitOptions(emptyOf(place)), pickValue(picks[place]));
  });
}

function defaultText(place) {
  const index = session.library.defaultOneshots[place];
  const oneshot = index === null ? null : kitOneshot(index);
  return oneshot ? `default · ${oneshot.name}` : 'default';
}

function paintPicks() {
  paintPickSelects(PICK_FIELDS, session.patch().oneshots, defaultText);
  FIRE_BUTTONS.forEach((id, place) => {
    const index = session.resolvedPick(place);
    byId(id).disabled = index === null || !kitOneshot(index);
  });
}

const patchName = slot => {
  const patch = slot === session.slot ? session.underneath() : session.library.slots[slot];
  return `${slot} · ${patch.name || '(unnamed)'}`;
};

function paintOver() {
  const slots = Library.filledSlots(session.library);
  if (session.slot !== null && !slots.includes(session.slot)) slots.unshift(session.slot);
  dom.setOptions(byId('overPatch'), slots.map(slot => [slot, patchName(slot)]), String(session.overSlotShown()));
}

const fireDraft = () => Playback.fireNote(noteOf(session.oneshotIndex));

function keepRepeating() {
  if (session.editingOneshot() && !session.editingCorner() && repeat && Playback.oneshotProgress() < 0) fireDraft();
}

function paintHead() {
  byId('oneshotLength').value = String(Patch.periodValue(Patch.periodStep(session.patch().length)));
  paintOver();
}

function open(index) {
  if (session.editingOneshot() && index === session.oneshotIndex) return;
  const keepsDraft = index === session.oneshotIndex && session.oneshotDraft;
  if (!keepsDraft && !leaveOneshotDraft()) return;
  const draft = keepsDraft ? session.oneshotDraft
    : session.library.kit[index] ? null : Library.newOneshot('untitled');
  session.openOneshot(index, draft);
  Playback.changed();
  Playback.cut();
  Transition.rebuild();
  Editor.paint();
  Rail.paintList();
}

function backToPatch() {
  Editor.open(session.slot, session.draft);
}

function editOver() {
  const slot = +byId('overPatch').value;
  if (slot === session.slot) {
    backToPatch();
    return;
  }
  if (!Rail.leaveDraft()) return;
  Editor.open(slot);
}

function chooseOver() {
  session.overSlot = +byId('overPatch').value;
  Playback.cut();
  Editor.paint();
}

function firePick(place) {
  const index = session.resolvedPick(place);
  if (index === null || !kitOneshot(index)) return;
  Playback.fireNote(Protocol.NOTE_PATCH_ONESHOT_FIRST + place);
}

function cell(index) {
  const oneshot = kitOneshot(index);
  const current = session.editingOneshot() && index === session.oneshotIndex;
  const button = element('button', 'cell');
  button.classList.toggle('empty', !oneshot);
  button.classList.toggle('on', current);
  button.classList.toggle('dirty', index === session.oneshotIndex && !!session.oneshotDraft);
  const head = element('span', 'cell-head');
  head.append(element('span', 'slot cc midi-number', placeText(index)));
  button.append(head, element('span', 'name', oneshot ? oneshot.name || '(unnamed)' : ''));
  button.addEventListener('click', () => open(index));
  wireDrag(button, index, !!oneshot);
  return button;
}

function wireDrag(button, index, filled) {
  button.draggable = filled;
  button.addEventListener('dragstart', event => {
    dragged = index;
    event.dataTransfer.effectAllowed = 'copyMove';
    button.classList.add('dragging');
  });
  button.addEventListener('dragend', () => {
    dragged = null;
    button.classList.remove('dragging');
  });
  button.addEventListener('dragover', event => {
    if (dragged === null || dragged === index) return;
    event.preventDefault();
    event.dataTransfer.dropEffect = event.altKey ? 'copy' : 'move';
    button.classList.add('drop');
  });
  button.addEventListener('dragleave', () => button.classList.remove('drop'));
  button.addEventListener('drop', event => {
    event.preventDefault();
    button.classList.remove('drop');
    if (dragged === null || dragged === index) return;
    if (event.altKey) copyPlace(dragged, index);
    else swapPlaces(dragged, index);
  });
}

const pickable = index => (index < Protocol.ONESHOTS ? index : null);

function swapPlaces(from, to) {
  if (!leaveOneshotDraft()) return;
  session.oneshotDraft = null;
  const { kit } = session.library;
  [kit[from], kit[to]] = [kit[to], kit[from]];
  const swapped = index => (index === from ? to : index === to ? from : index);
  const follow = pick => (pick === null ? null : pickable(swapped(pick)));
  Library.remapPicks(session.library, follow);
  if (session.draft) session.draft.oneshots = session.draft.oneshots.map(follow);
  if (session.oneshotIndex !== null) session.oneshotIndex = swapped(session.oneshotIndex);
  session.saveLibrary();
  Editor.paint();
  Rail.paintList();
  say(kit[from] ? `swapped the ${placeText(from)} and the ${placeText(to)}` : `moved "${kit[to].name}" to the ${placeText(to)}`);
}

function copyPlace(from, to) {
  const { kit } = session.library;
  if (kit[to] && !confirm(`Replace "${kit[to].name}" on the ${placeText(to)} with a copy of "${kit[from].name}"?`)) return;
  if (session.editingOneshot() && session.oneshotIndex === to) session.oneshotDraft = null;
  kit[to] = Library.cloneOneshot(kit[from]);
  session.saveLibrary();
  Editor.paint();
  Rail.paintList();
  say(`copied "${kit[from].name}" to the ${placeText(to)}`);
}

function paintKit() {
  byId('kitGrid').replaceChildren(...Array.from({ length: Protocol.ONESHOTS }, (_, index) => cell(index)));
  byId('cornerGrid').replaceChildren(...CORNERS_AS_ON_THE_PAD.map(corner => cell(Protocol.ONESHOTS + corner)));
  byId('kitCount').textContent = `${Library.filledKit(session.library).length} / ${Protocol.ONESHOTS}`;
  paintPickSelects(DEFAULT_FIELDS, session.library.defaultOneshots, () => '—');
  const saved = session.editingOneshot() && !!session.library.kit[session.oneshotIndex];
  byId('oneshotDelete').disabled = !saved || session.editingCorner();
  byId('cornerDelete').disabled = !saved || !session.editingCorner();
}

let firingText = null;

function paintFiring(progress) {
  const editing = session.editingOneshot();
  const length = editing ? Library.lengthBeats(session.oneshot()) : 0;
  const text = editing && progress !== null ? `${(progress * length).toFixed(1)} / ${length}` : '';
  const firingIndex = !editing ? Playback.oneshotIndex() : null;
  const state = `${text}|${firingIndex}`;
  if (state === firingText) return;
  firingText = state;
  byId('firingAt').textContent = text;
  byId('oneshotFire').classList.toggle('on', editing && progress !== null);
  FIRE_BUTTONS.forEach((id, place) => byId(id).classList.toggle('on',
    firingIndex !== null && session.resolvedPick(place) === firingIndex));
}

function save() {
  session.library.kit[session.oneshotIndex] = session.oneshotDraft;
  session.oneshotDraft = null;
  session.saveLibrary();
  Rail.paintList();
  say(`saved "${session.patch().name}" as the ${placeText(session.oneshotIndex)}`, 'ok');
}

function discard() {
  if (!leaveOneshotDraft()) return;
  session.oneshotDraft = null;
  if (session.library.kit[session.oneshotIndex]) {
    session.saveLibrary();
    Editor.paint();
    Rail.paintList();
  } else {
    backToPatch();
  }
}

function startNew() {
  const index = Library.firstEmptyKitPlace(session.library);
  if (index === null) {
    say(`the kit is full — all ${Protocol.ONESHOTS} notes hold a oneshot`, 'warn');
    return;
  }
  if (!leaveOneshotDraft()) return;
  session.oneshotDraft = null;
  session.oneshotIndex = null;
  open(index);
}

function remove() {
  const index = session.oneshotIndex;
  const oneshot = session.library.kit[index];
  if (!confirm(`Empty the ${placeText(index)}, "${oneshot.name}"?`)) return;
  session.library.kit[index] = null;
  session.oneshotDraft = null;
  const forget = pick => (pick === index ? null : pick);
  Library.remapPicks(session.library, forget);
  if (session.draft) session.draft.oneshots = session.draft.oneshots.map(forget);
  session.saveLibrary();
  backToPatch();
}

function wire() {
  const length = byId('oneshotLength');
  dom.setOptions(length, Patch.LFO_PERIOD_NAMES.map((text, step) => [Patch.periodValue(step), text]), '');
  length.addEventListener('change', () => {
    session.editing().length = +length.value;
    session.changed();
  });

  byId('oneshotPick0Note').textContent = `note ${Protocol.NOTE_PATCH_ONESHOT_FIRST}`;
  byId('oneshotPick1Note').textContent = `note ${Protocol.NOTE_PATCH_ONESHOT_SECOND}`;
  PICK_FIELDS.forEach((id, place) => byId(id).addEventListener('change', () => {
    session.editing().oneshots[place] = pickFrom(byId(id).value);
    session.changed();
  }));
  DEFAULT_FIELDS.forEach((id, place) => byId(id).addEventListener('change', () => {
    session.library.defaultOneshots[place] = pickFrom(byId(id).value);
    session.saveLibrary();
    Editor.paint();
  }));

  byId('oneshotFire').addEventListener('click', fireDraft);
  const repeatButton = byId('oneshotRepeat');
  repeatButton.classList.toggle('on', repeat);
  repeatButton.addEventListener('click', () => {
    repeat = !repeat;
    repeatButton.classList.toggle('on', repeat);
  });
  byId('overPatch').addEventListener('change', chooseOver);
  byId('editOver').addEventListener('click', editOver);
  FIRE_BUTTONS.forEach((id, place) => byId(id).addEventListener('click', () => firePick(place)));
  byId('oneshotDelete').addEventListener('click', remove);
  byId('cornerDelete').addEventListener('click', remove);
}

export { wire, open, paintKit, paintPicks, paintHead, paintFiring, keepRepeating, save, discard, startNew, leaveOneshotDraft };
