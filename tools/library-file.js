import { Protocol } from './cc.js';


const KIND = 'patch library';
const NAME_AT = Protocol.PATCH_HEAD_LENGTH - Protocol.PATCH_NAME_LENGTH;
const HEAD = { transitionTime: 0, accentTime: 1, oneshots: 2 };

const isSevenBit = value => Number.isInteger(value) && value >= 0 && value <= 127;
const isOneshotIndex = index => Number.isInteger(index) && index >= 0 && index < Protocol.KIT_PLACES;
const isPick = pick => pick === null || isOneshotIndex(pick);
const NO_PICKS = [null, null];
const LAST_PATCH_SLOT = Protocol.LAST_PATCH_SLOT;
const isPatchSlot = slot => Number.isInteger(slot) && slot > Protocol.PROGRAM_BLACKOUT && slot <= LAST_PATCH_SLOT;

const printableName = text =>
  String(text == null ? '' : text).replace(/[^\x20-\x7E]/g, '').slice(0, Protocol.PATCH_NAME_LENGTH);
const isPrintableName = name => typeof name === 'string' && printableName(name) === name;

function serialize(file) {
  const lines = [
    '{',
    `  "aurora": ${JSON.stringify(KIND)},`,
    `  "patchFormat": ${file.patchFormat},`,
    `  "savedAt": ${JSON.stringify(new Date().toISOString())},`,
    '  "patches": [',
  ];
  file.patches.forEach((patch, index) => {
    lines.push('    {');
    lines.push(`      "slot": ${patch.slot}, "name": ${JSON.stringify(patch.name)},`);
    lines.push(`      "transitionTime": ${patch.transitionTime}, "accentTime": ${patch.accentTime},`);
    lines.push(`      "oneshots": ${JSON.stringify(patch.oneshots || NO_PICKS)},`);
    lines.push('      "layers": [');
    patch.layers.forEach((layer, layerIndex) =>
      lines.push(`        [${layer.join(',')}]${layerIndex < patch.layers.length - 1 ? ',' : ''}`));
    lines.push('      ]');
    lines.push(`    }${index < file.patches.length - 1 ? ',' : ''}`);
  });
  lines.push('  ],');
  const oneshots = file.oneshots || [];
  lines.push(`  "defaultOneshots": ${JSON.stringify(file.defaultOneshots || NO_PICKS)},`);
  lines.push('  "oneshots": [');
  oneshots.forEach((oneshot, index) => {
    lines.push('    {');
    lines.push(`      "index": ${oneshot.index}, "name": ${JSON.stringify(oneshot.name)}, "length": ${oneshot.length},`);
    lines.push(`      "marks": [${oneshot.marks.join(',')}],`);
    lines.push(`      "bytes": [${oneshot.bytes.join(',')}]`);
    lines.push(`    }${index < oneshots.length - 1 ? ',' : ''}`);
  });
  lines.push('  ],');
  const songs = file.songs || [];
  lines.push('  "songs": [');
  songs.forEach((song, index) => {
    lines.push('    {');
    lines.push(`      "name": ${JSON.stringify(song.name)}, "oneshots": ${JSON.stringify(song.oneshots)},`);
    lines.push(`      "sections": ${JSON.stringify(song.sections)},`);
    lines.push(`      "patches": ${JSON.stringify(song.patches)}`);
    lines.push(`    }${index < songs.length - 1 ? ',' : ''}`);
  });
  lines.push('  ],');
  lines.push(`  "gig": ${JSON.stringify(file.gig || new Array(Protocol.SONGS).fill(null))}`);
  lines.push('}');
  return lines.join('\n');
}

function validatePatch(patch, where) {
  if (!patch || typeof patch !== 'object') return `${where} is not a patch`;
  if (!isPrintableName(patch.name)) {
    return `${where} needs a name of up to ${Protocol.PATCH_NAME_LENGTH} printable ASCII characters`;
  }
  if (!isSevenBit(patch.transitionTime) || !isSevenBit(patch.accentTime)) {
    return `${where} needs a transition time and an accent time of 0–127`;
  }
  if (patch.oneshots !== undefined
      && !(Array.isArray(patch.oneshots) && patch.oneshots.length === 2 && patch.oneshots.every(isPick))) {
    return `${where} picks oneshots that are not two kit numbers 0–${Protocol.ONESHOTS - 1} or empty`;
  }
  if (!Array.isArray(patch.layers) || patch.layers.length !== Protocol.PATCH_LAYERS) {
    return `${where} has ${patch.layers && patch.layers.length} layers, expected ${Protocol.PATCH_LAYERS}`;
  }
  for (let layer = 0; layer < Protocol.PATCH_LAYERS; layer++) {
    const bytes = patch.layers[layer];
    if (!Array.isArray(bytes) || bytes.length !== Protocol.PATCH_CC_COUNT) {
      return `${where} layer ${layer} is not ${Protocol.PATCH_CC_COUNT} bytes`;
    }
    if (!bytes.every(isSevenBit)) return `${where} layer ${layer} holds something that is not a 7-bit value`;
  }
  return null;
}

function validateOneshot(oneshot, where) {
  if (!oneshot || typeof oneshot !== 'object') return `${where} is not a oneshot`;
  if (!isOneshotIndex(oneshot.index)) return `${where} is in kit place ${oneshot.index}, not 0–${Protocol.KIT_PLACES - 1}`;
  if (!isPrintableName(oneshot.name)) {
    return `${where} needs a name of up to ${Protocol.PATCH_NAME_LENGTH} printable ASCII characters`;
  }
  if (!isSevenBit(oneshot.length)) return `${where} needs a length of 0–127`;
  if (!Array.isArray(oneshot.bytes) || oneshot.bytes.length !== Protocol.PATCH_CC_COUNT
      || !oneshot.bytes.every(isSevenBit)) {
    return `${where} is not ${Protocol.PATCH_CC_COUNT} 7-bit values`;
  }
  if (!Array.isArray(oneshot.marks) || !oneshot.marks.every(isSevenBit)) return `${where} marks something that is not a CC`;
  return null;
}

function validateKit(file) {
  if (file.defaultOneshots !== undefined
      && !(Array.isArray(file.defaultOneshots) && file.defaultOneshots.length === 2 && file.defaultOneshots.every(isPick))) {
    return 'the default oneshots are not two kit numbers or empty';
  }
  if (file.oneshots === undefined) return null;
  if (!Array.isArray(file.oneshots)) return 'the oneshots are not a list';
  const taken = new Set();
  for (const [position, oneshot] of file.oneshots.entries()) {
    const fault = validateOneshot(oneshot, `oneshot ${position}`);
    if (fault) return fault;
    if (taken.has(oneshot.index)) return `two oneshots in kit place ${oneshot.index}`;
    taken.add(oneshot.index);
  }
  return null;
}

const isPicks = picks => Array.isArray(picks) && picks.length === 2 && picks.every(isPick);

function validateSong(song, where) {
  if (!song || typeof song !== 'object') return `${where} is not a song`;
  if (!isPrintableName(song.name)) {
    return `${where} needs a name of up to ${Protocol.PATCH_NAME_LENGTH} printable ASCII characters`;
  }
  if (!isPicks(song.oneshots)) return `${where} picks oneshots that are not two kit numbers or empty`;
  if (!Array.isArray(song.sections) || !song.sections.every(isPrintableName)) {
    return `${where} has a section label that is not up to ${Protocol.PATCH_NAME_LENGTH} printable ASCII characters`;
  }
  if (!song.patches || typeof song.patches !== 'object' || Array.isArray(song.patches)) {
    return `${where} names no patches for its sections`;
  }
  for (const label of song.sections) {
    const slot = song.patches[label];
    if (slot === undefined) return `${where}: the section "${label}" names no patch`;
    if (slot !== null && !isPatchSlot(slot)) return `${where}: the section "${label}" names slot ${slot}`;
  }
  return null;
}

function validateSongs(file) {
  const songs = file.songs === undefined ? [] : file.songs;
  if (!Array.isArray(songs)) return 'the songs are not a list';
  for (const [index, song] of songs.entries()) {
    const fault = validateSong(song, `song ${index}`);
    if (fault) return fault;
  }
  if (file.gig === undefined) return null;
  const isPlace = place => place === null || (Number.isInteger(place) && place >= 0 && place < songs.length);
  if (!Array.isArray(file.gig) || file.gig.length !== Protocol.SONGS || !file.gig.every(isPlace)) {
    return `the gig is not ${Protocol.SONGS} places, each a song or empty`;
  }
  return null;
}

function validate(file) {
  if (!file || typeof file !== 'object') return 'not a library file';
  if (file.patchFormat !== Protocol.PATCH_FORMAT) {
    return `patch format ${file.patchFormat}, this page speaks ${Protocol.PATCH_FORMAT}`;
  }
  if (!Array.isArray(file.patches)) return 'no patch list in it';
  const taken = new Set();
  for (let index = 0; index < file.patches.length; index++) {
    const patch = file.patches[index];
    const slot = patch && patch.slot;
    if (!isPatchSlot(slot)) {
      return `patch ${index} is in slot ${slot}, and patches go in slots 1–${LAST_PATCH_SLOT}`;
    }
    if (taken.has(slot)) return `two patches in slot ${slot}`;
    taken.add(slot);
    const fault = validatePatch(patch, `patch ${index}`);
    if (fault) return fault;
  }
  return validateKit(file) || validateSongs(file);
}

const nameBytes = name => Array.from(printableName(name).padEnd(Protocol.PATCH_NAME_LENGTH, ' '), c => c.charCodeAt(0));
const nameFrom = bytes => String.fromCharCode(...bytes).trim();
const pickByte = pick => (pick === null || pick === undefined ? Protocol.NO_ONESHOT : pick);
const pickFrom = byte => (byte === Protocol.NO_ONESHOT ? null : byte);

function headBytes(patch) {
  const head = new Array(Protocol.PATCH_HEAD_LENGTH).fill(0);
  head[HEAD.transitionTime] = patch.transitionTime;
  head[HEAD.accentTime] = patch.accentTime;
  (patch.oneshots || NO_PICKS).forEach((pick, place) => { head[HEAD.oneshots + place] = pickByte(pick); });
  head.splice(NAME_AT, Protocol.PATCH_NAME_LENGTH, ...nameBytes(patch.name));
  return head;
}

const patchFromHead = head => ({
  name: nameFrom(head.slice(NAME_AT, NAME_AT + Protocol.PATCH_NAME_LENGTH)),
  transitionTime: head[HEAD.transitionTime],
  accentTime: head[HEAD.accentTime],
  oneshots: [pickFrom(head[HEAD.oneshots]), pickFrom(head[HEAD.oneshots + 1])],
});

function mapBytes(indexes, length) {
  const map = new Array(length).fill(0);
  for (const index of indexes) map[Math.floor(index / 7)] |= 1 << (index % 7);
  return map;
}

const indexesInMap = (map, count) => Array.from({ length: count }, (_, index) => index)
  .filter(index => (map[Math.floor(index / 7)] >> (index % 7)) & 1);

const libraryHeadBytes = file => [
  ...mapBytes(file.patches.map(patch => patch.slot), Protocol.SLOT_MAP_LENGTH),
  ...mapBytes((file.oneshots || []).map(oneshot => oneshot.index), Protocol.KIT_MAP_LENGTH),
  ...(file.defaultOneshots || NO_PICKS).map(pickByte),
];

function libraryFromHeadBytes(head) {
  const kitAt = Protocol.SLOT_MAP_LENGTH, defaultsAt = kitAt + Protocol.KIT_MAP_LENGTH;
  return {
    slots: indexesInMap(head.slice(0, kitAt), LAST_PATCH_SLOT + 1),
    kit: indexesInMap(head.slice(kitAt, defaultsAt), Protocol.KIT_PLACES),
    defaultOneshots: head.slice(defaultsAt, defaultsAt + 2).map(pickFrom),
  };
}

const oneshotBytes = oneshot => [
  oneshot.length,
  ...nameBytes(oneshot.name),
  ...mapBytes(oneshot.marks, Protocol.MARK_MAP_LENGTH),
  ...oneshot.bytes,
];

const NO_PATCH = 0;

function gigSongs(file) {
  const filled = new Set(file.patches.map(patch => patch.slot));
  return (file.gig || []).flatMap((index, place) => {
    if (index === null) return [];
    const song = file.songs[index];
    const slots = song.sections.map(label => (filled.has(song.patches[label]) ? song.patches[label] : NO_PATCH));
    return [{ place, name: song.name, picks: song.oneshots.map(pickByte), slots }];
  });
}

const songBytes = song => [song.place, ...song.picks, song.slots.length, ...song.slots];

function oneshotFromBytes(index, bytes) {
  const marksAt = 1 + Protocol.PATCH_NAME_LENGTH, controlsAt = marksAt + Protocol.MARK_MAP_LENGTH;
  return {
    index,
    name: nameFrom(bytes.slice(1, marksAt)),
    length: bytes[0],
    marks: indexesInMap(bytes.slice(marksAt, controlsAt), Protocol.PATCH_CC_COUNT),
    bytes: bytes.slice(controlsAt, controlsAt + Protocol.PATCH_CC_COUNT),
  };
}

export {
  printableName, serialize, validate, validatePatch, validateOneshot, isPatchSlot, LAST_PATCH_SLOT,
  headBytes, patchFromHead, libraryHeadBytes, libraryFromHeadBytes, oneshotBytes, oneshotFromBytes,
  gigSongs, songBytes,
};
