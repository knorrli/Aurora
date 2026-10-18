import { Protocol } from './controls.js';
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
  [Protocol.PATCH_LAYER_MOTION]: 'faderMotion',
  [Protocol.PATCH_LAYER_EXTENT]: 'faderExtent',
};

const micros = milliseconds => Math.round(milliseconds * 1000) >>> 0;
const link = () => Midi.link;

const clock = { sending: false, due: [], nextAt: null };
const brain = { dialing: true, layers: [] };
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
  for (let index = 0; index < Protocol.KIT_PLACES; index++) storeKitPlace(index);
  Preview.playback.defaultOneshots(session.library.defaultOneshots);
}

const dialedSlot = () => (session.editingOneshot() ? session.overSlotShown() : session.slot);
const dialedPatch = () => (session.editingOneshot() ? session.over() : session.underneath());


const sounded = bytes => session.sounding(Patch.controls(bytes)).bytes;

function dialedRecord() {
  const patch = dialedPatch();
  const record = patchRecord(patch);
  record.layers[Protocol.PATCH_LAYER_BASE] = sounded(patch.base);
  return record;
}

function dialBoth(slot, record, cut) {
  Preview.playback.dial(slot, record, cut);
  if (!link().output) return;
  record.layers.forEach((bytes, layer) => {
    const sent = bytes.join(',');
    if (!cut && sent === brain.layers[layer]) return;
    link().sendDialLayer(layer, bytes);
    brain.layers[layer] = sent;
  });
  link().sendDial(slot, record, cut);
}

function syncPin() {
  const layered = session.pinnedLayer();
  const pinned = session.pinnedControls();
  if (layered) {
    const bytes = sounded(layered.layer);
    const steps = Math.round(layered.position * Protocol.PIN_POSITION_STEPS);
    Preview.playback.pinLayer(bytes, layered.index, steps);
    if (link().output) link().sendPinLayer(bytes, layered.index, steps);
  } else if (pinned) {
    const bytes = session.sounding(pinned).bytes;
    Preview.playback.pin(bytes);
    if (link().output) link().sendPin(bytes);
  } else {
    Preview.playback.unpin();
    if (link().output) link().sendUnpin();
  }
}

function play() {
  brain.dialing = false;
}

function cut() {
  brain.dialing = true;
  dialBoth(dialedSlot() || 0, dialedRecord(), true);
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
  dialBoth(dialedSlot() || 0, dialedRecord(), false);
  syncPin();
}

function follow() {
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
  both(CONTROL_CHANGE, Protocol.CC[FADERS[layer]], Math.round(position * 127));
}

const movePad = (cc, value) => both(CONTROL_CHANGE, cc, value);

function run(fromSlot) {
  const from = session.patchAt(fromSlot);
  if (!from) return;
  play();
  dialBoth(fromSlot, patchRecord(from), true);
  press(session.slot);
  const beats = Preview.playback.beats();
  const length = Protocol.transitionBeats(session.patch().transitionTime);
  const releaseBeat = Math.round(beats) + Math.max(0, length - RELEASE_BEFORE_ARRIVAL_BEATS);
  runRelease = setTimeout(release, Math.max(0, (releaseBeat - beats) * 60000 / Midi.bpm()));
}

function frame(now) {
  pumpClock(now);
  shown = Preview.playback.frame(micros(now));
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
  FADERS, faders, start, syncLibrary, cut, changed, follow, press, release, fireNote, setFader, movePad, run,
  sendClock, frame, shownControls, beats, slot, oneshotProgress, oneshotIndex,
};
