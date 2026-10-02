(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const Library = global.AuroraLibrary;
  const Preview = global.AuroraPreview;
  const Editor = global.AuroraEditor;
  const { session } = Editor;

  const NOTE_ON = 0x90, NOTE_OFF = 0x80, CONTROL_CHANGE = 0xB0, PROGRAM_CHANGE = 0xC0;
  const CLOCK_TICK = 0xF8, CLOCK_START = 0xFA;
  const CHANNEL = Protocol.MIDI_CHANNEL - 1;
  const FULL_VELOCITY = 127;
  const LOOKAHEAD_MILLISECONDS = 250;
  const STALL_BEATS = 1;

  const FADERS = {
    [Protocol.PATCH_LAYER_COLOR]: 'faderColor',
    [Protocol.PATCH_LAYER_EXTENT]: 'faderExtent',
    [Protocol.PATCH_LAYER_MOTION]: 'faderMotion',
  };

  const micros = milliseconds => Math.round(milliseconds * 1000) >>> 0;
  const link = () => Editor.midi.link;

  const clock = { sending: false, due: [], nextAt: null };
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
    const period = 60000 / Editor.midi.bpm() / Protocol.TICKS_PER_BEAT;
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
      ? Library.bytesFromNamed(session.sounding(Library.namedFromBytes(oneshot.base))) : oneshot.base;
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
    const named = session.sounding(Library.namedFromBytes(dialedPatch().base));
    for (const name of Patch.NAMES) {
      const cc = Patch.CC[name];
      if (fed[cc] === named[name]) continue;
      local(CONTROL_CHANGE, cc, named[name]);
      fed[cc] = named[name];
    }
  }

  function syncPin() {
    const pinned = session.pinnedNamed();
    if (pinned) Preview.playback.pin(Library.bytesFromNamed(session.sounding(pinned)));
    else Preview.playback.unpin();
  }

  function cut() {
    const patch = dialedPatch();
    Preview.playback.cut(dialedSlot() || 0, patchRecord(patch));
    forgetFed(patch.base);
    feedBase();
    syncPin();
  }

  function changed() {
    if (session.slot !== null) storeSlot(session.slot);
    if (session.editingOneshot() && session.oneshotIndex !== null) storeKitPlace(session.oneshotIndex);
    Preview.playback.defaultOneshots(session.library.defaultOneshots);
    if (dialedSlot() !== null) Preview.playback.patchChanged(dialedSlot());
    feedBase();
    syncPin();
  }

  function follow(slot) {
    const patch = session.patchAt(slot);
    if (patch) forgetFed(patch.base);
    syncPin();
  }

  function press(slot) {
    both(NOTE_ON, Protocol.NOTE_KEY_HELD, FULL_VELOCITY);
    both(PROGRAM_CHANGE, slot);
  }

  const release = () => both(NOTE_OFF, Protocol.NOTE_KEY_HELD, 0);

  function fireNote(note) {
    both(NOTE_ON, note, FULL_VELOCITY);
    both(NOTE_OFF, note, 0);
  }

  function setFader(layer, position) {
    faders[layer] = position;
    both(CONTROL_CHANGE, Patch.CC[FADERS[layer]], Math.round(position * 127));
  }

  function run(fromSlot) {
    const from = session.patchAt(fromSlot);
    if (!from) return;
    Preview.playback.cut(fromSlot, patchRecord(from));
    forgetFed(from.base);
    both(PROGRAM_CHANGE, session.slot);
  }

  function frame(now) {
    pumpClock(now);
    shown = Preview.playback.frame(micros(now));
    return shown;
  }

  function shownNamed() {
    if (session.editingOneshot()) return session.liveNamed();
    return session.pinnedNamed() || Library.namedFromBytes(shown ? shown.drawn : dialedPatch().base);
  }

  function start() {
    Preview.playback.begin(micros(performance.now()));
    syncLibrary();
    cut();
  }

  Editor.playback = {
    FADERS, faders, start, syncLibrary, cut, changed, follow, press, release, fireNote, setFader, run,
    sendClock, frame, shownNamed,
    beats: () => Preview.playback.beats(),
    slot: () => Preview.playback.slot(),
    oneshotProgress: () => Preview.playback.oneshotProgress(),
    oneshotIndex: () => Preview.playback.oneshotIndex(),
  };
})(window);
