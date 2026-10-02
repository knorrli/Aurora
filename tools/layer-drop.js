import * as dom from './dom.js';
import * as Patch from './patch.js';
import { session } from './session.js';
import { say } from './status.js';

const { element } = dom;

let carried = null;
let picked = null;

function carry(piece) {
  carried = piece;
  document.body.classList.toggle('carrying', !!piece);
}

function pick(grip, piece) {
  drop();
  picked = grip;
  grip.closest('.movable').classList.add('picked');
  carry(piece);
  say(`picked up ${piece.label} — click a layer, or Escape`);
}

function drop() {
  if (picked) picked.closest('.movable').classList.remove('picked');
  picked = null;
  carry(null);
}

function moveTo(layer) {
  const { names, label } = carried;
  drop();
  const moved = session.moveToLayer(names, layer);
  if (moved.length) say(`moved ${label} to ${Patch.LAYER_NAMES[layer]}`, 'ok');
  else say(`${label} is already at rest on the base; nothing to move`, 'warn');
}

function source(handle, describe) {
  const grip = element('span', 'grip', '⠿');
  grip.draggable = true;
  handle.classList.add('movable');
  handle.prepend(grip);
  grip.addEventListener('dragstart', event => {
    if (session.isAboveBase()) {
      event.preventDefault();
      return;
    }
    drop();
    carry(describe());
    event.dataTransfer.effectAllowed = 'move';
  });
  grip.addEventListener('dragend', () => carry(null));
  grip.addEventListener('click', event => {
    event.stopPropagation();
    if (picked === grip) drop();
    else if (!session.isAboveBase()) pick(grip, describe());
  });
}

function target(tab, layer) {
  tab.classList.add('takes-drop');
  tab.addEventListener('dragover', event => {
    if (!carried) return;
    event.preventDefault();
    tab.classList.add('drop');
  });
  tab.addEventListener('dragleave', () => tab.classList.remove('drop'));
  tab.addEventListener('drop', event => {
    event.preventDefault();
    tab.classList.remove('drop');
    if (carried) moveTo(layer);
  });
  tab.addEventListener('click', event => {
    if (!picked) return;
    event.stopImmediatePropagation();
    moveTo(layer);
  }, true);
}

document.addEventListener('click', event => {
  if (picked && !event.target.closest('.takes-drop, .grip')) drop();
}, true);
document.addEventListener('keydown', event => { if (event.key === 'Escape' && picked) drop(); });

export { source, target };
