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
    return namedFromBytes(global.AuroraPreview.blend(fromBytes, toBytes, position, switches, switches === fromBytes));
  }

  function mix(patch, positions) {
    const weights = new Array(Protocol.PATCH_LAYERS).fill(0);
    for (const [layer, position] of positions) weights[layer] = position;
    const layers = Array.from({ length: Protocol.PATCH_LAYERS }, (_, layer) => layerBytes(patch, layer));
    return namedFromBytes(global.AuroraPreview.mix(patch.base, layers, weights));
  }

  const freeRouteFields = base => new Set(Patch.ROUTES
    .filter(route => !byteOf(base, route.destination))
    .flatMap(route => route.fields));

  function keepAsBase(patch, named) {
    for (const name of Patch.NAMES) writeBase(patch, name, named[name]);
  }

  function keepAsLayer(patch, layer, named) {
    const free = freeRouteFields(patch.base);
    const over = {};
    const leftOnBase = [];
    for (const name of Patch.NAMES) {
      const value = Patch.clampToSevenBits(named[name]);
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
    patches: filledSlots(library).map(slot => Object.assign({ slot }, patchToFile(library.slots[slot]))),
  });

  function libraryFromFile(file) {
    const slots = emptySlots();
    for (const filePatch of file.patches) slots[filePatch.slot] = patchFromFile(filePatch);
    return { slots };
  }

  function newLibrary() {
    const slots = emptySlots();
    slots[1] = newPatch('first');
    return { slots };
  }

  global.AuroraLibrary = {
    bytesFromNamed, namedFromBytes, writeCC,
    newPatch, clonePatch, layerBytes, changedIn, writeBase, freeRoute,
    blend, mix, keepAsBase, keepAsLayer, clearLayer, moveToLayer,
    patchToFile, patchFromFile, validatePatch, validateFile,
    filledSlots, firstEmptySlot, libraryToFile, libraryFromFile, newLibrary,
  };
})(window);
