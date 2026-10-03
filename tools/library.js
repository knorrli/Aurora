import { Protocol } from './cc.js';
import * as LibraryFile from './library-file.js';
import * as Patch from './patch.js';
import { Preview } from './preview.js';


const byteOf = (bytes, name) => bytes[Patch.CC[name]] | 0;

const writeCC = (bytes, name, value) => { bytes[Patch.CC[name]] = Patch.clampToSevenBits(value); };

const emptyOverrides = () => Array.from({ length: Protocol.PATCH_LAYERS },
  (_, layer) => (Patch.isAboveBase(layer) ? {} : null));

const NO_PICKS = [null, null];

const newPatch = name => ({
  name: LibraryFile.printableName(name || 'untitled'),
  transitionTime: Patch.transitionValue(4),
  accentTime: Patch.periodValue(10),
  oneshots: NO_PICKS.slice(),
  base: Patch.DEFAULT.bytes.slice(),
  overrides: emptyOverrides(),
});

const clonePatch = patch => ({
  name: patch.name,
  transitionTime: patch.transitionTime,
  accentTime: patch.accentTime,
  oneshots: patch.oneshots.slice(),
  base: patch.base.slice(),
  overrides: patch.overrides.map(over => (over ? Object.assign({}, over) : null)),
});

const newOneshot = name => ({
  name: LibraryFile.printableName(name || 'untitled'),
  length: Patch.periodValue(4),
  marks: {},
  base: Patch.DEFAULT.bytes.slice(),
  overrides: emptyOverrides(),
});

const cloneOneshot = oneshot => ({
  name: oneshot.name,
  length: oneshot.length,
  marks: Object.assign({}, oneshot.marks),
  base: oneshot.base.slice(),
  overrides: emptyOverrides(),
});

const MARKABLE = Patch.NAMES.filter(name => !Patch.ROUTES.some(route => route.fields.includes(name)));
const isMarkable = name => MARKABLE.includes(name);

function markBytes(oneshot) {
  const marks = new Array(Protocol.PATCH_CC_COUNT).fill(0);
  for (const name of Object.keys(oneshot.marks)) marks[Patch.CC[name]] = 1;
  return marks;
}

const oneshotToFile = (oneshot, index) => ({
  index,
  name: oneshot.name,
  length: oneshot.length,
  marks: Object.keys(oneshot.marks).map(name => Patch.CC[name]).sort((a, b) => a - b),
  bytes: oneshot.base.slice(),
});

const oneshotFromFile = fileOneshot => ({
  name: fileOneshot.name,
  length: fileOneshot.length,
  marks: Object.fromEntries(fileOneshot.marks
    .map(cc => Protocol.NAME_BY_CC[cc]).filter(isMarkable).map(name => [name, true])),
  base: fileOneshot.bytes.slice(),
  overrides: emptyOverrides(),
});

function layerBytes(patch, layer) {
  const bytes = patch.base.slice();
  for (const [name, value] of Object.entries(patch.overrides[layer] || {})) writeCC(bytes, name, value);
  return bytes;
}

const changedIn = (patch, layer) => Object.keys(patch.overrides[layer] || {});

function writeBase(patch, name, value) {
  writeCC(patch.base, name, value);
  for (const over of patch.overrides) {
    if (over && over[name] === byteOf(patch.base, name)) delete over[name];
  }
}

function freeRoute(patch, route) {
  for (const name of route.fields) {
    writeCC(patch.base, name, Patch.DEFAULT[name]);
    for (const over of patch.overrides) if (over) delete over[name];
  }
}

function blend(fromBytes, toBytes, position, switchesFrom) {
  const switches = switchesFrom || fromBytes;
  return Patch.controls(Preview.blend(fromBytes, toBytes, position, switches, switches === fromBytes));
}

const freeRouteFields = base => new Set(Patch.ROUTES
  .filter(route => !byteOf(base, route.destination))
  .flatMap(route => route.fields));

function keepAsBase(patch, shown) {
  for (const name of Patch.NAMES) writeBase(patch, name, shown[name]);
}

function keepAsLayer(patch, layer, shown) {
  const free = freeRouteFields(patch.base);
  const over = {};
  const leftOnBase = [];
  for (const name of Patch.NAMES) {
    const value = Patch.clampToSevenBits(shown[name]);
    if (Patch.isSwitch(name) || free.has(name)) {
      if (value !== byteOf(patch.base, name)) leftOnBase.push(name);
    } else if (value !== byteOf(patch.base, name)) {
      over[name] = value;
    }
  }
  patch.overrides[layer] = over;
  return leftOnBase;
}

const clearLayer = (patch, layer) => { patch.overrides[layer] = {}; };

function moveToLayer(patch, layer, names) {
  const over = patch.overrides[layer];
  const moved = [];
  for (const name of names) {
    if (Patch.isSwitch(name)) continue;
    const value = byteOf(patch.base, name);
    if (value === Patch.DEFAULT[name]) continue;
    writeBase(patch, name, Patch.DEFAULT[name]);
    over[name] = value;
    moved.push(name);
  }
  return moved;
}

const patchToFile = patch => ({
  name: patch.name,
  transitionTime: patch.transitionTime,
  accentTime: patch.accentTime,
  oneshots: patch.oneshots.slice(),
  layers: Array.from({ length: Protocol.PATCH_LAYERS }, (_, layer) => layerBytes(patch, layer)),
});

function patchFromFile(filePatch) {
  const base = filePatch.layers[Protocol.PATCH_LAYER_BASE].slice();
  const freeFields = freeRouteFields(base);
  const overrides = filePatch.layers.map((bytes, layer) => {
    if (!Patch.isAboveBase(layer)) return null;
    const over = {};
    for (const name of Patch.CONTINUOUS) {
      if (!freeFields.has(name) && byteOf(bytes, name) !== byteOf(base, name)) over[name] = byteOf(bytes, name);
    }
    return over;
  });
  return {
    name: filePatch.name,
    transitionTime: filePatch.transitionTime,
    accentTime: filePatch.accentTime,
    oneshots: (filePatch.oneshots || NO_PICKS).slice(),
    base,
    overrides,
  };
}

function destinationFault(filePatch, where) {
  for (const [layer, bytes] of filePatch.layers.entries()) {
    for (const route of Patch.ROUTES) {
      const destination = byteOf(bytes, route.destination);
      if (!Patch.routableDestination(destination)) {
        return `${where} layer ${layer}: ${route.name} aims at CC ${destination}, which no route can move`;
      }
    }
  }
  return null;
}

const validatePatch = (filePatch, where) =>
  LibraryFile.validatePatch(filePatch, where) || destinationFault(filePatch, where);

function validateFile(file) {
  const fault = LibraryFile.validate(file);
  if (fault) return fault;
  for (const [index, filePatch] of file.patches.entries()) {
    const routeFault = destinationFault(filePatch, `patch ${index}`);
    if (routeFault) return routeFault;
  }
  return null;
}

const emptySlots = () => new Array(Protocol.LAST_PATCH_SLOT + 1).fill(null);
const filledSlots = library => library.slots.flatMap((patch, slot) => (patch ? [slot] : []));
const firstEmptySlot = library => {
  const slot = library.slots.findIndex((patch, at) => !patch && LibraryFile.isPatchSlot(at));
  return slot < 0 ? null : slot;
};

const lengthBeats = oneshot => Protocol.lfoPeriodBeats(oneshot.length);
const emptyKit = () => new Array(Protocol.KIT_PLACES).fill(null);
const filledPlaces = library => library.kit.flatMap((oneshot, index) => (oneshot ? [index] : []));
const filledKit = library => filledPlaces(library).filter(index => index < Protocol.ONESHOTS);
const firstEmptyKitPlace = library => {
  const index = library.kit.slice(0, Protocol.ONESHOTS).findIndex(oneshot => !oneshot);
  return index < 0 ? null : index;
};

const emptyGig = () => new Array(Protocol.SONGS).fill(null);

const newSong = name => ({
  name: LibraryFile.printableName(name || 'untitled'),
  oneshots: NO_PICKS.slice(),
  sections: [],
  patches: {},
});

const cloneSong = song => ({
  name: song.name,
  oneshots: song.oneshots.slice(),
  sections: song.sections.slice(),
  patches: Object.assign({}, song.patches),
});

const sectionSlot = (song, section) => {
  const slot = song.patches[song.sections[section]];
  return slot === undefined ? null : slot;
};

function unusedLabel(song) {
  for (let number = song.sections.length + 1; ; number++) {
    const label = `section ${number}`;
    if (!song.sections.includes(label)) return label;
  }
}

function forgetUnusedLabels(song) {
  for (const label of Object.keys(song.patches)) {
    if (!song.sections.includes(label)) delete song.patches[label];
  }
}

function addSection(song) {
  const label = unusedLabel(song);
  song.sections.push(label);
  song.patches[label] = null;
  return song.sections.length - 1;
}

function relabelSection(song, section, label) {
  const previous = song.sections[section];
  if (label === previous) return;
  if (!(label in song.patches)) song.patches[label] = sectionSlot(song, section);
  song.sections[section] = label;
  forgetUnusedLabels(song);
}

function removeSection(song, section) {
  song.sections.splice(section, 1);
  forgetUnusedLabels(song);
}

function moveSection(song, from, to) {
  const [label] = song.sections.splice(from, 1);
  song.sections.splice(to, 0, label);
}

function remapSongSlots(library, remap) {
  for (const song of library.songs) {
    for (const [label, slot] of Object.entries(song.patches)) {
      if (slot !== null) song.patches[label] = remap(slot);
    }
  }
}

const swapSongSlots = (library, one, other) =>
  remapSongSlots(library, slot => (slot === one ? other : slot === other ? one : slot));

const forgetSongSlot = (library, gone) => remapSongSlots(library, slot => (slot === gone ? null : slot));

function remapPicks(library, remap) {
  for (const patch of library.slots) if (patch) patch.oneshots = patch.oneshots.map(remap);
  library.defaultOneshots = library.defaultOneshots.map(remap);
  for (const song of library.songs) song.oneshots = song.oneshots.map(remap);
}

function removeSong(library, index) {
  library.songs.splice(index, 1);
  library.gig = library.gig.map(place => (place === index ? null : place !== null && place > index ? place - 1 : place));
}

const songToFile = cloneSong;

const libraryToFile = library => ({
  patchFormat: Protocol.PATCH_FORMAT,
  patches: filledSlots(library).map(slot => Object.assign({ slot }, patchToFile(library.slots[slot]))),
  defaultOneshots: library.defaultOneshots.slice(),
  oneshots: filledPlaces(library).map(index => oneshotToFile(library.kit[index], index)),
  songs: (library.songs || []).map(songToFile),
  gig: (library.gig || emptyGig()).slice(),
});

function libraryFromFile(file) {
  const slots = emptySlots();
  for (const filePatch of file.patches) slots[filePatch.slot] = patchFromFile(filePatch);
  const kit = emptyKit();
  for (const fileOneshot of file.oneshots || []) kit[fileOneshot.index] = oneshotFromFile(fileOneshot);
  return {
    slots,
    kit,
    defaultOneshots: (file.defaultOneshots || NO_PICKS).slice(),
    songs: (file.songs || []).map(cloneSong),
    gig: (file.gig || emptyGig()).slice(),
  };
}

function newLibrary() {
  const slots = emptySlots();
  slots[1] = newPatch('first');
  return { slots, kit: emptyKit(), defaultOneshots: NO_PICKS.slice(), songs: [], gig: emptyGig() };
}

export {
  writeCC,
  newPatch, clonePatch, newOneshot, cloneOneshot, oneshotToFile, oneshotFromFile, isMarkable, markBytes, layerBytes, changedIn, writeBase, freeRoute,
  blend, keepAsBase, keepAsLayer, clearLayer, moveToLayer,
  patchToFile, patchFromFile, validatePatch, validateFile,
  filledSlots, firstEmptySlot, filledKit, firstEmptyKitPlace, lengthBeats, libraryToFile, libraryFromFile, newLibrary,
  newSong, sectionSlot, addSection, relabelSection, removeSection, moveSection,
  swapSongSlots, forgetSongSlot, remapPicks, removeSong,
};
