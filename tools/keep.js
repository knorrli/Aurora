(function (global) {
  'use strict';

  const Patch = global.AuroraPatch;
  const Library = global.AuroraLibrary;
  const Editor = global.AuroraEditor;
  const { session, dom } = Editor;
  const { byId } = dom;

  const NEW_PATCH = 'new';

  const labelOf = name => {
    if (Patch.CONTROLS[name]) return Patch.CONTROLS[name].label;
    const route = Patch.ROUTES.find(candidate => candidate.fields.includes(name));
    return route ? route.name : name;
  };

  function report(layer, leftOnBase) {
    const kept = `kept as ${Patch.LAYER_NAMES[layer]}`;
    if (!leftOnBase.length) {
      Editor.say(kept, 'ok');
      return;
    }
    const labels = [...new Set(leftOnBase.map(labelOf))].join(', ');
    Editor.say(`${kept}; ${labels} stayed on the base — switches belong to the whole patch`, 'warn');
  }

  function keepAsLayer(layer) {
    const named = session.liveNamed();
    const patch = session.editing();
    const leftOnBase = Patch.isAboveBase(layer) ? Library.keepAsLayer(patch, layer, named) : [];
    if (!Patch.isAboveBase(layer)) Library.keepAsBase(patch, named);
    session.resetPreview();
    Editor.transition.rebuild();
    session.changed();
    report(layer, leftOnBase);
  }

  function keepAsNewPatch() {
    const named = session.liveNamed();
    const current = session.patch();
    if (!Editor.rail.leaveDraft()) return;
    const patch = Library.newPatch(current.name);
    Object.assign(patch, { transitionTime: current.transitionTime, accentTime: current.accentTime });
    Library.keepAsBase(patch, named);
    Editor.show(null, patch);
    Editor.say('kept as a new patch — save it into a slot', 'ok');
  }

  function wire() {
    const select = byId('keepAs');
    dom.setOptions(select, [
      ['', 'keep as…'],
      ...Patch.LAYERS.map(layer => [layer, Patch.LAYER_NAMES[layer]]),
      [NEW_PATCH, 'new patch'],
    ], '');
    select.addEventListener('change', () => {
      const choice = select.value;
      select.value = '';
      if (choice === NEW_PATCH) keepAsNewPatch();
      else if (choice !== '') keepAsLayer(+choice);
    });
  }

  Editor.keep = { wire };
})(window);
