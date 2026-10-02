#include "playback.h"

#include <math.h>
#include <string.h>

#include "effects.h"
#include "morph.h"
#include "render_math.h"

namespace playback {

static const float HOLD_JUDGED_AFTER_MICROS = 200000.0f;
static const float STUTTER_OPEN_SHARE = 0.5f;
static const float STROBE_LIT_SHARE = 0.25f;
static const float CORNER_CUT_SHARE = 0.15f;

float Ramp::at(float beats) const {
  if (beats >= endBeat) return end;
  if (beats <= startBeat) return start;
  return start + (end - start) * (beats - startBeat) / (endBeat - startBeat);
}

static void defaultPatch(Patch &out) {
  out.transitionTime = 0;
  out.accentTime = 0;
  out.oneshots[0] = out.oneshots[1] = AURORA_NO_ONESHOT;
  uint8_t *base = out.layers[PATCH_LAYER_BASE];
  memset(base, 0, AURORA_PATCH_CC_COUNT);
  for (const AuroraControlDefault &control : AURORA_CONTROL_DEFAULTS) base[control.cc] = control.value;
  for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
    for (uint8_t field = 0; field < ROUTE_FIELDS; field++) {
      base[aurora_route_cc(route, field)] = AURORA_ROUTE_DEFAULTS[field];
    }
  }
  for (uint8_t layer = PATCH_LAYER_BASE + 1; layer < AURORA_PATCH_LAYERS; layer++) {
    memcpy(out.layers[layer], base, AURORA_PATCH_CC_COUNT);
  }
}

Playback::Playback(Library &library, uint32_t micros) : library(library), tempo(micros) {
  defaultPatch(playing);
  render::controlsOf(playing.layers[PATCH_LAYER_BASE], from);
}

float Playback::nextBeat(float beats) const {
  return tempo.running() ? floorf(beats) + 1.0f : beats;
}

float Playback::brightnessAt(float beats) const {
  return risingFromBlack ? morph.at(beats) : brightness.at(beats);
}

void Playback::programChange(uint8_t program, uint32_t micros) {
  const float beats = tempo.beatsAt(micros);
  advance(beats);
  pinned = false;
  if (program > AURORA_LAST_PATCH_SLOT) return;
  if (program != PROGRAM_BLACKOUT && !library.patch(program, arriving)) return;

  press = {};
  press.waiting = true;
  press.held = keyHeld;
  press.judging = keyHeld;
  press.tapped = !keyHeld;
  press.slot = program;
  if (keyHeld) {
    press.startBeat = tempo.running() ? roundf(beats) : beats;
    press.judgeBeat = fmaxf(press.startBeat, beats) + HOLD_JUDGED_AFTER_MICROS / tempo.microsPerBeat();
  } else {
    press.startBeat = tempo.running() ? ceilf(beats) : beats;
  }
  advance(beats);
}

void Playback::controlChange(uint8_t cc, uint8_t value) {
  switch (cc) {
    case CC_FADER_COLOR: moveFader(PATCH_LAYER_COLOR, value); return;
    case CC_FADER_MOTION: moveFader(PATCH_LAYER_MOTION, value); return;
    case CC_FADER_EXTENT: moveFader(PATCH_LAYER_EXTENT, value); return;
    default:
      if (movePad(cc, value)) return;
      if (cc < AURORA_PATCH_CC_COUNT) playing.layers[PATCH_LAYER_BASE][cc] = value;
  }
}

void Playback::markedStrips(bool *out) const {
  const uint8_t middle = render::STRIPS / 2;
  const uint8_t reach = pad.width == PAD_WIDTH_CENTER ? 0 : pad.width == PAD_WIDTH_MIDDLE ? 1 : middle;
  for (uint8_t strip = 0; strip < render::STRIPS; strip++) {
    const bool within = strip + reach >= middle && strip <= middle + reach;
    out[strip] = within && (!pad.gaps || (strip + reach - middle) % 2 == 0);
  }
}

render::PadFinger Playback::padFinger() const {
  render::PadFinger finger;
  finger.playing = pad.playing && pad.mode == PAD_MODE_PER_PATCH;
  finger.x = (float)pad.x / 127.0f;
  finger.y = (float)pad.y / 127.0f;
  markedStrips(finger.strips);
  return finger;
}

uint8_t Playback::padEffect() const {
  if (!pad.playing || pad.mode != PAD_MODE_EFFECTS) return PAD_EFFECTS;
  const uint8_t column = (uint8_t)((uint16_t)pad.x * PAD_EFFECTS / 128);
  return column < PAD_EFFECTS ? column : PAD_EFFECTS - 1;
}

float Playback::padSpeed() const {
  if (padEffect() != PAD_EFFECT_DOUBLE_TIME) return 1.0f;
  return pad.y < 64 ? 2.0f : 4.0f;
}

static float gateBeats(uint8_t y) {
  static const float BEATS[] = { 1.0f, 0.5f, 0.25f, 0.125f };
  const uint8_t steps = sizeof(BEATS) / sizeof(BEATS[0]);
  const uint8_t step = (uint8_t)((uint16_t)y * steps / 128);
  return BEATS[step < steps ? step : steps - 1];
}

void Playback::playEffect(float beats) {
  const uint8_t effect = padEffect();
  if (effect != PAD_EFFECT_FREEZE) frozen = false;
  if (effect == PAD_EFFECTS) return;

  bool marked[render::STRIPS];
  markedStrips(marked);
  const float strength = (float)pad.y / 127.0f;
  const float along = render::fract(beats / gateBeats(pad.y));
  switch (effect) {
    case PAD_EFFECT_FOCUS:
      render::darkenOutside(rendered, marked, strength);
      return;
    case PAD_EFFECT_FREEZE:
      if (!frozen) {
        memcpy(frozenPixels, rendered.pixels, sizeof(frozenPixels));
        memcpy(frozenPars, rendered.pars, sizeof(frozenPars));
        frozen = true;
      }
      render::blendToward(rendered, frozenPixels, frozenPars, marked, strength);
      return;
    case PAD_EFFECT_STUTTER:
      if (along >= STUTTER_OPEN_SHARE) render::blackOut(rendered, marked);
      return;
    case PAD_EFFECT_STROBE:
      if (along < STROBE_LIT_SHARE) render::flashWhite(rendered, marked);
      else render::blackOut(rendered, marked);
      return;
  }
}

bool Playback::movePad(uint8_t cc, uint8_t value) {
  switch (cc) {
    case CC_PAD_X: pad.x = value; break;
    case CC_PAD_Y: pad.y = value; break;
    case CC_PAD_TOUCH: pad.touching = aurora_switch_is_on(value); break;
    case CC_PAD_HOLD: pad.latching = aurora_switch_is_on(value); break;
    case CC_PAD_MODE: pad.mode = aurora_three_way_position(value); break;
    case CC_PAD_WIDTH: pad.width = aurora_three_way_position(value); break;
    case CC_PAD_GAPS: pad.gaps = aurora_switch_is_on(value); break;
    default: return false;
  }
  pad.playing = pad.touching || (pad.playing && pad.latching);
  const bool morphing = pad.playing && pad.mode == PAD_MODE_MORPH;
  if (morphing && !cornersLoaded) loadCorners();
  if (!morphing) cornersLoaded = false;
  return true;
}

void Playback::loadCorners() {
  for (uint8_t corner = 0; corner < AURORA_CORNERS; corner++) {
    cornerFilled[corner] = library.oneshot(AURORA_ONESHOTS + corner, corners[corner]);
  }
  const uint8_t right = pad.x >= 64 ? 1 : 0;
  const uint8_t top = pad.y >= 64 ? 2 : 0;
  switchCorner = (int8_t)(top + right);
  cornersLoaded = true;
}

static float cornerAxis(uint8_t value) {
  return render::clampUnit(((float)value / 127.0f - CORNER_CUT_SHARE) / (1.0f - 2.0f * CORNER_CUT_SHARE));
}

const float *Playback::morphCorners(const float *composed) {
  if (!cornersLoaded) return nullptr;
  const float right = cornerAxis(pad.x);
  const float top = cornerAxis(pad.y);
  const float weights[AURORA_CORNERS] = {
      (1.0f - right) * (1.0f - top), right * (1.0f - top), (1.0f - right) * top, right * top,
  };
  const uint8_t *controls[AURORA_CORNERS];
  const uint8_t *marks[AURORA_CORNERS];
  for (uint8_t corner = 0; corner < AURORA_CORNERS; corner++) {
    controls[corner] = cornerFilled[corner] ? corners[corner].controls : nullptr;
    marks[corner] = corners[corner].marks;
  }
  memcpy(morphed, composed, sizeof(morphed));
  render::morphCorners(composed, controls, marks, weights, switchCorner, morphed);
  return morphed;
}

void Playback::moveFader(uint8_t layer, uint8_t value) {
  faders[layer] = (float)value / 127.0f;
  pinned = false;
}

void Playback::noteOn(uint8_t note, uint32_t micros) {
  const float beats = tempo.beatsAt(micros);
  advance(beats);
  pinned = false;
  if (note == NOTE_KEY_HELD) {
    keyHeld = true;
  } else if (note == NOTE_PATCH_ONESHOT_FIRST || note == NOTE_PATCH_ONESHOT_SECOND) {
    const uint8_t place = note - NOTE_PATCH_ONESHOT_FIRST;
    const uint8_t pick = playing.oneshots[place];
    fire(pick != AURORA_NO_ONESHOT ? pick : library.defaultOneshot(place), beats);
  } else if (note >= NOTE_ONESHOT_FIRST && note < NOTE_ONESHOT_FIRST + AURORA_ONESHOTS) {
    fire(note - NOTE_ONESHOT_FIRST, beats);
  }
}

void Playback::noteOff(uint8_t note, uint32_t micros) {
  if (note != NOTE_KEY_HELD) return;
  const float beats = tempo.beatsAt(micros);
  advance(beats);
  keyHeld = false;
  if (press.held) release(beats);
}

void Playback::fire(uint8_t index, float beats) {
  if (index >= AURORA_ONESHOTS || !library.oneshot(index, oneshot)) return;
  firing = true;
  firedIndex = index;
  firedAtBeats = beats;
}

void Playback::release(float beats) {
  press.held = false;
  if (press.waiting) {
    press.tapped = true;
    press.judging = false;
    return;
  }
  if (press.judging) {
    press.judging = false;
    press.tapped = true;
    accentWaiting = false;
    if (press.slot == PROGRAM_BLACKOUT) {
      brightness = Ramp::steady(0.0f);
    } else {
      morph = Ramp::steady(1.0f);
      switchesLanded = true;
    }
    return;
  }
  if (press.slot == PROGRAM_BLACKOUT) {
    const float now = brightnessAt(beats);
    if (now > 0.0f) brightness = { beats, now, nextBeat(beats), 0.0f };
    return;
  }
  accentWaiting = false;
  const float position = morph.at(beats);
  if (position < 1.0f) {
    morph = { beats, position, nextBeat(beats), 1.0f };
    switchesLanded = true;
  } else if (accent.at(beats) > 0.0f || beats < accent.endBeat) {
    dropping = true;
    dropBeat = nextBeat(beats);
  } else {
    switchesLanded = true;
  }
}

void Playback::beginBlackout(float beats) {
  const float start = press.startBeat;
  const float length = aurora_transition_beats(playing.transitionTime);
  const float now = brightnessAt(beats);
  risingFromBlack = false;
  brightness = (press.tapped || length <= 0.0f) ? Ramp::steady(0.0f)
                                                : Ramp{ start, now, start + length, 0.0f };
}

void Playback::begin(float beats) {
  if (press.slot == PROGRAM_BLACKOUT) {
    beginBlackout(beats);
    return;
  }
  const float start = press.startBeat;
  const bool blackened = brightnessAt(beats) < 1.0f;
  computeLive(beats, from);
  dropping = false;

  if (press.held && press.slot == playingSlot) {
    morph = Ramp::steady(1.0f);
    accent = { start, accent.at(beats), start + aurora_lfo_period(playing.accentTime), 1.0f };
    accentWaiting = false;
    risingFromBlack = false;
    brightness = Ramp::steady(1.0f);
    return;
  }

  playing = arriving;
  playingSlot = press.slot;
  const float length = aurora_transition_beats(playing.transitionTime);
  const bool cut = press.tapped || length <= 0.0f;
  morph = cut ? Ramp::steady(1.0f) : Ramp{ start, 0.0f, start + length, 1.0f };
  switchesLanded = cut || !press.held;
  accent = Ramp::steady(0.0f);
  accentWaiting = press.held && !press.tapped;
  risingFromBlack = blackened;
  brightness = Ramp::steady(1.0f);
  render::clearTails(wall);
}

void Playback::advance(float beats) {
  if (press.waiting && beats >= press.startBeat) {
    press.waiting = false;
    begin(beats);
  }
  if (press.judging && beats >= press.judgeBeat) press.judging = false;
  if (accentWaiting && press.held && !press.judging && beats >= morph.endBeat) {
    const float start = fmaxf(morph.endBeat, press.judgeBeat);
    accent = { start, 0.0f, start + aurora_lfo_period(playing.accentTime), 1.0f };
    accentWaiting = false;
  }
  if (dropping && beats >= dropBeat) {
    accent = Ramp::steady(0.0f);
    switchesLanded = true;
    dropping = false;
  }
}

void Playback::computeLive(float beats, float *out) const {
  float base[AURORA_PATCH_CC_COUNT];
  render::controlsOf(playing.layers[PATCH_LAYER_BASE], base);
  const float *switches = switchesLanded ? base : from;
  render::blendPatches(from, base, morph.at(beats), switches, !switchesLanded, out);
  const float pushed = accent.at(beats);
  if (pushed > 0.0f) {
    float accentLayer[AURORA_PATCH_CC_COUNT];
    render::controlsOf(playing.layers[PATCH_LAYER_ACCENT], accentLayer);
    render::blendPatches(base, accentLayer, pushed, switches, false, out);
  }
}

void Playback::cutTo(const Patch &patch, uint8_t slot) {
  playing = patch;
  playingSlot = slot;
  render::controlsOf(playing.layers[PATCH_LAYER_BASE], from);
  morph = Ramp::steady(1.0f);
  accent = Ramp::steady(0.0f);
  switchesLanded = true;
  accentWaiting = false;
  dropping = false;
  brightness = Ramp::steady(1.0f);
  risingFromBlack = false;
  press = {};
  firing = false;
  render::clearTails(wall);
}

void Playback::patchChanged(uint8_t slot) {
  if (slot == PROGRAM_BLACKOUT || slot != playingSlot || !library.patch(slot, arriving)) return;
  playing.transitionTime = arriving.transitionTime;
  playing.accentTime = arriving.accentTime;
  playing.oneshots[0] = arriving.oneshots[0];
  playing.oneshots[1] = arriving.oneshots[1];
  for (uint8_t layer = PATCH_LAYER_BASE + 1; layer < AURORA_PATCH_LAYERS; layer++) {
    memcpy(playing.layers[layer], arriving.layers[layer], AURORA_PATCH_CC_COUNT);
  }
}

void Playback::pin(const uint8_t *controls) {
  render::controlsOf(controls, pinnedControls);
  pinned = true;
}

const render::Frame &Playback::frame(uint32_t micros) {
  const float beats = tempo.beatsAt(micros);
  advance(beats);
  lastBeats = beats;
  float live[AURORA_PATCH_CC_COUNT];
  computeLive(beats, live);

  render::OneshotClock clock = { 0.0f, 1.0f, false };
  uint8_t level = 255;
  if (pinned) {
    render::composeOneshot(pinnedControls, nullptr, nullptr, composed);
  } else {
    float mixed[AURORA_PATCH_CC_COUNT];
    render::mixLayers(live, playing.layers, faders, mixed);
    const float lengthBeats = aurora_lfo_period(oneshot.length);
    if (firing) {
      lastOneshotProgress = (beats - firedAtBeats) / lengthBeats;
      if (lastOneshotProgress < 0.0f || lastOneshotProgress >= 1.0f) firing = false;
    }
    render::composeOneshot(mixed, firing ? oneshot.controls : nullptr, oneshot.marks, composed);
    if (firing) clock = { lastOneshotProgress, lengthBeats, oneshot.marks[CC_FAN_LFO] != 0 };
    level = (uint8_t)lroundf(render::clampUnit(brightnessAt(beats)) * 255.0f);
  }

  render::PadFinger finger = padFinger();
  finger.morphed = morphCorners(composed);
  render::renderFrame(composed, beats, micros / 1000, clock, finger, padSpeed(), motion, wall, rendered);
  playEffect(beats);
  if (level < 255) {
    for (render::Rgb &pixel : rendered.pixels) {
      pixel = { render::scale8(pixel.r, level), render::scale8(pixel.g, level), render::scale8(pixel.b, level) };
    }
    for (render::Par &par : rendered.pars) par.value = render::scale8(par.value, level);
  }
  return rendered;
}

}
