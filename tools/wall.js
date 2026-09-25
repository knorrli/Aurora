(function (global) {
  'use strict';

  const Patch = global.AuroraPatch;
  const Library = global.AuroraLibrary;
  const Preview = global.AuroraPreview;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { byId } = dom;

  const BEAT_FLASH = 0.15;

  const walls = {};
  const view = { flipped: false, overlays: true, order: null };
  const beats = { position: 0, lastFrameAt: 0, dots: [] };

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
    return { context, glow, width, height, motion: Preview.makeMotion(), wallState: Preview.makeWallState() };
  }

  function clearTails() {
    for (const wall of Object.values(walls)) Preview.clearTails(wall.wallState);
  }

  const restartBeats = () => { beats.position = 0; };

  const defaultOrder = () => Preview.WALL_STRIP_ORDER.map(strip => strip - 1);

  function readOrder() {
    const input = byId('wallOrder');
    const strips = input.value.split(',').map(text => parseInt(text, 10) - 1);
    const valid = strips.length === Preview.STRIPS && new Set(strips).size === Preview.STRIPS
      && strips.every(strip => Number.isInteger(strip) && strip >= 0 && strip < Preview.STRIPS);
    input.classList.toggle('invalid', !valid);
    view.order = valid ? strips : defaultOrder();
  }

  function draw(wall, named) {
    const frame = Preview.render(Library.bytesFromNamed(session.sounding(named)), beats.position,
                                 wall.motion, wall.wallState);
    Preview.draw(wall.context, wall.glow, frame, view.order, view.flipped, wall.width, wall.height,
                 view.overlays && wall === walls.main);
  }

  function paintBeats() {
    const beat = Math.floor(beats.position);
    const hit = beats.position - beat < BEAT_FLASH;
    beats.dots.forEach((dot, index) => {
      const on = index === beat % beats.dots.length;
      dot.classList.toggle('on', on);
      dot.classList.toggle('hit', on && hit);
    });
  }

  function frame() {
    const now = performance.now();
    beats.position += (now - beats.lastFrameAt) / 60000 * Editor.midi.bpm();
    beats.lastFrameAt = now;
    paintBeats();
    draw(walls.main, session.liveNamed());
    Editor.tracks.paint();
    if (session.isTarget()) {
      Preview.copyMotion(walls.base.motion, walls.main.motion);
      Preview.copyMotion(walls.target.motion, walls.main.motion);
      draw(walls.base, Library.namedFromBytes(session.patch().base));
      draw(walls.target, session.targetNamed());
    }
    requestAnimationFrame(frame);
  }

  function paint() {
    byId('compare').hidden = !session.isTarget();
    if (session.isTarget()) byId('targetCaption').textContent = Patch.PART_NAMES[session.partIndex];
  }

  function start() {
    beats.dots = [...document.querySelectorAll('#beats i')];
    walls.main = makeWall('wallMain', 300, 480);
    walls.base = makeWall('wallBase', 150, 240);
    walls.target = makeWall('wallTarget', 150, 240);

    byId('wallOrder').value = Preview.WALL_STRIP_ORDER.join(',');
    byId('wallOrder').addEventListener('input', readOrder);
    readOrder();
    byId('wallOverlays').classList.toggle('on', view.overlays);
    byId('wallOverlays').addEventListener('click', () => {
      view.overlays = !view.overlays;
      byId('wallOverlays').classList.toggle('on', view.overlays);
    });
    byId('wallFlip').addEventListener('click', () => {
      view.flipped = !view.flipped;
      byId('wallFlip').textContent = view.flipped ? 'pixel 0 at top' : 'pixel 0 at bottom';
    });

    beats.lastFrameAt = performance.now();
    requestAnimationFrame(frame);
  }

  Editor.wall = { start, paint, clearTails, restartBeats };
})(window);
