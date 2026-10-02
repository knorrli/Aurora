(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const Library = global.AuroraLibrary;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { byId } = dom;

  const DEFAULT_BPM = 120;

  const link = new global.AuroraLink.Link();
  const lastSent = new Array(Protocol.PATCH_CC_COUNT).fill(-1);
  let clockSending = false;

  const bpm = () => +byId('bpm').value || DEFAULT_BPM;

  function sendLive(force) {
    const live = session.editingOneshot()
      ? Patch.controls(session.over().base)
      : session.sounding(session.pinnedControls() || Patch.controls(session.patch().base));
    for (const name of Patch.NAMES) {
      const cc = Patch.CC[name], value = live[name];
      if (!force && lastSent[cc] === value) continue;
      if (link.sendCC(cc, value)) lastSent[cc] = value;
    }
  }

  function setClock(sending) {
    clockSending = sending;
    byId('clockToggle').classList.toggle('on', sending);
    Editor.playback.sendClock(sending);
  }

  function paintPorts() {
    const select = byId('port');
    const outputs = link.outputs();
    dom.setOptions(select, outputs.map(port => [port.id, port.name]), link.output ? link.output.id : '');
    const state = byId('midiState');
    if (!link.output) {
      state.textContent = 'no MIDI outputs';
      state.className = 'bad';
    } else if (!link.input) {
      state.textContent = `${link.output.name} — no reply port`;
      state.className = 'warn';
    } else {
      state.textContent = 'connected';
      state.className = 'ok';
    }
  }

  function connected() {
    paintPorts();
    if (link.output) sendLive(true);
  }

  async function open() {
    try {
      await link.open();
    } catch (error) {
      byId('midiState').textContent = error.message;
      return;
    }
    byId('port').addEventListener('change', () => {
      link.choose(byId('port').value);
      connected();
    });
    link.onports = () => {
      if (link.keepOrPrefer()) connected();
      else paintPorts();
    };
    link.keepOrPrefer();
    connected();
  }

  function wire() {
    byId('clockToggle').addEventListener('click', () => setClock(!clockSending));
    let pressed = false;
    byId('rigBlackout').addEventListener('pointerdown', event => {
      if (event.button !== 0) return;
      pressed = true;
      Editor.playback.press(Protocol.PROGRAM_BLACKOUT);
    });
    document.addEventListener('pointerup', () => {
      if (!pressed) return;
      pressed = false;
      Editor.playback.release();
    });
  }

  Editor.midi = { link, bpm, sendLive, open, wire };
})(window);
