import { byId, element } from './dom.js';

export function say(text, className) {
  const at = new Date().toLocaleTimeString('en-US', { hour12: false });
  byId('status').replaceChildren(element('span', 'cc', at), '  ', element('span', className || '', text));
}
