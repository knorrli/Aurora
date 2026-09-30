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

  const bar = { scrub: null, run: null, runTime: null, comingFrom: null, transitionTo: null };
  const mixBar = { sliders: {}, readouts: {} };
  let animationFrame = null;

  const percentOf = position => Math.round(position * 100) + '%';

  function overriddenText(part, otherwise) {
    const count = Library.overriddenIn(session.patch(), part).length;
    return count ? `${count} overridden` : otherwise;
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
    if (session.transition.position === session.restPosition()) return;
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
    const rest = session.restPosition();
    if (session.transition.position === rest) {
      Editor.paint();
      return;
    }
    stop();
    animate(session.transition.position, rest, SPRING_BACK_MILLISECONDS, Editor.paint);
  }

  const transitionStep = patch => Patch.periodStep(patch.transitionTime);

  function transitionMilliseconds(patch) {
    return Protocol.LFO_PERIODS[transitionStep(patch)] * 60000 / Editor.midi.bpm();
  }

  function run() {
    if (animationFrame) {
      stop();
      springBack();
      return;
    }
    const destination = session.transitionDestination();
    if (!destination || !Editor.rail.leaveDraft()) return;
    const slot = session.transition.to;
    bar.run.classList.add('on');
    animate(0, 1, transitionMilliseconds(destination), () => Editor.show(slot));
  }

  function paintRun() {
    if (!bar.run) return;
    const destination = session.transitionDestination();
    bar.run.disabled = !destination;
    bar.runTime.disabled = !destination;
    bar.runTime.value = destination ? String(Patch.periodValue(transitionStep(destination))) : '';
  }

  function buildRunTime() {
    const select = element('select');
    dom.setOptions(select, Patch.LFO_PERIOD_NAMES.map((text, step) => [Patch.periodValue(step), text]), '');
    select.addEventListener('change', () => {
      session.transitionDestination().transitionTime = +select.value;
      session.saveLibrary();
    });
    bar.runTime = select;
    return labeledField('over', select);
  }

  function showingText() {
    const position = session.transition.position;
    if (session.isTarget()) return `${Patch.PART_NAMES[session.partIndex]} ${percentOf(position)}`;
    if (session.transitioning()) return `"${session.transitionDestination().name}" ${percentOf(position)}`;
    if (session.mixing()) {
      return Object.entries(session.mix).filter(([, amount]) => amount > 0)
        .map(([part, amount]) => `${Patch.PART_NAMES[part]} ${percentOf(amount)}`).join(' + ');
    }
    return 'the patch';
  }

  function paintShowing() {
    byId('wallShowing').textContent = showingText();
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
    const filled = Library.filledSlots(session.library);
    if (bar.comingFrom) {
      const from = session.transition.from;
      dom.setOptions(bar.comingFrom, patchOptions(filled), from === null ? '' : from);
    }
    if (bar.transitionTo) {
      const to = session.transition.to;
      dom.setOptions(bar.transitionTo,
        [['', '— nowhere —'], ...patchOptions(filled.filter(slot => slot !== session.slot))],
        to === null ? '' : to);
    }
    paintRun();
  }

  function buildComingFrom() {
    const select = element('select');
    select.addEventListener('change', () => {
      session.transition.from = select.value === '' ? null : +select.value;
      Editor.refresh();
    });
    const field = labeledField('Coming from', select);
    bar.comingFrom = select;
    return field;
  }

  function buildTransitionTo() {
    const select = element('select');
    select.addEventListener('change', () => {
      session.transition.to = select.value === '' ? null : +select.value;
      paintRun();
      Editor.refresh();
    });
    bar.transitionTo = select;
    return labeledField('to', select);
  }

  function targetMoves() {
    const moves = element('div', 'target-moves');
    const others = Patch.TARGETS.filter(part => part !== session.partIndex);
    const picker = (text, act) => {
      const select = element('select');
      dom.setOptions(select, [['', text], ...others.map(part => [part, Patch.PART_NAMES[part]])], '');
      select.addEventListener('change', () => {
        if (select.value === '') return;
        act(+select.value);
        select.value = '';
        session.changed();
      });
      const field = element('label', 'field');
      field.appendChild(select);
      return field;
    };
    const take = element('button', 'tiny', 'take the base’s changes');
    take.addEventListener('click', takeBaseChanges);
    const here = session.partIndex;
    moves.append(
      take,
      picker('copy from…', from => Library.copyOverrides(session.editing(), from, here)),
      picker('move onto…', to => Library.moveOverrides(session.editing(), here, to, false)),
      picker('swap with…', to => Library.moveOverrides(session.editing(), here, to, true)));
    return moves;
  }

  function takeBaseChanges() {
    const saved = session.slot !== null ? session.library.slots[session.slot] : null;
    const reference = saved ? saved.base : Library.newPatch().base;
    const { moved, kept } = Library.takeBaseChanges(session.editing(), reference, session.partIndex);
    if (!moved.length && !kept.length) {
      Editor.say('the base has no changes to take', 'bad');
      return;
    }
    session.changed();
    const plural = (count, word) => `${count} ${word}${count === 1 ? '' : 's'}`;
    const took = `${Patch.PART_NAMES[session.partIndex]} took ${plural(moved.length, 'change')} from the base`;
    if (!kept.length) {
      Editor.say(took, 'ok');
      return;
    }
    const labels = kept.map(name => (Patch.CONTROLS[name] ? Patch.CONTROLS[name].label : name)).join(', ');
    Editor.say(`${took}; ${labels} stay${kept.length === 1 ? 's' : ''} on the base — switches belong to the whole patch`, 'warn');
  }

  function build() {
    const host = byId('transitionBar');
    const target = session.isTarget();
    Object.assign(bar, { run: null, runTime: null, comingFrom: null, transitionTo: null });

    const scrub = element('div', 'scrub');
    bar.scrub = dom.rangeInput(SCRUB_STEPS);
    bar.scrub.id = 'transitionScrub';
    bar.scrub.addEventListener('input', () => {
      stop();
      session.transition.position = +bar.scrub.value / SCRUB_STEPS;
      paintLive();
    });
    bar.scrub.addEventListener('change', springBack);
    scrub.appendChild(bar.scrub);

    const heading = element('h2', null, target ? `Base to ${Patch.PART_NAMES[session.partIndex]}` : 'Transition');
    const fields = element('div', 'fields');
    host.replaceChildren(heading, scrub);
    if (!target) {
      bar.run = element('button', 'tiny', 'run');
      bar.run.id = 'transitionRun';
      bar.run.addEventListener('click', run);
      scrub.appendChild(bar.run);
      fields.append(buildTransitionTo(), buildRunTime());
      host.appendChild(fields);
    } else {
      if (session.partIndex === Protocol.PATCH_TARGET_ACCENT) {
        fields.appendChild(buildComingFrom());
        host.appendChild(fields);
      }
      host.appendChild(targetMoves());
    }
    showPosition();
    refreshPatchChoices();
    paintShowing();
  }

  function buildMix() {
    const host = byId('mixBar');
    host.hidden = session.isTarget();
    mixBar.sliders = {};
    mixBar.readouts = {};
    if (session.isTarget()) {
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
    for (const part of Patch.TARGETS) {
      const row = element('div', 'row');
      const label = dom.labeled('label', Patch.PART_NAMES[part],
        part === Protocol.PATCH_TARGET_ACCENT ? 'a held key' : 'fader');
      const track = element('div', 'track');
      const slider = dom.rangeInput(SCRUB_STEPS);
      slider.id = 'mix' + part;
      slider.value = Math.round(session.mix[part] * SCRUB_STEPS);
      const readout = dom.labeled('output', '', '');
      slider.addEventListener('input', () => {
        session.mix[part] = +slider.value / SCRUB_STEPS;
        paintMixReadout(part);
        paintLive();
      });
      track.append(slider);
      row.append(label, track, readout);
      rows.appendChild(row);
      mixBar.sliders[part] = slider;
      mixBar.readouts[part] = readout;
      paintMixReadout(part);
    }
    host.replaceChildren(head, rows,
      element('p', 'note', '⚠ The brain does not combine the faders yet — this is the editor’s guess.'));
  }

  function paintMix() {
    for (const [part, slider] of Object.entries(mixBar.sliders)) {
      slider.value = Math.round(session.mix[part] * SCRUB_STEPS);
      paintMixReadout(part);
    }
  }

  function paintMixReadout(part) {
    dom.fillLabeled(mixBar.readouts[part], percentOf(session.mix[part]), overriddenText(+part, 'nothing to reach'));
  }

  function rebuild() {
    stop();
    build();
    buildMix();
  }

  Editor.transition = {
    snap, rebuild, refreshPatchChoices, paintShowing, paintMix, overriddenText,
  };
})(window);
