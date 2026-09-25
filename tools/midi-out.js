(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { byId } = dom;

  const CLOCK_TICK = 0xF8, CLOCK_START = 0xFA;
  const LOOKAHEAD_MILLISECONDS = 250, SCHEDULE_EVERY_MILLISECONDS = 100;
  const DEFAULT_BPM = 120;

  const link = new global.AuroraLink.Link();
  const lastSent = new Array(Protocol.PATCH_CC_COUNT).fill(-1);
  const clock = { running: false, timer: null, nextTickAt: 0 };

  const bpm = () => +byId('bpm').value || DEFAULT_BPM;

  function sendLive(force) {
    const live = session.sounding(session.liveNamed());
    for (const name of Patch.NAMES) {
      const cc = Patch.CC[name], value = live[name];
      if (!force && lastSent[cc] === value) continue;
      if (link.sendCC(cc, value)) lastSent[cc] = value;
    }
  }

  function arrive() {
    sendLive(true);
    link.sendProgram(Protocol.PROGRAM_SHOW);
    Editor.wall.clearTails();
  }

  function blackout() {
    link.sendProgram(Protocol.PROGRAM_BLACKOUT);
    Editor.say(`PC ${Protocol.PROGRAM_BLACKOUT} — blackout`);
  }

  function pumpClock() {
    const millisecondsPerTick = 60000 / bpm() / Protocol.TICKS_PER_BEAT;
    const now = performance.now();
    if (clock.nextTickAt < now) clock.nextTickAt = now;
    while (clock.nextTickAt < now + LOOKAHEAD_MILLISECONDS) {
      link.sendRaw([CLOCK_TICK], clock.nextTickAt);
      clock.nextTickAt += millisecondsPerTick;
    }
  }

  function setClock(running) {
    clock.running = running;
    byId('clockToggle').classList.toggle('on', running);
    clearInterval(clock.timer);
    clock.timer = null;
    if (!running) return;
    link.sendRaw([CLOCK_START]);
    Editor.wall.restartBeats();
    clock.nextTickAt = performance.now();
    pumpClock();
    clock.timer = setInterval(pumpClock, SCHEDULE_EVERY_MILLISECONDS);
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
    byId('clockToggle').addEventListener('click', () => setClock(!clock.running));
    byId('rigBlackout').title = `Program Change ${Protocol.PROGRAM_BLACKOUT}. Sends past the patch and leaves it alone.`;
    byId('rigBlackout').addEventListener('click', blackout);
  }

  Editor.midi = { link, bpm, sendLive, arrive, open, wire };
})(window);
