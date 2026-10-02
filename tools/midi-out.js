import { Protocol } from './cc.js';
import * as dom from './dom.js';
import { Link } from './link.js';
import * as Playback from './playback.js';

const { byId } = dom;

const DEFAULT_BPM = 120;

const link = new Link();
let clockSending = false;

const bpm = () => +byId('bpm').value || DEFAULT_BPM;

function setClock(sending) {
  clockSending = sending;
  byId('clockToggle').classList.toggle('on', sending);
  Playback.sendClock(sending);
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
  if (link.output) Playback.dial();
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
    Playback.press(Protocol.PROGRAM_BLACKOUT);
  });
  document.addEventListener('pointerup', () => {
    if (!pressed) return;
    pressed = false;
    Playback.release();
  });
}

export { link, bpm, open, wire };
