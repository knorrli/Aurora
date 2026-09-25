// tools/editor.js — the Aurora patch editor.
//
// Three things are worth knowing before reading it.
//
// A patch is five parameter sets and the page edits one at a time, chosen by
// the tabs. Whichever is chosen, the wall shows what would be sent, which
// during an audition is the blend rather than the set — so the numbers on
// screen and the picture beside them are never two different things.
//
// A far end is never seen on its own. It is a destination, judged by how the
// trip toward it looks, which is why the scrubber springs back: you hold it to
// look, let go, and you are editing the far end again.
//
// The four switches belong to the patch and not to a set — DESIGN.md
// § "Switches belong to the patch" — so setting one writes it into all five.

(function () {
  'use strict';

  const A = window.AuroraCC;
  const P = window.AuroraPatch;
  const L = window.AuroraLibrary;
  const V = window.AuroraPreview;

  const $ = id => document.getElementById(id);
  const el = (tag, cls, html) => {
    const n = document.createElement(tag);
    if (cls) n.className = cls;
    if (html != null) n.innerHTML = html;
    return n;
  };

  // ---- state -------------------------------------------------------------

  const STORE = 'aurora.editor.library';
  const STORE_DRAFT = 'aurora.editor.draft';

  // Edits go into a draft of one patch, never into the library: the first
  // change opens it, and it is saved into a slot or discarded. `slot` is the
  // slot on screen, or null for a new patch not yet saved anywhere.
  let lib = loadStored() || L.newLibrary();
  let { slot, draft } = loadDraft();
  let setIndex = P.SET_BASE;

  const audition = { pos: 1, held: false, raf: null, seconds: 2, loop: false, from: null, to: null };
  // Where the three faders and a held key are standing, on the Base tab.
  const surfaces = { 1: 0, 2: 0, 3: 0, 4: 0 };
  const surfacesUp = () => Object.values(surfaces).some(v => v > 0);
  const link = new L.Link();
  const rows = {};
  const lastSent = new Array(P.CC_COUNT).fill(-1);

  const patch = () => draft || lib.slots[slot];
  const patchAt = s => (s === slot && draft ? draft : lib.slots[s]);
  const firstFilled = () => { const f = L.filledSlots(lib); return f.length ? f[0] : null; };

  function editing() {
    if (!draft) {
      draft = L.clonePatch(lib.slots[slot]);
      paintList();
    }
    return draft;
  }
  const baseSet = () => patch().base;
  const activeSet = () => L.materialize(patch(), setIndex);
  const isFarEnd = () => setIndex !== P.SET_BASE;
  const overrides = () => patch().overrides[setIndex] || {};

  // Stored in the same shape the file and the wire use, so there is one format
  // to be wrong about rather than two.
  function loadStored() {
    try {
      const raw = JSON.parse(localStorage.getItem(STORE));
      return raw && !L.validate(raw) ? L.libFromWire(raw) : null;
    } catch { return null; }
  }
  function save() {
    try {
      localStorage.setItem(STORE, JSON.stringify(L.libToWire(lib)));
      if (draft) localStorage.setItem(STORE_DRAFT, JSON.stringify({ slot, patch: draft }));
      else localStorage.removeItem(STORE_DRAFT);
    } catch {}
  }

  // A draft that no longer matches its slot, because the library it was
  // opened against was replaced, is dropped rather than guessed at.
  function loadDraft() {
    let stored = null;
    try { stored = JSON.parse(localStorage.getItem(STORE_DRAFT)); } catch {}
    const fits = stored && stored.patch && Array.isArray(stored.patch.base)
      && (stored.slot === null || lib.slots[stored.slot]);
    if (fits) return { slot: stored.slot, draft: stored.patch };
    const first = L.filledSlots(lib)[0];
    return first === undefined ? { slot: null, draft: L.newPatch('untitled') } : { slot: first, draft: null };
  }

  // ---- what is live ------------------------------------------------------

  // An accent plays with the SOURCE patch's switches, because the
  // destination's have not landed yet and will not until the key is released
  // — DESIGN.md § "Switches belong to the patch". So an accent dialed against
  // its own switches is judged on a picture it will rarely show, and this is
  // the picker that fixes it.
  function switchSource() {
    if (setIndex === P.SET_ACCENT && audition.from != null && patchAt(audition.from)) {
      return patchAt(audition.from).base;
    }
    return baseSet();
  }

  // On the base the scrubber rests at 1 meaning the patch itself, so the far
  // end of a journey is only shown while the scrubber is held there.
  const journeying = () => audition.pos < 1 || audition.held;

  function liveNamed() {
    const p = patch();
    if (!isFarEnd()) {
      const dest = audition.to != null ? patchAt(audition.to) : null;
      if (dest && journeying()) {
        return L.blend(p.base, dest.base, audition.pos, p.base);
      }
      if (surfacesUp()) return L.mix(p, Object.entries(surfaces).map(([k, v]) => [+k, v]), p.base);
      return L.namedFromSet(p.base);
    }
    return L.blend(p.base, L.materialize(p, setIndex), audition.pos, switchSource());
  }

  // On the base you are setting the patch. On a far end you are overriding it,
  // and an override that lands back on the base value is not an override — it
  // is the far end following again, which is what makes the change count mean
  // something.
  function setValue(name, value) {
    const p = editing();
    if (!isFarEnd() || P.SWITCHES.includes(name)) {
      L.writeCC(p.base, name, value);
    } else if (P.clamp7(value) === (p.base[P.CC[name]] | 0)) {
      delete p.overrides[setIndex][name];
    } else {
      p.overrides[setIndex][name] = P.clamp7(value);
    }
    save(); paint(); sendLive();
  }

  function applyNamed(named) {
    for (const [name, value] of Object.entries(named)) {
      if (name in P.CC) setValue(name, value);
    }
  }

  function dropOverrides(names) {
    const over = editing().overrides[setIndex];
    if (!over) return;
    for (const n of names) delete over[n];
    save(); paint(); sendLive();
  }

  // A reset means two different things and the difference is the point: on the
  // base, put the control back to no push; on a far end, stop overriding and
  // follow the patch again.
  const resetNames = names => {
    if (isFarEnd()) dropOverrides(names.filter(n => !P.SWITCHES.includes(n)));
    else applyNamed(Object.fromEntries(
      names.filter(n => n in P.NEUTRAL).map(n => [n, P.NEUTRAL[n]])));
  };

  // ---- midi out ----------------------------------------------------------

  function sendLive(force) {
    const live = sounding(liveNamed());
    for (const name of P.NAMES) {
      const cc = P.CC[name], v = live[name];
      if (!force && lastSent[cc] === v) continue;
      lastSent[cc] = v;
      link.sendCC(cc, v);
    }
  }

  // The program change goes last: the brain takes it as the patch having
  // landed, and clears the tails there.
  function arrive() {
    sendLive(true);
    link.sendPC(A.PRESET_GENERATOR);
    clearTails();
  }

  function sendPatchToWall() {
    arrive();
    say(`sent "${patch().name}"`);
  }

  // ---- control rows ------------------------------------------------------

  function faderRow(host, name) {
    const def = P.CONTROLS[name];
    const root = el('div', 'row');
    root.dataset.name = name;

    const label = el('label', null, `<b>${def.label}</b><span class="cc">CC ${P.CC[name]}</span>`);
    label.addEventListener('click', () => { snap(); resetNames([name]); });

    const track = el('div', 'track');
    const slider = el('input');
    slider.type = 'range'; slider.min = 0; slider.max = 127; slider.step = 1;
    const ghost = el('i', 'ghost');
    track.append(slider, ghost);

    const out = el('output');
    slider.addEventListener('pointerdown', snap);
    slider.addEventListener('input', () => setValue(name, +slider.value));

    const routes = routable(name) ? el('button', 'routesbtn', '~') : null;
    if (routes) {
      routes.addEventListener('click', () => toggleRoutePanel(name));
    }

    root.append(label, track, out, routes || el('span'));
    host.appendChild(root);
    rows[name] = {
      root, slider, ghost, out, label, def, routes, points: pointsFor(name), kind: 'fader',
    };
  }

  function pickRow(host, name) {
    const def = P.CONTROLS[name];
    const root = el('div', 'swrow');
    root.dataset.name = name;
    const label = el('label', null, `<b>${def.label}</b><span class="cc">CC ${P.CC[name]}</span>`);
    label.title = def.hint;
    const picks = el('div', 'picks');
    const options = typeof def.options === 'function' ? def.options() : def.options;
    const select = def.kind === 'pick' ? el('select') : null;
    if (select) {
      for (const [value, text] of options) {
        const o = el('option', null, text); o.value = value; select.appendChild(o);
      }
      select.addEventListener('change', () => { snap(); setValue(name, +select.value); });
      picks.appendChild(select);
    }
    const buttons = select ? [] : options.map(([value, text]) => {
      const b = el('button', null, text);
      b.addEventListener('click', () => {
        if (root.classList.contains('locked')) return;
        snap();
        setValue(name, value);
      });
      picks.appendChild(b);
      return [value, b];
    });
    const from = el('span', 'from');
    root.append(label, picks);
    picks.appendChild(from);
    host.appendChild(root);

    const lit = def.kind === 'pick' ? (value, live) => live === value
      : def.kind === 'three' ? (value, live) => P.band3(live) === P.band3(value)
      : (value, live) => P.isOn(live) === P.isOn(value);
    rows[name] = { root, buttons, select, from, def, kind: def.kind, lit };
  }

  function controlRow(host, name) {
    const def = P.CONTROLS[name];
    if (def.kind === 'fader') faderRow(host, name);
    else pickRow(host, name);
  }

  const buildRows = (host, names) => names.forEach(n => controlRow(host, n));

  // ---- painting ----------------------------------------------------------

  const readoutHtml = (value, text) => `<b>${value}</b><span class="cc">${text}</span>`;

  // The readout sits over an invisible copy of its longest text, so the row
  // is as tall as it will ever need and dragging never reflows the page.
  function writeReadout(r, value) {
    const derived = P.DERIVED[r.def.name];
    if (!r.now) {
      let longest = '';
      for (let v = 0; v < 128 && derived; v++) {
        const text = derived(v);
        if (text.length > longest.length) longest = text;
      }
      r.out.innerHTML = `<span class="now"></span><span class="room" aria-hidden="true">${readoutHtml(127, longest)}</span>`;
      r.now = r.out.firstChild;
    }
    r.now.innerHTML = readoutHtml(value, derived ? derived(value) : '');
  }

  function paint() {
    const live = liveNamed();
    paintCards(live);
    const base = L.namedFromSet(baseSet());
    const moved = overrides();
    const farEnd = isFarEnd();

    for (const name of Object.keys(rows)) {
      const r = rows[name];
      const value = live[name];

      if (r.kind === 'fader') {
        if (+r.slider.value !== value) r.slider.value = value;
        writeReadout(r, value);
        const overridden = farEnd && name in moved;
        r.root.classList.toggle('changed', overridden);
        if (overridden) r.ghost.style.left = `calc(${along(r.slider, base[name])} - 1px)`;
        r.label.title = farEnd
          ? (overridden
              ? `${r.def.hint}\nThe patch says ${base[name]}. Click to follow it again.`
              : `${r.def.hint}\nFollowing the patch. Move it to override.`)
          : `${r.def.hint}\nClick to put it back to ${P.NEUTRAL[name]}`;
        r.root.classList.toggle('inert', !!(r.def.inertWhen && r.def.inertWhen(live)));
        if (r.def.inertWhen) {
          r.root.title = r.def.inertWhen(live) ? 'Reaches nothing here — ' + r.def.inertWhy : '';
        }
      } else {
        for (const [value2, b] of r.buttons) b.classList.toggle('on', r.lit(value2, value));
        if (r.select) {
          if (+r.select.value !== value) r.select.value = value;
          r.select.disabled = farEnd;
        }
        r.root.classList.toggle('locked', farEnd);
        r.from.textContent = farEnd
          ? (setIndex === P.SET_ACCENT && audition.from != null && audition.from !== slot
              ? `held at "${patchAt(audition.from).name}"` : 'from the patch')
          : '';
      }
    }

    paintTabs();
    paintRoutePanel(live);
    paintRouteList(live);
    paintHead();
    paintCompare();
  }

  function paintCompare() {
    $('compare').hidden = !isFarEnd();
    if (isFarEnd()) $('farCaption').textContent = P.SET_NAMES[setIndex];
    paintShowing();
  }

  function paintShowing() {
    const label = $('wallShowing');
    if (!label) return;
    const journey = !isFarEnd() && audition.to != null && journeying();
    if (isFarEnd()) {
      label.textContent = `${P.SET_NAMES[setIndex]} ${Math.round(audition.pos * 100)}%`;
    } else if (journey) {
      label.textContent = `"${patchAt(audition.to).name}" ${Math.round(audition.pos * 100)}%`;
    } else if (surfacesUp()) {
      label.textContent = Object.entries(surfaces)
        .filter(([, v]) => v > 0)
        .map(([k, v]) => `${P.SET_NAMES[k]} ${Math.round(v * 100)}%`).join(' + ');
    } else {
      label.textContent = 'the patch';
    }
  }

  // ---- the surface -------------------------------------------------------

  // Sections are subheadings inside the one group, sharing its reset.
  function buildGroup(title, names, sections = [[null, names]]) {
    const box = el('div');
    const h = el('h3', null, title);
    const reset = el('button', 'reset tiny', 'reset');
    reset.style.marginLeft = 'auto';
    reset.addEventListener('click', () => resetNames(names));
    h.appendChild(reset);
    box.appendChild(h);
    const body = el('div');
    for (const [heading, list] of sections) {
      if (heading) body.appendChild(el('h4', 'subhead', heading));
      buildRows(body, list);
    }
    box.appendChild(body);
    return box;
  }

  function buildShape() {
    const host = $('laneShape');
    host.innerHTML = '';
    const head = el('div', 'lane-head');
    head.append(el('span', 'lane-name', P.SHAPE.name));
    host.appendChild(head);

    const groups = el('div', 'groups');
    for (const g of P.SHAPE.groups) {
      groups.appendChild(buildGroup(g.title, [...(g.switches || []), ...(g.controls || [])]));
    }
    host.appendChild(groups);
  }

  function modCard(mod) {
    const card = el('article', 'mod');
    card.dataset.tone = mod.tone;

    const head = el('div', 'mod-head');
    const state = el('span', 'mod-state');
    head.append(el('span', 'mod-name', mod.name), state);
    const bypass = el('button', 'tiny', 'bypass');
    bypass.title = 'Hear the patch without this card while you listen. Never saved.';
    bypass.addEventListener('click', () => {
      if (!bypassedCards.delete(mod)) bypassedCards.add(mod);
      paint(); sendLive();
    });
    head.appendChild(bypass);
    cards.push({ mod, card, state, bypass });
    const reset = el('button', 'tiny', 'reset');
    reset.addEventListener('click', () => resetNames([
      ...(mod.source || []), ...(mod.amounts || []), ...(mod.switches || []),
    ]));
    head.appendChild(reset);
    card.appendChild(head);

    const body = el('div', 'mod-body');

    const sources = [...(mod.switches || []), ...(mod.source || [])];
    if (sources.length) {
      const sourceBox = el('div');
      sourceBox.appendChild(el('h4', null, 'Source'));
      buildRows(sourceBox, sources);
      body.appendChild(sourceBox);
    }

    if (mod.amounts) {
      const amountBox = el('div');
      amountBox.appendChild(el('h4', null, 'Amounts'));
      buildRows(amountBox, mod.amounts);
      body.appendChild(amountBox);
    }
    if (body.children.length < 2) body.style.gridTemplateColumns = '1fr';
    card.appendChild(body);
    if (mod === P.LFO) card.appendChild(buildRouteList());
    return card;
  }

  function buildModulators() {
    $('lfo').replaceChildren(modCard(P.LFO));
    $('mods').replaceChildren(...P.MODULATORS.map(modCard));
  }

  function buildOutputs() {
    $('outputs').replaceChildren(
      buildGroup('5 strips', P.STRIPS.controls),
      buildGroup('4 PARs', P.PARS.controls, [
        ['Color', P.PARS.color], ['Hue across them', P.PARS.hue], ['LFO across them', P.PARS.lfo],
      ]));

  }

  // ---- a route's push, drawn ---------------------------------------------

  const WAVE_CYCLES = 2;

  // A rate swings around its dialed value, so its wave is drawn with its
  // average taken off, the way the renderer applies it.
  function departureAt(route, phase, live) {
    const ratio = A.routeRatio(live[route.ratio]);
    const wave = live[route.wave];
    const swings = (A.TAGS[A.NAME_BY_CC[live[route.destination]]] || []).includes('rate');
    const at = V.lfoWave(phase * ratio - live[route.phase] / 128, wave);
    return P.bip(live[route.amount]) * (swings ? at - V.waveMean(wave) : at);
  }

  function drawWave(canvas, dest, live) {
    const w = canvas.clientWidth, h = canvas.clientHeight;
    if (!w || !h) return;
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    canvas.width = Math.round(w * dpr);
    canvas.height = Math.round(h * dpr);
    const ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, w, h);

    const pad = 7;
    const y = v => (h / 2) - v * (h / 2 - pad);
    const rail = (v, style, dashed) => {
      ctx.setLineDash(dashed ? [3, 4] : []);
      ctx.strokeStyle = style; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(0, y(v) + 0.5); ctx.lineTo(w, y(v) + 0.5); ctx.stroke();
      ctx.setLineDash([]);
    };
    rail(1, 'rgba(255,255,255,.06)');
    rail(-1, 'rgba(255,255,255,.06)');
    rail(0, 'rgba(255,255,255,.22)', true);

    ctx.strokeStyle = '#cf7d93';
    ctx.lineWidth = 2; ctx.lineJoin = 'round';
    ctx.beginPath();
    for (let px = 0; px <= w; px++) {
      const v = departureAt(dest, px / w * WAVE_CYCLES, live);
      if (px === 0) ctx.moveTo(px, y(v)); else ctx.lineTo(px, y(v));
    }
    ctx.stroke();
  }

  // ---- routes, opened from the control they move --------------------------
  //
  // One panel for the whole page, moved under whichever control it was opened
  // from, so the row and its band stay in view while a route is dialed. It
  // floats over the rows below rather than pushing them down.

  const routable = name => (A.TAGS[name] || []).includes('patch')
    && !V.routeRefused(P.CC[name]);

  // Half of the way to the limit, so the band shows the moment a route exists:
  // one added at nothing looks like one that did not take.
  const NEW_ROUTE_AMOUNT = 96;

  const routePanel = { root: null, blocks: [], add: null, free: null, target: null };

  // Bypass is for listening at the desk and is never saved: a bypassed route
  // reaches the wall and the brain as a free slot, and the patch keeps it. A
  // bypassed card is sent with its amounts at rest, and a bypassed LFO frees
  // every route.
  const bypassed = new Set();
  const bypassedCards = new Set();
  function sounding(named) {
    if (!bypassed.size && !bypassedCards.size) return named;
    const out = { ...named };
    for (const route of bypassed) out[route.destination] = 0;
    for (const mod of bypassedCards) {
      if (mod === P.LFO) for (const route of P.ROUTES) out[route.destination] = 0;
      for (const name of mod.amounts || []) out[name] = P.NEUTRAL[name];
    }
    return out;
  }

  // A card with every amount at rest, or an LFO with no route pushing, changes
  // nothing on the wall however its source is set.
  function cardSilent(mod, live) {
    if (mod === P.LFO) {
      return !P.ROUTES.some(r => live[r.destination] !== 0 && live[r.amount] !== P.NEUTRAL[r.amount]);
    }
    return (mod.amounts || []).every(name => live[name] === P.NEUTRAL[name]);
  }

  const cards = [];
  function paintCards(live) {
    for (const { mod, card, state, bypass } of cards) {
      const off = bypassedCards.has(mod);
      const silent = cardSilent(mod, live);
      card.classList.toggle('bypassed', off);
      card.classList.toggle('silent', silent && !off);
      bypass.classList.toggle('on', off);
      state.textContent = off ? 'bypassed' : silent ? 'no effect' : '';
    }
  }

  function buildRoutePanel() {
    const root = el('div', 'routepanel');
    root.hidden = true;
    for (const route of P.ROUTES) {
      const block = el('div', 'routeblock');
      const head = el('div', 'dest-head');
      const remove = el('button', 'tiny', '\u00d7');
      remove.title = 'free this route';
      remove.addEventListener('click', () => freeRoute(route));
      const bypass = el('button', 'tiny', 'bypass');
      bypass.title = 'silence this route while you listen; not saved';
      bypass.addEventListener('click', () => toggleBypass(route));
      const buttons = el('span', 'dest-buttons');
      buttons.append(bypass, remove);
      head.append(el('span', 'dest-name', route.name), buttons);
      const body = el('div');
      buildRows(body, [route.amount, route.ratio, route.wave, route.phase]);
      const canvas = el('canvas', 'destwave');
      block.append(head, body, canvas);
      root.appendChild(block);
      routePanel.blocks.push({ route, block, canvas, remove, bypass });
    }
    const foot = el('div', 'routefoot');
    const add = el('button', 'tiny', '+ add a route');
    add.addEventListener('click', () => { snap(); addRoute(); });
    const free = el('span', 'cc');
    foot.append(add, free);
    root.append(foot);
    document.body.appendChild(root);
    Object.assign(routePanel, { root, add, free });

    document.addEventListener('keydown', e => { if (e.key === 'Escape') closeRoutePanel(); });
    document.addEventListener('pointerdown', e => {
      if (routePanel.target && !root.contains(e.target) && !e.target.closest('.routesbtn')) {
        closeRoutePanel();
      }
    });
    window.addEventListener('resize', placeRoutePanel);
  }

  const routeFields = route =>
    [route.destination, route.amount, route.ratio, route.wave, route.phase];

  function freeRoute(route) {
    snap();
    bypassed.delete(route);
    resetNames(routeFields(route));
    const aimed = L.namedFromSet(baseSet());
    if (routePanel.target && !P.ROUTES.some(r => aimed[r.destination] === P.CC[routePanel.target])) {
      closeRoutePanel();
    }
  }

  function toggleBypass(route) {
    if (!bypassed.delete(route)) bypassed.add(route);
    paint();
    sendLive();
  }

  // ---- every route, listed at the LFO ------------------------------------
  //
  // Read-only apart from bypass and free: the dials stay in the one panel
  // under the control a route moves, and a line opens it there.

  const routeList = { root: null, count: null, lines: [] };

  // Where a control sits on the page, as its card and label read.
  const PLACE = {};
  for (const g of P.SHAPE.groups) {
    for (const n of [...(g.switches || []), ...(g.controls || [])]) PLACE[n] = `${P.SHAPE.name} \u00b7 ${g.title}`;
  }
  for (const mod of P.MODULATORS) {
    for (const n of [...(mod.switches || []), ...(mod.source || []), ...(mod.amounts || [])]) PLACE[n] = mod.name;
  }
  for (const n of P.STRIPS.controls) PLACE[n] = '5 strips';
  for (const n of P.PARS.controls) PLACE[n] = '4 PARs';

  function buildRouteList() {
    const root = el('details', 'routelist');
    root.open = true;
    const count = el('span', 'cc');
    const summary = el('summary', null, 'Routes ');
    summary.appendChild(count);
    root.appendChild(summary);
    root.addEventListener('toggle', paint);

    for (const route of P.ROUTES) {
      const line = el('div', 'routeline');
      const target = el('button', 'routeline-target');
      target.title = 'open this route under the control it moves';
      target.addEventListener('click', () => {
        const name = A.NAME_BY_CC[liveNamed()[route.destination]];
        rows[name].root.scrollIntoView({ block: 'center' });
        if (routePanel.target !== name) toggleRoutePanel(name);
      });
      const values = el('span', 'cc routeline-values');
      const canvas = el('canvas', 'destwave routeline-wave');
      const remove = el('button', 'tiny', '\u00d7');
      remove.addEventListener('click', () => freeRoute(route));
      const bypass = el('button', 'tiny', 'bypass');
      bypass.title = 'silence this route while you listen; not saved';
      bypass.addEventListener('click', () => toggleBypass(route));
      const buttons = el('span', 'dest-buttons');
      buttons.append(bypass, remove);
      line.append(target, values, canvas, buttons);
      root.appendChild(line);
      routeList.lines.push({ route, line, target, values, canvas, remove, bypass });
    }
    Object.assign(routeList, { root, count });
    return root;
  }

  function paintRouteList(live) {
    if (!routeList.root) return;
    const farEnd = isFarEnd();
    let aimed = 0;
    for (const { route, line, target, values, canvas, remove, bypass } of routeList.lines) {
      const name = A.NAME_BY_CC[live[route.destination]];
      line.hidden = !name;
      if (!name) continue;
      aimed++;
      target.textContent = `${PLACE[name]} \u00b7 ${P.CONTROLS[name].label}`;
      values.textContent = [route.amount, route.ratio, route.wave, route.phase]
        .map(field => P.DERIVED[field](live[field])).join(' \u00b7 ');
      line.classList.toggle('bypassed', bypassed.has(route));
      bypass.classList.toggle('on', bypassed.has(route));
      remove.disabled = farEnd;
      remove.title = farEnd ? 'freed on the base: a route belongs to the whole patch' : 'free this route';
      if (routeList.root.open) drawWave(canvas, route, live);
    }
    routeList.count.textContent = aimed ? `${aimed} of ${P.ROUTES.length}` : 'none';
  }

  // A control with no route yet is opened to add one, so opening it adds it.
  function toggleRoutePanel(name) {
    if (routePanel.target === name) { closeRoutePanel(); return; }
    routePanel.target = name;
    const base = L.namedFromSet(baseSet());
    if (!P.ROUTES.some(route => base[route.destination] === P.CC[name])) {
      snap();
      addRoute();
    }
    paint();
  }

  function closeRoutePanel() {
    routePanel.target = null;
    paint();
  }

  // A destination has no middle, so like every switch it belongs to the patch:
  // a route is freed on the base, and a far end overrides how far and how fast.
  // Added on a far end, the route is aimed on the base at zero, so only that
  // far end pushes: the destination is a switch and belongs to the patch, but
  // the amount is an ordinary override.
  function addRoute() {
    const base = L.namedFromSet(baseSet());
    const free = P.ROUTES.find(route => !base[route.destination]);
    if (!free) return;
    if (isFarEnd()) {
      const patch = editing();
      for (const name of [free.amount, free.ratio, free.wave, free.phase]) {
        L.writeCC(patch.base, name, P.NEUTRAL[name]);
      }
    }
    applyNamed({
      [free.destination]: P.CC[routePanel.target],
      [free.amount]: NEW_ROUTE_AMOUNT,
      [free.ratio]: P.NEUTRAL[free.ratio],
      [free.wave]: P.NEUTRAL[free.wave],
      [free.phase]: P.NEUTRAL[free.phase],
    });
  }

  function paintRoutePanel(live) {
    const target = routePanel.target;
    const lfoBypassed = bypassedCards.has(P.LFO);
    for (const r of Object.values(rows)) {
      if (!r.routes) continue;
      const here = P.ROUTES.filter(route => live[route.destination] === P.CC[r.def.name]);
      const aimed = here.length;
      const sounding = !lfoBypassed && here.some(route => !bypassed.has(route));
      r.routes.classList.toggle('aimed', aimed > 0);
      r.routes.classList.toggle('sounding', sounding);
      r.routes.title = aimed && !sounding ? 'routes on this control, all bypassed' : 'routes on this control';
      r.routes.classList.toggle('open', r.def.name === target);
      r.routes.textContent = aimed > 1 ? '~' + aimed : '~';
    }
    routePanel.root.hidden = !target;
    if (!target) return;

    const farEnd = isFarEnd();
    let free = 0;
    for (const { route, block, canvas, remove, bypass } of routePanel.blocks) {
      if (!live[route.destination]) free++;
      block.hidden = live[route.destination] !== P.CC[target];
      block.classList.toggle('bypassed', bypassed.has(route));
      bypass.classList.toggle('on', bypassed.has(route));
      remove.disabled = farEnd;
      remove.title = farEnd ? 'freed on the base: a route belongs to the whole patch' : 'free this route';
      if (!block.hidden) drawWave(canvas, route, live);
    }
    routePanel.add.disabled = free === 0;
    routePanel.free.textContent = `${free} of ${P.ROUTES.length} free`;
    routePanel.add.title = farEnd
      ? 'Added here, the route sits on the base at zero and only this far end pushes.'
      : '';
    placeRoutePanel();
  }

  // Under the row, or above it where the window has no room below.
  function placeRoutePanel() {
    const target = routePanel.target;
    if (!target) return;
    const row = rows[target].root.getBoundingClientRect();
    const panel = routePanel.root;
    panel.style.width = `${Math.max(380, row.width)}px`;
    const height = panel.offsetHeight;
    const below = row.bottom + 4;
    const top = below + height > window.innerHeight && row.top - 4 - height > 0
      ? row.top - 4 - height : below;
    panel.style.left = `${row.left + window.scrollX}px`;
    panel.style.top = `${top + window.scrollY}px`;
  }

  // ---- set tabs ----------------------------------------------------------

  const tabs = [];

  function buildTabs() {
    const host = $('setTabs');
    host.innerHTML = '';
    for (let i = 0; i < P.SETS; i++) {
      const b = el('button', 'tab');
      const name = el('span', null, P.SET_NAMES[i]);
      const badge = el('span', 'badge');
      b.append(name, badge);
      b.title = P.SET_BLURB[i];
      b.addEventListener('click', () => selectSet(i));
      host.appendChild(b);
      tabs.push({ b, badge });
    }
  }

  function paintTabs() {
    for (let i = 0; i < P.SETS; i++) {
      tabs[i].b.classList.toggle('on', i === setIndex);
      if (i === P.SET_BASE) {
        tabs[i].badge.textContent = 'the look itself';
      } else {
        const n = L.overriddenIn(patch(), i).length;
        tabs[i].badge.textContent = n ? `${n} overridden` : 'follows the patch';
      }
    }
  }

  function selectSet(i) {
    setIndex = i;
    stopRun();
    audition.pos = 1;
    audition.held = false;
    if (audition.from === null) audition.from = slot;
    buildAudition();
    buildSurfaces();
    paint();
    paintList();
    sendLive();
  }

  // ---- the audition ------------------------------------------------------

  function stopRun() {
    if (audition.raf) cancelAnimationFrame(audition.raf);
    audition.raf = null;
    const b = $('auditionRun');
    if (b) b.classList.remove('on');
  }

  // Dialing always happens at the far end, so touching a control puts the
  // scrubber back there first. Without it the wall and the sliders would be
  // showing two different things the moment you reached for one.
  function snap() {
    if (audition.pos >= 1 && !surfacesUp()) return;
    stopRun();
    audition.pos = 1;
    audition.held = false;
    const s = $('auditionScrub');
    if (s) s.value = 1000;
    clearSurfaces();
    paint();
    sendLive();
  }

  function clearSurfaces() {
    for (const k of Object.keys(surfaces)) surfaces[k] = 0;
    for (let i = 1; i < P.SETS; i++) {
      const s = $('surface' + i);
      if (s) s.value = 0;
    }
  }

  function animate(from, to, ms, done) {
    const startedAt = performance.now();
    const scrub = $('auditionScrub');
    const step = now => {
      const k = Math.min(1, (now - startedAt) / ms);
      audition.pos = from + (to - from) * k;
      if (scrub) scrub.value = Math.round(audition.pos * 1000);
      paintLive();
      if (k < 1) { audition.raf = requestAnimationFrame(step); return; }
      audition.raf = null;
      if (done) done();
    };
    audition.raf = requestAnimationFrame(step);
  }

  function springBack() {
    audition.held = false;
    if (audition.pos >= 1) { paint(); return; }
    stopRun();
    animate(audition.pos, 1, 180, paint);
  }

  function runAudition() {
    if (audition.raf) { stopRun(); springBack(); return; }
    const ms = Math.max(0.2, audition.seconds) * 1000;
    $('auditionRun').classList.add('on');
    const leg = (from, to) => animate(from, to, ms, () => {
      if (audition.loop) leg(to, from);
      else { $('auditionRun').classList.remove('on'); paint(); }
    });
    leg(0, 1);
  }

  // The readouts and the wall follow the blend; the rest of the page does not
  // move, so a trip does not redraw the library on every frame.
  function paintLive() {
    const live = liveNamed();
    for (const name of Object.keys(rows)) {
      const r = rows[name];
      if (r.kind !== 'fader') continue;
      const value = live[name];
      if (+r.slider.value !== value) r.slider.value = value;
      writeReadout(r, value);
    }
    paintShowing();
    const readout = $('auditionReadout');
    if (readout) {
      readout.textContent = Math.round(audition.pos * 100) + '%';
    }
    sendLive();
  }

  // The three faders and a held key, all at once. It only exists on the Base
  // tab: there you are looking at the patch as a whole, and on a far-end tab
  // you are dialing one destination and want to see that destination.
  function buildSurfaces() {
    const host = $('surfaceBar');
    host.hidden = isFarEnd();
    if (isFarEnd()) return;
    host.innerHTML = '';

    const head = el('h2', null, 'The surfaces, all at once');
    const clear = el('button', 'tiny', 'all down');
    clear.addEventListener('click', () => { clearSurfaces(); paint(); sendLive(); });
    head.appendChild(clear);
    host.appendChild(head);

    const rows2 = el('div', 'srows');
    for (let i = 1; i < P.SETS; i++) {
      const row = el('div', 'row');
      const label = el('label', null,
        `<b>${P.SET_NAMES[i]}</b><span class="cc">${i === P.SET_ACCENT ? 'a held key' : 'fader'}</span>`);
      label.title = P.SET_BLURB[i];
      const track = el('div', 'track');
      const slider = el('input');
      slider.type = 'range'; slider.min = 0; slider.max = 1000; slider.step = 1;
      slider.id = 'surface' + i;
      slider.value = Math.round(surfaces[i] * 1000);
      const out = el('output');
      const show = () => {
        const n = L.overriddenIn(patch(), i).length;
        out.innerHTML = `<b>${Math.round(surfaces[i] * 100)}%</b>`
          + `<span class="cc">${n ? n + ' overridden' : 'nothing to reach'}</span>`;
      };
      show();
      slider.addEventListener('input', () => {
        surfaces[i] = +slider.value / 1000;
        show();
        paintShowing();
        paintLive();
      });
      track.append(slider);
      row.append(label, track, out);
      rows2.appendChild(row);
    }
    host.appendChild(rows2);
    host.appendChild(el('p', 'note', '\u26a0 The brain does not combine the faders yet \u2014 this is the editor\u2019s guess.'));
  }

  function buildAudition() {
    const host = $('auditionBar');
    host.innerHTML = '';

    const scrub = el('div', 'scrub');
    scrub.appendChild(el('span', 'end', isFarEnd() ? 'base' : 'this patch'));
    const range = el('input');
    range.type = 'range'; range.min = 0; range.max = 1000; range.step = 1;
    range.id = 'auditionScrub';
    range.value = Math.round(audition.pos * 1000);
    range.addEventListener('input', () => {
      stopRun();
      audition.held = true;
      audition.pos = +range.value / 1000;
      paintLive();
    });
    range.addEventListener('change', springBack);
    range.addEventListener('pointerup', springBack);
    scrub.appendChild(range);
    scrub.appendChild(el('span', 'end', isFarEnd() ? P.SET_NAMES[setIndex] : 'the patch you named'));
    host.appendChild(scrub);

    const run = el('button', null, 'run');
    run.id = 'auditionRun';
    run.addEventListener('click', runAudition);
    host.appendChild(run);

    const secs = el('input');
    secs.type = 'number'; secs.min = 0.2; secs.max = 60; secs.step = 0.2;
    secs.value = audition.seconds; secs.style.width = '64px';
    secs.addEventListener('input', () => { audition.seconds = +secs.value || 2; });
    host.append(secs, el('span', 'cc', 'seconds'));

    const loop = el('button', 'tiny' + (audition.loop ? ' on' : ''), 'loop');
    loop.addEventListener('click', () => {
      audition.loop = !audition.loop;
      loop.classList.toggle('on', audition.loop);
    });
    host.appendChild(loop);

    const readout = el('output', 'readout');
    readout.id = 'auditionReadout';
    readout.textContent = Math.round(audition.pos * 100) + '%';
    host.appendChild(readout);

    if (isFarEnd()) {
      // The accent belongs to no fader, but it is a far end like the other
      // three and a look dialed under one of them is as likely to belong here
      // as anywhere.
      if (setIndex === P.SET_ACCENT) {
        const wrap = el('label', 'field');
        wrap.appendChild(el('span', null, 'as heard from'));
        wrap.title = 'An accent plays with the switches of the patch you came from, because the destination\u2019s land on the release and not on arrival. Dial it against the patch it will actually follow.';
        const pick = el('select');
        for (const s of L.filledSlots(lib)) {
          const o = el('option', null, `${s} \u00b7 ${patchAt(s).name}`);
          o.value = s;
          pick.appendChild(o);
        }
        pick.value = audition.from == null ? slot : audition.from;
        pick.addEventListener('change', () => {
          audition.from = +pick.value;
          paint(); sendLive();
        });
        wrap.appendChild(pick);
        host.appendChild(wrap);
      }

      host.appendChild(slotMoves());

    } else {
      const wrap = el('label', 'field');
      wrap.appendChild(el('span', null, 'journey to'));
      const pick = el('select');
      pick.appendChild(el('option', null, '\u2014 nowhere \u2014')).value = '';
      for (const s of L.filledSlots(lib)) {
        if (s === slot) continue;
        const o = el('option', null, `${s} \u00b7 ${lib.slots[s].name}`);
        o.value = s;
        pick.appendChild(o);
      }
      pick.value = audition.to == null ? '' : audition.to;
      pick.addEventListener('change', () => {
        audition.to = pick.value === '' ? null : +pick.value;
        paint(); sendLive();
      });
      wrap.appendChild(pick);
      host.appendChild(wrap);
    }
  }

  // Moving a far end between the four surfaces. Only the override map travels.
  function slotMoves() {
    const move = el('div', 'slotmove');
    const others = [1, 2, 3, 4].filter(i => i !== setIndex);
    const picker = (text, act) => {
      const wrap = el('label', 'field');
      const sel = el('select');
      const head = el('option', null, text);
      head.value = '';
      sel.appendChild(head);
      for (const i of others) {
        const o = el('option', null, P.SET_NAMES[i]);
        o.value = i;
        sel.appendChild(o);
      }
      sel.addEventListener('change', () => {
        if (sel.value === '') return;
        act(+sel.value);
        sel.value = '';
        save(); buildAudition(); buildSurfaces(); paint(); sendLive();
      });
      wrap.appendChild(sel);
      return wrap;
    };
    const take = el('button', 'tiny', 'take the base\u2019s changes');
    take.title = 'Everything changed on the base since this edit began becomes this far end, and the base goes back. Switches stay on the base.';
    take.addEventListener('click', takeBaseChanges);
    move.append(
      take,
      picker('copy from\u2026', from => L.copyOverrides(editing(), from, setIndex)),
      picker('move onto\u2026', to => L.moveOverrides(editing(), setIndex, to, false)),
      picker('swap with\u2026', to => L.moveOverrides(editing(), setIndex, to, true)));
    return move;
  }

  // The reference is the patch as saved, or as it was created if it never was.
  function takeBaseChanges() {
    const saved = slot !== null ? lib.slots[slot] : null;
    const reference = saved ? saved.base : L.newPatch().base;
    const { moved, kept } = L.takeBaseChanges(editing(), reference, setIndex);
    const label = name => (P.CONTROLS[name] ? P.CONTROLS[name].label : name);
    if (!moved.length && !kept.length) {
      say('the base has no changes to take', 'bad');
      return;
    }
    save(); buildAudition(); buildSurfaces(); paint(); sendLive();
    const took = `${P.SET_NAMES[setIndex]} took ${moved.length} change${moved.length === 1 ? '' : 's'} from the base`;
    say(kept.length
      ? `${took}; ${kept.map(label).join(', ')} stay${kept.length === 1 ? 's' : ''} on the base — switches belong to the whole patch`
      : took, kept.length ? 'warn' : 'ok');
  }

  // ---- the patch head ----------------------------------------------------

  function buildHead() {
    for (const [id, field] of [['pRampJourney', 'rampJourney'], ['pRampAccent', 'rampAccent']]) {
      const sel = $(id);
      P.LFO_PERIOD_NAMES.forEach((text, step) => {
        const o = el('option', null, text); o.value = P.periodByte(step); sel.appendChild(o);
      });
      sel.title = 'Stepped to values that come back to the grid, so holding through a completed journey arrives on a beat.';
      sel.addEventListener('change', () => { editing()[field] = +sel.value; save(); });
    }

    const tempo = $('pTempo');
    P.DIVISIONS.forEach(([value, text]) => {
      const o = el('option', null, text); o.value = value; tempo.appendChild(o);
    });
    tempo.title = `What one tempo pulse stands for. Every rate scales with it. CC ${P.CC.tempoDivision}`;
    tempo.addEventListener('change', () => { snap(); setValue('tempoDivision', +tempo.value); });

    $('pName').addEventListener('input', () => {
      editing().name = $('pName').value.slice(0, P.NAME_LEN);
      save(); paintList();
    });
  }

  function paintHead() {
    const p = patch();
    if ($('pName').value !== p.name) $('pName').value = p.name;
    $('pRampJourney').value = String(P.periodByte(P.periodStep(p.rampJourney)));
    $('pRampAccent').value = String(P.periodByte(P.periodStep(p.rampAccent)));
    $('pTempo').value = String(liveNamed().tempoDivision);
  }

  // ---- the library rail --------------------------------------------------

  function paintList() {
    const host = $('patchList');
    host.innerHTML = '';
    const row = (s, p) => {
      const keys = s === null ? []
        : lib.keymap.map((k, n) => (k === s ? n + 1 : null)).filter(Boolean);
      const on = s === slot;
      const b = el('button', 'item' + (on ? ' on' : '') + (on && draft ? ' dirty' : ''));
      b.append(
        el('span', 'pc', s === null ? 'new' : String(s)),
        el('span', 'name', (on ? patch() : p).name || '(unnamed)'),
        el('span', 'keys', !keys.length ? ''
          : keys.length > 3 ? `${keys.length} keys` : 'key ' + keys.join(',')));
      b.title = keys.length ? `${p.name} — on keypad ${keys.join(', ')}` : p.name;
      if (s !== null) b.addEventListener('click', () => selectPatch(s));
      host.appendChild(b);
    };
    if (slot === null) row(null, draft);
    for (const s of L.filledSlots(lib)) row(s, lib.slots[s]);
    $('libCount').textContent = `${L.filledSlots(lib).length} / ${P.PATCH_MAX}`;
    paintSaving();
    paintKeypad();
  }

  function paintSaving() {
    $('patchSave').disabled = !draft || slot === null;
    $('patchDiscard').disabled = !draft;
    $('patchDel').disabled = slot === null;
    const target = $('saveSlot');
    if (target.value === '' || !target.dataset.touched) {
      target.value = slot !== null ? slot : lib.slots.findIndex(p => !p);
    }
    paintSaveTarget();
  }

  function paintSaveTarget() {
    const n = +$('saveSlot').value;
    const valid = $('saveSlot').value !== '' && Number.isInteger(n) && n >= 0 && n < P.PATCH_MAX;
    $('saveHere').disabled = !valid;
    $('saveOccupant').textContent = !valid ? ''
      : n === slot ? 'this patch' : lib.slots[n] ? lib.slots[n].name : 'empty';
  }

  function paintKeypad() {
    const host = $('keypad');
    host.innerHTML = '';
    for (let key = 1; key <= P.KEYS; key++) {
      const target = lib.keymap[key - 1];
      const who = lib.slots[target];
      const b = el('button', 'key');
      b.append(el('span', 'n', String(key)),
               el('span', 'who', who ? who.name : `${target} · empty`));
      b.title = (who ? `Key ${key} plays slot ${target}, "${who.name}". ` : `Key ${key} plays slot ${target}, which is empty. `)
              + (slot === null ? 'Save this patch into a slot to put it on a key.'
                               : `Click to put slot ${slot} here.`);
      b.addEventListener('click', () => {
        if (slot === null) { say('save the patch into a slot first', 'bad'); return; }
        lib.keymap[key - 1] = slot;
        save(); paintList();
        say(`key ${key} is now slot ${slot}, "${lib.slots[slot].name}"`);
      });
      host.appendChild(b);
    }
  }

  const leaveDraft = () => !draft
    || confirm(`Discard your changes to "${draft.name}"?`);

  function show(s, newDraft) {
    slot = s;
    draft = newDraft || null;
    bypassed.clear();
    bypassedCards.clear();
    stopRun();
    audition.pos = 1;
    audition.held = false;
    audition.from = s;
    audition.to = null;
    delete $('saveSlot').dataset.touched;
    $('saveSlot').value = '';
    save();
    buildAudition();
    buildSurfaces();
    paint();
    paintList();
    arrive();
  }

  function selectPatch(s) {
    if (s === slot || !leaveDraft()) return;
    show(s);
  }

  const showFirstOrNew = () => {
    const first = firstFilled();
    if (first === null) show(null, L.newPatch('untitled'));
    else show(first);
  };

  // ---- brain and files ---------------------------------------------------

  function say(text, cls) {
    const at = new Date().toLocaleTimeString('en-GB');
    $('log').insertAdjacentHTML('beforeend',
      `<span class="cc">${at}</span>  <span class="${cls || ''}">${text}</span>\n`);
    $('log').scrollTop = $('log').scrollHeight;
  }

  const guard = fn => async () => {
    try { await fn(); } catch (e) { say(e.message, 'bad'); }
  };

  function download(filename, text) {
    const url = URL.createObjectURL(new Blob([text], { type: 'application/json' }));
    const a = document.createElement('a');
    a.href = url; a.download = filename; a.click();
    URL.revokeObjectURL(url);
  }

  function wireBrain() {
    $('brainAsk').addEventListener('click', guard(async () => {
      const info = await link.queryLibrary();
      say(`protocol ${info.protocol}, patch format ${info.format}`);
      say(`${info.stateText} — ${info.count} patches`, info.state === 0 ? 'ok' : 'warn');
      if (info.count) say(`slots: ${info.slots.join(' ')}`);
      say(`keypad: ${info.keymap.join(' ')}`);
    }));

    $('brainPush').addEventListener('click', guard(async () => {
      const wire = L.libToWire(lib);
      const fault = L.validate(wire);
      if (fault) { say(fault, 'bad'); return; }
      say(`pushing ${wire.patches.length} patches${draft ? ' \u2014 not the unsaved draft' : ''}`);
      const started = performance.now();
      const r = await link.push(wire);
      if (r.ok) say(`stored ${r.count} patches in ${Math.round(performance.now() - started)} ms`, 'ok');
      else say(`${r.where}: ${L.STATUS[r.status] || r.status}`, 'bad');
    }));

    $('brainPull').addEventListener('click', guard(async () => {
      say('reading the brain back');
      const r = await link.pull((n, of) => { if (n === of) say(`read ${n} patches`); });
      if (r.error) { say(r.error, 'bad'); return; }
      if (!leaveDraft()) return;
      lib = L.libFromWire(r.lib);
      setIndex = P.SET_BASE;
      showFirstOrNew();
      say(`the editor now holds what the brain holds — ${r.lib.patches.length} patches`, 'ok');
    }));

    $('fileSave').addEventListener('click', () => {
      const stamp = new Date().toISOString().slice(0, 10);
      download(`aurora-library-${stamp}.json`, L.serialize(L.libToWire(lib)));
      say(`saved ${L.filledSlots(lib).length} patches to a file${draft ? ' \u2014 not the unsaved draft' : ''}`, 'ok');
    });

    $('fileLoad').addEventListener('click', () => $('filePick').click());
    $('filePick').addEventListener('change', guard(async () => {
      const file = $('filePick').files && $('filePick').files[0];
      $('filePick').value = '';
      if (!file) return;
      let loaded;
      try { loaded = JSON.parse(await file.text()); }
      catch { say(`${file.name} is not readable JSON`, 'bad'); return; }
      const fault = L.validate(loaded);
      if (fault) { say(`${file.name}: ${fault}`, 'bad'); return; }
      if (!leaveDraft()) return;
      lib = L.libFromWire(loaded);
      setIndex = P.SET_BASE;
      showFirstOrNew();
      say(`loaded ${loaded.patches.length} patches from ${file.name}`, 'ok');
    }));
  }

  function wireLibraryButtons() {
    $('patchSave').addEventListener('click', () => {
      lib.slots[slot] = draft;
      draft = null;
      save(); paintList();
      say(`saved "${lib.slots[slot].name}" in slot ${slot}`, 'ok');
    });
    $('patchDiscard').addEventListener('click', () => {
      if (!confirm(`Discard your changes to "${draft.name}"?`)) return;
      draft = null;
      if (slot === null) showFirstOrNew();
      else show(slot);
    });
    $('saveSlot').addEventListener('input', () => {
      $('saveSlot').dataset.touched = '1';
      paintSaveTarget();
    });
    $('saveHere').addEventListener('click', () => {
      const n = +$('saveSlot').value;
      const there = lib.slots[n];
      if (n !== slot && there && !confirm(`Slot ${n} holds "${there.name}". Replace it?`)) return;
      lib.slots[n] = L.clonePatch(patch());
      slot = n;
      draft = null;
      delete $('saveSlot').dataset.touched;
      save(); paint(); paintList();
      say(`saved "${lib.slots[n].name}" in slot ${n}`, 'ok');
    });
    $('patchNew').addEventListener('click', () => {
      if (!leaveDraft()) return;
      show(null, L.newPatch('untitled'));
    });
    $('patchDel').addEventListener('click', () => {
      const p = lib.slots[slot];
      if (!confirm(`Empty slot ${slot}, "${p.name}"? Keypad keys on it stay on the empty slot.`)) return;
      lib.slots[slot] = null;
      draft = null;
      showFirstOrNew();
    });
  }

  // ---- clock -------------------------------------------------------------

  let clockOn = false, clockTimer = null, nextTickAt = 0, startedAt = performance.now();
  const LOOKAHEAD_MS = 250, SCHEDULE_EVERY_MS = 100;
  const bpm = () => +$('bpm').value || 120;

  function pumpClock() {
    const msPerTick = 60000 / bpm() / 24;
    const now = performance.now();
    if (nextTickAt < now) nextTickAt = now;
    while (nextTickAt < now + LOOKAHEAD_MS) {
      link.sendRaw([0xF8], nextTickAt);
      nextTickAt += msPerTick;
    }
  }

  // Starting the clock sends a transport Start, which puts the brain's musical
  // position back to zero, and restarts the preview's at the same moment.
  // Stopping sends nothing: a Stop would freeze the brain at its phase, and
  // going quiet instead lets it notice the silence and free-run, which is what
  // it does on stage when a cable is pulled.
  function setClock(on) {
    clockOn = on;
    $('clockToggle').classList.toggle('on', on);
    if (on) {
      link.sendRaw([0xFA]);
      startedAt = performance.now();
      nextTickAt = performance.now();
      pumpClock();
      clockTimer = setInterval(pumpClock, SCHEDULE_EVERY_MS);
    } else {
      clearInterval(clockTimer);
      clockTimer = null;
    }
  }

  // ---- the walls ---------------------------------------------------------

  const walls = {};

  function makeWall(canvasId, w, h) {
    const canvas = $(canvasId);
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    canvas.width = w * dpr;
    canvas.height = h * dpr;
    canvas.style.aspectRatio = `${w} / ${h}`;
    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);
    const glow = document.createElement('canvas');
    glow.width = w; glow.height = h;
    return { canvas, ctx, glow, w, h, motion: V.makeMotion(), paths: V.makePaths() };
  }

  // A new patch puts its shapes somewhere else, and a tail left running would
  // streak across to them. The renderer cannot tell that from a fast fader.
  function clearTails() {
    for (const wall of Object.values(walls)) V.clearPaths(wall.paths);
  }

  let flipped = false;
  let showFan = true;

  function order() {
    const parsed = $('pvOrder').value.split(',')
      .map(n => parseInt(n, 10) - 1)
      .filter(n => Number.isInteger(n) && n >= 0 && n < V.STRIPS);
    return parsed.length === V.STRIPS ? parsed : V.WALL_STRIP_ORDER.map(n => n - 1);
  }

  // Every slider's track is painted here rather than by the browser, which
  // draws track and handle as one piece: anything laid over the slider would
  // cover the handle. In the track, the routes' band and marks sit under it,
  // and a mark resting at the dialed value hides behind the handle until the
  // route moves it.
  //
  // The band is as far as the routes can reach a control, and each mark is
  // where one strip has it this frame. The marks sit on top of one another
  // unless the fan spreads the strips' LFO phases.
  const HANDLE_RADIUS = 8;

  function along(input, v) {
    const min = input.min === '' ? 0 : +input.min;
    const max = input.max === '' ? 100 : +input.max;
    return `calc(${HANDLE_RADIUS}px + ${(v - min) / (max - min)} * (100% - ${2 * HANDLE_RADIUS}px))`;
  }

  // A value at either end reaches the end of the track, past where the
  // handle's center stops, or a band to the limit looks as if it falls short.
  function reaching(input, v) {
    if (v <= (input.min === '' ? 0 : +input.min)) return '0%';
    if (v >= (input.max === '' ? 100 : +input.max)) return '100%';
    return along(input, v);
  }

  const stops = (...pairs) => 'linear-gradient(90deg, '
    + pairs.map(([color, from, to]) => `${color} ${from}, ${color} ${to}`).join(', ') + ')';

  function pointLayer(at) {
    const off = px => `calc(${at} + ${px}px)`;
    return stops(['transparent', '0%', off(-1)], ['var(--bg)', off(-1), off(1)],
                 ['transparent', off(1), '100%']);
  }

  // A stepped control's notches sit in the middle of each step's run of
  // bytes, the safest place to land it. The two end steps are the track's ends.
  function stepPoints(valueOf) {
    const points = [];
    let start = 0;
    for (let v = 1; v <= 128; v++) {
      if (v < 128 && valueOf(v) === valueOf(start)) continue;
      if (start > 0 && v < 128) points.push(Math.round((start + v - 1) / 2));
      start = v;
    }
    return points;
  }

  // The values worth a notch in the track: the center of a control that
  // departs both ways from it, read off the renderer as the byte where its
  // value changes sign; the named shapes on a route's wave and the quarter
  // turns of its phase; and the steps of a stepped control.
  function pointsFor(name) {
    const route = P.ROUTES.find(r => [r.amount, r.ratio, r.wave, r.phase].includes(name));
    if (route) {
      if (name === route.wave) return [A.GEN_WAVE_SWELL, A.GEN_WAVE_SAW_DOWN, A.GEN_WAVE_SQUARE];
      if (name === route.phase) return [32, 64, 96];
      if (name === route.ratio) return stepPoints(A.routeRatio);
      return [64];
    }
    if (name === 'genBendAt') return [64];
    if (name === 'washLfoSpread') {
      const at = v => V.convert(P.CC[name], v);
      return stepPoints(at).filter(v => at(v) % 0.25 === 0);
    }
    if (name === 'genLfoRate' || name === 'genFanFreq' || name === 'washHuePeriod') {
      return stepPoints(v => V.convert(P.CC[name], v));
    }
    const cc = P.CC[name];
    if (cc === undefined || !(A.TAGS[name] || []).includes('patch')) return [];
    return V.convert(cc, 56) < 0 && V.convert(cc, 64) === 0 && V.convert(cc, 72) > 0 ? [64] : [];
  }

  function markLayer(at) {
    const off = px => `calc(${at} + ${px}px)`;
    return stops(['transparent', '0%', off(-2)], ['var(--bg)', off(-2), off(-1)],
                 ['#fff', off(-1), off(1)], ['var(--bg)', off(1), off(2)],
                 ['transparent', off(2), '100%']);
  }

  // The washes' own three, which each lamp reads at its own shift of the LFO.
  const READ_PER_WASH = new Set(['washLevel', 'washHueOffset', 'washSaturation']);

  function swingOf(name) {
    const cc = P.CC[name];
    const reach = cc === undefined ? null : V.routeReach(cc);
    if (!reach) return null;
    // A circular control's band runs off one end and comes back at the other.
    const [low, high] = reach;
    const spans = [[Math.max(0, low), Math.min(127, high)]];
    if (low < 0) spans.push([low + 128, 127]);
    if (high > 127) spans.push([0, high - 128]);
    return { spans, marks: READ_PER_WASH.has(name) ? V.washValues(cc) : V.stripValues(cc) };
  }

  // A fill from zero up to the handle means nothing where 0 and 127 are the
  // same place, and a route band wrapping round the end leaves a piece of it
  // showing as if it were a range.
  function paintTrack(input, swing, points, circular) {
    const layers = [];
    if (swing) for (const v of swing.marks) layers.push(markLayer(along(input, v)));
    for (const v of points) layers.push(pointLayer(along(input, v)));
    if (swing) {
      for (const [low, high] of swing.spans) {
        const from = reaching(input, low), to = reaching(input, high);
        layers.push(stops(['transparent', '0%', from], ['var(--lfo)', from, to],
                          ['transparent', to, '100%']));
      }
    }
    const at = reaching(input, +input.value);
    layers.push(circular ? stops(['var(--rest)', '0%', '100%'])
                         : stops(['var(--fill)', '0%', at], ['var(--rest)', at, '100%']));
    const track = layers.join(', ');
    if (input.dataset.track !== track) {
      input.dataset.track = track;
      input.style.setProperty('--track', track);
    }
  }

  // Routes are read off the last render, so only a frame the generator drew
  // has any to show.
  function paintTracks() {
    for (const input of document.querySelectorAll('input[type=range]')) {
      const r = rows[input.closest('.row')?.dataset.name];
      paintTrack(input, r ? swingOf(r.def.name) : null, r ? r.points : [],
                 r ? (A.TAGS[r.def.name] || []).includes('circular') : false);
    }
  }

  // The overlay goes on the big wall only. On a small one the five dots land
  // within a few pixels of each other and report nothing.
  function drawOne(wall, named, quarterNotes) {
    const frame = V.render(L.setFromNamed(sounding(named)), quarterNotes, wall.motion, wall.paths);
    V.draw(wall.ctx, wall.glow, frame, order(), flipped, wall.w, wall.h,
           showFan && wall === walls.main);
  }

  // The two small walls are drawn on the big wall's clock, not on clocks of
  // their own.
  //
  // A phase here carries an offset so that moving a rate does not teleport the
  // wall — the thing that makes Speed usable with a fader. The cost is that
  // the offset is a history: two walls that have seen different rate changes
  // sit a constant distance apart for ever, which is what made the small wall
  // read as exactly off-phase from the big one the moment Speed was touched.
  //
  // So each small wall renders from a copy of the big wall's phases, taken
  // fresh every frame, which puts all three at the same instant. What that
  // gives up is that a far end differing only in a rate looks identical in the
  // still: you see that difference by running the audition, which is what it
  // is for.
  const beatDots = [...document.querySelectorAll('#beats i')];
  const BEAT_FLASH = 0.15;

  function paintBeats(quarterNotes) {
    const beat = Math.floor(quarterNotes);
    const hit = quarterNotes - beat < BEAT_FLASH;
    beatDots.forEach((dot, i) => {
      const on = i === beat % beatDots.length;
      dot.classList.toggle('on', on);
      dot.classList.toggle('hit', on && hit);
    });
  }

  function frame() {
    const quarterNotes = ((performance.now() - startedAt) / 60000) * bpm();
    paintBeats(quarterNotes);
    drawOne(walls.main, liveNamed(), quarterNotes);
    paintTracks();
    if (isFarEnd()) {
      V.copyMotion(walls.base.motion, walls.main.motion);
      V.copyMotion(walls.far.motion, walls.main.motion);
      drawOne(walls.base, L.namedFromSet(baseSet()), quarterNotes);
      drawOne(walls.far, L.blend(baseSet(), L.materialize(patch(), setIndex), 1, switchSource()),
              quarterNotes);
    }
    requestAnimationFrame(frame);
  }

  // ---- midi in/out -------------------------------------------------------

  async function initMidi() {
    const state = $('midiState');
    let ports;
    try { ports = await link.open(); }
    catch (e) { state.textContent = e.message; return; }

    const sel = $('port');
    const refresh = () => {
      const outs = link.ports();
      sel.innerHTML = '';
      outs.forEach(o => {
        const opt = el('option', null, o.name); opt.value = o.id; sel.appendChild(opt);
      });
      if (!outs.length) {
        state.textContent = 'no MIDI outputs'; state.className = 'bad';
        return;
      }
      const preferred = link.preferred();
      sel.value = preferred.id;
      choose(preferred.id);
    };
    const choose = id => {
      const { output, input } = link.choose(id);
      state.textContent = input ? 'connected' : `${output.name} — no reply port`;
      state.className = input ? 'ok' : 'warn';
      sendLive(true);
    };
    sel.addEventListener('change', () => choose(sel.value));
    link.onports = refresh;
    refresh();
  }

  // ---- go ----------------------------------------------------------------

  V.ready.then(() => {
    buildShape();
    buildModulators();
    buildRoutePanel();
    buildOutputs();
    buildTabs();
    buildHead();
    wireBrain();
    wireLibraryButtons();

    $('pvOrder').value = V.WALL_STRIP_ORDER.join(',');
    $('pvFan').classList.toggle('on', showFan);
    $('pvFan').addEventListener('click', () => {
      showFan = !showFan;
      $('pvFan').classList.toggle('on', showFan);
    });
    $('pvFlip').addEventListener('click', () => {
      flipped = !flipped;
      $('pvFlip').textContent = flipped ? 'pixel 0 at top' : 'pixel 0 at bottom';
    });
    $('clockToggle').addEventListener('click', () => setClock(!clockOn));
    $('sendPatch').addEventListener('click', sendPatchToWall);
    $('rigBlackout').addEventListener('click', () => { link.sendPC(0); say('PC 0 — blackout'); });
    $('rigOrder').addEventListener('click', () => { link.sendPC(11); say('PC 11 — strip order'); });

    // The header wraps at narrow widths, so how far down the rails have to sit
    // is a measurement rather than a number.
    const topbar = $('topbar');
    const measureTop = () => document.documentElement.style
      .setProperty('--topbar', Math.round(topbar.getBoundingClientRect().height) + 'px');
    new ResizeObserver(measureTop).observe(topbar);
    measureTop();

    walls.main = makeWall('wallMain', 300, 480);
    walls.base = makeWall('wallBase', 150, 240);
    walls.far = makeWall('wallFar', 150, 240);

    audition.from = slot;
    buildAudition();
    buildSurfaces();
    paint();
    paintList();
    requestAnimationFrame(frame);
    initMidi();
    say('ready — ask the brain what it holds to check the link');
  });
})();
