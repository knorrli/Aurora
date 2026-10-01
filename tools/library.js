(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const LibraryFile = global.AuroraLibraryFile;

  const byteOf = (bytes, name) => bytes[Patch.CC[name]] | 0;

  const TURN = 128;

  function distance(name, from, to) {
    if (!Patch.isCircular(name)) return to - from;
    return ((to - from + TURN * 1.5) % TURN) - TURN / 2;
  }

  function settle(name, value) {
    const rounded = Math.round(value);
    return Patch.isCircular(name) ? ((rounded % TURN) + TURN) % TURN : Patch.clampToSevenBits(rounded);
  }

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

  const emptyOverrides = () => Array.from({ length: Protocol.PATCH_LAYERS },
    (_, layer) => (Patch.isAboveBase(layer) ? {} : null));

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

  function layerBytes(patch, layer) {
    const bytes = patch.base.slice();
    for (const [name, value] of Object.entries(patch.overrides[layer] || {})) writeCC(bytes, name, value);
    return bytes;
  }

  const overriddenIn = (patch, layer) => Object.keys(patch.overrides[layer] || {});
  const changedIn = (patch, layer) => Object.entries(patch.overrides[layer] || {})
    .filter(([name, value]) => value !== byteOf(patch.base, name)).map(([name]) => name);

  function freeRoute(patch, route) {
    for (const name of route.fields) {
      writeCC(patch.base, name, Patch.DEFAULT[name]);
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

  function unseenEngines(fromBytes, toBytes) {
    const preview = global.AuroraPreview;
    if (!preview.hiddenEngines) return { appearing: 0, vanishing: 0, engineOf: () => 0, shows: () => true };
    const hiddenFrom = preview.hiddenEngines(fromBytes);
    const hiddenTo = preview.hiddenEngines(toBytes);
    return {
      appearing: hiddenFrom & ~hiddenTo,
      vanishing: hiddenTo & ~hiddenFrom,
      engineOf: name => preview.engineOf(Patch.CC[name]),
      shows: name => preview.showsEngine(Patch.CC[name]),
    };
  }

  function blend(fromBytes, toBytes, position, switchesFrom) {
    const switches = switchesFrom || fromBytes;
    const unseen = unseenEngines(fromBytes, toBytes);
    const named = {};
    for (const name of Patch.CONTINUOUS) {
      const from = byteOf(fromBytes, name), to = byteOf(toBytes, name);
      const engine = unseen.shows(name) ? 0 : unseen.engineOf(name);
      if (engine & unseen.appearing) named[name] = to;
      else if ((engine & unseen.vanishing) && position < 1) named[name] = from;
      else named[name] = settle(name, from + distance(name, from, to) * position);
    }
    switchesInto(named, switches);
    if (switches === fromBytes) {
      for (const name of Patch.NAMES) {
        if (Patch.isSwitch(name) && (unseen.engineOf(name) & unseen.appearing)) named[name] = byteOf(toBytes, name);
      }
    }
    return holdRoutesChangingDestination(named, switches, toBytes);
  }

  function mix(patch, positions) {
    const named = {};
    for (const name of Patch.CONTINUOUS) {
      const from = byteOf(patch.base, name);
      let value = from;
      for (const [layer, position] of positions) {
        const over = patch.overrides[layer];
        if (position && over && over[name] !== undefined) value += position * distance(name, from, over[name]);
      }
      named[name] = settle(name, value);
    }
    return switchesInto(named, patch.base);
  }

  const freeRouteFields = base => new Set(Patch.ROUTES
    .filter(route => !byteOf(base, route.destination))
    .flatMap(route => route.fields));

  function keepAsBase(patch, named) {
    for (const name of Patch.NAMES) writeCC(patch.base, name, named[name]);
  }

  function keepAsLayer(patch, layer, named) {
    const free = freeRouteFields(patch.base);
    const over = {};
    const leftOnBase = [];
    for (const name of Patch.NAMES) {
      const value = Patch.clampToSevenBits(named[name]);
      if (Patch.isSwitch(name) || free.has(name)) {
        if (value !== byteOf(patch.base, name)) leftOnBase.push(name);
      } else {
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
      writeCC(patch.base, name, Patch.DEFAULT[name]);
      over[name] = value;
      moved.push(name);
    }
    return moved;
  }

  const patchToFile = patch => ({
    name: patch.name,
    transitionTime: patch.transitionTime,
    accentTime: patch.accentTime,
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

  const emptySlots = () => new Array(Protocol.PATCH_MAX).fill(null);
  const filledSlots = library => library.slots.flatMap((patch, slot) => (patch ? [slot] : []));
  const firstEmptySlot = library => {
    const slot = library.slots.findIndex((patch, at) => !patch && LibraryFile.isPatchSlot(at));
    return slot < 0 ? null : slot;
  };

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
    slots[1] = newPatch('first');
    return { keymap: new Array(Protocol.KEYPAD_KEYS).fill(Protocol.PROGRAM_BLACKOUT), slots };
  }

  global.AuroraLibrary = {
    bytesFromNamed, namedFromBytes, writeCC,
    newPatch, clonePatch, layerBytes, overriddenIn, changedIn, freeRoute,
    blend, mix, keepAsBase, keepAsLayer, clearLayer, moveToLayer,
    patchToFile, patchFromFile, validatePatch, validateFile,
    filledSlots, firstEmptySlot, libraryToFile, libraryFromFile, newLibrary,
  };
})(window);
