import { Protocol } from './cc.js';
import * as dom from './dom.js';
import * as Playback from './playback.js';

const { byId, element } = dom;

const OFF = 0, ON = 127;
const EFFECT_COLUMNS = 5;
const DOT_RADIUS = 6;

const threeWay = (positions, labels) =>
  Object.entries(positions).map(([key, position]) => [Protocol.threeWayValue(position), labels[key]]);

const ROCKERS = [
  { cc: Protocol.CC.padMode, label: 'Mode',
    options: threeWay(Protocol.PAD_MODE, { perPatch: 'per patch', morph: 'morph', effects: 'effects' }) },
  { cc: Protocol.CC.padHold, label: 'Hold', options: [[OFF, 'momentary'], [ON, 'latching']] },
  { cc: Protocol.CC.padWidth, label: 'Width',
    options: threeWay(Protocol.PAD_WIDTH, { center: 'strip 3', middle: '2–4', all: 'all' }) },
  { cc: Protocol.CC.padGaps, label: 'Gaps', options: [[OFF, 'solid'], [ON, 'every other']] },
];

const rockers = Object.fromEntries([
  [Protocol.CC.padMode, Protocol.threeWayValue(Protocol.PAD_MODE.perPatch)],
  [Protocol.CC.padHold, OFF],
  [Protocol.CC.padWidth, Protocol.threeWayValue(Protocol.PAD_WIDTH.all)],
  [Protocol.CC.padGaps, OFF],
]);
const finger = { x: 0, y: 0, touching: false, playing: false };
const sent = {};
let canvas = null;
const rows = [];

const latching = () => rockers[Protocol.CC.padHold] === ON;
const effects = () => Protocol.threeWayPosition(rockers[Protocol.CC.padMode]) === Protocol.PAD_MODE.effects;

function send(cc, value) {
  if (sent[cc] === value) return;
  sent[cc] = value;
  Playback.movePad(cc, value);
}

function settle() {
  finger.playing = finger.touching || (finger.playing && latching());
  draw();
}

function draw() {
  const width = canvas.clientWidth, height = canvas.clientHeight;
  const scale = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = Math.round(width * scale);
  canvas.height = Math.round(height * scale);
  const context = canvas.getContext('2d');
  context.setTransform(scale, 0, 0, scale, 0, 0);
  context.clearRect(0, 0, width, height);
  if (effects()) {
    context.strokeStyle = 'rgba(255,255,255,0.12)';
    context.lineWidth = 1;
    context.beginPath();
    for (let column = 1; column < EFFECT_COLUMNS; column++) {
      const x = Math.round(width * column / EFFECT_COLUMNS) + 0.5;
      context.moveTo(x, 0);
      context.lineTo(x, height);
    }
    context.stroke();
  }
  if (!finger.playing) return;
  context.fillStyle = finger.touching ? '#fff' : 'rgba(255,255,255,0.55)';
  context.beginPath();
  context.arc(finger.x / 127 * width, (1 - finger.y / 127) * height, DOT_RADIUS, 0, Math.PI * 2);
  context.fill();
}

function follow(event) {
  const bounds = canvas.getBoundingClientRect();
  const along = value => Math.round(Math.min(1, Math.max(0, value)) * 127);
  finger.x = along((event.clientX - bounds.left) / bounds.width);
  finger.y = along(1 - (event.clientY - bounds.top) / bounds.height);
  send(Protocol.CC.padX, finger.x);
  send(Protocol.CC.padY, finger.y);
}

function lift() {
  if (!finger.touching) return;
  finger.touching = false;
  send(Protocol.CC.padTouch, OFF);
  settle();
}

function wirePad() {
  canvas.addEventListener('pointerdown', event => {
    if (event.button !== 0) return;
    canvas.setPointerCapture(event.pointerId);
    follow(event);
    finger.touching = true;
    send(Protocol.CC.padTouch, ON);
    settle();
  });
  canvas.addEventListener('pointermove', event => {
    if (!finger.touching) return;
    follow(event);
    draw();
  });
  canvas.addEventListener('pointerup', lift);
  canvas.addEventListener('pointercancel', lift);
  window.addEventListener('resize', draw);
}

function rockerRow(rocker) {
  const root = element('div', 'switch-row');
  root.append(dom.ccLabeled('label', rocker.label, rocker.cc));
  const picks = element('div', 'picks');
  const buttons = rocker.options.map(([value, text]) => {
    const button = element('button', null, text);
    button.addEventListener('click', () => {
      rockers[rocker.cc] = value;
      send(rocker.cc, value);
      paintRockers();
      settle();
    });
    picks.appendChild(button);
    return [value, button];
  });
  root.appendChild(picks);
  rows.push({ rocker, buttons });
  return root;
}

function paintRockers() {
  for (const { rocker, buttons } of rows) {
    for (const [value, button] of buttons) button.classList.toggle('on', rockers[rocker.cc] === value);
  }
}

function start() {
  canvas = element('canvas', 'pad-surface');
  const host = byId('pad');
  host.replaceChildren(element('h2', null, 'The pad'), canvas, ...ROCKERS.map(rockerRow));
  wirePad();
  for (const [cc, value] of Object.entries(rockers)) send(+cc, value);
  paintRockers();
  draw();
}

export { start };
