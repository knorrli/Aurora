(function (global) {
  'use strict';

  const CC = {
    tempoDivision:      2,
    parHueShuffle:      3,
    parHueShuffleEvery: 4,
    parLfoShuffle:      5,
    faderColor:         12,
    faderExtent:        13,
    faderMotion:        14,
    touchpadX:          15,
    touchpadY:          16,
    touchpadPressure:   17,
    touchpadEngage:     18,
    rockerTouchpadA:    19,
    rockerTouchpadB:    20,
    rockerTouchpadC:    21,
    rockerTouchpadD:    22,
    rockerFaders:       23,
    audioFollower:      24,
    audioThreshold:     25,
    keyHeld:            26,
    parValue:           27,
    parHueOffset:       28,
    parSaturation:      29,
    parHueSpread:       30,
    parLfoSpread:       31,
    hue:                33,
    saturation:         34,
    value:              35,
    fieldForm:          36,
    fieldDirection:     37,
    fieldHue:           38,
    fieldWhite:         39,
    fieldDark:          40,
    fieldCount:         41,
    fieldWidth:         42,
    fieldEdge:          43,
    fieldSpeed:         44,
    flowHue:            45,
    flowWhite:          46,
    flowDark:           47,
    flowRate:           48,
    flowDensity:        49,
    lightHue:           50,
    lightWhite:         51,
    lightDark:          52,
    palette:            53,
    shapeBounce:        54,
    shapeWidth:         55,
    shapeCount:         56,
    shapeEdge:          57,
    shapeTail:          58,
    shapePosition:      59,
    shapeSpeed:         60,
    fanFrequency:       61,
    fanPhase:           62,
    fanRandomize:       63,
    fanSpread:          64,
    fanSpeed:           65,
    fanLfo:             66,
    lfoRate:            67,
    shapeBend:          68,
    shapeBendAt:        69,
    scatterSpread:      70,
    scatterRate:        71,
    scatterCount:       72,
    scatterWidth:       73,
    scatterEdge:        74,
    scatterRandomize:   75,
    scatterSlide:       76,
    scatterValue:       77,
    scatterHue:         78,
    scatterWhite:       79,
  };

  const TAGS = {"tempoDivision":["switch"],"parHueShuffle":["patch","plain"],"parHueShuffleEvery":["patch"],"parLfoShuffle":["patch","plain"],"faderColor":["ambient"],"faderExtent":["ambient"],"faderMotion":["ambient"],"touchpadX":["gesture"],"touchpadY":["gesture"],"touchpadPressure":["gesture"],"touchpadEngage":["gesture"],"rockerTouchpadA":["ambient"],"rockerTouchpadB":["ambient"],"rockerTouchpadC":["ambient"],"rockerTouchpadD":["ambient"],"rockerFaders":["ambient"],"audioFollower":["ambient"],"audioThreshold":["ambient"],"keyHeld":["gesture"],"parValue":["patch","plain"],"parHueOffset":["patch","circular","plain"],"parSaturation":["patch","plain"],"parHueSpread":["patch","plain"],"parLfoSpread":["patch","plain"],"hue":["patch","circular"],"saturation":["patch"],"value":["patch"],"fieldForm":["switch"],"fieldDirection":["switch"],"fieldHue":["patch"],"fieldWhite":["patch"],"fieldDark":["patch"],"fieldCount":["patch"],"fieldWidth":["patch"],"fieldEdge":["patch"],"fieldSpeed":["patch","rate"],"flowHue":["patch"],"flowWhite":["patch"],"flowDark":["patch"],"flowRate":["patch","rate"],"flowDensity":["patch"],"lightHue":["patch"],"lightWhite":["patch"],"lightDark":["patch"],"palette":["switch"],"shapeBounce":["switch"],"shapeWidth":["patch"],"shapeCount":["patch","plain"],"shapeEdge":["patch"],"shapeTail":["patch"],"shapePosition":["patch","circular"],"shapeSpeed":["patch","rate"],"fanFrequency":["patch","plain"],"fanPhase":["patch","circular","plain"],"fanRandomize":["patch","plain"],"fanSpread":["patch","plain"],"fanSpeed":["patch","rate","plain"],"fanLfo":["patch","plain"],"lfoRate":["patch"],"shapeBend":["patch","plain"],"shapeBendAt":["patch","plain"],"scatterSpread":["patch"],"scatterRate":["patch","rate"],"scatterCount":["patch"],"scatterWidth":["patch"],"scatterEdge":["patch"],"scatterRandomize":["patch"],"scatterSlide":["patch"],"scatterValue":["patch"],"scatterHue":["patch"],"scatterWhite":["patch"]};
  const CONTROL_DEFAULTS = {"parHueShuffleEvery":42,"parValue":127,"parSaturation":127,"parHueSpread":64,"parLfoSpread":64,"hue":20,"saturation":127,"value":127,"fieldDirection":64,"fieldHue":64,"fieldWidth":64,"fieldEdge":64,"fieldSpeed":64,"flowHue":64,"flowRate":50,"flowDensity":20,"lightHue":64,"shapeWidth":40,"shapeEdge":18,"shapePosition":64,"shapeSpeed":80,"fanFrequency":32,"fanSpread":64,"fanSpeed":64,"fanLfo":64,"lfoRate":64,"shapeBend":64,"shapeBendAt":64,"scatterRate":60,"scatterCount":80,"scatterWidth":34,"scatterEdge":40,"scatterRandomize":110,"scatterSlide":64,"scatterValue":64,"scatterHue":64};

  const MIDI_CHANNEL = 1;
  const PROGRAM_BLACKOUT = 0;
  const PROGRAM_SHOW = 10;
  const TICKS_PER_BEAT = 24;
  const TEMPO_DIVISION = {"quarter":0,"bar":1,"half":2,"eighth":3,"eighthTriplet":4,"sixteenth":5};
  const FIELD_FORM = {"gradient":0,"region":1,"allButRegion":2};
  const FIELD_DIRECTION = {"horizontal":0,"vertical":1,"shape":2};
  const SWITCH_ON_AT = 64;
  const THREE_WAY_STARTS = [0,43,86];
  const THREE_WAY_VALUES = [0,64,127];
  const WAVE_SWELL = 32;
  const WAVE_SNAP = 64;
  const WAVE_SQUARE = 96;
  const LFO_PERIODS = [16,12,8,6,4,3,2,1.5,1,0.75,0.5,0.375,0.25];
  const ROUTES = 8;
  const ROUTE_BASE = [80,85,90,95,100,105,110,115];
  const ROUTE_FIELD = {"destination":0,"amount":1,"ratio":2,"wave":3,"phase":4};
  const ROUTE_MAX_RATIO = 8;
  const PATCH_FORMAT = 1;
  const PATCH_MAX = 128;
  const PATCH_CC_COUNT = 128;
  const PATCH_NAME_LENGTH = 16;
  const PATCH_HEAD_LENGTH = 20;
  const PATCH_BASE = 0;
  const PATCH_TARGET_COLOR = 1;
  const PATCH_TARGET_EXTENT = 2;
  const PATCH_TARGET_MOTION = 3;
  const PATCH_TARGET_ACCENT = 4;
  const PATCH_PARTS = 5;
  const KEYPAD_KEYS = 9;
  const SLOT_MAP_LENGTH = 19;
  const SYSEX_ID = 125;
  const SYSEX_SIGNATURE_A = 65;
  const SYSEX_SIGNATURE_B = 85;
  const SYSEX_HEADER_LENGTH = 5;
  const SYSEX_TYPE = {"syncBegin":1,"patchHead":2,"patchPart":3,"syncCommit":4,"syncAbort":5,"queryLibrary":6,"queryPatch":7,"ack":64,"libraryInfo":65,"patchHeadOut":66,"patchPartOut":67};
  const SYSEX_STATUS = {"ok":0,"errorFormat":1,"errorSequence":2,"errorIncomplete":3,"errorStorage":4,"errorRange":5};
  const LIBRARY_STATE = {"stored":0,"empty":1,"unreadable":2};

  const tagged = tag => Object.keys(TAGS).filter(name => TAGS[name].includes(tag));
  const hasTag = (name, tag) => (TAGS[name] || []).includes(tag);

  const NAME_BY_CC = {};
  for (const [name, number] of Object.entries(CC)) NAME_BY_CC[number] = name;

  const steppedIndex = (value, count) =>
    Math.min(count - 1, Math.floor((value * (count - 1) + 63) / 127));

  const routeCC = (route, field) => ROUTE_BASE[route] + field;
  const routeRatio = value => 1 + steppedIndex(value, ROUTE_MAX_RATIO);

  const isOn = value => value >= SWITCH_ON_AT;
  const threeWayPosition = value =>
    THREE_WAY_STARTS.filter(start => value >= start).length - 1;

  global.AuroraProtocol = {
    CC, CONTROL_DEFAULTS, NAME_BY_CC, tagged, hasTag,
    MIDI_CHANNEL,
    PROGRAM_BLACKOUT,
    PROGRAM_SHOW,
    TICKS_PER_BEAT,
    TEMPO_DIVISION,
    FIELD_FORM,
    FIELD_DIRECTION,
    THREE_WAY_VALUES,
    WAVE_SWELL,
    WAVE_SNAP,
    WAVE_SQUARE,
    LFO_PERIODS,
    ROUTES,
    ROUTE_FIELD,
    PATCH_FORMAT,
    PATCH_MAX,
    PATCH_CC_COUNT,
    PATCH_NAME_LENGTH,
    PATCH_HEAD_LENGTH,
    PATCH_BASE,
    PATCH_TARGET_COLOR,
    PATCH_TARGET_EXTENT,
    PATCH_TARGET_MOTION,
    PATCH_TARGET_ACCENT,
    PATCH_PARTS,
    KEYPAD_KEYS,
    SLOT_MAP_LENGTH,
    SYSEX_ID,
    SYSEX_SIGNATURE_A,
    SYSEX_SIGNATURE_B,
    SYSEX_HEADER_LENGTH,
    SYSEX_TYPE,
    SYSEX_STATUS,
    LIBRARY_STATE,
    steppedIndex, routeCC, routeRatio, isOn, threeWayPosition,
  };
})(typeof window === 'undefined' ? globalThis : window);
