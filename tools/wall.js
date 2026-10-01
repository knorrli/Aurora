(function (global) {
  'use strict';

  const Protocol = global.AuroraProtocol;
  const Patch = global.AuroraPatch;
  const Library = global.AuroraLibrary;
  const Preview = global.AuroraPreview;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { byId } = dom;

  const BEAT_FLASH = 0.15;

  const walls = {};
  const OVERLAYS = ['centers', 'fan', 'bend', 'arp', 'palette', 'spots', 'field'];
  const STORE_OVERLAYS = 'aurora.editor.overlays';

  const view = { flipped: false, overlays: readOverlays(), order: null };
  const beats = { position: 0, lastFrameAt: 0, dots: [] };

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

  const lengthBeats = oneshot => Protocol.LFO_PERIODS[Patch.periodStep(oneshot.length)];

  function fire(index) {
    session.firing.index = session.editingOneshot() ? null : index;
    session.firing.at = beats.position;
  }

  function oneshotProgress() {
    const firing = session.firing;
    const fired = firing.at === null ? null : session.fired();
    if (!fired) return null;
    const length = lengthBeats(fired);
    if (beats.position < firing.at) firing.at = beats.position;
    if (beats.position - firing.at >= length) {
      if (!firing.repeat || !session.editingOneshot()) {
        firing.at = null;
        return null;
      }
      firing.at += length * Math.floor((beats.position - firing.at) / length);
    }
    return (beats.position - firing.at) / length;
  }

  function oneshotInput(progress) {
    if (progress === null) return null;
    return Object.assign(session.oneshotInput(), { progress, beats: lengthBeats(session.fired()) });
  }

  function drawBytes(wall, bytes, oneshot) {
    const frame = Preview.render(bytes, beats.position, Math.round(performance.now()), wall.motion, wall.wallState, oneshot);
    Preview.draw(wall.context, wall.glow, frame, view.order, view.flipped, wall.width, wall.height,
                 wall === walls.main ? view.overlays : {});
    return frame;
  }

  const draw = (wall, named, oneshot) => drawBytes(wall, Library.bytesFromNamed(session.sounding(named)), oneshot);

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
    const live = session.liveNamed();
    const progress = oneshotProgress();
    if (session.editingOneshot()) {
      drawBytes(walls.main, session.over().base, oneshotInput(progress));
      Editor.routes.paintPlayheads(progress, live);
    } else {
      Editor.routes.paintPlayheads(draw(walls.main, live, oneshotInput(progress)).lfo, live);
    }
    Editor.oneshots.paintFiring(progress);
    Editor.tracks.paint();
    if (session.isAboveBase()) {
      Preview.copyMotion(walls.base.motion, walls.main.motion);
      Preview.copyMotion(walls.layer.motion, walls.main.motion);
      draw(walls.base, Library.namedFromBytes(session.patch().base));
      draw(walls.layer, session.layerNamed());
    }
    requestAnimationFrame(frame);
  }

  function paint() {
    byId('compare').hidden = !session.isAboveBase();
    if (session.isAboveBase()) byId('layerCaption').textContent = Patch.LAYER_NAMES[session.layerIndex];
  }

  function start() {
    beats.dots = [...document.querySelectorAll('#beats i')];
    walls.main = makeWall('wallMain', 300, 480);
    walls.base = makeWall('wallBase', 150, 240);
    walls.layer = makeWall('wallLayer', 150, 240);

    byId('wallOrder').value = Preview.WALL_STRIP_ORDER.join(',');
    byId('wallOrder').addEventListener('input', readOrder);
    readOrder();
    for (const button of document.querySelectorAll('#wallOverlays button')) {
      const name = button.dataset.overlay;
      button.classList.toggle('on', view.overlays[name]);
      button.addEventListener('click', () => {
        view.overlays[name] = !view.overlays[name];
        button.classList.toggle('on', view.overlays[name]);
        writeOverlays();
      });
    }
    byId('wallFlip').addEventListener('click', () => {
      view.flipped = !view.flipped;
      byId('wallFlip').textContent = view.flipped ? 'pixel 0 at top' : 'pixel 0 at bottom';
    });

    beats.lastFrameAt = performance.now();
    requestAnimationFrame(frame);
  }

  Editor.wall = { start, paint, clearTails, restartBeats, fire };
})(window);
