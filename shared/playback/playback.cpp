#include "playback.h"

#include <math.h>
#include <string.h>

#include "morph.h"
#include "render_math.h"

namespace playback {

static const float HOLD_JUDGED_AFTER_MICROS = 200000.0f;

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
  memcpy(from, playing.layers[PATCH_LAYER_BASE], AURORA_PATCH_CC_COUNT);
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
  if (program > AURORA_LAST_PATCH_SLOT) return;
  if (program != PROGRAM_BLACKOUT && !library.patch(program, arriving)) return;

  press = {};
  press.waiting = true;
  press.held = keyHeld;
  press.judging = keyHeld;
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
    case CC_FADER_COLOR: faders[PATCH_LAYER_COLOR] = (float)value / 127.0f; return;
    case CC_FADER_EXTENT: faders[PATCH_LAYER_EXTENT] = (float)value / 127.0f; return;
    case CC_FADER_MOTION: faders[PATCH_LAYER_MOTION] = (float)value / 127.0f; return;
    default:
      if (cc < AURORA_PATCH_CC_COUNT) playing.layers[PATCH_LAYER_BASE][cc] = value;
  }
}

void Playback::noteOn(uint8_t note, uint32_t micros) {
  const float beats = tempo.beatsAt(micros);
  advance(beats);
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

void Playback::computeLive(float beats, uint8_t *out) const {
  const uint8_t *base = playing.layers[PATCH_LAYER_BASE];
  const uint8_t *switches = switchesLanded ? base : from;
  render::blendPatches(from, base, morph.at(beats), switches, !switchesLanded, out);
  const float pushed = accent.at(beats);
  if (pushed > 0.0f) {
    render::blendPatches(base, playing.layers[PATCH_LAYER_ACCENT], pushed, switches, false, out);
  }
}

void Playback::cutTo(const Patch &patch, uint8_t slot) {
  playing = patch;
  playingSlot = slot;
  memcpy(from, playing.layers[PATCH_LAYER_BASE], AURORA_PATCH_CC_COUNT);
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
  memcpy(pinnedControls, controls, AURORA_PATCH_CC_COUNT);
  pinned = true;
}

const render::Frame &Playback::frame(uint32_t micros) {
  const float beats = tempo.beatsAt(micros);
  advance(beats);
  lastBeats = beats;
  uint8_t live[AURORA_PATCH_CC_COUNT];
  computeLive(beats, live);

  render::OneshotClock clock = { 0.0f, 1.0f, false };
  uint8_t level = 255;
  if (pinned) {
    render::composeOneshot(pinnedControls, nullptr, nullptr, composed);
  } else {
    uint8_t mixed[AURORA_PATCH_CC_COUNT];
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

  render::renderFrame(composed, beats, micros / 1000, clock, motion, wall, rendered);
  if (level < 255) {
    for (render::Rgb &pixel : rendered.pixels) {
      pixel = { render::scale8(pixel.r, level), render::scale8(pixel.g, level), render::scale8(pixel.b, level) };
    }
    for (render::Par &par : rendered.pars) par.value = render::scale8(par.value, level);
  }
  return rendered;
}

}
