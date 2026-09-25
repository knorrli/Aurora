(function (global) {
  'use strict';

  const Patch = global.AuroraPatch;
  const Preview = global.AuroraPreview;
  const Editor = global.AuroraEditor;

  const HANDLE_RADIUS = 8;
  const READ_AT_PARS = new Set(['parValue', 'parHueOffset', 'parSaturation']);

  const rangeOf = input => [input.min === '' ? 0 : +input.min, input.max === '' ? 100 : +input.max];

  function along(input, value) {
    const [min, max] = rangeOf(input);
    return `calc(${HANDLE_RADIUS}px + ${(value - min) / (max - min)} * (100% - ${2 * HANDLE_RADIUS}px))`;
  }

  function reaching(input, value) {
    const [min, max] = rangeOf(input);
    if (value <= min) return '0%';
    if (value >= max) return '100%';
    return along(input, value);
  }

  const stops = (...bands) => 'linear-gradient(90deg, '
    + bands.map(([color, from, to]) => `${color} ${from}, ${color} ${to}`).join(', ') + ')';

  const offset = (at, pixels) => `calc(${at} + ${pixels}px)`;

  const pointLayer = at => stops(['transparent', '0%', offset(at, -1)], ['var(--bg)', offset(at, -1), offset(at, 1)],
                                 ['transparent', offset(at, 1), '100%']);

  const markLayer = at => stops(['transparent', '0%', offset(at, -2)], ['var(--bg)', offset(at, -2), offset(at, -1)],
                                ['#fff', offset(at, -1), offset(at, 1)], ['var(--bg)', offset(at, 1), offset(at, 2)],
                                ['transparent', offset(at, 2), '100%']);

  function swingOf(name) {
    const cc = Patch.CC[name];
    const reach = Preview.routeReach(cc);
    if (!reach) return null;
    const [low, high] = reach;
    const spans = [[Math.max(0, low), Math.min(127, high)]];
    if (low < 0) spans.push([low + 128, 127]);
    if (high > 127) spans.push([0, high - 128]);
    return { spans, marks: READ_AT_PARS.has(name) ? Preview.controlAtPars(cc) : Preview.controlAtStrips(cc) };
  }

  function paintTrack(input, swing, points, circular) {
    const layers = [];
    if (swing) for (const value of swing.marks) layers.push(markLayer(along(input, value)));
    for (const value of points) layers.push(pointLayer(along(input, value)));
    if (swing) {
      for (const [low, high] of swing.spans) {
        const from = reaching(input, low), to = reaching(input, high);
        layers.push(stops(['transparent', '0%', from], ['var(--lfo)', from, to], ['transparent', to, '100%']));
      }
    }
    const at = reaching(input, +input.value);
    layers.push(circular ? stops(['var(--rest)', '0%', '100%'])
                         : stops(['var(--fill)', '0%', at], ['var(--rest)', at, '100%']));
    const track = layers.join(', ');
    if (input.dataset.track !== track) {
      input.dataset.track = track;
      input.style.setProperty('--track', track);
    }
  }

  function paint() {
    for (const input of document.querySelectorAll('input[type=range]')) {
      const row = Editor.rows.rows[input.closest('.row') && input.closest('.row').dataset.name];
      if (row) paintTrack(input, swingOf(row.control.name), row.points, row.circular);
      else paintTrack(input, null, [], false);
    }
  }

  Editor.tracks = { along, paint };
})(window);
