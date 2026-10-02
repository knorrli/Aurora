(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Library = global.AuroraLibrary;
  const LibraryFile = global.AuroraLibraryFile;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { element, byId } = dom;

  const HOLD_AFTER_MILLISECONDS = 200;
  const PICK_NOTES = [Protocol.NOTE_PATCH_ONESHOT_FIRST, Protocol.NOTE_PATCH_ONESHOT_SECOND];

  const selection = { song: null, section: null };
  let rows = [];
  let draggedSection = null;
  let press = null;

  const songs = () => session.library.songs;
  const song = () => (selection.song === null ? null : songs()[selection.song] || null);
  const save = () => session.saveLibrary();
  const visible = () => !byId('songsPanel').hidden;

  function keyName(slot) {
    const bank = Math.floor((slot - 1) / Protocol.KEYPAD_KEYS);
    return `${String.fromCharCode('A'.charCodeAt(0) + bank)}${slot - bank * Protocol.KEYPAD_KEYS}`;
  }

  function patchText(slot) {
    const patch = session.patchAt(slot);
    return `${keyName(slot)} · ${patch ? patch.name || '(unnamed)' : 'empty'}`;
  }

  const slotValue = slot => (slot === null ? '' : String(slot));
  const slotFrom = value => (value === '' ? null : +value);

  function patchOptions(current) {
    const slots = new Set(Library.filledSlots(session.library));
    for (const slot of Object.values(current.patches)) if (slot !== null) slots.add(slot);
    return [['', '—'], ...[...slots].sort((a, b) => a - b).map(slot => [slot, patchText(slot)])];
  }

  const gigPlaces = index => session.library.gig.flatMap((place, at) => (place === index ? [at + 1] : []));

  function paintList() {
    const buttons = songs().map((each, index) => {
      const button = element('button', 'song-item');
      button.classList.toggle('on', index === selection.song);
      button.append(element('span', 'name', each.name || '(unnamed)'),
        element('span', 'cc', gigPlaces(index).join(' ')));
      button.addEventListener('click', () => choose(index));
      return button;
    });
    byId('songList').replaceChildren(...buttons);
    byId('songCount').textContent = String(songs().length);
    byId('songDelete').disabled = !song();
  }

  function choose(index) {
    if (index === selection.song) return;
    selection.song = index;
    selection.section = null;
    paint();
  }

  function labeledField(text, control, detail) {
    const field = element('div', 'field');
    const label = element('span', null, text);
    if (detail) label.append(' ', element('span', 'cc midi-number', detail));
    field.append(label, control);
    return field;
  }

  function nameField(current) {
    const input = element('input');
    input.type = 'text';
    input.id = 'songName';
    input.maxLength = Protocol.PATCH_NAME_LENGTH;
    input.value = current.name;
    input.addEventListener('input', () => {
      const printable = LibraryFile.printableName(input.value);
      if (printable !== input.value) input.value = printable;
      current.name = printable;
      save();
      paintList();
      paintGig();
    });
    return labeledField('Name', input);
  }

  const resolvedPick = place => {
    const pick = song().oneshots[place];
    return pick !== null ? pick : session.resolvedPick(place);
  };

  function pickField(current, place) {
    const select = element('select');
    dom.setOptions(select, [['', 'the patch’s'], ...Library.filledKit(session.library)
      .map(index => [index, `${Protocol.NOTE_ONESHOT_FIRST + index} · ${session.library.kit[index].name}`])],
      current.oneshots[place] === null ? '' : String(current.oneshots[place]));
    select.addEventListener('change', () => {
      current.oneshots[place] = select.value === '' ? null : +select.value;
      save();
    });
    const fire = element('button', 'tiny', 'fire');
    fire.addEventListener('click', () => {
      const index = resolvedPick(place);
      if (index !== null && session.kitOneshot(index)) Editor.wall.fire(index);
    });
    const line = element('div', 'pick-line');
    line.append(select, fire);
    return labeledField(`Oneshot ${place + 1}`, line, `note ${PICK_NOTES[place]}`);
  }

  function followMove(from, to) {
    const at = selection.section;
    if (at === null) return;
    if (at === from) selection.section = to;
    else if (from < at && to >= at) selection.section = at - 1;
    else if (from > at && to <= at) selection.section = at + 1;
  }

  function wireDrag(row, grip, index) {
    grip.draggable = true;
    grip.addEventListener('dragstart', event => {
      draggedSection = index;
      event.dataTransfer.effectAllowed = 'move';
      row.classList.add('dragging');
    });
    grip.addEventListener('dragend', () => {
      draggedSection = null;
      row.classList.remove('dragging');
    });
    row.addEventListener('dragover', event => {
      if (draggedSection === null || draggedSection === index) return;
      event.preventDefault();
      row.classList.add('drop');
    });
    row.addEventListener('dragleave', () => row.classList.remove('drop'));
    row.addEventListener('drop', event => {
      event.preventDefault();
      if (draggedSection === null || draggedSection === index) return;
      Library.moveSection(song(), draggedSection, index);
      followMove(draggedSection, index);
      save();
      paintSong();
    });
  }

  function sectionRow(current, index, options) {
    const row = element('div', 'section-row');
    const grip = element('span', 'section-grip', '⠿');
    wireDrag(row, grip, index);

    const label = element('input');
    label.type = 'text';
    label.maxLength = Protocol.PATCH_NAME_LENGTH;
    label.value = current.sections[index];
    label.addEventListener('input', () => {
      const printable = LibraryFile.printableName(label.value);
      if (printable !== label.value) label.value = printable;
    });
    label.addEventListener('change', () => {
      Library.relabelSection(current, index, label.value);
      save();
      refreshRows();
    });

    const patch = element('select');
    dom.setOptions(patch, options, '');
    patch.addEventListener('change', () => {
      current.patches[current.sections[index]] = slotFrom(patch.value);
      save();
      refreshRows();
    });

    const play = element('button', 'tiny', 'play');
    wirePress(play, () => index);

    const remove = element('button', 'tiny', '×');
    remove.addEventListener('click', () => {
      Library.removeSection(current, index);
      if (selection.section === index) selection.section = null;
      else if (selection.section !== null && selection.section > index) selection.section -= 1;
      save();
      paintSong();
    });

    row.append(grip, element('span', 'cc', String(index + 1)), label, patch, play, remove);
    return { row, label, patch, index };
  }

  function refreshRows() {
    const current = song();
    for (const { row, label, patch, index } of rows) {
      const slot = Library.sectionSlot(current, index);
      if (document.activeElement !== label) label.value = current.sections[index];
      patch.value = slotValue(slot);
      row.classList.toggle('on', index === selection.section);
      row.classList.toggle('missing', slot === null || !session.library.slots[slot]);
    }
    const last = current.sections.length - 1;
    byId('songPrevious').disabled = selection.section === null || selection.section <= 0;
    byId('songNext').disabled = last < 0 || selection.section === last;
  }

  function stepButton(id, text, section) {
    const button = element('button', 'tiny', text);
    button.id = id;
    wirePress(button, section);
    return button;
  }

  function paintSong() {
    const host = byId('songEditor');
    const current = song();
    rows = [];
    if (!current) {
      host.replaceChildren();
      return;
    }
    const fields = element('div', 'fields');
    fields.append(nameField(current), pickField(current, 0), pickField(current, 1));

    const steps = element('div', 'rowline song-steps');
    steps.append(
      stepButton('songPrevious', 'prev', () => Math.max(0, (selection.section || 0) - 1)),
      stepButton('songNext', 'next', () => (selection.section === null ? 0
        : Math.min(song().sections.length - 1, selection.section + 1))));

    const options = patchOptions(current);
    rows = current.sections.map((_, index) => sectionRow(current, index, options));
    const list = element('div', 'sections');
    list.append(...rows.map(({ row }) => row));

    const add = element('button', 'tiny', 'add section');
    add.addEventListener('click', () => {
      const index = Library.addSection(current);
      save();
      paintSong();
      rows[index].label.focus();
      rows[index].label.select();
    });

    host.replaceChildren(fields, steps, list, add);
    refreshRows();
  }

  function paintGig() {
    const places = session.library.gig.map((place, at) => {
      const select = element('select');
      dom.setOptions(select, [['', '—'], ...songs().map((each, index) => [index, each.name || '(unnamed)'])],
        place === null ? '' : String(place));
      select.addEventListener('change', () => {
        session.library.gig[at] = select.value === '' ? null : +select.value;
        save();
        paintList();
        paintGigCount();
      });
      const line = element('label', 'gig-place');
      const number = element('span', 'gig-number');
      number.append(element('span', null, String(at + 1)),
        element('span', 'cc midi-number', `PC ${Protocol.PROGRAM_SONG_FIRST + at}`));
      line.append(number, select);
      return line;
    });
    byId('gigPlaces').replaceChildren(...places);
    paintGigCount();
  }

  function paintGigCount() {
    byId('gigCount').textContent = `${session.library.gig.filter(place => place !== null).length} / ${Protocol.SONGS}`;
  }

  function paint() {
    if (!visible()) return;
    if (selection.song !== null && selection.song >= songs().length) {
      selection.song = songs().length ? songs().length - 1 : null;
      selection.section = null;
    }
    if (selection.song === null && songs().length) selection.song = 0;
    paintList();
    paintSong();
    paintGig();
  }

  function play(section, held) {
    const current = song();
    const slot = Library.sectionSlot(current, section);
    selection.section = section;
    if (slot === session.slot) {
      if (session.editingOneshot()) Editor.show(slot, session.draft);
      else refreshRows();
      return;
    }
    Editor.show(slot, null, held ? 0 : 1);
    if (held) Editor.transition.run();
  }

  function startPress(sectionOf) {
    const current = song();
    if (press || !current || !current.sections.length) return;
    const section = sectionOf();
    const slot = Library.sectionSlot(current, section);
    if (slot === null || !session.library.slots[slot]) {
      selection.section = section;
      refreshRows();
      Editor.say(`"${current.sections[section]}" plays no patch`, 'warn');
      return;
    }
    if (slot !== session.slot && session.draft) {
      if (Editor.rail.leaveDraft()) play(section, false);
      return;
    }
    press = { section, held: false };
    press.timer = setTimeout(() => {
      press.held = true;
      play(press.section, true);
    }, HOLD_AFTER_MILLISECONDS);
  }

  function endPress() {
    if (!press) return;
    clearTimeout(press.timer);
    if (!press.held) play(press.section, false);
    else if (Editor.transition.running()) Editor.transition.run();
    press = null;
  }

  function wirePress(button, sectionOf) {
    button.addEventListener('pointerdown', event => {
      if (event.button === 0) startPress(sectionOf);
    });
  }

  const STEP_KEYS = { ArrowLeft: 'songPrevious', ArrowRight: 'songNext' };
  const typingIn = target => target instanceof Element && !!target.closest('input, select, textarea');

  function wireKeys() {
    document.addEventListener('keydown', event => {
      const id = STEP_KEYS[event.key];
      if (!id || !visible() || !song() || typingIn(event.target) || event.repeat) return;
      event.preventDefault();
      const button = byId(id);
      if (button && !button.disabled) button.dispatchEvent(new PointerEvent('pointerdown', { button: 0 }));
    });
    document.addEventListener('keyup', event => {
      if (STEP_KEYS[event.key]) endPress();
    });
  }

  function wire() {
    document.addEventListener('pointerup', endPress);
    document.addEventListener('pointercancel', endPress);
    wireKeys();
    byId('songNew').addEventListener('click', () => {
      songs().push(Library.newSong('untitled'));
      selection.song = songs().length - 1;
      selection.section = null;
      save();
      paint();
      byId('songName').focus();
      byId('songName').select();
    });
    byId('songDelete').addEventListener('click', () => {
      const current = song();
      if (!current || !confirm(`Delete the song "${current.name}"?`)) return;
      Library.removeSong(session.library, selection.song);
      selection.section = null;
      save();
      paint();
    });
  }

  Editor.songs = { wire, paint };
})(window);
