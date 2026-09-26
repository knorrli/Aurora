(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const Library = global.AuroraLibrary;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { element, byId } = dom;

  const rows = {};
  const cards = [];

  const controlLabel = name => dom.labeled('label', Patch.CONTROLS[name].label, `CC ${Patch.CC[name]}`);

  function faderRow(host, name) {
    const control = Patch.CONTROLS[name];
    const root = element('div', 'row');
    root.dataset.name = name;

    const label = controlLabel(name);
    label.addEventListener('click', () => { Editor.transition.snap(); session.resetNames([name]); });
    const swatch = control.swatch ? element('i', 'swatch') : null;
    if (swatch) label.firstChild.appendChild(swatch);

    const track = element('div', 'track');
    const slider = dom.rangeInput(127);
    const ghost = element('i', 'ghost');
    track.append(slider, ghost);
    slider.addEventListener('pointerdown', () => Editor.transition.snap());
    slider.addEventListener('keydown', () => Editor.transition.snap());
    slider.addEventListener('input', () => session.setValue(name, +slider.value));

    const readout = element('output');
    const now = dom.labeled('span', '', '');
    now.className = 'now';
    const room = dom.labeled('span', '127', longestReadout(name));
    room.className = 'room';
    room.setAttribute('aria-hidden', 'true');
    readout.append(now, room);

    const routes = Patch.routable(name) ? element('button', 'routes-button', '~') : null;
    if (routes) routes.addEventListener('click', () => Editor.routes.toggle(name));

    root.append(label, track, readout, routes || element('span'));
    host.appendChild(root);
    rows[name] = {
      kind: 'fader', control, root, label, slider, ghost, now, routes, swatch,
      points: Patch.pointsFor(name), circular: Patch.isCircular(name),
    };
  }

  function longestReadout(name) {
    const readout = Patch.READOUTS[name];
    let longest = '';
    for (let value = 0; value < 128 && readout; value++) {
      const text = readout(value);
      if (text.length > longest.length) longest = text;
    }
    return longest;
  }

  function pickRow(host, name) {
    const control = Patch.CONTROLS[name];
    const root = element('div', 'switch-row');
    root.dataset.name = name;
    const label = controlLabel(name);
    label.title = control.hint;
    const picks = element('div', 'picks');
    const options = typeof control.options === 'function' ? control.options() : control.options;
    const pick = value => {
      if (root.classList.contains('locked')) return;
      Editor.transition.snap();
      session.setValue(name, value);
    };

    let select = null;
    let buttons = [];
    if (control.kind === 'pick') {
      select = element('select');
      dom.setOptions(select, options, options[0][0]);
      select.addEventListener('change', () => pick(+select.value));
      picks.appendChild(select);
    } else {
      buttons = options.map(([value, text]) => {
        const button = element('button', null, text);
        button.addEventListener('click', () => pick(value));
        picks.appendChild(button);
        return [value, button];
      });
    }
    const from = element('span', 'from');
    picks.appendChild(from);
    root.append(label, picks);
    host.appendChild(root);

    const positionOf = control.kind === 'three' ? Protocol.threeWayPosition
      : control.kind === 'steps' ? control.step : Protocol.isOn;
    const lit = (value, live) => positionOf(live) === positionOf(value);
    rows[name] = { kind: control.kind, control, root, select, buttons, from, lit };
  }

  function buildRows(host, names) {
    for (const name of names) {
      if (Patch.CONTROLS[name].kind === 'fader') faderRow(host, name);
      else pickRow(host, name);
    }
  }

  function resetButton(names) {
    const button = element('button', 'tiny', 'reset');
    button.addEventListener('click', () => session.resetNames(names));
    return button;
  }

  function buildGroup(title, sections) {
    const box = element('div');
    const heading = element('h3', null, title);
    const reset = resetButton(sections.flatMap(([, names]) => names));
    reset.classList.add('push-right');
    heading.appendChild(reset);
    const body = element('div');
    for (const [subheading, names] of sections) {
      if (subheading) body.appendChild(element('h4', 'subhead', subheading));
      buildRows(body, names);
    }
    box.append(heading, body);
    return box;
  }

  function buildCard(card) {
    const root = element('article', 'card');
    root.dataset.tone = card.tone;

    const head = element('div', 'card-head');
    const state = element('span', 'card-state');
    const bypass = element('button', 'tiny', 'bypass');
    bypass.title = 'Hear the patch without this card while you listen. Never saved.';
    bypass.addEventListener('click', () => {
      session.toggleCardBypass(card);
      Editor.refresh();
    });
    head.append(element('span', 'card-name', card.name), state, bypass, resetButton(Patch.cardNames(card)));
    root.appendChild(head);

    const body = element('div', 'card-body');
    for (const [title, names] of [['Source', card.source], ['Amounts', card.amounts]]) {
      if (!names.length) continue;
      const box = element('div');
      box.appendChild(element('h4', null, title));
      buildRows(box, names);
      body.appendChild(box);
    }
    if (body.children.length < 2) body.classList.add('single');
    root.appendChild(body);
    if (card === Patch.LFO) root.appendChild(Editor.routes.buildList());
    cards.push({ card, root, state, bypass });
    return root;
  }

  function build() {
    byId('outputs').replaceChildren(...Patch.OUTPUTS.map(output => buildGroup(output.title, output.sections)));
    byId('lfo').replaceChildren(buildCard(Patch.LFO));
    const shape = element('div', 'groups');
    shape.append(...Patch.SHAPE.groups.map(group => buildGroup(group.title, [[null, group.names]])));
    byId('shape').replaceChildren(element('h2', 'shape-name', Patch.SHAPE.name), shape);
    byId('modulators').replaceChildren(...Patch.MODULATORS.map(buildCard));
  }

  function paintFaderValue(row, value) {
    if (+row.slider.value !== value) row.slider.value = value;
    const readout = Patch.READOUTS[row.control.name];
    dom.fillLabeled(row.now, String(value), readout ? readout(value) : '');
  }

  function paintFaderValues(live) {
    for (const row of Object.values(rows)) {
      if (row.kind === 'fader') paintFaderValue(row, live[row.control.name]);
    }
  }

  function faderTitle(row, overridden, baseValue) {
    const hint = row.control.hint;
    if (!session.isTarget()) return `${hint}\nClick to put it back to ${Patch.NEUTRAL[row.control.name]}`;
    return overridden
      ? `${hint}\nThe patch says ${baseValue}. Click to follow it again.`
      : `${hint}\nFollowing the patch. Move it to override.`;
  }

  function paintFader(row, live, base, overrides) {
    const name = row.control.name;
    paintFaderValue(row, live[name]);
    const overridden = session.isTarget() && name in overrides;
    row.root.classList.toggle('changed', overridden);
    if (overridden) row.ghost.style.left = `calc(${Editor.tracks.along(row.slider, base[name])} - 1px)`;
    row.label.title = faderTitle(row, overridden, base[name]);
    if (row.swatch) row.swatch.style.background = `rgb(${row.control.swatch(live).join(',')})`;
    const inert = !!(row.control.inertWhen && row.control.inertWhen(live));
    row.root.classList.toggle('inert', inert);
    if (row.control.inertWhen) row.root.title = inert ? 'Reaches nothing here — ' + row.control.inertWhy : '';
  }

  function paintPick(row, live) {
    const value = live[row.control.name];
    const target = session.isTarget();
    for (const [option, button] of row.buttons) button.classList.toggle('on', row.lit(option, value));
    if (row.select) {
      if (+row.select.value !== value) row.select.value = value;
      row.select.disabled = target;
    }
    row.root.classList.toggle('locked', target);
    const held = session.heldAt();
    row.from.textContent = !target ? '' : held ? `held at "${held.name}"` : 'from the patch';
  }

  function paintCards(live) {
    for (const { card, root, state, bypass } of cards) {
      const bypassed = session.cardBypassed(card);
      const noEffect = session.hasNoEffect(card, live);
      root.classList.toggle('bypassed', bypassed);
      root.classList.toggle('no-effect', noEffect && !bypassed);
      bypass.classList.toggle('on', bypassed);
      state.textContent = bypassed ? 'bypassed' : noEffect ? 'no effect' : '';
    }
  }

  function paint(live) {
    const base = Library.namedFromBytes(session.patch().base);
    const overrides = session.overrides();
    for (const row of Object.values(rows)) {
      if (row.kind === 'fader') paintFader(row, live, base, overrides);
      else paintPick(row, live);
    }
    paintCards(live);
  }

  Editor.rows = { rows, buildRows, build, paint, paintFaderValues };
})(window);
