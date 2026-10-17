import { Protocol } from './cc.js';
import * as Patch from './patch.js';
import { Preview } from './preview.js';

const SHOWN_LIVES = 2;
const SUSTAIN_SHOWN = 1.25;
const PADDING = 5;
const OVERHANG = 10;

const beatsOf = (live, name) => Preview.convert(Patch.CC[name], live[name]);
const endOf = live => Protocol.threeWayPosition(live.cellEnd);

function beatsShown(live, envelope) {
  const names = Patch.ENVELOPES[envelope];
  const gate = beatsOf(live, 'cellGate');
  if (endOf(live) === Protocol.CELL_END.startOver) return gate * SHOWN_LIVES;
  if (endOf(live) === Protocol.CELL_END.gone) return gate + beatsOf(live, names.release);
  const stages = beatsOf(live, names.delay) + beatsOf(live, names.attack) + beatsOf(live, names.hold)
    + beatsOf(live, names.decay);
  return Math.max(stages * SUSTAIN_SHOWN, 1);
}

const levelAt = (live, envelope, age) =>
  Preview.envelopeLevel(Patch.envelopeStages(live, envelope), live.cellGate, live.cellEnd, live.cellRate, age);

function draw(canvas, live, envelope) {
  const width = canvas.clientWidth, height = canvas.clientHeight;
  if (!width || !height) return;
  const scale = Math.min(2, window.devicePixelRatio || 1);
  canvas.width = Math.round(width * scale);
  canvas.height = Math.round(height * scale);
  const context = canvas.getContext('2d');
  context.setTransform(scale, 0, 0, scale, 0, 0);
  context.clearRect(0, 0, width, height);

  const span = beatsShown(live, envelope);
  const inner = width - 2 * OVERHANG;
  const xAt = beats => OVERHANG + beats / span * inner;
  const beatsAt = x => (x - OVERHANG) / inner * span;
  const yAt = level => height - PADDING - level * (height - 2 * PADDING);
  const levelOf = beats => (beats < 0 ? 0 : levelAt(live, envelope, beats));
  const line = (x, style, dashed) => {
    context.strokeStyle = style;
    context.setLineDash(dashed ? [3, 3] : []);
    context.beginPath();
    context.moveTo(Math.round(x) + 0.5, 0);
    context.lineTo(Math.round(x) + 0.5, height);
    context.stroke();
    context.setLineDash([]);
  };

  context.strokeStyle = 'rgba(255,255,255,.08)';
  context.lineWidth = 1;
  for (const level of [0, 1]) {
    context.beginPath();
    context.moveTo(0, Math.round(yAt(level)) + 0.5);
    context.lineTo(width, Math.round(yAt(level)) + 0.5);
    context.stroke();
  }
  line(xAt(0), 'rgba(255,255,255,.22)');
  if (endOf(live) === Protocol.CELL_END.gone) line(xAt(span), 'rgba(255,255,255,.22)');
  if (endOf(live) !== Protocol.CELL_END.stay) {
    const gate = beatsOf(live, 'cellGate');
    for (let beats = gate; beats < span - 0.0001; beats += gate) line(xAt(beats), 'rgba(255,255,255,.3)', true);
  }

  const color = getComputedStyle(canvas).color;
  const trace = () => {
    context.beginPath();
    context.moveTo(0, yAt(levelOf(beatsAt(0))));
    for (let x = 0; x <= width; x++) {
      const beats = beatsAt(x);
      if (beats >= 0 && beatsAt(x - 1) < 0) {
        context.lineTo(xAt(0), yAt(0));
        context.lineTo(xAt(0), yAt(levelOf(0)));
      }
      context.lineTo(x, yAt(levelOf(beats)));
    }
  };
  trace();
  context.lineTo(width, yAt(0));
  context.lineTo(0, yAt(0));
  context.closePath();
  context.globalAlpha = 0.18;
  context.fillStyle = color;
  context.fill();
  context.globalAlpha = 1;
  trace();
  context.strokeStyle = color;
  context.lineWidth = 2;
  context.lineJoin = 'round';
  context.stroke();
}

export { beatsShown, levelAt, draw };
