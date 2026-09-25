// tools/library.js — a patch, a library, and the wire that carries them.
//
// The byte layout is shared/aurora_protocol.h § "System Exclusive" and
// § "What a patch is made of". The reasoning behind the four decisions that
// shape it is DESIGN.md § "How a library gets there": a sync replaces the
// whole library, it is strictly ordered, it stages and commits on one
// rename, and nothing is acknowledged but the two ends.
//
// A library is P.PATCH_MAX fixed slots, any of them empty: `lib.slots[n]` is
// the patch Program Change n plays, or null. On the wire and in a file only
// the filled ones travel, each carrying its slot.

(function (global) {
  'use strict';

  const A = global.AuroraCC;
  const P = global.AuroraPatch;

  const SYSEX_ID = 0x7D, SIG_A = 0x41, SIG_B = 0x55;

  const T = {
    SYNC_BEGIN: 0x01, PATCH_HEAD: 0x02, PATCH_SET: 0x03, SYNC_COMMIT: 0x04,
    SYNC_ABORT: 0x05, QUERY_LIBRARY: 0x06, QUERY_PATCH: 0x07,
    ACK: 0x40, LIBRARY_INFO: 0x41, PATCH_HEAD_OUT: 0x42, PATCH_SET_OUT: 0x43,
  };

  const STATUS = ['ok', 'patch format not understood', 'out of sequence',
                  'incomplete', 'storage failed', 'slot or length out of range'];
  const LIB_STATE = ['a synced library is live', 'empty — running compiled defaults',
                     'something stored that cannot be read'];

  const HEAD_LEN = 4 + P.NAME_LEN;

  // ---- a patch in the editor's hands -------------------------------------

  const emptySet = () => new Array(P.CC_COUNT).fill(0);

  function setFromNamed(named) {
    const bytes = emptySet();
    for (const name of P.NAMES) {
      if (name in named) bytes[P.CC[name]] = P.clamp7(named[name]);
    }
    return bytes;
  }

  function namedFromSet(bytes) {
    const named = {};
    for (const name of P.NAMES) named[name] = bytes[P.CC[name]] | 0;
    return named;
  }

  const readCC = (bytes, name) => bytes[P.CC[name]] | 0;
  const writeCC = (bytes, name, value) => { bytes[P.CC[name]] = P.clamp7(value); };

  // A far end is the base plus the handful of controls it overrides, and not a
  // second copy of all 72. Two reasons, and the second is the one that bites.
  //
  // It is what a far end is. Going through what one actually changes: roughly
  // four controls each for Color, Extent and Motion, and everything else
  // byte-identical. Storing the identical part again says the patch has 72
  // decisions in it where it has four.
  //
  // And a copy goes stale. Dial the base after building a far end and the copy
  // keeps the old value for every control you have since touched, so the far
  // end quietly stops being "this patch, but more" and becomes a different
  // patch that happens to share a name. Inheriting means dialing the base
  // moves the far ends with it, everywhere you did not deliberately push.
  //
  // The wire and the file still carry five whole sets: materialize() is where
  // that happens, and fromWire() is the same trick backwards.

  function newPatch(name) {
    return {
      name: (name || 'untitled').slice(0, P.NAME_LEN),
      rampJourney: P.periodByte(4),   // four beats, one bar
      rampAccent: P.periodByte(10),   // half a beat
      base: setFromNamed(P.DEFAULT),
      overrides: [null, {}, {}, {}, {}],
    };
  }

  const clonePatch = p => ({
    name: p.name,
    rampJourney: p.rampJourney, rampAccent: p.rampAccent,
    base: p.base.slice(),
    overrides: p.overrides.map(o => (o ? Object.assign({}, o) : null)),
  });

  // A switch is never an override: a patch and its own morph target share
  // switches, which DESIGN.md § "Switches belong to the patch" states as an
  // authoring rule rather than a runtime one. So every set takes the base's.
  function materialize(patch, setIndex) {
    const bytes = patch.base.slice();
    const over = patch.overrides[setIndex];
    if (over) {
      for (const [name, v] of Object.entries(over)) bytes[P.CC[name]] = P.clamp7(v);
    }
    return bytes;
  }

  const overriddenIn = (patch, setIndex) =>
    (patch.overrides[setIndex] ? Object.keys(patch.overrides[setIndex]) : []);

  function toWire(patch) {
    return {
      name: patch.name,
      rampJourney: patch.rampJourney, rampAccent: patch.rampAccent,
      sets: Array.from({ length: P.SETS }, (_, i) => materialize(patch, i)),
    };
  }

  // A far end whose switches differ from the base is read as sharing the
  // base's, which is the authoring rule above. Nothing else is lost: a set
  // that differs in a continuous CC comes back as an override on exactly that
  // CC, so a library pulled off the brain and pushed straight back is the same
  // library.
  function fromWire(w) {
    const base = w.sets[P.SET_BASE].slice();
    const overrides = [null];
    for (let i = 1; i < P.SETS; i++) {
      const o = {};
      for (const name of P.CONTINUOUS) {
        const v = w.sets[i][P.CC[name]] | 0;
        if (v !== (base[P.CC[name]] | 0)) o[name] = v;
      }
      overrides.push(o);
    }
    return {
      name: w.name,
      rampJourney: w.rampJourney, rampAccent: w.rampAccent, base, overrides,
    };
  }

  const emptySlots = () => new Array(P.PATCH_MAX).fill(null);
  const filledSlots = lib => lib.slots.flatMap((p, slot) => (p ? [slot] : []));

  const libToWire = lib => ({
    patchFormat: P.PATCH_FORMAT,
    keymap: lib.keymap.slice(),
    patches: filledSlots(lib).map(slot => Object.assign({ slot }, toWire(lib.slots[slot]))),
  });

  // A library written before slots existed numbers its patches in order.
  function libFromWire(w) {
    const slots = emptySlots();
    w.patches.forEach((p, i) => { slots[p.slot ?? i] = fromWire(p); });
    return { patchFormat: P.PATCH_FORMAT, keymap: w.keymap.slice(), slots };
  }

  function newLibrary() {
    const slots = emptySlots();
    slots[0] = newPatch('first');
    return { patchFormat: P.PATCH_FORMAT, keymap: new Array(P.KEYS).fill(0), slots };
  }

  // A morph interpolates the continuous CCs and never a switch, and within one
  // patch a far end has no switches of its own to take — DESIGN.md
  // § "Switches belong to the patch". `switchesFrom` is what an accent needs:
  // in performance it plays with the SOURCE patch's switches, because the
  // destination's have not landed yet and will not until the key is released.
  function blend(baseSet, farSet, position, switchesFrom) {
    const named = {};
    for (const name of P.CONTINUOUS) {
      const a = baseSet[P.CC[name]] | 0, b = farSet[P.CC[name]] | 0;
      named[name] = Math.round(a + (b - a) * position);
    }
    const sw = switchesFrom || baseSet;
    for (const name of P.SWITCHES) named[name] = sw[P.CC[name]] | 0;
    return named;
  }

  // Three faders at once, plus the accent if a key is being held.
  //
  // **The departures add.** Each surface contributes its position times the
  // distance from the patch to its own far end, and the sum is clamped per
  // byte. One surface alone is exactly the blend above, so nothing changes for
  // the case that already worked.
  //
  // Adding is the reading the rest of Aurora already uses — the color layer's
  // three sources push on the same three qualities and their pushes add — and
  // it is the one that does nothing surprising in the common case, which is
  // two far ends moving different controls. Where two far ends do move the
  // same control they fight, and summing is the honest answer to that rather
  // than a rule about which one wins.
  //
  // UNTESTED AND UNBUILT: the brain does not do this yet, so this is the
  // editor proposing a rule rather than showing one. See docs/editor.md.
  function mix(patch, positions, switchesFrom) {
    const base = patch.base;
    const named = {};
    for (const name of P.CONTINUOUS) {
      const from = base[P.CC[name]] | 0;
      let value = from;
      for (const [setIndex, position] of positions) {
        if (!position) continue;
        const over = patch.overrides[setIndex];
        const to = over && over[name] !== undefined ? over[name] : from;
        value += position * (to - from);
      }
      named[name] = P.clamp7(Math.round(value));
    }
    const sw = switchesFrom || base;
    for (const name of P.SWITCHES) named[name] = sw[P.CC[name]] | 0;
    return named;
  }

  // Moving a far end onto another surface, or trading two. Only the override
  // map travels: the base is the patch and does not move with them.
  function moveOverrides(patch, from, to, swap) {
    const a = patch.overrides[from] || {};
    const b = patch.overrides[to] || {};
    patch.overrides[to] = Object.assign({}, a);
    patch.overrides[from] = swap ? Object.assign({}, b) : {};
  }

  const copyOverrides = (patch, from, to) => {
    patch.overrides[to] = Object.assign({}, patch.overrides[from] || {});
  };

  // ---- the file ----------------------------------------------------------
  //
  // Hand-laid out rather than JSON.stringify'd so that one parameter set is
  // one line: a changed patch is then a handful of changed lines in a diff
  // instead of one enormous one.

  function serialize(lib) {
    const out = ['{'];
    out.push('  "aurora": "patch library",');
    out.push(`  "patchFormat": ${lib.patchFormat || P.PATCH_FORMAT},`);
    out.push(`  "savedAt": ${JSON.stringify(new Date().toISOString())},`);
    out.push(`  "keymap": [${lib.keymap.join(', ')}],`);
    out.push('  "patches": [');
    lib.patches.forEach((p, i) => {
      out.push('    {');
      out.push(`      "slot": ${p.slot}, "name": ${JSON.stringify(p.name)},`);
      out.push(`      "rampJourney": ${p.rampJourney}, "rampAccent": ${p.rampAccent},`);
      out.push('      "sets": [');
      p.sets.forEach((set, n) => out.push(`        [${set.join(',')}]${n < P.SETS - 1 ? ',' : ''}`));
      out.push('      ]');
      out.push(`    }${i < lib.patches.length - 1 ? ',' : ''}`);
    });
    out.push('  ]');
    out.push('}');
    return out.join('\n');
  }

  // A file is checked before any of it is sent, because SYNC_BEGIN declares a
  // slot map the brain then waits for: discovering a bad patch halfway leaves
  // the sync open, and the previous library is only safe because nothing
  // commits.
  function validate(lib) {
    if (!lib || typeof lib !== 'object') return 'not a library file';
    if (lib.patchFormat !== P.PATCH_FORMAT)
      return `patch format ${lib.patchFormat}, this page speaks ${P.PATCH_FORMAT}`;
    if (!Array.isArray(lib.patches) || !lib.patches.length) return 'no patches in it';
    if (!Array.isArray(lib.keymap) || lib.keymap.length !== P.KEYS
        || lib.keymap.some(k => !Number.isInteger(k) || k < 0 || k >= P.PATCH_MAX))
      return `the keymap should be ${P.KEYS} slots`;
    const taken = new Set();
    for (let i = 0; i < lib.patches.length; i++) {
      const p = lib.patches[i];
      const slot = p.slot ?? i;
      if (!Number.isInteger(slot) || slot < 0 || slot >= P.PATCH_MAX)
        return `patch ${i} is in slot ${slot}, and the slots run 0–${P.PATCH_MAX - 1}`;
      if (taken.has(slot)) return `two patches in slot ${slot}`;
      taken.add(slot);
      if (!Array.isArray(p.sets) || p.sets.length !== P.SETS)
        return `patch ${i} has ${p.sets && p.sets.length} parameter sets, expected ${P.SETS}`;
      for (let n = 0; n < P.SETS; n++) {
        if (!Array.isArray(p.sets[n]) || p.sets[n].length !== P.CC_COUNT)
          return `patch ${i} set ${n} is not ${P.CC_COUNT} bytes`;
        if (p.sets[n].some(v => !Number.isInteger(v) || v < 0 || v > 127))
          return `patch ${i} set ${n} holds something that is not a 7-bit value`;
      }
    }
    return null;
  }

  // ---- the wire ----------------------------------------------------------

  // The brain's side of the wire still carries a pattern and a palette byte.
  // Every patch runs the generator, and its palette is CC_PALETTE in the sets.
  function headBytes(p) {
    const name = String(p.name || '').padEnd(P.NAME_LEN, ' ').slice(0, P.NAME_LEN);
    const out = [A.PRESET_GENERATOR, 0,
                 p.rampJourney & 0x7F, p.rampAccent & 0x7F];
    for (let i = 0; i < P.NAME_LEN; i++) out.push(name.charCodeAt(i) & 0x7F);
    return out;
  }

  const nameOf = head => head.slice(4).map(c => String.fromCharCode(c)).join('').trim();

  const MAP_LEN = Math.ceil(P.PATCH_MAX / 7);

  function slotMapBytes(slots) {
    const map = new Array(MAP_LEN).fill(0);
    for (const slot of slots) map[Math.floor(slot / 7)] |= 1 << (slot % 7);
    return map;
  }

  const slotsInMap = map => Array.from({ length: P.PATCH_MAX }, (_, slot) => slot)
    .filter(slot => (map[Math.floor(slot / 7)] >> (slot % 7)) & 1);

  function Link() {
    this.input = null;
    this.output = null;
    this.access = null;
    this.waiters = [];
    this.onports = null;
  }

  Link.prototype.frame = function (type, payload) {
    return new Uint8Array([0xF0, SYSEX_ID, SIG_A, SIG_B, type, ...(payload || []), 0xF7]);
  };

  Link.prototype.send = function (type, payload) {
    if (!this.output) throw new Error('no MIDI output selected');
    this.output.send(this.frame(type, payload));
  };

  Link.prototype.sendCC = function (cc, value) {
    if (this.output) this.output.send([0xB0, cc & 0x7F, P.clamp7(value)]);
  };

  Link.prototype.sendPC = function (pc) {
    if (this.output) this.output.send([0xC0, pc & 0x7F]);
  };

  Link.prototype.sendRaw = function (bytes, at) {
    if (this.output) this.output.send(bytes, at);
  };

  Link.prototype.handle = function (e) {
    const d = e.data;
    if (d.length < 6 || d[0] !== 0xF0 || d[1] !== SYSEX_ID) return;
    if (d[2] !== SIG_A || d[3] !== SIG_B) return;
    const msg = { type: d[4], payload: Array.from(d.slice(5, d.length - 1)) };
    for (let i = this.waiters.length - 1; i >= 0; i--) {
      if (this.waiters[i].wants.includes(msg.type)) {
        this.waiters.splice(i, 1)[0].resolve(msg);
        return;
      }
    }
  };

  Link.prototype.expect = function (wants, ms) {
    const waiters = this.waiters;
    return new Promise((resolve, reject) => {
      const w = { wants: [].concat(wants), resolve };
      waiters.push(w);
      setTimeout(() => {
        const i = waiters.indexOf(w);
        if (i >= 0) { waiters.splice(i, 1); reject(new Error('the brain did not answer')); }
      }, ms || 4000);
    });
  };

  Link.prototype.ackFor = async function (type, payload) {
    this.send(type, payload);
    const m = await this.expect([T.ACK]);
    return { inReplyTo: m.payload[0], status: m.payload[1] };
  };

  Link.prototype.queryLibrary = async function () {
    this.send(T.QUERY_LIBRARY);
    const m = await this.expect([T.LIBRARY_INFO]);
    const [major, minor, format, state, ...rest] = m.payload;
    const slots = slotsInMap(rest.slice(P.KEYS, P.KEYS + MAP_LEN));
    return {
      protocol: major + '.' + minor, format, state, slots, count: slots.length,
      stateText: LIB_STATE[state] || `unknown state ${state}`,
      keymap: rest.slice(0, P.KEYS),
    };
  };

  // Takes the wire shape, so the patches go out lowest slot first as the
  // brain requires.
  Link.prototype.push = async function (lib, onProgress) {
    const patches = lib.patches.slice().sort((a, b) => a.slot - b.slot);
    const count = patches.length;

    const begun = await this.ackFor(T.SYNC_BEGIN, [lib.patchFormat || P.PATCH_FORMAT,
      ...lib.keymap.map(k => k & 0x7F), ...slotMapBytes(patches.map(p => p.slot))]);
    if (begun.status !== 0) return { ok: false, where: 'sync begin', status: begun.status };

    patches.forEach((p, i) => {
      this.send(T.PATCH_HEAD, [p.slot, ...headBytes(p)]);
      for (let set = 0; set < P.SETS; set++) {
        this.send(T.PATCH_SET, [p.slot, set, ...p.sets[set]]);
      }
      if (onProgress) onProgress(i + 1, count);
    });

    const done = await this.ackFor(T.SYNC_COMMIT);
    return { ok: done.status === 0, where: 'commit', status: done.status, count };
  };

  Link.prototype.readPatch = async function (slot) {
    this.send(T.QUERY_PATCH, [slot]);
    const first = await this.expect([T.PATCH_HEAD_OUT, T.ACK]);
    if (first.type === T.ACK) return { error: first.payload[1] };

    const head = first.payload.slice(1);
    const sets = [];
    for (let s = 0; s < P.SETS; s++) {
      const m = await this.expect([T.PATCH_SET_OUT, T.ACK]);
      if (m.type === T.ACK) return { error: m.payload[1] };
      sets.push(m.payload.slice(2));
    }
    return {
      patch: {
        name: nameOf(head),
        rampJourney: head[2], rampAccent: head[3], sets,
      },
    };
  };

  // The export path. A library pulled back off the brain after the editor's
  // machine is gone has to be a library rather than a heap of anonymous
  // looks, which is the whole reason a patch carries a name the brain never
  // reads.
  Link.prototype.pull = async function (onProgress) {
    const info = await this.queryLibrary();
    if (info.state !== 0 || !info.count) return { error: info.stateText, info };

    const patches = [];
    for (const slot of info.slots) {
      const got = await this.readPatch(slot);
      if (got.error !== undefined) {
        return { error: `slot ${slot}: ${STATUS[got.error] || got.error}`, info };
      }
      patches.push(Object.assign({ slot }, got.patch));
      if (onProgress) onProgress(patches.length, info.count);
    }
    return { lib: { patchFormat: P.PATCH_FORMAT, keymap: info.keymap, patches }, info };
  };

  Link.prototype.open = async function () {
    if (!navigator.requestMIDIAccess) throw new Error('Web MIDI unavailable — use Chrome or Edge');
    // Without sysex: true the browser silently drops every SysEx this page
    // sends, so the permission prompt is not optional.
    this.access = await navigator.requestMIDIAccess({ sysex: true });
    this.access.onstatechange = () => { if (this.onports) this.onports(this.ports()); };
    return this.ports();
  };

  Link.prototype.ports = function () {
    return this.access ? [...this.access.outputs.values()] : [];
  };

  // Web MIDI keeps the two directions apart, so the reply arrives on a
  // separate port object that has to be matched to the output by name.
  Link.prototype.choose = function (id) {
    this.output = this.access.outputs.get(id) || null;
    [...this.access.inputs.values()].forEach(i => { i.onmidimessage = null; });
    this.input = null;
    if (this.output) {
      this.input = [...this.access.inputs.values()].find(i => i.name === this.output.name)
                || [...this.access.inputs.values()].find(i => /teensy|aurora/i.test(i.name))
                || null;
      if (this.input) this.input.onmidimessage = e => this.handle(e);
    }
    return { output: this.output, input: this.input };
  };

  Link.prototype.preferred = function () {
    const outs = this.ports();
    return outs.find(o => /teensy|aurora/i.test(o.name)) || outs[0] || null;
  };

  global.AuroraLibrary = {
    T, STATUS, LIB_STATE, HEAD_LEN,
    emptySet, setFromNamed, namedFromSet, readCC, writeCC,
    newPatch, clonePatch, newLibrary, emptySlots, filledSlots, materialize, overriddenIn,
    mix, moveOverrides, copyOverrides,
    toWire, fromWire, libToWire, libFromWire, blend,
    serialize, validate, headBytes, nameOf, slotMapBytes, slotsInMap, Link,
  };
})(window);
