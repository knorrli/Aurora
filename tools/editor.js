(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const LibraryFile = global.AuroraLibraryFile;
  const Library = global.AuroraLibrary;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { element, byId } = dom;

  const tabs = [];

  function buildTabs() {
    const buttons = Patch.LAYERS.map(layer => {
      const button = element('button', 'tab');
      const badge = element('span', 'badge');
      button.append(element('span', null, Patch.LAYER_NAMES[layer]), badge);
      button.addEventListener('click', () => selectLayer(layer));
      if (Patch.isAboveBase(layer)) Editor.layerDrop.target(button, layer);
      tabs.push({ layer, button, badge });
      return button;
    });
    byId('layerTabs').replaceChildren(...buttons);
  }

  function paintTabs() {
    document.body.classList.toggle('above-base', session.isAboveBase());
    for (const { layer, button, badge } of tabs) {
      button.classList.toggle('on', layer === session.layerIndex);
      button.classList.toggle('unsaved', session.layerUnsaved(layer));
      badge.textContent = Patch.isAboveBase(layer)
        ? Editor.transition.changesText(layer) : '';
    }
  }

  function selectLayer(layer) {
    session.selectLayer(layer);
    Editor.transition.rebuild();
    paint();
    Editor.rail.paintList();
    Editor.playback.changed();
    Editor.midi.sendLive();
  }

  const TIME_FIELDS = {
    transitionTime: { names: Patch.TRANSITION_NAMES, step: Patch.transitionStep, value: Patch.transitionValue },
    accentTime: { names: Patch.LFO_PERIOD_NAMES, step: Patch.periodStep, value: Patch.periodValue },
  };

  function buildHead() {
    for (const [field, steps] of Object.entries(TIME_FIELDS)) {
      const select = byId(field);
      dom.setOptions(select, steps.names.map((text, step) => [steps.value(step), text]), '');
      select.addEventListener('change', () => {
        session.editing()[field] = +select.value;
        session.changed();
      });
    }

    const tempo = byId('tempoDivision');
    dom.setOptions(tempo, Patch.TEMPO_DIVISIONS, Protocol.TEMPO_DIVISION.quarter);
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

  function paintHead() {
    const patch = session.patch();
    byId('subjectKind').textContent = session.editingOneshot() ? 'Oneshot' : 'Patch';
    if (byId('patchName').value !== patch.name) byId('patchName').value = patch.name;
    if (session.editingOneshot()) {
      Editor.oneshots.paintHead();
      return;
    }
    Editor.oneshots.paintPicks();
    for (const [field, steps] of Object.entries(TIME_FIELDS)) {
      byId(field).value = String(steps.value(steps.step(patch[field])));
    }
    byId('tempoDivision').value = String(Library.namedFromBytes(patch.base).tempoDivision);
    const accentReachesNothing = !Library.changedIn(patch, Protocol.PATCH_LAYER_ACCENT).length;
    byId('accentTime').closest('.field').classList.toggle('inert', accentReachesNothing);
  }

  function paint() {
    document.body.classList.toggle('oneshot-mode', session.editingOneshot());
    Editor.rows.paint(Editor.playback.shownNamed());
    paintTabs();
    Editor.routes.paint(session.liveNamed());
    paintHead();
    Editor.transition.paintShowing();
    Editor.transition.paintMix();
  }

  function refresh() {
    Editor.playback.changed();
    paint();
    Editor.midi.sendLive();
  }

  function show(slot, draft) {
    session.select(slot, draft);
    Editor.transition.rebuild();
    Editor.playback.cut();
    paint();
    Editor.rail.paintList();
    Editor.midi.sendLive(true);
  }

  function follow(slot) {
    session.select(slot, null);
    Editor.transition.rebuild();
    Editor.playback.follow(slot);
    paint();
    Editor.rail.paintList();
  }

  function sendPatchToWall() {
    Editor.midi.sendLive(true);
    Editor.say(`sent "${session.patch().name}"`);
  }

  const STORE_SHOW_MIDI = 'aurora.editor.showMidi';

  function showMidi(shown) {
    document.body.classList.toggle('show-midi', shown);
    byId('midiToggle').classList.toggle('on', shown);
    try {
      localStorage.setItem(STORE_SHOW_MIDI, JSON.stringify(shown));
    } catch {
      return;
    }
  }

  function wireMidiToggle() {
    let stored = false;
    try {
      stored = JSON.parse(localStorage.getItem(STORE_SHOW_MIDI)) === true;
    } catch {
      stored = false;
    }
    showMidi(stored);
    byId('midiToggle').addEventListener('click', () => showMidi(!document.body.classList.contains('show-midi')));
    byId('tempoDivisionCC').textContent = `CC ${Patch.CC.tempoDivision}`;
  }

  function measureTopbar() {
    const topbar = byId('topbar');
    const measure = () => document.documentElement.style
      .setProperty('--topbar', Math.round(topbar.getBoundingClientRect().height) + 'px');
    new ResizeObserver(measure).observe(topbar);
    measure();
  }

  Object.assign(Editor, { paint, refresh, show, follow, selectLayer });

  global.AuroraPreview.ready.then(() => {
    session.load();
    session.transition.from = session.slot;
    session.onChange = refresh;
    session.onDraftStarted = () => Editor.rail.paintList();
    session.onLibrarySaved = () => Editor.playback.syncLibrary();
    window.addEventListener('pagehide', session.flush);
    document.addEventListener('visibilitychange', session.flush);

    Editor.rows.build();
    Editor.routes.buildPanel();
    buildTabs();
    buildHead();
    Editor.rail.wire();
    Editor.oneshots.wire();
    Editor.songs.wire();
    Editor.keep.wire();
    Editor.midi.wire();
    byId('sendPatch').addEventListener('click', sendPatchToWall);
    wireMidiToggle();
    measureTopbar();

    Editor.playback.start();
    Editor.wall.start();
    Editor.transition.rebuild();
    paint();
    Editor.rail.paintList();
    Editor.midi.open();
    Editor.say('ready — ask the brain what it holds to check the link');
  });
})(window);
