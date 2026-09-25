(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Library = global.AuroraLibrary;
  const LibraryFile = global.AuroraLibraryFile;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { element, byId } = dom;

  const BRAIN_BUTTONS = ['brainAsk', 'brainPush', 'brainPull'];

  function say(text, className) {
    const log = byId('log');
    const at = new Date().toLocaleTimeString('en-US', { hour12: false });
    log.append(element('span', 'cc', at), '  ', element('span', className || '', text), '\n');
    log.scrollTop = log.scrollHeight;
  }

  const leaveDraft = () => !session.draft || confirm(`Discard your changes to "${session.draft.name}"?`);

  function keysOn(slot) {
    return session.library.keymap.flatMap((keySlot, index) => (keySlot === slot ? [index + 1] : []));
  }

  function listItem(slot, patch) {
    const keys = slot === null ? [] : keysOn(slot);
    const current = slot === session.slot;
    const item = element('button', 'item');
    item.classList.toggle('on', current);
    item.classList.toggle('dirty', current && !!session.draft);
    const name = (current ? session.patch() : patch).name;
    item.append(
      element('span', 'slot', slot === null ? 'new' : String(slot)),
      element('span', 'name', name || '(unnamed)'),
      element('span', 'keys', !keys.length ? '' : keys.length > 3 ? `${keys.length} keys` : 'key ' + keys.join(',')));
    item.title = keys.length ? `${name} — on keypad ${keys.join(', ')}` : name;
    if (slot !== null) item.addEventListener('click', () => selectPatch(slot));
    return item;
  }

  function paintList() {
    const filled = Library.filledSlots(session.library);
    const items = filled.map(slot => listItem(slot, session.library.slots[slot]));
    if (session.slot === null) items.unshift(listItem(null, session.draft));
    byId('patchList').replaceChildren(...items);
    byId('libraryCount').textContent = `${filled.length} / ${Protocol.PATCH_MAX}`;
    paintSaving();
    paintKeypad();
    Editor.transition.refreshPatchChoices();
  }

  function paintSaving() {
    byId('patchSave').disabled = !session.draft || session.slot === null;
    byId('patchDiscard').disabled = !session.draft;
    byId('patchDelete').disabled = session.slot === null;
    const target = byId('saveSlot');
    if (target.value === '' || !target.dataset.touched) {
      target.value = session.slot !== null ? session.slot : session.library.slots.findIndex(patch => !patch);
    }
    paintSaveTarget();
  }

  function saveTarget() {
    const input = byId('saveSlot');
    const slot = +input.value;
    return input.value !== '' && Number.isInteger(slot) && slot >= 0 && slot < Protocol.PATCH_MAX ? slot : null;
  }

  function paintSaveTarget() {
    const slot = saveTarget();
    byId('saveHere').disabled = slot === null;
    byId('saveOccupant').textContent = slot === null ? ''
      : slot === session.slot ? 'this patch'
      : session.library.slots[slot] ? session.library.slots[slot].name : 'empty';
  }

  function paintKeypad() {
    const keys = [];
    for (let key = 1; key <= Protocol.KEYPAD_KEYS; key++) {
      const slot = session.library.keymap[key - 1];
      const patch = session.library.slots[slot];
      const button = element('button', 'key');
      button.append(element('span', 'number', String(key)),
                    element('span', 'who', patch ? patch.name : `${slot} · empty`));
      button.title = (patch ? `Key ${key} plays slot ${slot}, "${patch.name}". ` : `Key ${key} plays slot ${slot}, which is empty. `)
        + (session.slot === null ? 'Save this patch into a slot to put it on a key.' : `Click to put slot ${session.slot} here.`);
      button.addEventListener('click', () => {
        if (session.slot === null) {
          say('save the patch into a slot first', 'bad');
          return;
        }
        session.library.keymap[key - 1] = session.slot;
        session.saveLibrary();
        paintList();
        say(`key ${key} is now slot ${session.slot}, "${session.library.slots[session.slot].name}"`);
      });
      keys.push(button);
    }
    byId('keypad').replaceChildren(...keys);
  }

  function selectPatch(slot) {
    if (slot === session.slot || !leaveDraft()) return;
    Editor.show(slot);
  }

  function showFirstOrNew() {
    const first = session.firstFilled();
    if (first === null) Editor.show(null, Library.newPatch('untitled'));
    else Editor.show(first);
  }

  function replaceLibrary(file) {
    session.library = Library.libraryFromFile(file);
    session.partIndex = Protocol.PATCH_BASE;
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
      say(`protocol ${info.protocol}, patch format ${info.format}`);
      say(`${info.stateText} — ${info.slots.length} patches`,
          info.state === Protocol.LIBRARY_STATE.stored ? 'ok' : 'warn');
      if (info.slots.length) say(`slots: ${info.slots.join(' ')}`);
      say(`keypad: ${info.keymap.join(' ')}`);
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
      const fault = Library.validateFile(result.file);
      if (fault) { say(`the brain's library: ${fault}`, 'bad'); return; }
      if (!leaveDraft()) return;
      replaceLibrary(result.file);
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
      const fault = Library.validateFile(loaded);
      if (fault) { say(`${file.name}: ${fault}`, 'bad'); return; }
      if (!leaveDraft()) return;
      replaceLibrary(loaded);
      say(`loaded ${loaded.patches.length} patches from ${file.name}`, 'ok');
    });
  }

  function wireLibrary() {
    byId('patchSave').addEventListener('click', () => {
      session.library.slots[session.slot] = session.draft;
      session.draft = null;
      session.saveLibrary();
      paintList();
      say(`saved "${session.library.slots[session.slot].name}" in slot ${session.slot}`, 'ok');
    });
    byId('patchDiscard').addEventListener('click', () => {
      if (!leaveDraft()) return;
      session.draft = null;
      if (session.slot === null) showFirstOrNew();
      else Editor.show(session.slot);
    });
    byId('saveSlot').max = Protocol.PATCH_MAX - 1;
    byId('saveSlot').addEventListener('input', () => {
      byId('saveSlot').dataset.touched = '1';
      paintSaveTarget();
    });
    byId('saveHere').addEventListener('click', () => {
      const slot = saveTarget();
      const occupant = session.library.slots[slot];
      if (slot !== session.slot && occupant && !confirm(`Slot ${slot} holds "${occupant.name}". Replace it?`)) return;
      session.library.slots[slot] = Library.clonePatch(session.patch());
      session.slot = slot;
      session.draft = null;
      delete byId('saveSlot').dataset.touched;
      session.saveLibrary();
      Editor.paint();
      paintList();
      say(`saved "${session.library.slots[slot].name}" in slot ${slot}`, 'ok');
    });
    byId('patchNew').addEventListener('click', () => {
      if (!leaveDraft()) return;
      Editor.show(null, Library.newPatch('untitled'));
    });
    byId('patchDelete').addEventListener('click', () => {
      const patch = session.library.slots[session.slot];
      if (!confirm(`Empty slot ${session.slot}, "${patch.name}"? Keypad keys on it stay on the empty slot.`)) return;
      session.library.slots[session.slot] = null;
      session.draft = null;
      session.saveLibrary();
      showFirstOrNew();
    });
  }

  function wire() {
    wireLibrary();
    wireBrain();
    wireFiles();
  }

  Editor.say = say;
  Editor.rail = { wire, paintList };
})(window);
