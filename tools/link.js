import { Protocol } from './cc.js';
import * as LibraryFile from './library-file.js';


const TYPE = Protocol.SYSEX_TYPE;
const STATUS = Protocol.SYSEX_STATUS;
const SYSEX_START = 0xF0, SYSEX_END = 0xF7;
const CONTROL_CHANGE = 0xB0, PROGRAM_CHANGE = 0xC0;
const CHANNEL = Protocol.MIDI_CHANNEL - 1;
const ANSWER_MILLISECONDS = 4000;

const STATUS_TEXT = {
  ok: 'ok',
  errorFormat: 'patch format not understood',
  errorSequence: 'out of sequence',
  errorIncomplete: 'incomplete',
  errorStorage: 'storage failed',
  errorRange: 'slot or length out of range',
};
const LIBRARY_STATE_TEXT = {
  stored: 'a synced library is live',
  empty: 'empty',
  unreadable: 'something stored that cannot be read',
};
const textFor = (codes, texts, value, unknown) => {
  const key = Object.keys(codes).find(name => codes[name] === value);
  return key ? texts[key] : `${unknown} ${value}`;
};
const statusText = status => textFor(STATUS, STATUS_TEXT, status, 'unknown status');
const libraryStateText = state =>
  textFor(Protocol.LIBRARY_STATE, LIBRARY_STATE_TEXT, state, 'unknown state');

class Link {
  constructor(name, { prefers, avoids = null, fallsBack = false }) {
    this.name = name;
    this.prefers = prefers;
    this.avoids = avoids;
    this.fallsBack = fallsBack;
    this.access = null;
    this.output = null;
    this.input = null;
    this.waiters = [];
    this.busy = false;
    this.onports = null;
  }

  async open() {
    if (!navigator.requestMIDIAccess) throw new Error('Web MIDI unavailable — use Chrome');
    this.access = await navigator.requestMIDIAccess({ sysex: true });
    this.access.onstatechange = () => { if (this.onports) this.onports(); };
  }

  outputs() {
    return this.access
      ? [...this.access.outputs.values()]
        .filter(port => port.state !== 'disconnected' && !(this.avoids && this.avoids.test(port.name))) : [];
  }

  preferred() {
    const outputs = this.outputs();
    return outputs.find(port => this.prefers.test(port.name)) || (this.fallsBack && outputs[0]) || null;
  }

  choose(id) {
    const inputs = this.access ? [...this.access.inputs.values()] : [];
    if (this.input) this.input.onmidimessage = null;
    this.output = this.outputs().find(port => port.id === id) || null;
    this.input = null;
    if (this.output) {
      const live = inputs.filter(port => port.state !== 'disconnected');
      this.input = live.find(port => port.name === this.output.name)
        || live.find(port => this.prefers.test(port.name)) || null;
      if (this.input) this.input.onmidimessage = event => this.receive(event.data);
    }
    return { output: this.output, input: this.input };
  }

  keepOrPrefer() {
    if (this.output && this.outputs().includes(this.output)) return false;
    const hadOutput = !!this.output;
    const preferred = this.preferred();
    this.choose(preferred ? preferred.id : null);
    return hadOutput || !!this.output;
  }

  transmit(bytes, at) {
    if (!this.output) return false;
    try {
      this.output.send(bytes, at);
      return true;
    } catch {
      return false;
    }
  }

  sendCC(cc, value) { return this.transmit([CONTROL_CHANGE | CHANNEL, cc & 0x7F, value & 0x7F]); }
  sendProgram(program) { return this.transmit([PROGRAM_CHANGE | CHANNEL, program & 0x7F]); }
  sendRaw(bytes, at) { return this.transmit(bytes, at); }

  sendSysEx(type, payload) {
    if (!this.output) throw new Error('no MIDI output selected');
    this.output.send([SYSEX_START, Protocol.SYSEX_ID, Protocol.SYSEX_SIGNATURE_A,
                      Protocol.SYSEX_SIGNATURE_B, type, ...(payload || []), SYSEX_END]);
  }

  receive(data) {
    if (data.length < Protocol.SYSEX_HEADER_LENGTH + 1 || data[0] !== SYSEX_START) return;
    if (data[1] !== Protocol.SYSEX_ID) return;
    if (data[2] !== Protocol.SYSEX_SIGNATURE_A || data[3] !== Protocol.SYSEX_SIGNATURE_B) return;
    const message = {
      type: data[4],
      payload: Array.from(data.slice(Protocol.SYSEX_HEADER_LENGTH, data.length - 1)),
    };
    const waiter = this.waiters.find(candidate => candidate.accepts(message));
    if (!waiter) return;
    this.waiters.splice(this.waiters.indexOf(waiter), 1);
    waiter.resolve(message);
  }

  expect(accepts) {
    return new Promise((resolve, reject) => {
      const waiter = { accepts, resolve };
      this.waiters.push(waiter);
      setTimeout(() => {
        const at = this.waiters.indexOf(waiter);
        if (at < 0) return;
        this.waiters.splice(at, 1);
        reject(new Error(`${this.name} did not answer`));
      }, ANSWER_MILLISECONDS);
    });
  }

  static brain() {
    return new Link('the brain', { prefers: /aurora brain|teensy/i, avoids: /aurora controller/i, fallsBack: true });
  }

  static controller() { return new Link('the controller', { prefers: /aurora controller/i }); }

  static isAckFor(type, message) {
    return message.type === TYPE.ack && message.payload[0] === type;
  }

  async request(type, payload) {
    const answer = this.expect(message => Link.isAckFor(type, message));
    this.sendSysEx(type, payload);
    return (await answer).payload[1];
  }

  async exclusively(work) {
    if (this.busy) throw new Error(`${this.name} is still answering the last request`);
    this.busy = true;
    try {
      return await work();
    } finally {
      this.busy = false;
    }
  }

  begin(file) {
    return this.request(TYPE.syncBegin, [file.patchFormat, ...LibraryFile.libraryHeadBytes(file)]);
  }

  sendPatch(patch) {
    this.sendSysEx(TYPE.patchHead, [patch.slot, ...LibraryFile.headBytes(patch)]);
    patch.layers.forEach((bytes, layer) => this.sendSysEx(TYPE.patchLayer, [patch.slot, layer, ...bytes]));
  }

  sendPin(controls) {
    this.sendSysEx(TYPE.pin, controls);
  }

  sendOneshot(oneshot) {
    this.sendSysEx(TYPE.oneshot, [oneshot.index, ...LibraryFile.oneshotBytes(oneshot)]);
  }

  commit() { return this.request(TYPE.syncCommit); }

  abort() {
    try {
      this.sendSysEx(TYPE.syncAbort);
    } catch {
      return false;
    }
    return true;
  }

  async push(file) {
    const patches = file.patches.slice().sort((a, b) => a.slot - b.slot);
    const oneshots = (file.oneshots || []).slice().sort((a, b) => a.index - b.index);
    const begun = await this.begin(file);
    if (begun !== STATUS.ok) return { ok: false, where: 'sync begin', status: begun };
    try {
      patches.forEach(patch => this.sendPatch(patch));
      oneshots.forEach(oneshot => this.sendOneshot(oneshot));
      const committed = await this.commit();
      return { ok: committed === STATUS.ok, where: 'commit', status: committed, count: patches.length };
    } catch (error) {
      this.abort();
      throw error;
    }
  }

  async pushSongs(songs) {
    const begun = await this.request(TYPE.songsBegin);
    if (begun !== STATUS.ok) return { ok: false, where: 'songs begin', status: begun };
    songs.forEach(song => this.sendSysEx(TYPE.song, LibraryFile.songBytes(song)));
    const committed = await this.request(TYPE.songsCommit);
    return { ok: committed === STATUS.ok, where: 'songs commit', status: committed, count: songs.length };
  }

  async queryLibrary() {
    const answer = this.expect(message => message.type === TYPE.libraryInfo);
    this.sendSysEx(TYPE.queryLibrary);
    const [format, state, ...head] = (await answer).payload;
    return Object.assign({ format, state, stateText: libraryStateText(state) },
      LibraryFile.libraryFromHeadBytes(head.slice(0, Protocol.LIBRARY_HEAD_LENGTH)));
  }

  async readPatch(slot) {
    const refused = message => Link.isAckFor(TYPE.queryPatch, message);
    const answer = type => this.expect(message =>
      refused(message) || (message.type === type && message.payload[0] === slot));
    const head = answer(TYPE.patchHeadOut);
    this.sendSysEx(TYPE.queryPatch, [slot]);
    const first = await head;
    if (refused(first)) return { status: first.payload[1] };
    const layers = [];
    for (let layer = 0; layer < Protocol.PATCH_LAYERS; layer++) {
      const message = await answer(TYPE.patchLayerOut);
      if (refused(message)) return { status: message.payload[1] };
      layers.push(message.payload.slice(2));
    }
    return {
      head: first.payload.slice(1),
      patch: Object.assign({ slot }, LibraryFile.patchFromHead(first.payload.slice(1)), { layers }),
    };
  }

  async readOneshot(index) {
    const answer = this.expect(message => Link.isAckFor(TYPE.queryOneshot, message)
      || (message.type === TYPE.oneshotOut && message.payload[0] === index));
    this.sendSysEx(TYPE.queryOneshot, [index]);
    const message = await answer;
    if (message.type !== TYPE.oneshotOut) return { status: message.payload[1] };
    return { oneshot: LibraryFile.oneshotFromBytes(index, message.payload.slice(1)) };
  }

  async pull() {
    const info = await this.queryLibrary();
    if (info.state !== Protocol.LIBRARY_STATE.stored || !info.slots.length) {
      return { error: info.stateText, info };
    }
    const patches = [];
    for (const slot of info.slots) {
      const read = await this.readPatch(slot);
      if (!read.patch) return { error: `slot ${slot}: ${statusText(read.status)}`, info };
      patches.push(read.patch);
    }
    const oneshots = [];
    for (const index of info.kit) {
      const read = await this.readOneshot(index);
      if (!read.oneshot) return { error: `oneshot ${index}: ${statusText(read.status)}`, info };
      oneshots.push(read.oneshot);
    }
    return {
      file: { patchFormat: Protocol.PATCH_FORMAT, patches, oneshots, defaultOneshots: info.defaultOneshots },
      info,
    };
  }
}

export { Link, statusText };
