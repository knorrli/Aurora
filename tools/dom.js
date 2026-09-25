(function (global) {
  'use strict';

  const byId = id => document.getElementById(id);

  function element(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text != null) node.textContent = text;
    return node;
  }

  function option(value, text) {
    const node = element('option', null, text);
    node.value = value;
    return node;
  }

  function rangeInput(max) {
    const input = element('input');
    input.type = 'range';
    input.min = 0;
    input.max = max;
    input.step = 1;
    return input;
  }

  function labeled(tag, title, detail) {
    const node = element(tag);
    node.append(element('b', null, title), element('span', 'cc', detail));
    return node;
  }

  function fillLabeled(node, title, detail) {
    if (node.firstChild.textContent !== title) node.firstChild.textContent = title;
    if (node.lastChild.textContent !== detail) node.lastChild.textContent = detail;
  }

  function setOptions(select, options, value) {
    select.replaceChildren(...options.map(([optionValue, text]) => option(optionValue, text)));
    select.value = value;
  }

  global.AuroraEditor = global.AuroraEditor || {};
  global.AuroraEditor.dom = { byId, element, option, rangeInput, labeled, fillLabeled, setOptions };
})(window);
