(function (global) {
  'use strict';

  const Library = global.AuroraLibrary;
  const Preview = global.AuroraPreview;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { byId } = dom;

  const BEAT_FLASH = 0.15;

  let wall = null;
  const OVERLAYS = ['centers', 'fan', 'bend', 'arp', 'palette', 'spots', 'field'];
  const STORE_OVERLAYS = 'aurora.editor.overlays';

  const view = { flipped: false, overlays: readOverlays(), order: null, parOrder: null };
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
    Preview.clearTails(wall.wallState);
  }

  const restartBeats = () => { beats.position = 0; };

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

  function fire(index) {
    session.firing.index = session.editingOneshot() ? null : index;
    session.firing.at = beats.position;
  }

  function oneshotProgress() {
    const firing = session.firing;
    const fired = firing.at === null ? null : session.fired();
    if (!fired) return null;
    const length = Library.lengthBeats(fired);
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
    return Object.assign(session.oneshotInput(), { progress, beats: Library.lengthBeats(session.fired()) });
  }

  function drawBytes(bytes, oneshot) {
    const frame = Preview.render(bytes, beats.position, Math.round(performance.now()), wall.motion, wall.wallState, oneshot);
    Preview.draw(wall.context, wall.glow, frame, view.order, view.parOrder, view.flipped, wall.width, wall.height, view.overlays);
    return frame;
  }

  const draw = (named, oneshot) => drawBytes(Library.bytesFromNamed(session.sounding(named)), oneshot);

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
      drawBytes(session.over().base, oneshotInput(progress));
      Editor.routes.paintPlayheads(progress, live);
    } else {
      Editor.routes.paintPlayheads(draw(live, oneshotInput(progress)).lfo, live);
    }
    Editor.oneshots.paintFiring(progress);
    Editor.tracks.paint();
    requestAnimationFrame(frame);
  }

  function start() {
    beats.dots = [...document.querySelectorAll('#beats i')];
    wall = makeWall('wallMain', 300, 480);

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
    byId('wallFlip').addEventListener('click', () => {
      view.flipped = !view.flipped;
      byId('wallFlip').textContent = view.flipped ? 'pixel 0 at top' : 'pixel 0 at bottom';
    });

    beats.lastFrameAt = performance.now();
    requestAnimationFrame(frame);
  }

  Editor.wall = { start, clearTails, restartBeats, fire };
})(window);
