(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;

  const KIND = 'patch library';
  const NAME_AT = Protocol.PATCH_HEAD_LENGTH - Protocol.PATCH_NAME_LENGTH;
  const HEAD = { program: 0, transitionTime: 2, accentTime: 3 };

  const isSevenBit = value => Number.isInteger(value) && value >= 0 && value <= 127;
  const isSlot = slot => Number.isInteger(slot) && slot >= 0 && slot < Protocol.PATCH_MAX;

  const printableName = text =>
    String(text == null ? '' : text).replace(/[^\x20-\x7E]/g, '').slice(0, Protocol.PATCH_NAME_LENGTH);
  const isPrintableName = name => typeof name === 'string' && printableName(name) === name;

  function serialize(file) {
    const lines = [
      '{',
      `  "aurora": ${JSON.stringify(KIND)},`,
      `  "patchFormat": ${file.patchFormat},`,
      `  "savedAt": ${JSON.stringify(new Date().toISOString())},`,
      `  "keymap": [${file.keymap.join(', ')}],`,
      '  "patches": [',
    ];
    file.patches.forEach((patch, index) => {
      lines.push('    {');
      lines.push(`      "slot": ${patch.slot}, "name": ${JSON.stringify(patch.name)},`);
      lines.push(`      "transitionTime": ${patch.transitionTime}, "accentTime": ${patch.accentTime},`);
      lines.push('      "parts": [');
      patch.parts.forEach((part, partIndex) =>
        lines.push(`        [${part.join(',')}]${partIndex < patch.parts.length - 1 ? ',' : ''}`));
      lines.push('      ]');
      lines.push(`    }${index < file.patches.length - 1 ? ',' : ''}`);
    });
    lines.push('  ]', '}');
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
    if (!Array.isArray(patch.parts) || patch.parts.length !== Protocol.PATCH_PARTS) {
      return `${where} has ${patch.parts && patch.parts.length} parts, expected ${Protocol.PATCH_PARTS}`;
    }
    for (let part = 0; part < Protocol.PATCH_PARTS; part++) {
      const bytes = patch.parts[part];
      if (!Array.isArray(bytes) || bytes.length !== Protocol.PATCH_CC_COUNT) {
        return `${where} part ${part} is not ${Protocol.PATCH_CC_COUNT} bytes`;
      }
      if (!bytes.every(isSevenBit)) return `${where} part ${part} holds something that is not a 7-bit value`;
    }
    return null;
  }

  function validate(file) {
    if (!file || typeof file !== 'object') return 'not a library file';
    if (file.patchFormat !== Protocol.PATCH_FORMAT) {
      return `patch format ${file.patchFormat}, this page speaks ${Protocol.PATCH_FORMAT}`;
    }
    if (!Array.isArray(file.patches)) return 'no patch list in it';
    if (!Array.isArray(file.keymap) || file.keymap.length !== Protocol.KEYPAD_KEYS || !file.keymap.every(isSlot)) {
      return `the keymap should be ${Protocol.KEYPAD_KEYS} slots`;
    }
    const taken = new Set();
    for (let index = 0; index < file.patches.length; index++) {
      const patch = file.patches[index];
      const slot = patch && patch.slot;
      if (!isSlot(slot)) {
        return `patch ${index} is in slot ${slot}, and the slots run 0–${Protocol.PATCH_MAX - 1}`;
      }
      if (taken.has(slot)) return `two patches in slot ${slot}`;
      taken.add(slot);
      const fault = validatePatch(patch, `patch ${index}`);
      if (fault) return fault;
    }
    return null;
  }

  function headBytes(patch) {
    const head = new Array(Protocol.PATCH_HEAD_LENGTH).fill(0);
    head[HEAD.program] = Protocol.PROGRAM_SHOW;
    head[HEAD.transitionTime] = patch.transitionTime;
    head[HEAD.accentTime] = patch.accentTime;
    const name = printableName(patch.name).padEnd(Protocol.PATCH_NAME_LENGTH, ' ');
    for (let i = 0; i < Protocol.PATCH_NAME_LENGTH; i++) head[NAME_AT + i] = name.charCodeAt(i);
    return head;
  }

  const patchFromHead = head => ({
    name: String.fromCharCode(...head.slice(NAME_AT, NAME_AT + Protocol.PATCH_NAME_LENGTH)).trim(),
    transitionTime: head[HEAD.transitionTime],
    accentTime: head[HEAD.accentTime],
  });

  function slotMapBytes(slots) {
    const map = new Array(Protocol.SLOT_MAP_LENGTH).fill(0);
    for (const slot of slots) map[Math.floor(slot / 7)] |= 1 << (slot % 7);
    return map;
  }

  const slotsInMap = map => Array.from({ length: Protocol.PATCH_MAX }, (_, slot) => slot)
    .filter(slot => (map[Math.floor(slot / 7)] >> (slot % 7)) & 1);

  global.AuroraLibraryFile = {
    printableName, serialize, validate, validatePatch,
    headBytes, patchFromHead, slotMapBytes, slotsInMap,
  };
})(window);
