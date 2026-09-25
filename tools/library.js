(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const LibraryFile = global.AuroraLibraryFile;

  const byteOf = (bytes, name) => bytes[Patch.CC[name]] | 0;

  function bytesFromNamed(named) {
    const bytes = new Array(Protocol.PATCH_CC_COUNT).fill(0);
    for (const name of Patch.NAMES) {
      if (name in named) bytes[Patch.CC[name]] = Patch.clampToSevenBits(named[name]);
    }
    return bytes;
  }

  function namedFromBytes(bytes) {
    const named = {};
    for (const name of Patch.NAMES) named[name] = byteOf(bytes, name);
    return named;
  }

  const writeCC = (bytes, name, value) => { bytes[Patch.CC[name]] = Patch.clampToSevenBits(value); };

  const emptyOverrides = () => Array.from({ length: Protocol.PATCH_PARTS },
    (_, part) => (Patch.isTarget(part) ? {} : null));

  const newPatch = name => ({
    name: LibraryFile.printableName(name || 'untitled'),
    transitionTime: Patch.periodValue(4),
    accentTime: Patch.periodValue(10),
    base: bytesFromNamed(Patch.DEFAULT),
    overrides: emptyOverrides(),
  });

  const clonePatch = patch => ({
    name: patch.name,
    transitionTime: patch.transitionTime,
    accentTime: patch.accentTime,
    base: patch.base.slice(),
    overrides: patch.overrides.map(over => (over ? Object.assign({}, over) : null)),
  });

  function partBytes(patch, part) {
    const bytes = patch.base.slice();
    for (const [name, value] of Object.entries(patch.overrides[part] || {})) writeCC(bytes, name, value);
    return bytes;
  }

  const overriddenIn = (patch, part) => Object.keys(patch.overrides[part] || {});

  function freeRoute(patch, route) {
    for (const name of route.fields) {
      writeCC(patch.base, name, Patch.NEUTRAL[name]);
      for (const over of patch.overrides) if (over) delete over[name];
    }
  }

  function switchesInto(named, switchesFrom) {
    for (const name of Patch.NAMES) {
      if (Patch.isSwitch(name)) named[name] = byteOf(switchesFrom, name);
    }
    return named;
  }

  function holdRoutesChangingDestination(named, switchesFrom, toBytes) {
    for (const route of Patch.ROUTES) {
      if (byteOf(switchesFrom, route.destination) === byteOf(toBytes, route.destination)) continue;
      for (const name of route.fields) named[name] = byteOf(switchesFrom, name);
    }
    return named;
  }

  function blend(fromBytes, toBytes, position, switchesFrom) {
    const switches = switchesFrom || fromBytes;
    const named = {};
    for (const name of Patch.CONTINUOUS) {
      const from = byteOf(fromBytes, name), to = byteOf(toBytes, name);
      named[name] = Math.round(from + (to - from) * position);
    }
    return holdRoutesChangingDestination(switchesInto(named, switches), switches, toBytes);
  }

  function mix(patch, positions) {
    const named = {};
    for (const name of Patch.CONTINUOUS) {
      const from = byteOf(patch.base, name);
      let value = from;
      for (const [part, position] of positions) {
        const over = patch.overrides[part];
        if (position && over && over[name] !== undefined) value += position * (over[name] - from);
      }
      named[name] = Patch.clampToSevenBits(Math.round(value));
    }
    return switchesInto(named, patch.base);
  }

  function moveOverrides(patch, from, to, swap) {
    const moving = Object.assign({}, patch.overrides[from]);
    patch.overrides[from] = swap ? Object.assign({}, patch.overrides[to]) : {};
    patch.overrides[to] = moving;
  }

  const copyOverrides = (patch, from, to) => {
    patch.overrides[to] = Object.assign({}, patch.overrides[from]);
  };

  function takeBaseChanges(patch, reference, to) {
    const over = patch.overrides[to];
    const moved = [], kept = [];
    for (const name of Patch.NAMES) {
      const now = byteOf(patch.base, name), then = byteOf(reference, name);
      if (now === then) continue;
      if (Patch.isSwitch(name)) { kept.push(name); continue; }
      over[name] = now;
      writeCC(patch.base, name, then);
      moved.push(name);
    }
    return { moved, kept };
  }

  const patchToFile = patch => ({
    name: patch.name,
    transitionTime: patch.transitionTime,
    accentTime: patch.accentTime,
    parts: Array.from({ length: Protocol.PATCH_PARTS }, (_, part) => partBytes(patch, part)),
  });

  function patchFromFile(filePatch) {
    const base = filePatch.parts[Protocol.PATCH_BASE].slice();
    const freeFields = new Set(Patch.ROUTES
      .filter(route => !byteOf(base, route.destination))
      .flatMap(route => route.fields));
    const overrides = filePatch.parts.map((bytes, part) => {
      if (!Patch.isTarget(part)) return null;
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
      base,
      overrides,
    };
  }

  function destinationFault(filePatch, where) {
    for (const [part, bytes] of filePatch.parts.entries()) {
      for (const route of Patch.ROUTES) {
        const destination = byteOf(bytes, route.destination);
        if (!Patch.routableDestination(destination)) {
          return `${where} part ${part}: ${route.name} aims at CC ${destination}, which no route can move`;
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

  const emptySlots = () => new Array(Protocol.PATCH_MAX).fill(null);
  const filledSlots = library => library.slots.flatMap((patch, slot) => (patch ? [slot] : []));

  const libraryToFile = library => ({
    patchFormat: Protocol.PATCH_FORMAT,
    keymap: library.keymap.slice(),
    patches: filledSlots(library).map(slot => Object.assign({ slot }, patchToFile(library.slots[slot]))),
  });

  function libraryFromFile(file) {
    const slots = emptySlots();
    for (const filePatch of file.patches) slots[filePatch.slot] = patchFromFile(filePatch);
    return { keymap: file.keymap.slice(), slots };
  }

  function newLibrary() {
    const slots = emptySlots();
    slots[0] = newPatch('first');
    return { keymap: new Array(Protocol.KEYPAD_KEYS).fill(0), slots };
  }

  global.AuroraLibrary = {
    bytesFromNamed, namedFromBytes, writeCC,
    newPatch, clonePatch, partBytes, overriddenIn, freeRoute,
    blend, mix, moveOverrides, copyOverrides, takeBaseChanges,
    patchToFile, patchFromFile, validatePatch, validateFile,
    filledSlots, libraryToFile, libraryFromFile, newLibrary,
  };
})(window);
