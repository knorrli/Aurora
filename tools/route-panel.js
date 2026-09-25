(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const Preview = global.AuroraPreview;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { element } = dom;

  const WAVE_CYCLES = 2;
  const PANEL_MIN_WIDTH = 380;
  const FREE_ON_BASE = 'freed on the base: a route belongs to the whole patch';

  const panel = { root: null, blocks: [], add: null, free: null, target: null };
  const list = { root: null, count: null, lines: [] };

  function departureAt(route, phase, live) {
    const wave = live[route.wave];
    const destination = Protocol.NAME_BY_CC[live[route.destination]];
    const at = Preview.lfoWave(phase * Protocol.routeRatio(live[route.ratio]) - live[route.phase] / 128, wave);
    return Patch.bipolar(live[route.amount]) * (Patch.swings(destination) ? at - Preview.waveMean(wave) : at);
  }

  function drawWave(canvas, route, live) {
    const width = canvas.clientWidth, height = canvas.clientHeight;
    if (!width || !height) return;
    const scale = Math.min(2, window.devicePixelRatio || 1);
    canvas.width = Math.round(width * scale);
    canvas.height = Math.round(height * scale);
    const context = canvas.getContext('2d');
    context.setTransform(scale, 0, 0, scale, 0, 0);
    context.clearRect(0, 0, width, height);

    const padding = 7;
    const y = value => (height / 2) - value * (height / 2 - padding);
    const rail = (value, style, dashed) => {
      context.setLineDash(dashed ? [3, 4] : []);
      context.strokeStyle = style;
      context.lineWidth = 1;
      context.beginPath();
      context.moveTo(0, y(value) + 0.5);
      context.lineTo(width, y(value) + 0.5);
      context.stroke();
      context.setLineDash([]);
    };
    rail(1, 'rgba(255,255,255,.06)');
    rail(-1, 'rgba(255,255,255,.06)');
    rail(0, 'rgba(255,255,255,.22)', true);

    context.strokeStyle = '#cf7d93';
    context.lineWidth = 2;
    context.lineJoin = 'round';
    context.beginPath();
    for (let x = 0; x <= width; x++) {
      const value = departureAt(route, x / width * WAVE_CYCLES, live);
      if (x === 0) context.moveTo(x, y(value));
      else context.lineTo(x, y(value));
    }
    context.stroke();
  }

  function routeButtons(route) {
    const bypass = element('button', 'tiny', 'bypass');
    bypass.title = 'silence this route while you listen; not saved';
    bypass.addEventListener('click', () => {
      session.toggleRouteBypass(route);
      Editor.refresh();
    });
    const remove = element('button', 'tiny', '×');
    remove.addEventListener('click', () => free(route));
    const root = element('span', 'route-buttons');
    root.append(bypass, remove);
    return { root, bypass, remove };
  }

  function paintButtons(buttons, route) {
    const bypassed = session.routeBypassed(route);
    const held = session.heldAt();
    buttons.bypass.classList.toggle('on', bypassed);
    buttons.remove.disabled = session.isTarget();
    buttons.remove.title = held ? `held at "${held.name}": its routes are in force here`
      : session.isTarget() ? FREE_ON_BASE : 'free this route';
    return bypassed;
  }

  function free(route) {
    Editor.transition.snap();
    session.freeRoute(route);
    if (panel.target && !session.routesOn(panel.target, session.liveNamed()).length) close();
  }

  function buildPanel() {
    const root = element('div', 'route-panel');
    root.hidden = true;
    for (const route of Patch.ROUTES) {
      const block = element('div', 'route-block');
      const head = element('div', 'route-head');
      const buttons = routeButtons(route);
      head.append(element('span', 'route-name', route.name), buttons.root);
      const body = element('div');
      Editor.rows.buildRows(body, route.controls);
      const canvas = element('canvas', 'route-wave');
      block.append(head, body, canvas);
      root.appendChild(block);
      panel.blocks.push({ route, block, canvas, buttons });
    }
    const foot = element('div', 'route-foot');
    const add = element('button', 'tiny', '+ add a route');
    add.addEventListener('click', () => {
      Editor.transition.snap();
      session.addRoute(panel.target);
    });
    const freeCount = element('span', 'cc');
    foot.append(add, freeCount);
    root.append(foot);
    document.body.appendChild(root);
    Object.assign(panel, { root, add, free: freeCount });

    document.addEventListener('keydown', event => { if (event.key === 'Escape') close(); });
    document.addEventListener('pointerdown', event => {
      if (panel.target && !root.contains(event.target) && !event.target.closest('.routes-button')) close();
    });
    window.addEventListener('resize', place);
  }

  function buildList() {
    const root = element('details', 'route-list');
    root.open = true;
    const count = element('span', 'cc');
    const summary = element('summary', null, 'Routes ');
    summary.appendChild(count);
    root.appendChild(summary);
    root.addEventListener('toggle', () => Editor.paint());

    for (const route of Patch.ROUTES) {
      const line = element('div', 'route-line');
      const target = element('button', 'route-line-target');
      target.title = 'open this route under the control it moves';
      target.addEventListener('click', () => {
        const name = Protocol.NAME_BY_CC[session.liveNamed()[route.destination]];
        const row = Editor.rows.rows[name];
        if (!row) return;
        row.root.scrollIntoView({ block: 'center' });
        if (panel.target !== name) toggle(name);
      });
      const values = element('span', 'cc route-line-values');
      const canvas = element('canvas', 'route-wave route-line-wave');
      const buttons = routeButtons(route);
      line.append(target, values, canvas, buttons.root);
      root.appendChild(line);
      list.lines.push({ route, line, target, values, canvas, buttons });
    }
    Object.assign(list, { root, count });
    return root;
  }

  function destinationText(destination) {
    const name = Protocol.NAME_BY_CC[destination];
    const control = Patch.CONTROLS[name];
    return control ? `${Patch.PLACES[name]} · ${control.label}` : `CC ${destination}`;
  }

  function paintList(live) {
    let aimed = 0;
    for (const { route, line, target, values, canvas, buttons } of list.lines) {
      const destination = live[route.destination];
      line.hidden = !destination;
      if (!destination) continue;
      aimed++;
      target.textContent = destinationText(destination);
      values.textContent = route.controls.map(name => Patch.READOUTS[name](live[name])).join(' · ');
      line.classList.toggle('bypassed', paintButtons(buttons, route));
      if (list.root.open) drawWave(canvas, route, live);
    }
    list.count.textContent = aimed ? `${aimed} of ${Patch.ROUTES.length}` : 'none';
  }

  function paintMarkers(live) {
    const lfoBypassed = session.cardBypassed(Patch.LFO);
    for (const row of Object.values(Editor.rows.rows)) {
      if (!row.routes) continue;
      const here = session.routesOn(row.control.name, live);
      const sounding = !lfoBypassed && here.some(route => !session.routeBypassed(route));
      row.routes.classList.toggle('aimed', here.length > 0);
      row.routes.classList.toggle('sounding', sounding);
      row.routes.classList.toggle('open', row.control.name === panel.target);
      row.routes.title = here.length && !sounding ? 'routes on this control, all bypassed' : 'routes on this control';
      row.routes.textContent = here.length > 1 ? '~' + here.length : '~';
    }
  }

  function paintPanel(live) {
    panel.root.hidden = !panel.target;
    if (!panel.target) return;
    const held = session.heldAt();
    const destination = Patch.CC[panel.target];
    for (const { route, block, canvas, buttons } of panel.blocks) {
      block.hidden = live[route.destination] !== destination;
      block.classList.toggle('bypassed', paintButtons(buttons, route));
      if (!block.hidden) drawWave(canvas, route, live);
    }
    const free = session.freeRouteSlots(live).length;
    panel.add.disabled = free === 0 || !!held;
    panel.free.textContent = `${free} of ${Patch.ROUTES.length} free`;
    panel.add.title = held ? `held at "${held.name}": its routes are in force here`
      : session.isTarget() ? 'Added here, the route sits on the base at zero and only this target pushes.' : '';
    place();
  }

  function paint(live) {
    paintMarkers(live);
    paintPanel(live);
    paintList(live);
  }

  function place() {
    const row = panel.target && Editor.rows.rows[panel.target];
    if (!row) return;
    const bounds = row.root.getBoundingClientRect();
    const root = panel.root;
    root.style.width = `${Math.max(PANEL_MIN_WIDTH, bounds.width)}px`;
    const height = root.offsetHeight;
    const below = bounds.bottom + 4;
    const above = bounds.top - 4 - height;
    const top = below + height > window.innerHeight && above > 0 ? above : below;
    root.style.left = `${bounds.left + window.scrollX}px`;
    root.style.top = `${top + window.scrollY}px`;
  }

  function toggle(name) {
    if (panel.target === name) {
      close();
      return;
    }
    panel.target = name;
    if (!session.heldAt() && !session.routesOn(name, session.liveNamed()).length) {
      Editor.transition.snap();
      session.addRoute(name);
    }
    Editor.paint();
  }

  function close() {
    panel.target = null;
    Editor.paint();
  }

  Editor.routes = { buildPanel, buildList, paint, toggle };
})(window);
