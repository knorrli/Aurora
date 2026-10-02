import { Protocol } from './cc.js';
import * as Library from './library.js';
import * as Midi from './midi-out.js';
import * as Patch from './patch.js';
import { Preview } from './preview.js';
import { session } from './session.js';


const NOTE_ON = 0x90, NOTE_OFF = 0x80, CONTROL_CHANGE = 0xB0, PROGRAM_CHANGE = 0xC0;
const CLOCK_TICK = 0xF8, CLOCK_START = 0xFA;
const CHANNEL = Protocol.MIDI_CHANNEL - 1;
const FULL_VELOCITY = 127;
const LOOKAHEAD_MILLISECONDS = 250;
const STALL_BEATS = 1;
const RELEASE_BEFORE_ARRIVAL_BEATS = 0.05;

const FADERS = {
  [Protocol.PATCH_LAYER_COLOR]: 'faderColor',
  [Protocol.PATCH_LAYER_EXTENT]: 'faderExtent',
  [Protocol.PATCH_LAYER_MOTION]: 'faderMotion',
};

const micros = milliseconds => Math.round(milliseconds * 1000) >>> 0;
const link = () => Midi.link;

const clock = { sending: false, due: [], nextAt: null };
const brain = { dialing: true, pinned: null };
const fed = new Array(Protocol.PATCH_CC_COUNT).fill(-1);
const faders = Object.fromEntries(Object.keys(FADERS).map(layer => [layer, 0]));
let shown = null;

function local(status, first, second, at) {
  Preview.playback.message(status, first, second, micros(at === undefined ? performance.now() : at));
}

function both(status, first, second) {
  local(status | CHANNEL, first, second || 0);
  link().sendRaw(status === PROGRAM_CHANGE ? [status | CHANNEL, first] : [status | CHANNEL, first, second]);
}

function pumpClock(now) {
  const period = 60000 / Midi.bpm() / Protocol.TICKS_PER_BEAT;
  if (clock.nextAt === null || clock.nextAt < now - period * Protocol.TICKS_PER_BEAT * STALL_BEATS) clock.nextAt = now;
  while (clock.nextAt <= now + LOOKAHEAD_MILLISECONDS) {
    clock.due.push(clock.nextAt);
    if (clock.sending) link().sendRaw([CLOCK_TICK], clock.nextAt);
    clock.nextAt += period;
  }
  while (clock.due.length && clock.due[0] <= now) local(CLOCK_TICK, 0, 0, clock.due.shift());
}

function sendClock(sending) {
  clock.sending = sending;
  if (!sending) return;
  const now = performance.now();
  clock.due = [];
  clock.nextAt = now;
  link().sendRaw([CLOCK_START]);
  local(CLOCK_START, 0, 0, now);
}

const patchRecord = patch => ({
  transitionTime: patch.transitionTime,
  accentTime: patch.accentTime,
  oneshots: patch.oneshots,
  layers: Library.patchToFile(patch).layers,
});

function oneshotRecord(index, oneshot) {
  const editing = session.editingOneshot() && index === session.oneshotIndex;
  const controls = editing
    ? session.sounding(Patch.controls(oneshot.base)).bytes : oneshot.base;
  return { length: oneshot.length, marks: Library.markBytes(oneshot), controls };
}

function storeSlot(slot) {
  const patch = session.patchAt(slot);
  if (patch) Preview.playback.storePatch(slot, patchRecord(patch));
  else Preview.playback.clearPatch(slot);
}

function storeKitPlace(index) {
  const oneshot = session.kitOneshot(index);
  if (oneshot) Preview.playback.storeOneshot(index, oneshotRecord(index, oneshot));
  else Preview.playback.clearOneshot(index);
}

function syncLibrary() {
  for (let slot = 1; slot <= Protocol.LAST_PATCH_SLOT; slot++) storeSlot(slot);
  for (let index = 0; index < Protocol.ONESHOTS; index++) storeKitPlace(index);
  Preview.playback.defaultOneshots(session.library.defaultOneshots);
}

const dialedSlot = () => (session.editingOneshot() ? session.overSlotShown() : session.slot);
const dialedPatch = () => (session.editingOneshot() ? session.over() : session.underneath());

function forgetFed(bytes) {
  for (const name of Patch.NAMES) fed[Patch.CC[name]] = bytes[Patch.CC[name]] | 0;
}

function feedBase() {
  if (Preview.playback.slot() !== (dialedSlot() || 0)) return;
  const live = session.sounding(Patch.controls(dialedPatch().base));
  for (const name of Patch.NAMES) {
    const cc = Patch.CC[name];
    if (fed[cc] === live[name]) continue;
    local(CONTROL_CHANGE, cc, live[name]);
    fed[cc] = live[name];
  }
}

function syncPin() {
  const pinned = session.pinnedControls();
  if (pinned) Preview.playback.pin(session.sounding(pinned).bytes);
  else Preview.playback.unpin();
}

function dial() {
  brain.dialing = true;
  brain.pinned = null;
}

function play() {
  brain.dialing = false;
}

function pinBrain(drawn) {
  if (!brain.dialing || !link().output) return;
  const controls = drawn.slice(0, Protocol.PATCH_CC_COUNT);
  const sent = controls.join(',');
  if (sent === brain.pinned) return;
  link().sendPin(controls);
  brain.pinned = sent;
}

function cut() {
  dial();
  const patch = dialedPatch();
  Preview.playback.cut(dialedSlot() || 0, patchRecord(patch));
  forgetFed(patch.base);
  feedBase();
  syncPin();
}

function changed() {
  const resuming = !brain.dialing || Preview.playback.slot() !== (dialedSlot() || 0);
  if (session.slot !== null) storeSlot(session.slot);
  if (session.editingOneshot() && session.oneshotIndex !== null) storeKitPlace(session.oneshotIndex);
  Preview.playback.defaultOneshots(session.library.defaultOneshots);
  if (resuming) {
    cut();
    return;
  }
  if (dialedSlot() !== null) Preview.playback.patchChanged(dialedSlot());
  feedBase();
  syncPin();
}

function follow(slot) {
  const patch = session.patchAt(slot);
  if (patch) forgetFed(patch.base);
  syncPin();
}

let runRelease = null;

function press(slot) {
  clearTimeout(runRelease);
  play();
  both(NOTE_ON, Protocol.NOTE_KEY_HELD, FULL_VELOCITY);
  both(PROGRAM_CHANGE, slot);
}

const release = () => both(NOTE_OFF, Protocol.NOTE_KEY_HELD, 0);

function fireNote(note) {
  play();
  both(NOTE_ON, note, FULL_VELOCITY);
  both(NOTE_OFF, note, 0);
}

function setFader(layer, position) {
  play();
  faders[layer] = position;
  both(CONTROL_CHANGE, Patch.CC[FADERS[layer]], Math.round(position * 127));
}

function run(fromSlot) {
  const from = session.patchAt(fromSlot);
  if (!from) return;
  play();
  Preview.playback.cut(fromSlot, patchRecord(from));
  forgetFed(from.base);
  press(session.slot);
  const beats = Preview.playback.beats();
  const length = Protocol.transitionBeats(session.patch().transitionTime);
  const releaseBeat = Math.round(beats) + Math.max(0, length - RELEASE_BEFORE_ARRIVAL_BEATS);
  runRelease = setTimeout(release, Math.max(0, (releaseBeat - beats) * 60000 / Midi.bpm()));
}

function frame(now) {
  pumpClock(now);
  shown = Preview.playback.frame(micros(now));
  pinBrain(shown.drawn);
  return shown;
}

function shownControls() {
  if (session.editingOneshot()) return session.liveControls();
  return session.pinnedControls() || Patch.controls(shown ? shown.drawn : dialedPatch().base);
}

function start() {
  Preview.playback.begin(micros(performance.now()));
  syncLibrary();
  cut();
}

const beats = () => Preview.playback.beats();
const slot = () => Preview.playback.slot();
const oneshotProgress = () => Preview.playback.oneshotProgress();
const oneshotIndex = () => Preview.playback.oneshotIndex();

export {
  FADERS, faders, start, syncLibrary, dial, cut, changed, follow, press, release, fireNote, setFader, run,
  sendClock, frame, shownControls, beats, slot, oneshotProgress, oneshotIndex,
};
