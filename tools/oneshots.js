(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const Library = global.AuroraLibrary;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { element, byId } = dom;

  const PICK_FIELDS = ['oneshotPick0', 'oneshotPick1'];
  const DEFAULT_FIELDS = ['defaultOneshot0', 'defaultOneshot1'];

  const noteOf = index => Protocol.NOTE_ONESHOT_FIRST + index;
  const kitOneshot = index => (index === session.oneshotIndex && session.oneshotDraft) || session.library.kit[index];

  const leaveOneshotDraft = () =>
    !session.oneshotDraft || confirm(`Discard your changes to the oneshot "${session.oneshotDraft.name}"?`);

  function kitOptions(empty) {
    return [['', empty], ...Library.filledKit(session.library)
      .map(index => [index, `${noteOf(index)} · ${session.library.kit[index].name}`])];
  }

  const pickValue = pick => (pick === null ? '' : String(pick));
  const pickFrom = value => (value === '' ? null : +value);

  function paintPickSelects(ids, picks, empty) {
    ids.forEach((id, place) => {
      dom.setOptions(byId(id), kitOptions(empty), pickValue(picks[place]));
    });
  }

  function paintPicks() {
    paintPickSelects(PICK_FIELDS, session.patch().oneshots, 'default');
  }

  function paintHead() {
    byId('oneshotLength').value = String(Patch.periodValue(Patch.periodStep(session.patch().length)));
    byId('backToPatch').textContent = session.underneath().name || '(unnamed)';
  }

  function open(index) {
    if (session.editingOneshot() && index === session.oneshotIndex) return;
    const keepsDraft = index === session.oneshotIndex && session.oneshotDraft;
    if (!keepsDraft && !leaveOneshotDraft()) return;
    const draft = keepsDraft ? session.oneshotDraft
      : session.library.kit[index] ? null : Library.newOneshot('untitled');
    session.openOneshot(index, draft);
    if (session.firing.repeat) Editor.wall.fire();
    Editor.transition.rebuild();
    Editor.paint();
    Editor.rail.paintList();
  }

  function backToPatch() {
    Editor.show(session.slot, session.draft);
  }

  function cell(index) {
    const oneshot = kitOneshot(index);
    const current = session.editingOneshot() && index === session.oneshotIndex;
    const button = element('button', 'cell');
    button.classList.toggle('empty', !oneshot);
    button.classList.toggle('on', current);
    button.classList.toggle('dirty', index === session.oneshotIndex && !!session.oneshotDraft);
    const head = element('span', 'cell-head');
    head.append(element('span', 'slot cc midi-number', `note ${noteOf(index)}`));
    button.append(head, element('span', 'name', oneshot ? oneshot.name || '(unnamed)' : ''));
    button.addEventListener('click', () => open(index));
    return button;
  }

  function paintKit() {
    byId('kitGrid').replaceChildren(...Array.from({ length: Protocol.ONESHOTS }, (_, index) => cell(index)));
    byId('kitCount').textContent = `${Library.filledKit(session.library).length} / ${Protocol.ONESHOTS}`;
    paintPickSelects(DEFAULT_FIELDS, session.library.defaultOneshots, '—');
    byId('oneshotDelete').disabled = !session.editingOneshot() || !session.library.kit[session.oneshotIndex];
  }

  let firingText = null;

  function paintFiring(progress) {
    const length = Protocol.LFO_PERIODS[Patch.periodStep(session.patch().length)];
    const text = progress === null ? '' : `${(progress * length).toFixed(1)} / ${length}`;
    if (text === firingText) return;
    firingText = text;
    byId('firingAt').textContent = text;
    byId('oneshotFire').classList.toggle('on', progress !== null);
  }

  function save() {
    session.library.kit[session.oneshotIndex] = session.oneshotDraft;
    session.oneshotDraft = null;
    session.saveLibrary();
    Editor.rail.paintList();
    Editor.say(`saved the oneshot "${session.patch().name}" on note ${noteOf(session.oneshotIndex)}`, 'ok');
  }

  function discard() {
    if (!leaveOneshotDraft()) return;
    session.oneshotDraft = null;
    if (session.library.kit[session.oneshotIndex]) {
      session.saveLibrary();
      Editor.paint();
      Editor.rail.paintList();
    } else {
      backToPatch();
    }
  }

  function startNew() {
    const index = Library.firstEmptyKitPlace(session.library);
    if (index === null) {
      Editor.say(`the kit is full — all ${Protocol.ONESHOTS} notes hold a oneshot`, 'warn');
      return;
    }
    if (!leaveOneshotDraft()) return;
    session.oneshotDraft = null;
    session.oneshotIndex = null;
    open(index);
  }

  function remove() {
    const index = session.oneshotIndex;
    const oneshot = session.library.kit[index];
    if (!confirm(`Empty note ${noteOf(index)}, "${oneshot.name}"?`)) return;
    session.library.kit[index] = null;
    session.oneshotDraft = null;
    for (const patch of session.library.slots) {
      if (patch) patch.oneshots = patch.oneshots.map(pick => (pick === index ? null : pick));
    }
    if (session.draft) session.draft.oneshots = session.draft.oneshots.map(pick => (pick === index ? null : pick));
    session.library.defaultOneshots = session.library.defaultOneshots.map(pick => (pick === index ? null : pick));
    session.saveLibrary();
    backToPatch();
  }

  function wire() {
    const length = byId('oneshotLength');
    dom.setOptions(length, Patch.LFO_PERIOD_NAMES.map((text, step) => [Patch.periodValue(step), text]), '');
    length.addEventListener('change', () => {
      session.editing().length = +length.value;
      session.changed();
    });

    byId('oneshotPick0Note').textContent = `note ${Protocol.NOTE_PATCH_ONESHOT_FIRST}`;
    byId('oneshotPick1Note').textContent = `note ${Protocol.NOTE_PATCH_ONESHOT_SECOND}`;
    PICK_FIELDS.forEach((id, place) => byId(id).addEventListener('change', () => {
      session.editing().oneshots[place] = pickFrom(byId(id).value);
      session.changed();
    }));
    DEFAULT_FIELDS.forEach((id, place) => byId(id).addEventListener('change', () => {
      session.library.defaultOneshots[place] = pickFrom(byId(id).value);
      session.saveLibrary();
    }));

    byId('oneshotFire').addEventListener('click', Editor.wall.fire);
    const repeat = byId('oneshotRepeat');
    repeat.classList.toggle('on', session.firing.repeat);
    repeat.addEventListener('click', () => {
      session.firing.repeat = !session.firing.repeat;
      repeat.classList.toggle('on', session.firing.repeat);
    });
    byId('backToPatch').addEventListener('click', backToPatch);
    byId('oneshotDelete').addEventListener('click', remove);
  }

  Editor.oneshots = { wire, open, paintKit, paintPicks, paintHead, paintFiring, save, discard, startNew, leaveOneshotDraft };
})(window);
