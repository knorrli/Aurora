import { Protocol } from './cc.js';
import * as dom from './dom.js';
import * as Playback from './playback.js';
import { Preview } from './preview.js';

const { byId, element } = dom;

const OFF = 0, ON = 127;
const EFFECT_COLUMNS = 5;
const DOT_RADIUS = 7;

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
let surface = null;
const rows = [];

const latching = () => rockers[Protocol.CC.padHold] === ON;
const effects = () => Protocol.threeWayPosition(rockers[Protocol.CC.padMode]) === Protocol.PAD_MODE.effects;

function send(cc, value) {
  if (sent[cc] === value) return;
  sent[cc] = value;
  Playback.movePad(cc, value);
}

function fingerLevel(source) {
  const perPatch = Protocol.threeWayPosition(rockers[Protocol.CC.padMode]) === Protocol.PAD_MODE.perPatch;
  if (!finger.playing || !perPatch) return null;
  return (source === Protocol.ROUTE_SOURCE.padX ? finger.x : finger.y) / 127;
}

function settle() {
  finger.playing = finger.touching || (finger.playing && latching());
}

const stripHeight = height => height * (1 - Preview.PAR_BAND_SHARE);

function drawFinger(context, width, wallHeight) {
  const height = stripHeight(wallHeight);
  context.save();
  if (effects() && finger.touching) {
    context.strokeStyle = 'rgba(255,255,255,0.18)';
    context.lineWidth = 1;
    context.setLineDash([3, 4]);
    context.beginPath();
    for (let column = 1; column < EFFECT_COLUMNS; column++) {
      const x = Math.round(width * column / EFFECT_COLUMNS) + 0.5;
      context.moveTo(x, 0);
      context.lineTo(x, height);
    }
    context.stroke();
    context.setLineDash([]);
  }
  if (finger.playing) {
    context.beginPath();
    context.arc(finger.x / 127 * width, (1 - finger.y / 127) * height, DOT_RADIUS, 0, Math.PI * 2);
    context.strokeStyle = 'rgba(10,11,14,0.85)';
    context.lineWidth = 4;
    context.stroke();
    context.strokeStyle = '#fff';
    context.lineWidth = 1.5;
    context.stroke();
    if (finger.touching) {
      context.fillStyle = 'rgba(255,255,255,0.35)';
      context.fill();
    }
  }
  context.restore();
}

function follow(event) {
  const bounds = surface.getBoundingClientRect();
  const along = value => Math.round(Math.min(1, Math.max(0, value)) * 127);
  finger.x = along((event.clientX - bounds.left) / bounds.width);
  finger.y = along(1 - (event.clientY - bounds.top) / stripHeight(bounds.height));
  send(Protocol.CC.padX, finger.x);
  send(Protocol.CC.padY, finger.y);
}

function onStrips(event) {
  const bounds = surface.getBoundingClientRect();
  return event.clientY - bounds.top <= stripHeight(bounds.height);
}

function lift() {
  if (!finger.touching) return;
  finger.touching = false;
  send(Protocol.CC.padTouch, OFF);
  settle();
}

function wireSurface() {
  surface.addEventListener('pointerdown', event => {
    if (event.button !== 0 || !onStrips(event)) return;
    surface.setPointerCapture(event.pointerId);
    follow(event);
    finger.touching = true;
    send(Protocol.CC.padTouch, ON);
    settle();
  });
  surface.addEventListener('pointermove', event => {
    if (finger.touching) follow(event);
  });
  surface.addEventListener('pointerup', lift);
  surface.addEventListener('pointercancel', lift);
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

function start(wall) {
  surface = wall;
  byId('pad').replaceChildren(...ROCKERS.map(rockerRow));
  wireSurface();
  for (const [cc, value] of Object.entries(rockers)) send(+cc, value);
  paintRockers();
}

export { start, drawFinger, fingerLevel };
