(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const Preview = global.AuroraPreview;
  const Library = global.AuroraLibrary;
  const LibraryFile = global.AuroraLibraryFile;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { element, byId } = dom;

  const BRAIN_BUTTONS = ['brainAsk', 'brainPush', 'brainPull'];

  function say(text, className) {
    const status = byId('status');
    const at = new Date().toLocaleTimeString('en-US', { hour12: false });
    status.replaceChildren(element('span', 'cc', at), '  ', element('span', className || '', text));
  }

  const leaveDraft = () => !session.draft || confirm(`Discard your changes to "${session.draft.name}"?`);

  let target = null;
  let dragged = null;

  function patchColor(patch) {
    const named = Library.namedFromBytes(patch.base);
    const hue = Preview.convert(Patch.CC.hue, named.hue) & 255;
    const saturation = Preview.convert(Patch.CC.saturation, named.saturation);
    return `rgb(${Preview.paletteColor(named.palette, hue, saturation).join(',')})`;
  }

  const firstEmptySlot = () => Library.firstEmptySlot(session.library);
  const isSaved = () => session.slot !== null && !!session.library.slots[session.slot];

  function saveTarget() {
    if (target !== null && target !== session.slot) return target;
    return session.slot === null ? firstEmptySlot() : null;
  }

  const BANKED_SLOTS = Protocol.BANKS * Protocol.KEYPAD_KEYS;

  const bankLetter = bank => String.fromCharCode('A'.charCodeAt(0) + bank);

  function rowLabel(text) {
    return element('span', 'row-label', text);
  }

  function gridRows() {
    const rows = [[rowLabel(''), ...Array.from({ length: Protocol.KEYPAD_KEYS }, (_, key) => rowLabel(String(key + 1)))]];
    for (let bank = 0; bank < Protocol.BANKS; bank++) {
      rows.push([rowLabel(bankLetter(bank)),
        ...Array.from({ length: Protocol.KEYPAD_KEYS }, (_, key) => cell(bank * Protocol.KEYPAD_KEYS + key + 1))]);
    }
    for (let first = BANKED_SLOTS + 1; first < Protocol.PATCH_MAX; first += Protocol.KEYPAD_KEYS) {
      const last = Math.min(first + Protocol.KEYPAD_KEYS, Protocol.PATCH_MAX);
      rows.push([rowLabel(first === BANKED_SLOTS + 1 ? 'PC' : ''),
        ...Array.from({ length: last - first }, (_, offset) => cell(first + offset))]);
    }
    return rows.flat();
  }

  function cell(slot) {
    const current = slot === session.slot;
    const patch = current ? session.underneath() : session.library.slots[slot];
    const button = element('button', 'cell');
    button.classList.toggle('empty', !patch);
    button.classList.toggle('on', current && !session.editingOneshot());
    button.classList.toggle('dirty', current && !!session.draft);
    button.classList.toggle('target', slot === saveTarget());

    const head = element('span', 'cell-head');
    head.append(element('span', 'slot cc midi-number', `PC ${slot}`));
    const name = element('span', 'name', patch ? patch.name || '(unnamed)' : slot === saveTarget() ? 'new' : '');
    if (patch) {
      const color = element('i', 'color');
      color.style.background = patchColor(patch);
      button.appendChild(color);
    }
    button.append(head, name);

    button.addEventListener('click', event => {
      if (patch && !event.shiftKey) selectPatch(slot);
      else pickTarget(slot);
    });
    wireDrag(button, slot, !!session.library.slots[slot]);
    return button;
  }

  function pickTarget(slot) {
    target = slot === target ? null : slot;
    paintList();
  }

  function wireDrag(button, slot, filled) {
    button.draggable = filled;
    button.addEventListener('dragstart', event => {
      dragged = slot;
      event.dataTransfer.effectAllowed = 'move';
      button.classList.add('dragging');
    });
    button.addEventListener('dragend', () => {
      dragged = null;
      button.classList.remove('dragging');
    });
    button.addEventListener('dragover', event => {
      if (dragged === null || dragged === slot) return;
      event.preventDefault();
      button.classList.add('drop');
    });
    button.addEventListener('dragleave', () => button.classList.remove('drop'));
    button.addEventListener('drop', event => {
      event.preventDefault();
      if (dragged !== null && dragged !== slot) swapSlots(dragged, slot);
    });
  }

  function swapSlots(from, to) {
    const { slots } = session.library;
    [slots[from], slots[to]] = [slots[to], slots[from]];
    Library.swapSongSlots(session.library, from, to);
    if (session.slot === from) session.slot = to;
    else if (session.slot === to) session.slot = from;
    if (target === from || target === to) target = null;
    session.saveLibrary();
    paintList();
    say(slots[from] ? `swapped slots ${from} and ${to}` : `moved "${slots[to].name}" to slot ${to}`);
  }

  function paintList() {
    const filled = Library.filledSlots(session.library);
    byId('patchGrid').replaceChildren(...gridRows());
    byId('libraryCount').textContent = `${filled.length} / ${Protocol.PATCH_MAX - 1}`;
    paintSaving();
    Editor.oneshots.paintKit();
    Editor.transition.refreshPatchChoices();
    Editor.songs.paint();
  }

  function paintSaving() {
    const oneshot = session.editingOneshot();
    byId('subjectKind').classList.toggle('unsaved', !!(oneshot ? session.oneshotDraft : session.draft));
    byId('patchSave').disabled = oneshot ? !session.oneshotDraft : !session.draft || session.slot === null;
    byId('patchDiscard').disabled = oneshot ? !session.oneshotDraft : !session.draft;
    byId('patchDelete').disabled = !isSaved();
    const slot = saveTarget();
    const saveHere = byId('saveHere');
    saveHere.disabled = slot === null;
    saveHere.textContent = slot === null ? 'save to slot' : `save to slot ${slot}`;
  }

  function selectPatch(slot) {
    if (slot === session.slot && session.editingOneshot()) {
      Editor.show(slot, session.draft);
      return;
    }
    if (slot === session.slot || !leaveDraft()) return;
    Editor.show(slot);
  }

  function showFirstOrNew() {
    const first = session.firstFilled();
    if (first === null) Editor.show(firstEmptySlot(), Library.newPatch('untitled'));
    else Editor.show(first);
  }

  function replaceLibrary(file) {
    session.library = Library.libraryFromFile(file);
    session.layerIndex = Protocol.PATCH_LAYER_BASE;
    session.saveLibrary();
    showFirstOrNew();
  }

  function download(filename, text) {
    const url = URL.createObjectURL(new Blob([text], { type: 'application/json' }));
    const anchor = element('a');
    anchor.href = url;
    anchor.download = filename;
    anchor.click();
    URL.revokeObjectURL(url);
  }

  const notTheDraft = () => (session.draft ? ' — not the unsaved draft' : '');

  function brainButton(id, work) {
    byId(id).addEventListener('click', async () => {
      const link = Editor.midi.link;
      if (link.busy) {
        say('the brain is still answering the last request', 'warn');
        return;
      }
      for (const button of BRAIN_BUTTONS) byId(button).disabled = true;
      try {
        await link.exclusively(work);
      } catch (error) {
        say(error.message, 'bad');
      } finally {
        for (const button of BRAIN_BUTTONS) byId(button).disabled = false;
      }
    });
  }

  function wireBrain() {
    const link = Editor.midi.link;
    brainButton('brainAsk', async () => {
      const info = await link.queryLibrary();
      say(`protocol ${info.protocol}, format ${info.format} · ${info.stateText} — ${info.slots.length} patches`,
          info.state === Protocol.LIBRARY_STATE.stored ? 'ok' : 'warn');
    });

    brainButton('brainPush', async () => {
      const file = Library.libraryToFile(session.library);
      const fault = Library.validateFile(file);
      if (fault) { say(fault, 'bad'); return; }
      if (!file.patches.length) { say('the library is empty; there is nothing to push', 'bad'); return; }
      say(`pushing ${file.patches.length} patches${notTheDraft()}`);
      const startedAt = performance.now();
      const result = await link.push(file);
      if (result.ok) say(`stored ${result.count} patches in ${Math.round(performance.now() - startedAt)} ms`, 'ok');
      else say(`${result.where}: ${global.AuroraLink.statusText(result.status)}`, 'bad');
    });

    brainButton('brainPull', async () => {
      say('reading the brain back');
      const result = await link.pull();
      if (result.error) { say(result.error, 'bad'); return; }
      say(`read ${result.file.patches.length} patches`);
      const pulled = Library.upgradeFile(result.file);
      const fault = Library.validateFile(pulled);
      if (fault) { say(`the brain's library: ${fault}`, 'bad'); return; }
      if (!leaveDraft()) return;
      const kept = Library.libraryToFile(Object.assign({}, session.library, { slots: [] }));
      replaceLibrary(Object.assign({}, pulled, kept, { patches: pulled.patches }));
      say(`the editor now holds what the brain holds — ${result.file.patches.length} patches`, 'ok');
    });
  }

  function wireFiles() {
    byId('fileSave').addEventListener('click', () => {
      const stamp = new Date().toISOString().slice(0, 10);
      download(`aurora-library-${stamp}.json`, LibraryFile.serialize(Library.libraryToFile(session.library)));
      say(`saved ${Library.filledSlots(session.library).length} patches to a file${notTheDraft()}`, 'ok');
    });

    byId('fileLoad').addEventListener('click', () => byId('filePick').click());
    byId('filePick').addEventListener('change', async () => {
      const picker = byId('filePick');
      const file = picker.files && picker.files[0];
      picker.value = '';
      if (!file) return;
      let loaded;
      try {
        loaded = JSON.parse(await file.text());
      } catch {
        say(`${file.name} is not readable JSON`, 'bad');
        return;
      }
      loaded = Library.upgradeFile(loaded);
      const fault = Library.validateFile(loaded);
      if (fault) { say(`${file.name}: ${fault}`, 'bad'); return; }
      if (!leaveDraft()) return;
      replaceLibrary(loaded);
      say(`loaded ${loaded.patches.length} patches from ${file.name}`, 'ok');
    });
  }

  function wireLibrary() {
    byId('patchSave').addEventListener('click', () => {
      if (session.editingOneshot()) {
        Editor.oneshots.save();
        return;
      }
      session.library.slots[session.slot] = session.draft;
      session.draft = null;
      session.saveLibrary();
      Editor.paint();
      paintList();
      say(`saved "${session.library.slots[session.slot].name}" in slot ${session.slot}`, 'ok');
    });
    byId('patchDiscard').addEventListener('click', () => {
      if (session.editingOneshot()) {
        Editor.oneshots.discard();
        return;
      }
      if (!leaveDraft()) return;
      session.draft = null;
      if (isSaved()) Editor.show(session.slot);
      else showFirstOrNew();
    });
    byId('saveHere').addEventListener('click', () => {
      const slot = saveTarget();
      const occupant = session.library.slots[slot];
      if (slot !== session.slot && occupant && !confirm(`Slot ${slot} holds "${occupant.name}". Replace it?`)) return;
      session.library.slots[slot] = Library.clonePatch(session.underneath());
      session.slot = slot;
      session.draft = null;
      target = null;
      session.saveLibrary();
      Editor.paint();
      paintList();
      say(`saved "${session.library.slots[slot].name}" in slot ${slot}`, 'ok');
    });
    byId('patchNew').addEventListener('click', () => {
      if (session.editingOneshot()) {
        Editor.oneshots.startNew();
        return;
      }
      if (!leaveDraft()) return;
      session.layerIndex = Protocol.PATCH_LAYER_BASE;
      Editor.show(firstEmptySlot(), Library.newPatch('untitled'));
    });
    byId('patchDelete').addEventListener('click', () => {
      const patch = session.library.slots[session.slot];
      if (!confirm(`Empty slot ${session.slot}, "${patch.name}"? Keypad keys on it stay on the empty slot.`)) return;
      session.library.slots[session.slot] = null;
      Library.forgetSongSlot(session.library, session.slot);
      session.draft = null;
      session.saveLibrary();
      showFirstOrNew();
    });
  }

  const VIEWS = { libraryView: 'libraryPanel', songsView: 'songsPanel' };

  function showView(chosen) {
    for (const [button, panel] of Object.entries(VIEWS)) {
      byId(panel).hidden = button !== chosen;
      byId(button).classList.toggle('on', button === chosen);
    }
    byId('editorView').hidden = chosen !== null;
    if (chosen === 'songsView') Editor.songs.paint();
  }

  function wireViewToggle() {
    for (const button of Object.keys(VIEWS)) {
      byId(button).addEventListener('click', () => showView(byId(button).classList.contains('on') ? null : button));
    }
  }

  function wire() {
    wireViewToggle();
    wireLibrary();
    wireBrain();
    wireFiles();
  }

  Editor.say = say;
  Editor.rail = { wire, paintList, leaveDraft };
})(window);
