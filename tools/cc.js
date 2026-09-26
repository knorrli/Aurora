(function (global) {
  'use strict';

  const CC = {
    rockerTouchpadA:  2,
    rockerTouchpadB:  3,
    rockerTouchpadC:  4,
    rockerTouchpadD:  5,
    rockerFaders:     6,
    audioFollower:    8,
    audioThreshold:   9,
    faderColor:       12,
    faderExtent:      13,
    faderMotion:      14,
    touchpadX:        15,
    touchpadY:        16,
    touchpadPressure: 17,
    touchpadEngage:   18,
    keyHeld:          19,
    palette:          20,
    hue:              21,
    saturation:       22,
    value:            23,
    parHueOffset:     24,
    parSaturation:    25,
    parValue:         26,
    parHueLayout:     27,
    parHueRange:      28,
    arpMode:          29,
    arpSpread:        30,
    tempoDivision:    33,
    lfoRate:          34,
    shapeCount:       35,
    shapeWidth:       36,
    shapeEdge:        37,
    shapeTail:        38,
    shapeBounce:      39,
    shapeSpeed:       40,
    shapePosition:    41,
    shapeBend:        42,
    shapeBendAt:      43,
    fanSpread:        44,
    fanSpeed:         45,
    fanLfo:           46,
    fanFrequency:     47,
    fanPhase:         48,
    fanRandomize:     49,
    scatterCount:     50,
    scatterWidth:     51,
    scatterEdge:      52,
    scatterRate:      53,
    scatterRandomize: 54,
    scatterSpread:    55,
    scatterSlide:     56,
    scatterHue:       57,
    scatterWhite:     58,
    scatterValue:     59,
    fieldForm:        60,
    fieldDirection:   61,
    fieldCount:       62,
    fieldWidth:       63,
    fieldEdge:        65,
    fieldSpeed:       66,
    fieldHue:         67,
    fieldWhite:       68,
    fieldDark:        69,
    flowDensity:      70,
    flowRate:         71,
    flowHue:          72,
    flowWhite:        73,
    flowDark:         74,
    lightHue:         75,
    lightWhite:       76,
    lightDark:        77,
  };

  const TAGS = {"rockerTouchpadA":["ambient"],"rockerTouchpadB":["ambient"],"rockerTouchpadC":["ambient"],"rockerTouchpadD":["ambient"],"rockerFaders":["ambient"],"audioFollower":["ambient"],"audioThreshold":["ambient"],"faderColor":["ambient"],"faderExtent":["ambient"],"faderMotion":["ambient"],"touchpadX":["gesture"],"touchpadY":["gesture"],"touchpadPressure":["gesture"],"touchpadEngage":["gesture"],"keyHeld":["gesture"],"palette":["switch"],"hue":["patch","circular"],"saturation":["patch"],"value":["patch"],"parHueOffset":["patch","circular","plain"],"parSaturation":["patch","plain"],"parValue":["patch","plain"],"parHueLayout":["switch"],"parHueRange":["patch","plain"],"arpMode":["switch"],"arpSpread":["patch","plain"],"tempoDivision":["switch"],"lfoRate":["patch"],"shapeCount":["patch","plain"],"shapeWidth":["patch"],"shapeEdge":["patch"],"shapeTail":["patch"],"shapeBounce":["switch"],"shapeSpeed":["patch","rate"],"shapePosition":["patch","circular"],"shapeBend":["patch","plain"],"shapeBendAt":["patch","plain"],"fanSpread":["patch","plain"],"fanSpeed":["patch","rate","plain"],"fanLfo":["patch","plain"],"fanFrequency":["patch","plain"],"fanPhase":["patch","circular","plain"],"fanRandomize":["patch","plain"],"scatterCount":["patch"],"scatterWidth":["patch"],"scatterEdge":["patch"],"scatterRate":["patch","rate"],"scatterRandomize":["patch"],"scatterSpread":["patch"],"scatterSlide":["patch"],"scatterHue":["patch"],"scatterWhite":["patch"],"scatterValue":["patch"],"fieldForm":["switch"],"fieldDirection":["switch"],"fieldCount":["patch"],"fieldWidth":["patch"],"fieldEdge":["patch"],"fieldSpeed":["patch","rate"],"fieldHue":["patch"],"fieldWhite":["patch"],"fieldDark":["patch"],"flowDensity":["patch"],"flowRate":["patch","rate"],"flowHue":["patch"],"flowWhite":["patch"],"flowDark":["patch"],"lightHue":["patch"],"lightWhite":["patch"],"lightDark":["patch"]};
  const CONTROL_DEFAULTS = {"hue":20,"saturation":127,"value":127,"parSaturation":127,"parValue":127,"parHueRange":64,"arpSpread":127,"lfoRate":64,"shapeWidth":40,"shapeEdge":18,"shapeSpeed":80,"shapePosition":64,"shapeBend":64,"shapeBendAt":64,"fanSpread":64,"fanSpeed":64,"fanLfo":64,"fanFrequency":32,"scatterCount":80,"scatterWidth":34,"scatterEdge":40,"scatterRate":60,"scatterRandomize":110,"scatterSlide":64,"scatterHue":64,"scatterValue":64,"fieldDirection":64,"fieldWidth":64,"fieldEdge":64,"fieldSpeed":64,"fieldHue":64,"flowDensity":20,"flowRate":50,"flowHue":64,"lightHue":64};

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
  const ARP = {"unison":0,"turns":1,"ripple":2};
  const ARP_MODE = {"together":0,"sequence":1,"bounce":2,"evensOdds":3,"pairs":4,"mirror":5,"random":6};
  const ARP_MODE_COUNT = 7;
  const HUE_LAYOUT = {"gradient":0,"evensOdds":1,"pairs":2,"mirror":3,"random":4};
  const HUE_LAYOUT_COUNT = 5;
  const ARP_DESTINATION_BASE = 120;
  const ARP_CONTROLS = ["parHueOffset","parSaturation","parValue"];
  const WAVE_SWELL = 32;
  const WAVE_SNAP = 64;
  const WAVE_SQUARE = 96;
  const LFO_PERIODS = [16,12,8,6,4,3,2,1.5,1,0.75,0.5,0.375,0.25];
  const ROUTES = 8;
  const ROUTE_BASE = [80,85,90,95,100,105,110,115];
  const ROUTE_FIELD = {"destination":0,"amount":1,"ratio":2,"wave":3,"phase":4};
  const ROUTE_DEFAULTS = {"destination":0,"amount":64,"ratio":0,"wave":32,"phase":0};
  const ROUTE_MAX_RATIO = 8;
  const PATCH_FORMAT = 2;
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

  const arpMode = value => steppedIndex(value, ARP_MODE_COUNT);
  const arpModeValue = mode => Math.round(mode * 127 / (ARP_MODE_COUNT - 1));
  const hueLayout = value => steppedIndex(value, HUE_LAYOUT_COUNT);
  const hueLayoutValue = layout => Math.round(layout * 127 / (HUE_LAYOUT_COUNT - 1));

  const routeArp = destination => (destination < ARP_DESTINATION_BASE ? ARP.unison
    : ARP.turns + (destination - ARP_DESTINATION_BASE) % 2);
  const routeTarget = destination => {
    if (destination < ARP_DESTINATION_BASE) return destination;
    const name = ARP_CONTROLS[Math.floor((destination - ARP_DESTINATION_BASE) / 2)];
    return name ? CC[name] : 0;
  };
  const routeDestination = (target, arp) => {
    const index = ARP_CONTROLS.indexOf(NAME_BY_CC[target]);
    if (arp === ARP.unison || index < 0) return target;
    return ARP_DESTINATION_BASE + index * 2 + (arp - ARP.turns);
  };

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
    ARP,
    ARP_MODE,
    HUE_LAYOUT,
    ARP_CONTROLS,
    WAVE_SWELL,
    WAVE_SNAP,
    WAVE_SQUARE,
    LFO_PERIODS,
    ROUTES,
    ROUTE_FIELD,
    ROUTE_DEFAULTS,
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
    arpMode, arpModeValue, hueLayout, hueLayoutValue, routeArp, routeTarget, routeDestination,
  };
})(typeof window === 'undefined' ? globalThis : window);
