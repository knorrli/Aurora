(function (global) {
  'use strict';

  const CC = {
    rockerTouchpadA:   2,
    rockerTouchpadB:   3,
    rockerTouchpadC:   4,
    rockerTouchpadD:   5,
    rockerFaders:      6,
    audioFollower:     8,
    audioThreshold:    9,
    faderColor:        12,
    faderExtent:       13,
    faderMotion:       14,
    touchpadX:         15,
    touchpadY:         16,
    touchpadPressure:  17,
    touchpadEngage:    18,
    palette:           20,
    hue:               21,
    saturation:        22,
    value:             23,
    parHueOffset:      24,
    parSaturation:     25,
    parValue:          26,
    parHueLayout:      27,
    parHueRange:       28,
    tempoDivision:     29,
    lfoRate:           30,
    arpMode:           31,
    arpSpread:         33,
    shapeCount:        35,
    shapeWidth:        36,
    shapeEdge:         37,
    shapeTail:         38,
    shapeBounce:       39,
    shapeSpeed:        40,
    shapePosition:     41,
    shapeBend:         42,
    shapeBendAt:       43,
    fanSpread:         44,
    fanSpeed:          45,
    fanLfo:            46,
    fanFrequency:      47,
    fanPhase:          48,
    fanRandomize:      49,
    scatterCount:      50,
    scatterWidth:      51,
    scatterEdge:       52,
    scatterRate:       53,
    scatterSpeed:      54,
    scatterRandomize:  55,
    scatterPosition:   56,
    scatterMix:        57,
    scatterHue:        58,
    scatterSaturation: 59,
    scatterValue:      60,
    fieldForm:         61,
    fieldDirection:    62,
    fieldCount:        63,
    fieldWidth:        65,
    fieldEdge:         66,
    fieldSpeed:        67,
    fieldPosition:     68,
    fieldHue:          69,
    fieldWhite:        70,
    fieldDark:         71,
    flowDensity:       72,
    flowRate:          73,
    flowHue:           74,
    flowWhite:         75,
    flowDark:          76,
    coreHue:           77,
    coreWhite:         78,
    coreDark:          79,
  };

  const TAGS = {"rockerTouchpadA":["ambient"],"rockerTouchpadB":["ambient"],"rockerTouchpadC":["ambient"],"rockerTouchpadD":["ambient"],"rockerFaders":["ambient"],"audioFollower":["ambient"],"audioThreshold":["ambient"],"faderColor":["ambient"],"faderExtent":["ambient"],"faderMotion":["ambient"],"touchpadX":["gesture"],"touchpadY":["gesture"],"touchpadPressure":["gesture"],"touchpadEngage":["gesture"],"palette":["switch"],"hue":["patch","circular"],"saturation":["patch"],"value":["patch"],"parHueOffset":["patch","circular","plain"],"parSaturation":["patch","plain"],"parValue":["patch","plain"],"parHueLayout":["switch"],"parHueRange":["patch","plain"],"tempoDivision":["switch"],"lfoRate":["patch"],"arpMode":["switch"],"arpSpread":["patch","plain"],"shapeCount":["patch","plain"],"shapeWidth":["patch"],"shapeEdge":["patch"],"shapeTail":["patch"],"shapeBounce":["switch"],"shapeSpeed":["patch","rate"],"shapePosition":["patch"],"shapeBend":["patch","plain"],"shapeBendAt":["patch","plain"],"fanSpread":["patch","plain"],"fanSpeed":["patch","rate","plain"],"fanLfo":["patch","plain"],"fanFrequency":["patch","plain"],"fanPhase":["patch","circular","plain"],"fanRandomize":["patch","plain"],"scatterCount":["patch"],"scatterWidth":["patch"],"scatterEdge":["patch"],"scatterRate":["patch","rate"],"scatterSpeed":["patch"],"scatterRandomize":["patch"],"scatterPosition":["patch"],"scatterMix":["patch"],"scatterHue":["patch"],"scatterSaturation":["patch"],"scatterValue":["patch"],"fieldForm":["switch"],"fieldDirection":["switch"],"fieldCount":["patch"],"fieldWidth":["patch"],"fieldEdge":["patch"],"fieldSpeed":["patch","rate"],"fieldPosition":["patch"],"fieldHue":["patch"],"fieldWhite":["patch"],"fieldDark":["patch"],"flowDensity":["patch"],"flowRate":["patch","rate"],"flowHue":["patch"],"flowWhite":["patch"],"flowDark":["patch"],"coreHue":["patch"],"coreWhite":["patch"],"coreDark":["patch"]};
  const CONTROL_DEFAULTS = {"hue":20,"saturation":127,"value":127,"parSaturation":127,"parValue":127,"parHueRange":64,"arpSpread":127,"lfoRate":64,"shapeWidth":64,"shapeSpeed":64,"shapePosition":64,"shapeBend":64,"shapeBendAt":64,"fanSpread":64,"fanSpeed":64,"fanLfo":64,"fanFrequency":32,"scatterCount":80,"scatterWidth":34,"scatterEdge":40,"scatterRate":80,"scatterRandomize":110,"scatterSpeed":64,"scatterPosition":64,"scatterHue":64,"scatterSaturation":127,"scatterValue":127,"fieldDirection":64,"fieldCount":24,"fieldWidth":64,"fieldEdge":64,"fieldSpeed":64,"fieldPosition":64,"fieldHue":64,"flowDensity":20,"flowRate":50,"flowHue":64,"coreHue":64};

  const MIDI_CHANNEL = 1;
  const PROGRAM_BLACKOUT = 0;
  const PROGRAM_SHOW = 10;
  const NOTE_KEY_HELD = 57;
  const NOTE_PATCH_ONESHOT_FIRST = 58;
  const NOTE_PATCH_ONESHOT_SECOND = 59;
  const NOTE_ONESHOT_FIRST = 60;
  const ONESHOTS = 20;
  const TICKS_PER_BEAT = 24;
  const TEMPO_DIVISION = {"quarter":0,"bar":1,"half":2,"eighth":3,"eighthTriplet":4,"sixteenth":5};
  const FIELD_FORM = {"gradient":0,"region":1,"allButRegion":2};
  const FIELD_DIRECTION = {"horizontal":0,"vertical":1,"shape":2};
  const SWITCH_ON_AT = 64;
  const THREE_WAY_STARTS = [0,43,86];
  const THREE_WAY_VALUES = [0,64,127];
  const ARP = {"unison":0,"steps":1,"ripple":2};
  const ARP_MODE = {"sequence":0,"bounce":1,"evensOdds":2,"pairs":3,"mirror":4,"random":5};
  const ARP_MODE_COUNT = 6;
  const HUE_LAYOUT = {"gradient":0,"evensOdds":1,"pairs":2,"mirror":3,"random":4};
  const HUE_LAYOUT_COUNT = 5;
  const ARP_DESTINATION_BASE = 120;
  const ARP_CONTROLS = ["parHueOffset","parSaturation","parValue"];
  const BIPOLAR_DESTINATION_BASE = 80;
  const BIPOLAR_CONTROLS = ["hue","parHueOffset","parHueRange","arpSpread","shapePosition","shapeBend","shapeBendAt","fanSpread","fanLfo","fanPhase","fieldPosition","fieldHue","flowHue","coreHue"];
  const WAVE_SWELL = 32;
  const WAVE_FALL = 64;
  const WAVE_SQUARE = 96;
  const LFO_PERIODS = [16,12,8,6,4,3,2,1.5,1,0.75,0.5,0.375,0.25];
  const ROUTES = 8;
  const ROUTE_BASE = [80,85,90,95,100,105,110,115];
  const ROUTE_FIELD = {"destination":0,"amount":1,"ratio":2,"wave":3,"phase":4};
  const ROUTE_DEFAULTS = {"destination":0,"amount":64,"ratio":0,"wave":32,"phase":0};
  const ROUTE_MAX_RATIO = 8;
  const ROUTE_PHASE_STEPS = 16;
  const PATCH_FORMAT = 5;
  const PATCH_MAX = 128;
  const PATCH_CC_COUNT = 128;
  const PATCH_NAME_LENGTH = 16;
  const PATCH_HEAD_LENGTH = 20;
  const PATCH_LAYER_BASE = 0;
  const PATCH_LAYER_COLOR = 1;
  const PATCH_LAYER_EXTENT = 2;
  const PATCH_LAYER_MOTION = 3;
  const PATCH_LAYER_ACCENT = 4;
  const PATCH_LAYERS = 5;
  const KEYPAD_KEYS = 9;
  const BANKS = 12;
  const SLOT_MAP_LENGTH = 19;
  const SYSEX_ID = 125;
  const SYSEX_SIGNATURE_A = 65;
  const SYSEX_SIGNATURE_B = 85;
  const SYSEX_HEADER_LENGTH = 5;
  const SYSEX_TYPE = {"syncBegin":1,"patchHead":2,"patchLayer":3,"syncCommit":4,"syncAbort":5,"queryLibrary":6,"queryPatch":7,"ack":64,"libraryInfo":65,"patchHeadOut":66,"patchLayerOut":67};
  const SYSEX_STATUS = {"ok":0,"errorFormat":1,"errorSequence":2,"errorIncomplete":3,"errorStorage":4,"errorRange":5};
  const LIBRARY_STATE = {"stored":0,"empty":1,"unreadable":2};

  const tagged = tag => Object.keys(TAGS).filter(name => TAGS[name].includes(tag));
  const hasTag = (name, tag) => (TAGS[name] || []).includes(tag);

  const NAME_BY_CC = {};
  for (const [name, number] of Object.entries(CC)) NAME_BY_CC[number] = name;

  const steppedIndex = (value, count) =>
    Math.min(count - 1, Math.floor((value * (count - 1) + 63) / 127));

  const routeCC = (route, field) => ROUTE_BASE[route] + field;
  const routeRatioStep = value => steppedIndex(value, 2 * ROUTE_MAX_RATIO);
  const routeRatio = value => 1 + routeRatioStep(value) % ROUTE_MAX_RATIO;
  const routeOnce = value => routeRatioStep(value) >= ROUTE_MAX_RATIO;
  const routeRatioValue = (ratio, once) =>
    Math.round(((once ? ROUTE_MAX_RATIO : 0) + ratio - 1) * 127 / (2 * ROUTE_MAX_RATIO - 1));
  const routePhaseStep = value => steppedIndex(value, ROUTE_PHASE_STEPS);

  const arpMode = value => steppedIndex(value, ARP_MODE_COUNT);
  const arpModeValue = mode => Math.round(mode * 127 / (ARP_MODE_COUNT - 1));
  const hueLayout = value => steppedIndex(value, HUE_LAYOUT_COUNT);
  const hueLayoutValue = layout => Math.round(layout * 127 / (HUE_LAYOUT_COUNT - 1));

  const routeArp = destination => (destination < ARP_DESTINATION_BASE ? ARP.unison
    : ARP.steps + (destination - ARP_DESTINATION_BASE) % 2);
  const routeBipolar = destination => destination >= BIPOLAR_DESTINATION_BASE
    && destination < BIPOLAR_DESTINATION_BASE + BIPOLAR_CONTROLS.length;
  const routeTarget = destination => {
    if (destination < BIPOLAR_DESTINATION_BASE) return destination;
    if (routeBipolar(destination)) return CC[BIPOLAR_CONTROLS[destination - BIPOLAR_DESTINATION_BASE]];
    if (destination < ARP_DESTINATION_BASE) return 0;
    const name = ARP_CONTROLS[Math.floor((destination - ARP_DESTINATION_BASE) / 2)];
    return name ? CC[name] : 0;
  };
  const routeDestination = (target, arp, bipolar) => {
    if (arp === ARP.unison) {
      const index = BIPOLAR_CONTROLS.indexOf(NAME_BY_CC[target]);
      return bipolar && index >= 0 ? BIPOLAR_DESTINATION_BASE + index : target;
    }
    const index = ARP_CONTROLS.indexOf(NAME_BY_CC[target]);
    return index < 0 ? target : ARP_DESTINATION_BASE + index * 2 + (arp - ARP.steps);
  };

  const isOn = value => value >= SWITCH_ON_AT;
  const threeWayPosition = value =>
    THREE_WAY_STARTS.filter(start => value >= start).length - 1;

  global.AuroraProtocol = {
    CC, CONTROL_DEFAULTS, NAME_BY_CC, tagged, hasTag,
    MIDI_CHANNEL,
    PROGRAM_BLACKOUT,
    PROGRAM_SHOW,
    NOTE_KEY_HELD,
    NOTE_PATCH_ONESHOT_FIRST,
    NOTE_PATCH_ONESHOT_SECOND,
    NOTE_ONESHOT_FIRST,
    ONESHOTS,
    TICKS_PER_BEAT,
    TEMPO_DIVISION,
    FIELD_FORM,
    FIELD_DIRECTION,
    THREE_WAY_VALUES,
    ARP,
    ARP_MODE,
    HUE_LAYOUT,
    ARP_CONTROLS,
    BIPOLAR_CONTROLS,
    WAVE_SWELL,
    WAVE_FALL,
    WAVE_SQUARE,
    LFO_PERIODS,
    ROUTES,
    ROUTE_FIELD,
    ROUTE_DEFAULTS,
    ROUTE_MAX_RATIO,
    ROUTE_PHASE_STEPS,
    PATCH_FORMAT,
    PATCH_MAX,
    PATCH_CC_COUNT,
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
    SYSEX_ID,
    SYSEX_SIGNATURE_A,
    SYSEX_SIGNATURE_B,
    SYSEX_HEADER_LENGTH,
    SYSEX_TYPE,
    SYSEX_STATUS,
    LIBRARY_STATE,
    steppedIndex, routeCC, routeRatio, routeOnce, routeRatioValue, routePhaseStep, isOn, threeWayPosition,
    arpMode, arpModeValue, hueLayout, hueLayoutValue, routeArp, routeBipolar, routeTarget, routeDestination,
  };
})(typeof window === 'undefined' ? globalThis : window);
