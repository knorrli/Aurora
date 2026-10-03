#pragma once

#include <stdint.h>

#include "aurora_protocol.h"
#include "render.h"
#include "tempo.h"

namespace playback {

struct Patch {
  uint8_t transitionTime;
  uint8_t accentTime;
  uint8_t oneshots[2];
  uint8_t layers[AURORA_PATCH_LAYERS][AURORA_PATCH_CC_COUNT];
};

struct Oneshot {
  uint8_t length;
  uint8_t marks[AURORA_PATCH_CC_COUNT];
  uint8_t controls[AURORA_PATCH_CC_COUNT];
};

class Library {
 public:
  virtual bool patch(uint8_t slot, Patch &out) = 0;
  virtual bool oneshot(uint8_t index, Oneshot &out) = 0;
  virtual uint8_t defaultOneshot(uint8_t place) = 0;
};

struct Ramp {
  float startBeat;
  float start;
  float endBeat;
  float end;

  float at(float beats) const;
  static Ramp steady(float value) { return { 0.0f, value, 0.0f, value }; }
};

struct Press {
  bool waiting;
  bool judging;
  bool held;
  bool tapped;
  uint8_t slot;
  float startBeat;
  float judgeBeat;
};

struct Pad {
  uint8_t x = 0;
  uint8_t y = 0;
  bool touching = false;
  bool latching = false;
  bool playing = false;
  uint8_t mode = PAD_MODE_PER_PATCH;
  uint8_t width = PAD_WIDTH_ALL;
  bool gaps = false;
};

class Playback {
 public:
  Playback(Library &library, uint32_t micros);

  void clockTick(uint32_t micros) { tempo.tick(micros); }
  void clockStart(uint32_t micros) { tempo.start(micros); }
  void clockContinue(uint32_t micros) { tempo.resume(micros); }
  void clockStop() { tempo.stop(); }

  void programChange(uint8_t program, uint32_t micros);
  void controlChange(uint8_t cc, uint8_t value);
  void noteOn(uint8_t note, uint32_t micros);
  void noteOff(uint8_t note, uint32_t micros);

  void cutTo(const Patch &patch, uint8_t slot);
  void patchChanged(uint8_t slot);
  void pin(const uint8_t *controls);
  void unpin() { pinned = false; }

  const render::Frame &frame(uint32_t micros);

  float beats() const { return lastBeats; }
  uint8_t slot() const { return playingSlot; }
  const float *drawnControls() const { return composed; }
  float oneshotProgress() const { return firing ? lastOneshotProgress : -1.0f; }
  uint8_t oneshotIndex() const { return firing ? firedIndex : AURORA_NO_ONESHOT; }
  render::PadFinger padFinger() const;

 private:
  void advance(float beats);
  void begin(float beats);
  void beginBlackout(float beats);
  void release(float beats);
  void computeLive(float beats, float *out) const;
  float brightnessAt(float beats) const;
  float nextBeat(float beats) const;
  void fire(uint8_t index, float beats);
  void moveFader(uint8_t layer, uint8_t value);
  bool movePad(uint8_t cc, uint8_t value);
  void markedStrips(bool *out) const;
  uint8_t padEffect() const;
  float padSpeed() const;
  void playEffect(float beats);
  void loadCorners();
  float padReach() const;
  const float *morphCorners(const float *composed);

  Library &library;
  Tempo tempo;

  Patch playing;
  Patch arriving;
  uint8_t playingSlot = 0;
  float from[AURORA_PATCH_CC_COUNT];
  Ramp morph = Ramp::steady(1.0f);
  Ramp accent = Ramp::steady(0.0f);
  bool switchesLanded = true;
  bool accentWaiting = false;
  bool dropping = false;
  float dropBeat = 0.0f;
  Ramp brightness = Ramp::steady(1.0f);
  bool risingFromBlack = false;

  Press press = {};
  bool keyHeld = false;

  float faders[AURORA_PATCH_LAYERS] = {};
  Pad pad;
  Oneshot corners[AURORA_CORNERS];
  bool cornerFilled[AURORA_CORNERS] = {};
  bool cornersLoaded = false;
  int8_t switchCorner = -1;
  float morphed[render::RENDER_CONTROL_COUNT];
  bool frozen = false;
  render::Rgb frozenPixels[render::STRIPS * render::PIXELS];
  render::Par frozenPars[render::PARS];

  bool firing = false;
  Oneshot oneshot;
  uint8_t firedIndex = AURORA_NO_ONESHOT;
  float firedAtBeats = 0.0f;
  float lastOneshotProgress = 0.0f;

  bool pinned = false;
  float pinnedControls[AURORA_PATCH_CC_COUNT];

  float lastBeats = 0.0f;
  float composed[render::RENDER_CONTROL_COUNT];
  render::Motion motion;
  render::Wall wall;
  render::Frame rendered;
};

}
