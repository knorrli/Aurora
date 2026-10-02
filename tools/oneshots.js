import { Protocol } from './cc.js';
import * as dom from './dom.js';
import * as Editor from './editor.js';
import * as Rail from './library-rail.js';
import * as Library from './library.js';
import * as Midi from './midi-out.js';
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
const kitOneshot = session.kitOneshot;
let repeat = true;

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
  if (session.editingOneshot() && repeat && Playback.oneshotProgress() < 0) fireDraft();
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
  Midi.sendLive(true);
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
  head.append(element('span', 'slot cc midi-number', `note ${noteOf(index)}`));
  button.append(head, element('span', 'name', oneshot ? oneshot.name || '(unnamed)' : ''));
  button.addEventListener('click', () => open(index));
  return button;
}

function paintKit() {
  byId('kitGrid').replaceChildren(...Array.from({ length: Protocol.ONESHOTS }, (_, index) => cell(index)));
  byId('kitCount').textContent = `${Library.filledKit(session.library).length} / ${Protocol.ONESHOTS}`;
  paintPickSelects(DEFAULT_FIELDS, session.library.defaultOneshots, () => '—');
  byId('oneshotDelete').disabled = !session.editingOneshot() || !session.library.kit[session.oneshotIndex];
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
  say(`saved the oneshot "${session.patch().name}" on note ${noteOf(session.oneshotIndex)}`, 'ok');
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
  if (!confirm(`Empty note ${noteOf(index)}, "${oneshot.name}"?`)) return;
  session.library.kit[index] = null;
  session.oneshotDraft = null;
  for (const patch of session.library.slots) {
    if (patch) patch.oneshots = patch.oneshots.map(pick => (pick === index ? null : pick));
  }
  if (session.draft) session.draft.oneshots = session.draft.oneshots.map(pick => (pick === index ? null : pick));
  session.library.defaultOneshots = session.library.defaultOneshots.map(pick => (pick === index ? null : pick));
  Library.forgetSongOneshot(session.library, index);
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
}

export { wire, open, paintKit, paintPicks, paintHead, paintFiring, keepRepeating, save, discard, startNew, leaveOneshotDraft };
