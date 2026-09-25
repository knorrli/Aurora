(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const LibraryFile = global.AuroraLibraryFile;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { element, byId } = dom;

  const tabs = [];

  function buildTabs() {
    const buttons = Patch.PART_NAMES.map((name, part) => {
      const button = element('button', 'tab');
      const badge = element('span', 'badge');
      button.append(element('span', null, name), badge);
      button.title = Patch.PART_BLURBS[part];
      button.addEventListener('click', () => selectPart(part));
      tabs.push({ button, badge });
      return button;
    });
    byId('partTabs').replaceChildren(...buttons);
  }

  function paintTabs() {
    tabs.forEach(({ button, badge }, part) => {
      button.classList.toggle('on', part === session.partIndex);
      badge.textContent = Patch.isTarget(part)
        ? Editor.transition.overriddenText(part, 'follows the patch') : 'the look itself';
    });
  }

  function selectPart(part) {
    session.selectPart(part);
    Editor.transition.rebuild();
    paint();
    Editor.rail.paintList();
    Editor.midi.sendLive();
  }

  const TIME_FIELDS = ['transitionTime', 'accentTime'];

  function buildHead() {
    for (const field of TIME_FIELDS) {
      const select = byId(field);
      dom.setOptions(select, Patch.LFO_PERIOD_NAMES.map((text, step) => [Patch.periodValue(step), text]), '');
      select.title = 'Stepped to values that come back to the grid, so holding through a completed transition arrives on a beat.';
      select.addEventListener('change', () => {
        session.editing()[field] = +select.value;
        session.changed();
      });
    }

    const tempo = byId('tempoDivision');
    dom.setOptions(tempo, Patch.TEMPO_DIVISIONS, Protocol.TEMPO_DIVISION.quarter);
    tempo.title = `What one tempo pulse stands for. Every rate scales with it. CC ${Patch.CC.tempoDivision}`;
    tempo.addEventListener('change', () => {
      Editor.transition.snap();
      session.setValue('tempoDivision', +tempo.value);
    });

    const name = byId('patchName');
    name.maxLength = Protocol.PATCH_NAME_LENGTH;
    name.addEventListener('input', () => {
      const printable = LibraryFile.printableName(name.value);
      if (printable !== name.value) name.value = printable;
      session.editing().name = printable;
      session.changed();
      Editor.rail.paintList();
    });
  }

  function paintHead(live) {
    const patch = session.patch();
    if (byId('patchName').value !== patch.name) byId('patchName').value = patch.name;
    for (const field of TIME_FIELDS) {
      byId(field).value = String(Patch.periodValue(Patch.periodStep(patch[field])));
    }
    byId('tempoDivision').value = String(live.tempoDivision);
  }

  function paint() {
    const live = session.liveNamed();
    Editor.rows.paint(live);
    paintTabs();
    Editor.routes.paint(live);
    paintHead(live);
    Editor.wall.paint();
    Editor.transition.paintShowing();
    Editor.transition.paintMix();
  }

  function refresh() {
    paint();
    Editor.midi.sendLive();
  }

  function show(slot, draft) {
    session.select(slot, draft);
    delete byId('saveSlot').dataset.touched;
    byId('saveSlot').value = '';
    Editor.transition.rebuild();
    paint();
    Editor.rail.paintList();
    Editor.midi.arrive();
  }

  function sendPatchToWall() {
    Editor.midi.arrive();
    Editor.say(`sent "${session.patch().name}"`);
  }

  function measureTopbar() {
    const topbar = byId('topbar');
    const measure = () => document.documentElement.style
      .setProperty('--topbar', Math.round(topbar.getBoundingClientRect().height) + 'px');
    new ResizeObserver(measure).observe(topbar);
    measure();
  }

  Object.assign(Editor, { paint, refresh, show });

  global.AuroraPreview.ready.then(() => {
    session.load();
    session.transition.from = session.slot;
    session.onChange = refresh;
    session.onDraftStarted = () => Editor.rail.paintList();
    window.addEventListener('pagehide', session.flush);
    document.addEventListener('visibilitychange', session.flush);

    Editor.rows.build();
    Editor.routes.buildPanel();
    buildTabs();
    buildHead();
    Editor.rail.wire();
    Editor.midi.wire();
    byId('sendPatch').addEventListener('click', sendPatchToWall);
    measureTopbar();

    Editor.wall.start();
    Editor.transition.rebuild();
    paint();
    Editor.rail.paintList();
    Editor.midi.open();
    Editor.say('ready — ask the brain what it holds to check the link');
  });
})(window);
