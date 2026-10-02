import * as Rows from './control-rows.js';
import * as dom from './dom.js';
import * as Oneshots from './oneshots.js';
import * as Pad from './pad.js';
import * as Playback from './playback.js';
import { Preview } from './preview.js';
import * as Routes from './route-panel.js';
import { session } from './session.js';
import * as Tracks from './tracks.js';

const { byId } = dom;

const BEAT_FLASH = 0.15;

let wall = null;
const OVERLAYS = ['centers', 'fan', 'bend', 'arp', 'palette', 'spots', 'field'];
const STORE_OVERLAYS = 'aurora.editor.overlays';

const view = { overlays: readOverlays(), order: null, parOrder: null };
const beats = { dots: [] };
let drawnText = null;

function readOverlays() {
  let stored = null;
  try {
    stored = JSON.parse(localStorage.getItem(STORE_OVERLAYS));
  } catch {
    stored = null;
  }
  return Object.fromEntries(OVERLAYS.map(name => [name, stored?.[name] ?? true]));
}

function writeOverlays() {
  try {
    localStorage.setItem(STORE_OVERLAYS, JSON.stringify(view.overlays));
  } catch {
    return;
  }
}

function makeWall(id, width, height) {
  const canvas = byId(id);
  const scale = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = width * scale;
  canvas.height = height * scale;
  canvas.style.aspectRatio = `${width} / ${height}`;
  const context = canvas.getContext('2d');
  context.scale(scale, scale);
  const glow = document.createElement('canvas');
  glow.width = width;
  glow.height = height;
  return { context, glow, width, height };
}

function readOrder(input, fallback) {
  const count = fallback.length;
  const places = input.value.split(',').map(text => parseInt(text, 10) - 1);
  const valid = places.length === count && new Set(places).size === count
    && places.every(place => Number.isInteger(place) && place >= 0 && place < count);
  input.classList.toggle('invalid', !valid);
  return valid ? places : fallback.map(number => number - 1);
}

function wireOrder(id, fallback, apply) {
  const input = byId(id);
  input.value = fallback.join(',');
  const read = () => apply(readOrder(input, fallback));
  input.addEventListener('input', read);
  read();
}

function paintBeats(position) {
  const beat = Math.floor(position);
  const hit = position - beat < BEAT_FLASH;
  beats.dots.forEach((dot, index) => {
    const on = index === beat % beats.dots.length;
    dot.classList.toggle('on', on);
    dot.classList.toggle('hit', on && hit);
  });
}

function paintShownValues() {
  const shown = Playback.shownControls();
  const text = shown.bytes.join(',');
  if (text === drawnText) return;
  drawnText = text;
  Rows.paintFaderValues(shown);
}

function frame() {
  const drawn = Playback.frame(performance.now());
  Preview.draw(wall.context, wall.glow, drawn, view.order, view.parOrder, wall.width, wall.height, view.overlays);
  Pad.drawFinger(wall.context, wall.width, wall.height);
  paintBeats(Playback.beats());
  Oneshots.keepRepeating();
  const progress = Playback.oneshotProgress();
  const live = session.liveControls();
  Routes.paintPlayheads(session.editingOneshot() ? (progress < 0 ? null : progress) : drawn.lfo, live);
  Oneshots.paintFiring(progress < 0 ? null : progress);
  paintShownValues();
  Tracks.paint();
  requestAnimationFrame(frame);
}

function start() {
  beats.dots = [...document.querySelectorAll('#beats i')];
  wall = makeWall('wallMain', 300, 480);
  Pad.start(byId('wallMain'));

  wireOrder('stripOrder', Preview.WALL_STRIP_ORDER, order => { view.order = order; });
  wireOrder('parOrder', Preview.WALL_PAR_ORDER, order => { view.parOrder = order; });
  for (const button of document.querySelectorAll('#wallOverlays button')) {
    const name = button.dataset.overlay;
    button.classList.toggle('on', view.overlays[name]);
    button.addEventListener('click', () => {
      view.overlays[name] = !view.overlays[name];
      button.classList.toggle('on', view.overlays[name]);
      writeOverlays();
    });
  }

  requestAnimationFrame(frame);
}

export { start };
