(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const Library = global.AuroraLibrary;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { element, byId } = dom;

  const SCRUB_STEPS = 1000;
  const SPRING_BACK_MILLISECONDS = 180;

  const bar = { scrub: null, run: null, from: null, clear: null };
  const mixBar = { sliders: {}, readouts: {} };
  let animationFrame = null;

  const MIX_FADERS = {
    [Protocol.PATCH_LAYER_COLOR]: 'faderColor',
    [Protocol.PATCH_LAYER_MOTION]: 'faderMotion',
    [Protocol.PATCH_LAYER_EXTENT]: 'faderExtent',
  };

  const mixSource = layer => layer === Protocol.PATCH_LAYER_ACCENT
    ? `note ${Protocol.NOTE_KEY_HELD}`
    : `CC ${Patch.CC[MIX_FADERS[layer]]}`;

  const percentOf = position => Math.round(position * 100) + '%';

  function changesText(layer) {
    const count = Library.changedIn(session.patch(), layer).length;
    if (!count) return 'no changes';
    return `${count} change${count === 1 ? '' : 's'}`;
  }

  function stop() {
    if (animationFrame) cancelAnimationFrame(animationFrame);
    animationFrame = null;
    if (bar.run) bar.run.classList.remove('on');
  }

  function showPosition() {
    if (bar.scrub) bar.scrub.value = Math.round(session.transition.position * SCRUB_STEPS);
  }

  function snap() {
    if (session.transition.position === 1) return;
    stop();
    session.resetTransition();
    showPosition();
    Editor.refresh();
  }

  function animate(from, to, milliseconds, done) {
    const startedAt = performance.now();
    const step = now => {
      const progress = Math.min(1, (now - startedAt) / milliseconds);
      session.transition.position = from + (to - from) * progress;
      showPosition();
      paintLive();
      if (progress < 1) {
        animationFrame = requestAnimationFrame(step);
        return;
      }
      animationFrame = null;
      if (done) done();
    };
    animationFrame = requestAnimationFrame(step);
  }

  function springBack() {
    if (session.transition.position === 1) {
      Editor.paint();
      return;
    }
    stop();
    animate(session.transition.position, 1, SPRING_BACK_MILLISECONDS, Editor.paint);
  }

  function transitionMilliseconds(patch) {
    return Protocol.LFO_PERIODS[Patch.periodStep(patch.transitionTime)] * 60000 / Editor.midi.bpm();
  }

  function run() {
    if (animationFrame) {
      stop();
      springBack();
      return;
    }
    if (!session.comingFrom()) return;
    bar.run.classList.add('on');
    animate(0, 1, transitionMilliseconds(session.patch()), () => {
      stop();
      Editor.paint();
    });
  }

  function paintRun() {
    if (bar.run) bar.run.disabled = !session.comingFrom();
  }

  function showingText() {
    if (session.editingOneshot()) return `over "${session.over().name}"`;
    const position = session.transition.position;
    if (session.isAboveBase()) return `${Patch.LAYER_NAMES[session.layerIndex]} ${percentOf(position)}`;
    if (session.transitioning()) return `from "${session.comingFrom().name}" ${percentOf(position)}`;
    if (session.mixing()) {
      return Object.entries(session.mix).filter(([, amount]) => amount > 0)
        .map(([layer, amount]) => `${Patch.LAYER_NAMES[layer]} ${percentOf(amount)}`).join(' + ');
    }
    return 'the patch';
  }

  function clearLayer() {
    const layer = session.layerIndex;
    Library.clearLayer(session.editing(), layer);
    session.changed();
    Editor.say(`cleared ${Patch.LAYER_NAMES[layer]}`, 'ok');
  }

  function paintShowing() {
    byId('wallShowing').textContent = showingText();
    if (bar.clear) bar.clear.disabled = !Library.changedIn(session.patch(), session.layerIndex).length;
  }

  function paintLive() {
    Editor.rows.paintFaderValues(session.liveNamed());
    paintShowing();
    Editor.midi.sendLive();
  }

  function labeledField(text, control) {
    const field = element('label', 'field');
    field.append(element('span', null, text), control);
    return field;
  }

  function patchOptions(slots) {
    return slots.map(slot => [slot, `${slot} · ${session.patchAt(slot).name}`]);
  }

  function refreshPatchChoices() {
    session.forgetMissingPatches();
    if (bar.from) {
      const from = session.transition.from;
      const others = Library.filledSlots(session.library).filter(slot => slot !== session.slot);
      dom.setOptions(bar.from, [['', '— nowhere —'], ...patchOptions(others)],
        from === null || from === session.slot ? '' : from);
    }
    paintRun();
  }

  function buildFrom() {
    const select = element('select');
    select.addEventListener('change', () => {
      session.transition.from = select.value === '' ? session.slot : +select.value;
      paintRun();
      Editor.refresh();
    });
    bar.from = select;
    return labeledField('from', select);
  }

  function build() {
    const host = byId('transitionBar');
    host.hidden = session.editingOneshot();
    const aboveBase = session.isAboveBase();
    Object.assign(bar, { run: null, from: null, clear: null });

    const scrub = element('div', 'scrub');
    bar.scrub = dom.rangeInput(SCRUB_STEPS);
    bar.scrub.id = 'transitionScrub';
    bar.scrub.addEventListener('input', () => {
      stop();
      session.transition.position = +bar.scrub.value / SCRUB_STEPS;
      paintLive();
    });
    scrub.appendChild(bar.scrub);

    const heading = element('h2', null, aboveBase ? `Base to ${Patch.LAYER_NAMES[session.layerIndex]}` : 'Transition');
    if (aboveBase) {
      bar.clear = element('button', 'tiny', 'clear');
      bar.clear.addEventListener('click', clearLayer);
      heading.appendChild(bar.clear);
    }
    const fields = element('div', 'fields');
    host.replaceChildren(heading, scrub);
    if (!aboveBase) {
      bar.run = element('button', 'tiny', 'run');
      bar.run.id = 'transitionRun';
      bar.run.addEventListener('click', run);
      scrub.appendChild(bar.run);
    }
    if (!aboveBase || session.layerIndex === Protocol.PATCH_LAYER_ACCENT) {
      fields.appendChild(buildFrom());
      host.appendChild(fields);
    }
    showPosition();
    refreshPatchChoices();
    paintShowing();
  }

  function buildMix() {
    const host = byId('mixBar');
    const hidden = session.isAboveBase() || session.editingOneshot();
    host.hidden = hidden;
    mixBar.sliders = {};
    mixBar.readouts = {};
    if (hidden) {
      host.replaceChildren();
      return;
    }

    const head = element('h2', null, 'Mix');
    const allDown = element('button', 'tiny', 'all down');
    allDown.addEventListener('click', () => {
      session.resetPreview();
      paintMix();
      Editor.refresh();
    });
    head.appendChild(allDown);

    const rows = element('div', 'mix-rows');
    for (const layer of Patch.LAYERS_ABOVE_BASE) {
      const row = element('div', 'row');
      const label = dom.midiLabeled('label', Patch.LAYER_NAMES[layer], mixSource(layer));
      const track = element('div', 'track');
      const slider = dom.rangeInput(SCRUB_STEPS);
      slider.id = 'mix' + layer;
      slider.value = Math.round(session.mix[layer] * SCRUB_STEPS);
      const readout = element('output');
      slider.addEventListener('input', () => {
        session.mix[layer] = +slider.value / SCRUB_STEPS;
        paintMixReadout(layer);
        paintLive();
      });
      track.append(slider);
      row.append(label, track, readout);
      rows.appendChild(row);
      mixBar.sliders[layer] = slider;
      mixBar.readouts[layer] = readout;
      paintMixReadout(layer);
    }
    host.replaceChildren(head, rows,
      element('p', 'note', '⚠ The brain does not combine the faders yet — this is the editor’s guess.'));
  }

  function paintMix() {
    for (const [layer, slider] of Object.entries(mixBar.sliders)) {
      slider.value = Math.round(session.mix[layer] * SCRUB_STEPS);
      paintMixReadout(layer);
    }
  }

  function paintMixReadout(layer) {
    mixBar.readouts[layer].textContent = percentOf(session.mix[layer]);
  }

  function rebuild() {
    stop();
    build();
    buildMix();
  }

  Editor.transition = {
    snap, rebuild, refreshPatchChoices, paintShowing, paintMix, changesText,
  };
})(window);
