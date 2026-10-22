const CC = {
  padHold:     3,
  padMode:     4,
  padWidth:    5,
  padGaps:     6,
  faderColor:  12,
  faderMotion: 13,
  faderExtent: 14,
  padX:        15,
  padY:        16,
  padTouch:    18,
};

const NUMBER = {
  palette:           0,
  hue:               1,
  saturation:        2,
  parHueOffset:      3,
  parSaturation:     4,
  parValue:          5,
  parHueLayout:      6,
  parHueRange:       7,
  tempoDivision:     8,
  lfoRate:           9,
  arpMode:           10,
  arpDirection:      11,
  arpRate:           12,
  arpGate:           13,
  cellEnds:          14,
  cellChord:         15,
  cellSpread:        16,
  cellPosition:      17,
  cellBend:          18,
  cellBendAt:        19,
  cellWidth:         20,
  cellEdge:          21,
  cellTail:          22,
  cellEnd:           23,
  cellRate:          24,
  cellDelay:         25,
  cellGate:          26,
  cellDirection:     27,
  cellBrightness:    28,
  cellCurve:         29,
  cellCurveAt:       30,
  scatterCount:      31,
  scatterWidth:      32,
  scatterEdge:       33,
  scatterRate:       34,
  scatterSpeed:      35,
  scatterRandomize:  36,
  scatterPosition:   37,
  scatterMix:        38,
  scatterHue:        39,
  scatterSaturation: 40,
  scatterValue:      41,
  fieldForm:         42,
  fieldDirection:    43,
  fieldCount:        44,
  fieldWidth:        45,
  fieldEdge:         46,
  fieldSpeed:        47,
  fieldPosition:     48,
  fieldHue:          49,
  fieldWhite:        50,
  fieldDark:         51,
  flowDensity:       52,
  flowRate:          53,
  flowHue:           54,
  flowWhite:         55,
  flowDark:          56,
  coreHue:           57,
  coreWhite:         58,
  coreDark:          59,
  envelope1Moves:    60,
  envelope1Delay:    61,
  envelope1Attack:   62,
  envelope1Hold:     63,
  envelope1Decay:    64,
  envelope1Sustain:  65,
  envelope1Release:  66,
  envelope2Moves:    67,
  envelope2Delay:    68,
  envelope2Attack:   69,
  envelope2Hold:     70,
  envelope2Decay:    71,
  envelope2Sustain:  72,
  envelope2Release:  73,
  envelope3Moves:    74,
  envelope3Delay:    75,
  envelope3Attack:   76,
  envelope3Hold:     77,
  envelope3Decay:    78,
  envelope3Sustain:  79,
  envelope3Release:  80,
};

const TAGS = {"palette":["switch"],"hue":["patch","circular"],"saturation":["patch"],"parHueOffset":["patch","circular"],"parSaturation":["patch"],"parValue":["patch"],"parHueLayout":["switch"],"parHueRange":["patch"],"tempoDivision":["switch"],"lfoRate":["patch"],"arpMode":["switch"],"arpDirection":["switch"],"arpRate":["switch"],"arpGate":["patch"],"cellEnds":["switch"],"cellChord":["patch"],"cellSpread":["patch"],"cellPosition":["patch"],"cellBend":["patch"],"cellBendAt":["patch"],"cellWidth":["patch"],"cellEdge":["patch"],"cellTail":["patch"],"cellEnd":["switch"],"cellRate":["switch"],"cellDelay":["patch"],"cellGate":["patch"],"cellDirection":["switch"],"cellBrightness":["patch"],"cellCurve":["patch"],"cellCurveAt":["patch"],"scatterCount":["patch"],"scatterWidth":["patch"],"scatterEdge":["patch"],"scatterRate":["patch","rate"],"scatterSpeed":["patch"],"scatterRandomize":["patch"],"scatterPosition":["patch"],"scatterMix":["patch"],"scatterHue":["patch"],"scatterSaturation":["patch"],"scatterValue":["patch"],"fieldForm":["switch"],"fieldDirection":["switch"],"fieldCount":["patch"],"fieldWidth":["patch"],"fieldEdge":["patch"],"fieldSpeed":["patch","rate"],"fieldPosition":["patch"],"fieldHue":["patch"],"fieldWhite":["patch"],"fieldDark":["patch"],"flowDensity":["patch"],"flowRate":["patch","rate"],"flowHue":["patch"],"flowWhite":["patch"],"flowDark":["patch"],"coreHue":["patch"],"coreWhite":["patch"],"coreDark":["patch"],"envelope1Moves":["switch"],"envelope1Delay":["patch"],"envelope1Attack":["patch"],"envelope1Hold":["patch"],"envelope1Decay":["patch"],"envelope1Sustain":["patch"],"envelope1Release":["patch"],"envelope2Moves":["switch"],"envelope2Delay":["patch"],"envelope2Attack":["patch"],"envelope2Hold":["patch"],"envelope2Decay":["patch"],"envelope2Sustain":["patch"],"envelope2Release":["patch"],"envelope3Moves":["switch"],"envelope3Delay":["patch"],"envelope3Attack":["patch"],"envelope3Hold":["patch"],"envelope3Decay":["patch"],"envelope3Sustain":["patch"],"envelope3Release":["patch"]};
const CONTROL_DEFAULTS = {"hue":20,"saturation":127,"cellBrightness":127,"parSaturation":127,"parValue":127,"parHueRange":64,"arpGate":64,"lfoRate":64,"cellEnd":127,"cellWidth":20,"cellPosition":64,"cellBend":64,"cellBendAt":64,"cellSpread":127,"cellGate":64,"cellCurve":64,"cellCurveAt":64,"envelope1Sustain":127,"envelope2Sustain":127,"envelope3Sustain":127,"scatterCount":80,"scatterWidth":34,"scatterEdge":40,"scatterRate":80,"scatterRandomize":110,"scatterSpeed":64,"scatterPosition":64,"scatterHue":64,"scatterSaturation":127,"scatterValue":127,"fieldCount":24,"fieldWidth":64,"fieldEdge":64,"fieldSpeed":64,"fieldPosition":64,"fieldHue":64,"flowDensity":20,"flowRate":50,"flowHue":64,"coreHue":64};

const MIDI_CHANNEL = 1;
const PROGRAM_BLACKOUT = 0;
const PROGRAM_SONG_FIRST = 109;
const SONGS = 19;
const NOTE_KEY_HELD = 57;
const NOTE_PATCH_ONESHOT_FIRST = 58;
const NOTE_PATCH_ONESHOT_SECOND = 59;
const NOTE_ONESHOT_FIRST = 60;
const ONESHOTS = 20;
const TICKS_PER_BEAT = 24;
const TEMPO_DIVISION = {"quarter":0,"bar":1,"half":2,"eighth":3,"eighthTriplet":4,"sixteenth":5};
const CELL_END = {"gone":0,"startOver":1,"stay":2};
const DIRECTION = {"vertical":0,"horizontal":1,"cell":2,"cellPosition":3};
const ENVELOPES = 3;
const ENVELOPE_STAGE = {"delay":0,"attack":1,"hold":2,"decay":3,"sustain":4,"release":5};
const FIELD_FORM = {"gradient":0,"region":1,"allButRegion":2};
const ARP = {"unison":0,"steps":1};
const ARP_MODE = {"sequence":0,"bounce":1,"evensOdds":2,"pairs":3,"mirror":4,"random":5};
const GROUP_PATTERN = {"sequence":0,"mirror":1,"evensOdds":2,"pairs":3,"random":4,"patterns":5};
const HUE_LAYOUT = {"gradient":0,"evensOdds":1,"pairs":2,"mirror":3,"random":4};
const PAD_MODE = {"perPatch":0,"morph":1,"effects":2};
const CORNER = {"bottomLeft":0,"bottomRight":1,"topLeft":2,"topRight":3};
const KIT_PLACES = 24;
const ROUTE_SOURCE = {"lfo":0,"padX":1,"padY":2,"envelope1":3,"envelope2":4,"envelope3":5,"strip":6,"note":7,"birth":8};
const PAD_WIDTH = {"center":0,"middle":1,"all":2};
const WAVE_SWELL = 32;
const WAVE_FALL = 64;
const WAVE_SQUARE = 96;
const LFO_PERIODS = [16,12,8,6,4,3,2,1.5,1,0.75,0.5,0.375,0.25];
const ROUTES = 8;
const FIRST_ROUTE_CONTROL = 81;
const ROUTE_FIELD = {"amount":0,"phase":1,"destination":2,"source":3,"cycle":4,"ratio":5,"wave":6,"pattern":7,"smooth":8,"hold":9,"polarity":10,"arp":11};
const ROUTE_DEFAULTS = {"amount":64,"phase":0,"destination":0,"source":0,"cycle":0,"ratio":0,"wave":32,"pattern":0,"smooth":0,"hold":0,"polarity":0,"arp":0};
const ROUTE_MAX_RATIO = 8;
const ROUTE_PHASE_STEPS = 16;
const PATCH_FORMAT = 8;
const LAST_PATCH_SLOT = 108;
const PATCH_CONTROLS = 177;
const PATCH_NAME_LENGTH = 16;
const PATCH_HEAD_LENGTH = 20;
const PATCH_LAYER_BASE = 0;
const PATCH_LAYER_COLOR = 1;
const PATCH_LAYER_EXTENT = 3;
const PATCH_LAYER_MOTION = 2;
const PATCH_LAYER_ACCENT = 4;
const PATCH_LAYERS = 5;
const KEYPAD_KEYS = 9;
const BANKS = 12;
const SLOT_MAP_LENGTH = 16;
const KIT_MAP_LENGTH = 4;
const MARK_MAP_LENGTH = 26;
const NO_ONESHOT = 127;
const PIN_POSITION_STEPS = 16383;
const SONG_SECTIONS = 32;
const SONG_HEAD_LENGTH = 4;
const LIBRARY_HEAD_LENGTH = 22;
const SYSEX_ID = 125;
const SYSEX_SIGNATURE_A = 65;
const SYSEX_SIGNATURE_B = 85;
const SYSEX_HEADER_LENGTH = 5;
const SYSEX_TYPE = {"syncBegin":1,"patchHead":2,"patchLayer":3,"syncCommit":4,"syncAbort":5,"queryLibrary":6,"queryPatch":7,"oneshot":8,"queryOneshot":9,"pin":10,"songsBegin":11,"song":12,"songsCommit":13,"dialLayer":14,"dial":15,"pinLayer":16,"unpin":17,"ack":64,"libraryInfo":65,"patchHeadOut":66,"patchLayerOut":67,"oneshotOut":68};
const SYSEX_STATUS = {"ok":0,"errorFormat":1,"errorSequence":2,"errorIncomplete":3,"errorStorage":4,"errorRange":5};
const LIBRARY_STATE = {"stored":0,"empty":1,"unreadable":2};

const tagged = tag => Object.keys(TAGS).filter(name => TAGS[name].includes(tag));
const hasTag = (name, tag) => (TAGS[name] || []).includes(tag);

const NAME_BY_NUMBER = {};
for (const [name, value] of Object.entries(NUMBER)) NAME_BY_NUMBER[value] = name;

const routeNumber = (route, field) => FIRST_ROUTE_CONTROL + field * ROUTES + route;

export const Protocol = {
  CC, NUMBER, CONTROL_DEFAULTS, NAME_BY_NUMBER, tagged, hasTag,
  MIDI_CHANNEL,
  PROGRAM_BLACKOUT,
  PROGRAM_SONG_FIRST,
  SONGS,
  NOTE_KEY_HELD,
  NOTE_PATCH_ONESHOT_FIRST,
  NOTE_PATCH_ONESHOT_SECOND,
  NOTE_ONESHOT_FIRST,
  ONESHOTS,
  TICKS_PER_BEAT,
  TEMPO_DIVISION,
  CELL_END,
  DIRECTION,
  ENVELOPES,
  ENVELOPE_STAGE,
  FIELD_FORM,
  ARP,
  ARP_MODE,
  GROUP_PATTERN,
  HUE_LAYOUT,
  PAD_MODE,
  CORNER,
  KIT_PLACES,
  ROUTE_SOURCE,
  PAD_WIDTH,
  WAVE_SWELL,
  WAVE_FALL,
  WAVE_SQUARE,
  LFO_PERIODS,
  ROUTES,
  FIRST_ROUTE_CONTROL,
  ROUTE_FIELD,
  ROUTE_DEFAULTS,
  ROUTE_MAX_RATIO,
  ROUTE_PHASE_STEPS,
  PATCH_FORMAT,
  LAST_PATCH_SLOT,
  PATCH_CONTROLS,
  PATCH_NAME_LENGTH,
  PATCH_HEAD_LENGTH,
  PATCH_LAYER_BASE,
  PATCH_LAYER_COLOR,
  PATCH_LAYER_EXTENT,
  PATCH_LAYER_MOTION,
  PATCH_LAYER_ACCENT,
  PATCH_LAYERS,
  KEYPAD_KEYS,
  BANKS,
  SLOT_MAP_LENGTH,
  KIT_MAP_LENGTH,
  MARK_MAP_LENGTH,
  NO_ONESHOT,
  PIN_POSITION_STEPS,
  SONG_SECTIONS,
  SONG_HEAD_LENGTH,
  LIBRARY_HEAD_LENGTH,
  SYSEX_ID,
  SYSEX_SIGNATURE_A,
  SYSEX_SIGNATURE_B,
  SYSEX_HEADER_LENGTH,
  SYSEX_TYPE,
  SYSEX_STATUS,
  LIBRARY_STATE,
  routeNumber,
};
