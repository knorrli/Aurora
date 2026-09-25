(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const Library = global.AuroraLibrary;

  const STORE_LIBRARY = 'aurora.editor.library';
  const STORE_DRAFT = 'aurora.editor.draft';
  const DRAFT_SAVE_DELAY_MILLISECONDS = 500;

  const session = {
    library: null,
    slot: null,
    draft: null,
    partIndex: Protocol.PATCH_BASE,
    transition: { position: 0, seconds: 2, loop: false, from: null, to: null },
    mix: Object.fromEntries(Patch.TARGETS.map(part => [part, 0])),
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
    const stored = readStored(STORE_LIBRARY);
    session.library = stored && !Library.validateFile(stored)
      ? Library.libraryFromFile(stored) : Library.newLibrary();
    const draft = readStored(STORE_DRAFT);
    const fits = draft && !Library.validatePatch(draft.patch, 'the draft')
      && (draft.slot === null || (Number.isInteger(draft.slot) && session.library.slots[draft.slot]));
    if (fits) {
      session.slot = draft.slot;
      session.draft = Library.patchFromFile(draft.patch);
      return;
    }
    const first = firstFilled();
    session.slot = first;
    session.draft = first === null ? Library.newPatch('untitled') : null;
  }

  let draftTimer = null;

  function saveDraft() {
    clearTimeout(draftTimer);
    draftTimer = null;
    writeStored(STORE_DRAFT, session.draft
      ? { slot: session.slot, patch: Library.patchToFile(session.draft) } : null);
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

  const patch = () => session.draft || session.library.slots[session.slot];
  const patchAt = slot => (slot === session.slot && session.draft ? session.draft : session.library.slots[slot]) || null;
  const isTarget = () => Patch.isTarget(session.partIndex);
  const overrides = () => patch().overrides[session.partIndex] || {};

  function editing() {
    if (!session.draft) {
      session.draft = Library.clonePatch(session.library.slots[session.slot]);
      if (session.onDraftStarted) session.onDraftStarted();
    }
    return session.draft;
  }

  function heldAt() {
    const from = session.transition.from;
    if (session.partIndex !== Protocol.PATCH_TARGET_ACCENT || from === null || from === session.slot) return null;
    return patchAt(from);
  }

  const switchSource = () => (heldAt() || patch()).base;

  const restPosition = () => (isTarget() ? 1 : 0);
  const mixing = () => Object.values(session.mix).some(position => position > 0);
  const transitionDestination = () =>
    (session.transition.to === null ? null : patchAt(session.transition.to));
  const transitioning = () =>
    !isTarget() && !!transitionDestination() && session.transition.position > 0;
  const previewing = () => session.transition.position !== restPosition() || mixing();

  function liveNamed() {
    const current = patch();
    const position = session.transition.position;
    if (isTarget()) {
      return Library.blend(current.base, Library.partBytes(current, session.partIndex), position, switchSource());
    }
    if (transitioning()) return Library.blend(current.base, transitionDestination().base, position, current.base);
    if (mixing()) {
      return Library.mix(current, Object.entries(session.mix).map(([part, amount]) => [+part, amount]));
    }
    return Library.namedFromBytes(current.base);
  }

  const targetNamed = () =>
    Library.blend(patch().base, Library.partBytes(patch(), session.partIndex), 1, switchSource());

  function changed() {
    saveDraftSoon();
    if (session.onChange) session.onChange();
  }

  function write(name, value) {
    const current = editing();
    const over = current.overrides[session.partIndex];
    if (!isTarget() || Patch.isSwitch(name)) {
      Library.writeCC(current.base, name, value);
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

  function resetNames(names) {
    if (isTarget()) {
      const over = editing().overrides[session.partIndex];
      for (const name of names) if (!Patch.isSwitch(name)) delete over[name];
      changed();
    } else {
      applyNamed(Object.fromEntries(names.filter(name => name in Patch.NEUTRAL)
        .map(name => [name, Patch.NEUTRAL[name]])));
    }
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
      for (const name of card.amounts) out[name] = Patch.NEUTRAL[name];
    }
    return out;
  }

  function hasNoEffect(card, live) {
    if (card === Patch.LFO) {
      return !Patch.ROUTES.some(route =>
        live[route.destination] !== 0 && live[route.amount] !== Patch.NEUTRAL[route.amount]);
    }
    return card.amounts.every(name => live[name] === Patch.NEUTRAL[name]);
  }

  const routesOn = (name, live) =>
    Patch.ROUTES.filter(route => live[route.destination] === Patch.CC[name]);
  const freeRouteSlots = live => Patch.ROUTES.filter(route => !live[route.destination]);

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
    session.slot = slot;
    session.draft = draft || null;
    session.bypassedRoutes.clear();
    session.bypassedCards.clear();
    session.transition.from = slot;
    session.transition.to = null;
    resetPreview();
    saveDraft();
  }

  function selectPart(partIndex) {
    session.partIndex = partIndex;
    if (session.transition.from === null) session.transition.from = session.slot;
    resetPreview();
  }

  function resetPreview() {
    session.transition.position = restPosition();
    for (const part of Object.keys(session.mix)) session.mix[part] = 0;
  }

  function forgetMissingPatches() {
    const transition = session.transition;
    if (transition.to !== null && (transition.to === session.slot || !patchAt(transition.to))) transition.to = null;
    if (transition.from !== null && !patchAt(transition.from)) transition.from = session.slot;
  }

  Object.assign(session, {
    load, saveLibrary, flush, firstFilled,
    patch, patchAt, isTarget, overrides, editing, heldAt,
    restPosition, mixing, transitionDestination, transitioning, previewing,
    liveNamed, targetNamed, setValue, resetNames, changed,
    routeBypassed, cardBypassed, sounding, hasNoEffect, routesOn, freeRouteSlots,
    freeRoute, addRoute, select, selectPart, resetPreview, forgetMissingPatches,
    toggleRouteBypass: route => toggleIn(session.bypassedRoutes, route),
    toggleCardBypass: card => toggleIn(session.bypassedCards, card),
  });

  global.AuroraEditor = global.AuroraEditor || {};
  global.AuroraEditor.session = session;
})(window);
