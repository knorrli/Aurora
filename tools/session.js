(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const Library = global.AuroraLibrary;
  const LibraryFile = global.AuroraLibraryFile;

  const STORE_LIBRARY = 'aurora.editor.library';
  const STORE_DRAFT = 'aurora.editor.draft';
  const STORE_ONESHOT_DRAFT = 'aurora.editor.oneshotDraft';
  const DRAFT_SAVE_DELAY_MILLISECONDS = 500;

  const session = {
    library: null,
    slot: null,
    draft: null,
    mode: 'patch',
    oneshotIndex: null,
    oneshotDraft: null,
    firing: { at: null, repeat: true, index: null },
    overSlot: null,
    layerIndex: Protocol.PATCH_LAYER_BASE,
    transition: { position: 1, from: null },
    mix: Object.fromEntries(Patch.LAYERS_ABOVE_BASE.map(layer => [layer, 0])),
    bypassedRoutes: new Set(),
    bypassedCards: new Set(),
    onChange: null,
    onDraftStarted: null,
  };

  const readStored = key => {
    try {
      return JSON.parse(localStorage.getItem(key));
    } catch {
      return null;
    }
  };

  const writeStored = (key, value) => {
    try {
      if (value === null) localStorage.removeItem(key);
      else localStorage.setItem(key, JSON.stringify(value));
    } catch {
      return false;
    }
    return true;
  };

  function load() {
    const stored = Library.upgradeFile(readStored(STORE_LIBRARY));
    session.library = stored && !Library.validateFile(stored)
      ? Library.libraryFromFile(stored) : Library.newLibrary();
    loadOneshotDraft();
    const draft = readStored(STORE_DRAFT);
    const draftPatch = draft && Library.upgradeDraftPatch(draft.patch, draft.format);
    const fits = draft && !Library.validatePatch(draftPatch, 'the draft')
      && (draft.slot === null || LibraryFile.isPatchSlot(draft.slot));
    if (fits) {
      session.slot = draft.slot;
      session.draft = Library.patchFromFile(draftPatch);
      return;
    }
    const first = firstFilled();
    session.slot = first === null ? Library.firstEmptySlot(session.library) : first;
    session.draft = first === null ? Library.newPatch('untitled') : null;
  }

  let draftTimer = null;

  function saveDraft() {
    clearTimeout(draftTimer);
    draftTimer = null;
    writeStored(STORE_DRAFT, session.draft
      ? { slot: session.slot, format: Protocol.PATCH_FORMAT, patch: Library.patchToFile(session.draft) } : null);
    writeStored(STORE_ONESHOT_DRAFT, session.oneshotDraft
      ? { index: session.oneshotIndex, oneshot: Library.oneshotToFile(session.oneshotDraft, session.oneshotIndex) } : null);
  }

  function loadOneshotDraft() {
    const stored = readStored(STORE_ONESHOT_DRAFT);
    if (!stored || LibraryFile.validateOneshot(stored.oneshot, 'the oneshot draft')) return;
    session.oneshotIndex = stored.index;
    session.oneshotDraft = Library.oneshotFromFile(stored.oneshot);
  }

  function saveDraftSoon() {
    if (!draftTimer) draftTimer = setTimeout(saveDraft, DRAFT_SAVE_DELAY_MILLISECONDS);
  }

  function saveLibrary() {
    writeStored(STORE_LIBRARY, Library.libraryToFile(session.library));
    saveDraft();
  }

  const flush = () => { if (draftTimer) saveDraft(); };

  const firstFilled = () => {
    const filled = Library.filledSlots(session.library);
    return filled.length ? filled[0] : null;
  };

  const editingOneshot = () => session.mode === 'oneshot';
  const underneath = () => session.draft || session.library.slots[session.slot];
  const oneshot = () => session.oneshotDraft || session.library.kit[session.oneshotIndex];
  const patch = () => (editingOneshot() ? oneshot() : underneath());
  const over = () => (session.overSlot !== null && session.overSlot !== session.slot
    && session.library.slots[session.overSlot]) || underneath();
  const kitOneshot = index =>
    (index === session.oneshotIndex && session.oneshotDraft) || session.library.kit[index] || null;
  const resolvedPick = place => {
    const pick = underneath().oneshots[place];
    return pick !== null ? pick : session.library.defaultOneshots[place];
  };
  const oneshotBeats = () => (editingOneshot() ? Library.lengthBeats(oneshot()) : null);
  const fired = () => (editingOneshot() ? oneshot() : session.firing.index === null ? null : kitOneshot(session.firing.index));
  const patchAt = slot => (slot === session.slot && session.draft ? session.draft : session.library.slots[slot]) || null;
  const isAboveBase = () => Patch.isAboveBase(session.layerIndex);
  const overrides = () => patch().overrides[session.layerIndex] || {};

  function editing() {
    if (editingOneshot()) {
      if (!session.oneshotDraft) {
        session.oneshotDraft = Library.cloneOneshot(session.library.kit[session.oneshotIndex]);
        if (session.onDraftStarted) session.onDraftStarted();
      }
      return session.oneshotDraft;
    }
    if (!session.draft) {
      session.draft = Library.clonePatch(session.library.slots[session.slot]);
      if (session.onDraftStarted) session.onDraftStarted();
    }
    return session.draft;
  }

  function comingFrom() {
    const from = session.transition.from;
    return from === null || from === session.slot ? null : patchAt(from);
  }

  const heldAt = () => (session.layerIndex === Protocol.PATCH_LAYER_ACCENT ? comingFrom() : null);

  const switchSource = () => (heldAt() || patch()).base;

  const mixing = () => !editingOneshot() && Object.values(session.mix).some(position => position > 0);
  const transitioning = () =>
    !editingOneshot() && !isAboveBase() && !!comingFrom() && session.transition.position < 1;

  const isRouteField = name => !Library.isMarkable(name);
  const marked = name => !!oneshot().marks[name];

  function oneshotNamed() {
    const under = Library.namedFromBytes(over().base);
    const own = Library.namedFromBytes(oneshot().base);
    const named = {};
    for (const name of Patch.NAMES) named[name] = isRouteField(name) || marked(name) ? own[name] : under[name];
    return named;
  }

  function oneshotInput() {
    const firedOneshot = fired();
    const named = Library.namedFromBytes(firedOneshot.base);
    const own = editingOneshot() ? sounding(named) : named;
    return { bytes: Library.bytesFromNamed(own), marks: Library.markBytes(firedOneshot) };
  }

  function liveNamed() {
    if (editingOneshot()) return oneshotNamed();
    const current = patch();
    const position = session.transition.position;
    if (isAboveBase()) {
      return Library.blend(current.base, Library.layerBytes(current, session.layerIndex), position, switchSource());
    }
    if (transitioning()) return Library.blend(comingFrom().base, current.base, position, comingFrom().base);
    if (mixing()) {
      return Library.mix(current, Object.entries(session.mix).map(([layer, amount]) => [+layer, amount]));
    }
    return Library.namedFromBytes(current.base);
  }

  const layerNamed = () =>
    Library.blend(patch().base, Library.layerBytes(patch(), session.layerIndex), 1, switchSource());

  function changed() {
    saveDraftSoon();
    if (session.onChange) session.onChange();
  }

  function write(name, value) {
    const current = editing();
    if (editingOneshot()) {
      Library.writeCC(current.base, name, value);
      if (!isRouteField(name)) current.marks[name] = true;
      return;
    }
    const over = current.overrides[session.layerIndex];
    if (!isAboveBase() || Patch.isSwitch(name)) {
      Library.writeBase(current, name, value);
    } else if (Patch.clampToSevenBits(value) === (current.base[Patch.CC[name]] | 0)) {
      delete over[name];
    } else {
      over[name] = Patch.clampToSevenBits(value);
    }
  }

  function setValue(name, value) {
    write(name, value);
    changed();
  }

  function applyNamed(named) {
    for (const [name, value] of Object.entries(named)) {
      if (name in Patch.CC) write(name, value);
    }
    changed();
  }

  function toggleMark(name) {
    if (marked(name)) {
      delete editing().marks[name];
      changed();
    } else {
      setValue(name, oneshotNamed()[name]);
    }
  }

  function resetNames(names) {
    if (editingOneshot()) {
      const current = editing();
      for (const name of names) delete current.marks[name];
      changed();
    } else if (isAboveBase()) {
      const over = editing().overrides[session.layerIndex];
      for (const name of names) if (!Patch.isSwitch(name)) delete over[name];
      changed();
    } else {
      applyNamed(Object.fromEntries(names.filter(name => name in Patch.DEFAULT)
        .map(name => [name, Patch.DEFAULT[name]])));
    }
  }

  function moveToLayer(names, layer) {
    const moved = Library.moveToLayer(editing(), layer, names);
    if (moved.length) changed();
    return moved;
  }

  const routeBypassed = route => session.bypassedRoutes.has(route);
  const cardBypassed = card => session.bypassedCards.has(card);
  const toggleIn = (set, item) => { if (!set.delete(item)) set.add(item); };

  function sounding(named) {
    if (!session.bypassedRoutes.size && !session.bypassedCards.size) return named;
    const out = Object.assign({}, named);
    for (const route of session.bypassedRoutes) out[route.destination] = 0;
    for (const card of session.bypassedCards) {
      if (card === Patch.LFO) for (const route of Patch.ROUTES) out[route.destination] = 0;
      for (const name of card.amounts) out[name] = Patch.DEFAULT[name];
    }
    return out;
  }

  function hasNoEffect(card, live) {
    if (card === Patch.LFO) {
      return !Patch.ROUTES.some(route =>
        live[route.destination] !== 0 && live[route.amount] !== Patch.DEFAULT[route.amount]);
    }
    return (card.shownBy || card.amounts).every(name => live[name] === Patch.DEFAULT[name]
      && !routesOn(name, live).some(route => live[route.amount] !== Patch.DEFAULT[route.amount]));
  }

  function routesOn(name, live) {
    return Patch.ROUTES.filter(route => Protocol.routeTarget(live[route.destination]) === Patch.CC[name]);
  }

  const editedTarget = route => Protocol.routeTarget(editing().base[Patch.CC[route.destination]] | 0);

  function setRouteArp(route, arp) {
    setValue(route.destination, Protocol.routeDestination(editedTarget(route), arp, false));
  }

  function setRouteBipolar(route, bipolar) {
    setValue(route.destination, Protocol.routeDestination(editedTarget(route), Protocol.ARP.unison, bipolar));
  }
  const freeRouteSlots = live => Patch.ROUTES.filter(route => !live[route.destination]);

  function sameOverrides(one, other) {
    const names = Object.keys(one || {});
    return names.length === Object.keys(other || {}).length && names.every(name => one[name] === other[name]);
  }

  function layerUnsaved(layer) {
    if (editingOneshot() || !session.draft) return false;
    const saved = session.library.slots[session.slot];
    if (Patch.isAboveBase(layer)) {
      return saved ? !sameOverrides(session.draft.overrides[layer], saved.overrides[layer])
                   : Library.changedIn(session.draft, layer).length > 0;
    }
    return !saved || session.draft.base.some((byte, cc) => byte !== saved.base[cc]);
  }

  function routeRemovable(route) {
    if (editingOneshot() || !isAboveBase()) return true;
    const current = patch();
    const base = Library.namedFromBytes(current.base);
    const tunedOnBase = route.fields.some(name => name !== route.destination && base[name] !== Patch.DEFAULT[name]);
    const tunedOnOtherLayer = current.overrides.some((over, layer) =>
      layer !== session.layerIndex && over && route.fields.some(name => name in over));
    return !tunedOnBase && !tunedOnOtherLayer;
  }

  function freeRoute(route) {
    session.bypassedRoutes.delete(route);
    Library.freeRoute(editing(), route);
    changed();
  }

  const NEW_ROUTE_AMOUNT = 96;

  function addRoute(name) {
    const free = freeRouteSlots(Library.namedFromBytes(patch().base))[0];
    if (!free) return null;
    const current = editing();
    Library.freeRoute(current, free);
    Library.writeCC(current.base, free.destination, Patch.CC[name]);
    write(free.amount, NEW_ROUTE_AMOUNT);
    changed();
    return free;
  }

  function select(slot, draft) {
    session.mode = 'patch';
    session.firing.at = null;
    session.overSlot = null;
    const previous = session.slot;
    if (previous !== slot) session.transition.from = previous !== null && patchAt(previous) ? previous : slot;
    session.slot = slot;
    session.draft = draft || null;
    session.bypassedRoutes.clear();
    session.bypassedCards.clear();
    resetPreview();
    saveDraft();
  }

  function openOneshot(index, draft) {
    session.mode = 'oneshot';
    session.oneshotIndex = index;
    session.oneshotDraft = draft || null;
    session.layerIndex = Protocol.PATCH_LAYER_BASE;
    session.firing.at = null;
    session.firing.index = null;
    session.bypassedRoutes.clear();
    session.bypassedCards.clear();
    resetPreview();
    saveDraft();
  }

  function selectLayer(layerIndex) {
    session.layerIndex = layerIndex;
    if (session.transition.from === null) session.transition.from = session.slot;
    resetPreview();
  }

  function resetTransition() {
    session.transition.position = 1;
  }

  function resetPreview() {
    resetTransition();
    for (const layer of Object.keys(session.mix)) session.mix[layer] = 0;
  }

  function forgetMissingPatches() {
    const transition = session.transition;
    if (transition.from !== null && !patchAt(transition.from)) transition.from = session.slot;
  }

  Object.assign(session, {
    load, saveLibrary, flush, firstFilled,
    editingOneshot, underneath, over, kitOneshot, resolvedPick, fired, oneshot, oneshotBeats, marked, toggleMark, oneshotInput, openOneshot,
    patch, patchAt, isAboveBase, overrides, editing, comingFrom, heldAt,
    mixing, transitioning,
    liveNamed, layerNamed, setValue, resetNames, changed,
    routeBypassed, cardBypassed, sounding, hasNoEffect, routesOn, freeRouteSlots,
    layerUnsaved, routeRemovable, freeRoute, addRoute, moveToLayer, setRouteArp, setRouteBipolar, select, selectLayer, resetTransition, resetPreview, forgetMissingPatches,
    toggleRouteBypass: route => toggleIn(session.bypassedRoutes, route),
    toggleCardBypass: card => toggleIn(session.bypassedCards, card),
  });

  global.AuroraEditor = global.AuroraEditor || {};
  global.AuroraEditor.session = session;
})(window);
