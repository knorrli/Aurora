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

  let lib = loadStored() || L.newLibrary();
  let patchIndex = 0;
  let setIndex = P.SET_BASE;

  const audition = { pos: 1, raf: null, seconds: 2, loop: false, from: null, to: null };
  const link = new L.Link();
  const rows = {};
  const lastSent = new Array(P.CC_COUNT).fill(-1);

  const patch = () => lib.patches[patchIndex];
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
    try { localStorage.setItem(STORE, JSON.stringify(L.libToWire(lib))); } catch {}
  }

  // ---- what is live ------------------------------------------------------

  // An accent plays with the SOURCE patch's switches, because the
  // destination's have not landed yet and will not until the key is released
  // — DESIGN.md § "Switches belong to the patch". So an accent dialed against
  // its own switches is judged on a picture it will rarely show, and this is
  // the picker that fixes it.
  function switchSource() {
    if (setIndex === P.SET_ACCENT && audition.from != null && lib.patches[audition.from]) {
      return lib.patches[audition.from].base;
    }
    return baseSet();
  }

  function liveNamed() {
    const p = patch();
    if (!isFarEnd()) {
      const dest = audition.to != null ? lib.patches[audition.to] : null;
      if (dest && audition.pos < 1) {
        return L.blend(p.base, dest.base, audition.pos, p.base);
      }
      return L.namedFromSet(p.base);
    }
    return L.blend(p.base, L.materialize(p, setIndex), audition.pos, switchSource());
  }

  // On the base you are setting the patch. On a far end you are overriding it,
  // and an override that lands back on the base value is not an override — it
  // is the far end following again, which is what makes the change count mean
  // something.
  function setValue(name, value) {
    const p = patch();
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
    const over = patch().overrides[setIndex];
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
    const live = liveNamed();
    for (const name of P.NAMES) {
      const cc = P.CC[name], v = live[name];
      if (!force && lastSent[cc] === v) continue;
      lastSent[cc] = v;
      link.sendCC(cc, v);
    }
  }

  function sendPatchToWall() {
    link.sendPC(patch().pattern);
    sendLive(true);
    say(`sent "${patch().name}" on PC ${patch().pattern}`);
  }

  // ---- control rows ------------------------------------------------------

  function faderRow(host, name) {
    const def = P.CONTROLS[name];
    const root = el('div', 'row');
    root.dataset.name = name;

    const label = el('label', null, `<b>${def.label}</b><span class="cc">CC ${P.CC[name]}</span>`);
    label.addEventListener('click', () => { snap(); resetNames([name]); });
    const reached = el('span', 'reached');
    label.appendChild(reached);

    const track = el('div', 'track');
    const slider = el('input');
    slider.type = 'range'; slider.min = 0; slider.max = 127; slider.step = 1;
    const ghost = el('i', 'ghost');
    track.append(slider, ghost);

    const out = el('output');
    slider.addEventListener('pointerdown', snap);
    slider.addEventListener('input', () => setValue(name, +slider.value));

    root.append(label, track, out);
    host.appendChild(root);
    rows[name] = { root, slider, ghost, out, reached, label, def, kind: 'fader' };
  }

  function pickRow(host, name) {
    const def = P.CONTROLS[name];
    const root = el('div', 'swrow');
    root.dataset.name = name;
    const label = el('label', null, `<b>${def.label}</b><span class="cc">CC ${P.CC[name]}</span>`);
    label.title = def.hint;
    const picks = el('div', 'picks');
    const buttons = def.options.map(([value, text]) => {
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

    const lit = def.kind === 'three'
      ? (value, live) => P.band3(live) === P.band3(value)
      : (value, live) => P.isOn(live) === P.isOn(value);
    rows[name] = { root, buttons, from, def, kind: def.kind, lit };
  }

  function controlRow(host, name) {
    const def = P.CONTROLS[name];
    if (def.kind === 'fader') faderRow(host, name);
    else pickRow(host, name);
  }

  const buildRows = (host, names) => names.forEach(n => controlRow(host, n));

  // ---- painting ----------------------------------------------------------

  const isNeutral = (name, v) => v === P.NEUTRAL[name];

  // The escape from putting the amounts at the source: a marker beside the
  // target saying which sources reach it and how hard.
  const REACHED_CONTROL = {
    width: 'width', hue: 'hue',
    parLevel: 'washLevel', parHue: 'washHueOffset', parSat: 'washSaturation',
  };

  // Brightness and Lit White rest at the bottom of their travel rather than
  // in the middle, so there is no sign to read off them.
  const amountFraction = (name, v) => (P.NEUTRAL[name] === 0 ? P.unit(v) : P.bip(v));

  function reachText(destKey, live) {
    const dest = P.DESTINATIONS.find(d => d.key === destKey);
    if (!dest) return '';
    const parts = [];
    for (const [source, amount] of Object.entries(dest.from)) {
      const v = live[amount];
      if (isNeutral(amount, v)) continue;
      const r = amountFraction(amount, v);
      parts.push(`${source} ${r >= 0 ? '+' : '−'}${Math.round(Math.abs(r) * 100)}%`);
    }
    return parts.length ? '← ' + parts.join(' · ') : '';
  }

  function paint() {
    const live = liveNamed();
    const base = L.namedFromSet(baseSet());
    const moved = overrides();
    const farEnd = isFarEnd();

    for (const name of Object.keys(rows)) {
      const r = rows[name];
      const value = live[name];

      if (r.kind === 'fader') {
        if (+r.slider.value !== value) r.slider.value = value;
        const derived = P.DERIVED[name] ? P.DERIVED[name](value) : '';
        r.out.innerHTML = `<b>${value}</b><span class="cc">${derived}</span>`;
        const overridden = farEnd && name in moved;
        r.root.classList.toggle('changed', overridden);
        if (overridden) r.ghost.style.left = `calc(${base[name] / 127 * 100}% - 1px)`;
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
        r.root.classList.toggle('locked', farEnd);
        r.from.textContent = farEnd
          ? (setIndex === P.SET_ACCENT && audition.from != null && audition.from !== patchIndex
              ? `held at "${lib.patches[audition.from].name}"` : 'from the patch')
          : '';
      }
    }

    for (const [target, destKey] of Object.entries(
      { width: 'width', hue: 'hue', washLevel: 'parLevel',
        washHueOffset: 'parHue', washSaturation: 'parSat' })) {
      const r = rows[target];
      if (r && r.reached) r.reached.textContent = reachText(destKey, live);
    }

    paintTabs();
    paintMatrix(live);
    paintPulseWaves(live);
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
    if (audition.pos < 1) {
      label.textContent = Math.round(audition.pos * 100) + '% of the way';
    } else if (isFarEnd()) {
      label.textContent = P.SET_NAMES[setIndex] + ' \u2014 the far end';
    } else {
      label.textContent = 'the patch';
    }
  }

  // ---- the surface -------------------------------------------------------

  function buildLanes() {
    for (const lane of P.LANES) {
      const host = $(lane.key === 'shape' ? 'laneShape' : 'laneColor');
      host.innerHTML = '';
      const head = el('div', 'lane-head');
      head.append(el('span', 'lane-name', lane.name), el('span', 'lane-does', lane.does));
      host.appendChild(head);

      const groups = el('div', 'groups');
      for (const g of lane.groups) {
        const box = el('div');
        const h = el('h3', null, g.title);
        const reset = el('button', 'reset tiny', 'reset');
        reset.style.marginLeft = 'auto';
        reset.addEventListener('click', () =>
          resetNames([...(g.controls || []), ...(g.switches || [])]));
        h.appendChild(reset);
        box.appendChild(h);
        const body = el('div');
        buildRows(body, g.controls || []);
        if (g.switches) buildRows(body, g.switches);
        box.appendChild(body);
        if (g.note) box.appendChild(el('p', 'note', g.note));
        groups.appendChild(box);
      }
      host.appendChild(groups);
    }
  }

  const destCards = {};

  function buildModulators() {
    const host = $('mods');
    host.innerHTML = '';
    for (const mod of P.MODULATORS) {
      const card = el('article', 'mod');
      card.dataset.tone = mod.tone;

      const head = el('div', 'mod-head');
      head.append(el('span', 'mod-name', mod.name), el('span', 'mod-when', mod.when));
      if (mod.unbuilt) head.appendChild(el('span', 'unbuilt', 'no firmware yet'));
      const reset = el('button', 'tiny', 'reset');
      reset.addEventListener('click', () => resetNames([
        ...(mod.source || []), ...(mod.amounts || []), ...(mod.switches || []),
        ...(mod.dests || []).flatMap(d => [d.amount, d.shape, d.skew]),
      ]));
      head.appendChild(reset);
      card.appendChild(head);

      const body = el('div', 'mod-body');

      const sourceBox = el('div');
      sourceBox.appendChild(el('h4', null, 'Source'));
      if (mod.switches) buildRows(sourceBox, mod.switches);
      if (mod.source && mod.source.length) buildRows(sourceBox, mod.source);
      if (!mod.switches && (!mod.source || !mod.source.length)) {
        sourceBox.appendChild(el('p', 'note',
          'Nothing to set. Its value is what the shape lane left, so the shape lane is its control.'));
      }
      body.appendChild(sourceBox);

      if (mod.amounts) {
        const amountBox = el('div');
        amountBox.appendChild(el('h4', null, 'Amounts'));
        buildRows(amountBox, mod.amounts);
        body.appendChild(amountBox);
      } else {
        body.style.gridTemplateColumns = '1fr';
      }
      card.appendChild(body);

      if (mod.dests) {
        const destBox = el('div');
        destBox.appendChild(el('h4', null, 'Destinations'));
        const grid = el('div', 'dests');
        for (const d of mod.dests) {
          const cell = el('div', 'dest');
          const dh = el('div', 'dest-head');
          dh.append(el('span', 'dest-name', d.name), el('span', 'dest-where', d.where));
          cell.appendChild(dh);
          const body2 = el('div');
          buildRows(body2, [d.amount, d.shape, d.skew]);
          cell.appendChild(body2);
          const canvas = el('canvas', 'destwave');
          cell.appendChild(canvas);
          cell.appendChild(el('p', 'note', d.note));
          grid.appendChild(cell);
          destCards[d.key] = { cell, canvas, dest: d };
        }
        destBox.appendChild(grid);
        card.appendChild(destBox);
      }

      if (mod.unbuilt) card.appendChild(el('p', 'note warn', mod.unbuilt));
      card.appendChild(el('p', 'note', mod.note));
      host.appendChild(card);
    }
  }

  // A starting point writes into whichever set is on screen, which is what
  // makes it useful on a far end too: land Strobe on the Motion tab and every
  // control it names becomes an override in one click.
  function buildStarts() {
    const add = (hostId, table) => {
      const host = $(hostId);
      host.innerHTML = '';
      for (const [name, values] of Object.entries(table)) {
        const b = el('button', null, name);
        b.addEventListener('click', () => { snap(); applyNamed(values); });
        host.appendChild(b);
      }
    };
    add('shapeStarts', P.ANCHORS);
    add('colorStarts', P.COLOR_LOOKS);
  }

  function buildOutputs() {
    const host = $('parControls');
    host.innerHTML = '';
    buildRows(host, P.PARS.controls);
    $('parNote').textContent = P.PARS.note;

    $('timingGroup').innerHTML = '';
    buildRows($('timingGroup'), P.TIMING.controls);
    $('timingNote').textContent = P.TIMING.note;

    $('slotGroup').innerHTML = '';
    buildRows($('slotGroup'), P.SLOTS.controls);
    $('slotNote').textContent = P.SLOTS.note;
  }

  // ---- the matrix, read from the target's end ----------------------------

  const matrixCells = {};
  const SOURCE_KEYS = P.MODULATORS.map(m => m.key);

  function buildMatrix() {
    const table = $('matrix');
    table.innerHTML = '';
    const head = el('thead');
    const hr = el('tr');
    hr.appendChild(el('th', null, ''));
    for (const m of P.MODULATORS) hr.appendChild(el('th', null, m.name.replace(/^The /, '')));
    head.appendChild(hr);
    table.appendChild(head);

    const body = el('tbody');
    for (const dest of P.DESTINATIONS) {
      const tr = el('tr');
      tr.appendChild(el('th', null, dest.name));
      for (const key of SOURCE_KEYS) {
        const td = el('td', null, '·');
        const amount = dest.from[key];
        if (amount) {
          td.addEventListener('click', () => {
            if (!rows[amount]) return;
            rows[amount].root.scrollIntoView({ behavior: 'smooth', block: 'center' });
            rows[amount].root.classList.remove('flash');
            void rows[amount].root.offsetWidth;
            rows[amount].root.classList.add('flash');
          });
        }
        tr.appendChild(td);
        matrixCells[dest.key + '/' + key] = { td, amount };
      }
      body.appendChild(tr);
      matrixCells[dest.key + '/row'] = tr;
    }
    table.appendChild(body);
  }

  function paintMatrix(live) {
    for (const dest of P.DESTINATIONS) {
      let any = false;
      for (const key of SOURCE_KEYS) {
        const cell = matrixCells[dest.key + '/' + key];
        if (!cell) continue;
        if (!cell.amount) { cell.td.textContent = ''; continue; }
        const v = live[cell.amount];
        if (isNeutral(cell.amount, v)) {
          cell.td.textContent = '·';
          cell.td.classList.remove('live');
        } else {
          cell.td.textContent = P.DERIVED[cell.amount]
            ? P.DERIVED[cell.amount](v) : String(v);
          cell.td.classList.add('live');
          any = true;
        }
      }
      matrixCells[dest.key + '/row'].classList.toggle('quiet', !any);
    }
  }

  // ---- the pulse's waves -------------------------------------------------

  const WAVE_CYCLES = 2;

  function departureAt(dest, phase, live) {
    const wave = V.pulseWave(phase, live[dest.shape] / 127, P.bip(live[dest.skew]));
    if (dest.key === 'light') return -(live.pulseDepth / 127) * (1 - wave);
    return P.bip(live[dest.amount]) * wave;
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

  function paintPulseWaves(live) {
    for (const key of Object.keys(destCards)) {
      const { cell, canvas, dest } = destCards[key];
      cell.classList.toggle('idle', isNeutral(dest.amount, live[dest.amount]));
      drawWave(canvas, dest, live);
    }
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
    if (audition.from === null) audition.from = patchIndex;
    buildAudition();
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
    if (audition.pos >= 1) return;
    stopRun();
    audition.pos = 1;
    const s = $('auditionScrub');
    if (s) s.value = 1000;
    paint();
    sendLive();
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
    if (audition.pos >= 1) return;
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
      const derived = P.DERIVED[name] ? P.DERIVED[name](value) : '';
      r.out.innerHTML = `<b>${value}</b><span class="cc">${derived}</span>`;
    }
    paintShowing();
    const readout = $('auditionReadout');
    if (readout) {
      readout.textContent = audition.pos <= 0 ? 'base'
        : audition.pos >= 1 ? 'far end' : Math.round(audition.pos * 100) + '%';
    }
    sendLive();
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
    range.addEventListener('input', () => { stopRun(); audition.pos = +range.value / 1000; paintLive(); });
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
    readout.textContent = 'far end';
    host.appendChild(readout);

    if (setIndex === P.SET_ACCENT) {
      const wrap = el('label', 'field');
      wrap.appendChild(el('span', null, 'as heard from'));
      const pick = el('select');
      lib.patches.forEach((p, i) => {
        const o = el('option', null, `${i} · ${p.name}`);
        o.value = i;
        pick.appendChild(o);
      });
      pick.value = audition.from == null ? patchIndex : audition.from;
      pick.addEventListener('change', () => {
        audition.from = +pick.value;
        paint(); sendLive();
      });
      wrap.appendChild(pick);
      host.appendChild(wrap);
      host.appendChild(el('p', 'note', 'An accent plays with the switches of the patch you came from, because the destination’s land on the release and not on arrival. Dial it against the patch it will actually follow.'));
    } else if (!isFarEnd()) {
      const wrap = el('label', 'field');
      wrap.appendChild(el('span', null, 'journey to'));
      const pick = el('select');
      pick.appendChild(el('option', null, '— nowhere —')).value = '';
      lib.patches.forEach((p, i) => {
        if (i === patchIndex) return;
        const o = el('option', null, `${i} · ${p.name}`);
        o.value = i;
        pick.appendChild(o);
      });
      pick.value = audition.to == null ? '' : audition.to;
      pick.addEventListener('change', () => {
        audition.to = pick.value === '' ? null : +pick.value;
        paint(); sendLive();
      });
      wrap.appendChild(pick);
      host.appendChild(wrap);
      host.appendChild(el('p', 'note', 'A patch change, so the switches stay at this patch’s the whole way and land only when the key is let go. Which patch precedes which is compositional: the reveal fires only where the two differ in a switch at all.'));
    } else {
      host.appendChild(el('p', 'note', P.SET_BLURB[setIndex] + '. Hold the scrubber to watch the trip; let go and you are editing the far end again.'));
    }
  }

  // ---- the patch head ----------------------------------------------------

  const PATTERNS = [
    [10, '10 · the generator'], [0, '0 · blackout'],
    [1, '1 · fill / starfield'], [2, '2 · breathe / wave'],
    [3, '3 · plasma / aurora'], [4, '4 · pulse / bars'],
    [5, '5 · sweep / cross'], [6, '6 · rain / storm'],
    [7, '7 · strip / comet'], [8, '8 · strobe / stutter'],
    [9, '9 · chaos / glitch'], [11, '11 · strip order'],
  ];

  function buildHead() {
    const pattern = $('pPattern');
    PATTERNS.forEach(([v, text]) => {
      const o = el('option', null, text); o.value = v; pattern.appendChild(o);
    });
    pattern.addEventListener('change', () => {
      patch().pattern = +pattern.value; save(); link.sendPC(patch().pattern); paint();
    });

    const palette = $('pPalette');
    for (let i = 0; i < 9; i++) {
      const o = el('option', null, 'palette ' + i); o.value = i; palette.appendChild(o);
    }
    palette.title = 'Carried, stored, and read by nothing — what a palette is has not been settled.';
    palette.addEventListener('change', () => { patch().palette = +palette.value; save(); });

    for (const [id, field] of [['pRampJourney', 'rampJourney'], ['pRampAccent', 'rampAccent']]) {
      const sel = $(id);
      P.PULSE_PERIOD_NAMES.forEach((text, step) => {
        const o = el('option', null, text); o.value = P.periodByte(step); sel.appendChild(o);
      });
      sel.title = 'Stepped to values that come back to the grid, so holding through a completed journey arrives on a beat.';
      sel.addEventListener('change', () => { patch()[field] = +sel.value; save(); });
    }

    $('pName').addEventListener('input', () => {
      patch().name = $('pName').value.slice(0, P.NAME_LEN);
      save(); paintList();
    });
  }

  function paintHead() {
    const p = patch();
    if ($('pName').value !== p.name) $('pName').value = p.name;
    $('pPattern').value = p.pattern;
    $('pPalette').value = p.palette;
    $('pRampJourney').value = String(P.periodByte(P.periodStep(p.rampJourney)));
    $('pRampAccent').value = String(P.periodByte(P.periodStep(p.rampAccent)));
  }

  // ---- the library rail --------------------------------------------------

  function paintList() {
    const host = $('patchList');
    host.innerHTML = '';
    lib.patches.forEach((p, i) => {
      const keys = lib.keymap.map((k, n) => k === i ? n + 1 : null).filter(Boolean);
      const b = el('button', 'item' + (i === patchIndex ? ' on' : ''));
      b.append(
        el('span', 'pc', String(i)),
        el('span', null, p.name || '(unnamed)'),
        el('span', 'keys', keys.length ? 'key ' + keys.join(',') : ''));
      b.addEventListener('click', () => selectPatch(i));
      host.appendChild(b);
    });
    $('libCount').textContent =
      `${lib.patches.length} of ${P.PATCH_MAX}. The index is the Program Change that names it.`;
    paintKeypad();
  }

  function paintKeypad() {
    const host = $('keypad');
    host.innerHTML = '';
    for (let key = 1; key <= P.KEYS; key++) {
      const who = lib.patches[lib.keymap[key - 1]];
      const b = el('button', 'key');
      b.append(el('span', 'n', String(key)),
               el('span', 'who', who ? who.name : '—'));
      b.title = `Put the selected patch on key ${key}`;
      b.addEventListener('click', () => {
        lib.keymap[key - 1] = patchIndex;
        save(); paintList();
        say(`key ${key} is now "${patch().name}"`);
      });
      host.appendChild(b);
    }
    const zero = el('button', 'key zero');
    zero.append(el('span', 'n', '0'), el('span', 'who', 'blackout — an override, not a patch'));
    host.appendChild(zero);
  }

  function selectPatch(i) {
    patchIndex = i;
    stopRun();
    audition.pos = 1;
    audition.from = i;
    audition.to = null;
    buildAudition();
    paint();
    paintList();
    link.sendPC(patch().pattern);
    sendLive(true);
  }

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
      say(`keypad: ${info.keymap.join(' ')}`);
    }));

    $('brainPush').addEventListener('click', guard(async () => {
      const wire = L.libToWire(lib);
      const fault = L.validate(wire);
      if (fault) { say(fault, 'bad'); return; }
      say(`pushing ${lib.patches.length} patches`);
      const started = performance.now();
      const r = await link.push(wire);
      if (r.ok) say(`stored ${r.count} patches in ${Math.round(performance.now() - started)} ms`, 'ok');
      else say(`${r.where}: ${L.STATUS[r.status] || r.status}`, 'bad');
    }));

    $('brainPull').addEventListener('click', guard(async () => {
      say('reading the brain back');
      const r = await link.pull((n, of) => { if (n === of) say(`read ${n} patches`); });
      if (r.error) { say(r.error, 'bad'); return; }
      lib = L.libFromWire(r.lib);
      patchIndex = 0; setIndex = P.SET_BASE;
      save(); buildAudition(); paint(); paintList();
      say(`the editor now holds what the brain holds — ${lib.patches.length} patches`, 'ok');
    }));

    $('fileSave').addEventListener('click', () => {
      const stamp = new Date().toISOString().slice(0, 10);
      download(`aurora-library-${stamp}.json`, L.serialize(L.libToWire(lib)));
      say(`saved ${lib.patches.length} patches to a file`, 'ok');
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
      lib = L.libFromWire(loaded);
      patchIndex = 0; setIndex = P.SET_BASE;
      save(); buildAudition(); paint(); paintList();
      say(`loaded ${lib.patches.length} patches from ${file.name}`, 'ok');
    }));
  }

  function wireLibraryButtons() {
    $('patchNew').addEventListener('click', () => {
      if (lib.patches.length >= P.PATCH_MAX) { say('the brain holds 128', 'bad'); return; }
      lib.patches.push(L.newPatch('patch ' + lib.patches.length));
      save(); selectPatch(lib.patches.length - 1);
    });
    $('patchDup').addEventListener('click', () => {
      if (lib.patches.length >= P.PATCH_MAX) { say('the brain holds 128', 'bad'); return; }
      const copy = L.clonePatch(patch());
      copy.name = (copy.name + ' 2').slice(0, P.NAME_LEN);
      lib.patches.splice(patchIndex + 1, 0, copy);
      shiftKeymap(patchIndex + 1, +1);
      save(); selectPatch(patchIndex + 1);
    });
    $('patchDel').addEventListener('click', () => {
      if (lib.patches.length === 1) { say('a library needs one patch', 'bad'); return; }
      if (!confirm(`Delete "${patch().name}"? The keypad keys pointing at it fall back to patch 0.`)) return;
      lib.patches.splice(patchIndex, 1);
      lib.keymap = lib.keymap.map(k => k === patchIndex ? 0 : k > patchIndex ? k - 1 : k);
      save(); selectPatch(Math.max(0, patchIndex - 1));
    });
    $('patchUp').addEventListener('click', () => movePatch(-1));
    $('patchDown').addEventListener('click', () => movePatch(+1));
  }

  const shiftKeymap = (from, by) =>
    lib.keymap = lib.keymap.map(k => k >= from ? k + by : k);

  // The index is the Program Change that names a patch, so moving one moves
  // what a DAW's automation lane points at. The keymap follows it here; a
  // Mainstage set does not.
  function movePatch(by) {
    const to = patchIndex + by;
    if (to < 0 || to >= lib.patches.length) return;
    const [p] = lib.patches.splice(patchIndex, 1);
    lib.patches.splice(to, 0, p);
    lib.keymap = lib.keymap.map(k =>
      k === patchIndex ? to : k === to ? patchIndex : k);
    save();
    selectPatch(to);
    say('moved — a patch’s index is the Program Change that names it', 'warn');
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
    return { canvas, ctx, glow, w, h, motion: V.makeMotion() };
  }

  let flipped = false;

  function order() {
    const parsed = $('pvOrder').value.split(',')
      .map(n => parseInt(n, 10) - 1)
      .filter(n => Number.isInteger(n) && n >= 0 && n < V.STRIPS);
    return parsed.length === V.STRIPS ? parsed : V.WALL_STRIP_ORDER.map(n => n - 1);
  }

  function drawOne(wall, named, beats, pattern) {
    let p = null;
    if (pattern === 11) V.renderStripOrder();
    else if (pattern === 0) V.wall.fill(0);
    else p = V.render(named, beats, wall.motion);
    V.draw(wall.ctx, wall.glow, order(), flipped,
           p ? V.parColor(p) : [0, 0, 0], wall.w, wall.h);
  }

  function frame() {
    const beats = ((performance.now() - startedAt) / 60000) * bpm();
    const pattern = patch().pattern;
    drawOne(walls.main, liveNamed(), beats, pattern);
    if (isFarEnd()) {
      drawOne(walls.base, L.namedFromSet(baseSet()), beats, pattern);
      drawOne(walls.far, L.blend(baseSet(), L.materialize(patch(), setIndex), 1, switchSource()), beats, pattern);
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

  buildLanes();
  buildModulators();
  buildStarts();
  buildOutputs();
  buildMatrix();
  buildTabs();
  buildHead();
  wireBrain();
  wireLibraryButtons();

  $('pvOrder').value = V.WALL_STRIP_ORDER.join(',');
  $('pvFlip').addEventListener('click', () => {
    flipped = !flipped;
    $('pvFlip').textContent = flipped ? 'pixel 0 at top' : 'pixel 0 at bottom';
  });
  $('clockToggle').addEventListener('click', () => setClock(!clockOn));
  $('sendPatch').addEventListener('click', sendPatchToWall);
  $('rigBlackout').addEventListener('click', () => { link.sendPC(0); say('PC 0 — blackout'); });
  $('rigOrder').addEventListener('click', () => { link.sendPC(11); say('PC 11 — strip order'); });

  walls.main = makeWall('wallMain', 300, 480);
  walls.base = makeWall('wallBase', 150, 240);
  walls.far = makeWall('wallFar', 150, 240);

  audition.from = patchIndex;
  buildAudition();
  paint();
  paintList();
  requestAnimationFrame(frame);
  initMidi();
  say('ready — ask the brain what it holds to check the link');
})();
