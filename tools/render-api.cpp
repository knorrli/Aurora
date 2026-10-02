#include <stddef.h>
#include <string.h>

#include <emscripten/emscripten.h>
#include <arp.h>
#include <bend.h>
#include <fan.h>
#include <engines.h>
#include <morph.h>
#include <palettes.h>
#include <pars.h>
#include <playback.h>
#include <reading.h>
#include <render.h>
#include <routes.h>

static uint8_t live[AURORA_PATCH_CC_COUNT];
static uint8_t controls[render::RENDER_CONTROL_COUNT];
static render::Frame frame;
static float reach[2];
static render::ArpPass arpPass;
static uint8_t morphFrom[AURORA_PATCH_CC_COUNT];
static uint8_t morphTo[AURORA_PATCH_CC_COUNT];
static uint8_t morphSwitches[AURORA_PATCH_CC_COUNT];
static uint8_t morphOut[AURORA_PATCH_CC_COUNT];

class EditorLibrary : public playback::Library {
 public:
  playback::Patch patches[AURORA_LAST_PATCH_SLOT + 1];
  bool filled[AURORA_LAST_PATCH_SLOT + 1] = {};
  playback::Oneshot kit[AURORA_ONESHOTS];
  bool kitFilled[AURORA_ONESHOTS] = {};
  uint8_t defaults[2] = { AURORA_NO_ONESHOT, AURORA_NO_ONESHOT };

  bool patch(uint8_t slot, playback::Patch &out) override {
    if (slot > AURORA_LAST_PATCH_SLOT || !filled[slot]) return false;
    out = patches[slot];
    return true;
  }

  bool oneshot(uint8_t index, playback::Oneshot &out) override {
    if (index >= AURORA_ONESHOTS || !kitFilled[index]) return false;
    out = kit[index];
    return true;
  }

  uint8_t defaultOneshot(uint8_t place) override { return defaults[place]; }
};

static EditorLibrary library;
static playback::Patch patchBuffer;
static playback::Oneshot oneshotBuffer;
static playback::Playback *player = nullptr;

static_assert(sizeof(playback::Patch) == 4 + AURORA_PATCH_LAYERS * AURORA_PATCH_CC_COUNT, "");
static_assert(sizeof(playback::Oneshot) == 1 + 2 * AURORA_PATCH_CC_COUNT, "");
static_assert(offsetof(render::FanReading, curve) == sizeof(float) * render::STRIPS, "");
static_assert(offsetof(render::FanReading, turns)
                  == sizeof(float) * (render::STRIPS + render::FAN_CURVE_POINTS), "");
static_assert(sizeof(render::Par) == 4, "");
static_assert(offsetof(render::ArpPass, count) == sizeof(float) * 3, "");
static_assert(offsetof(render::ArpPass, starts) == sizeof(float) * 4, "");
static_assert(offsetof(render::ArpPass, pars)
                  == sizeof(float) * (4 + render::PASS_MARKS), "");
static_assert(sizeof(render::FanReading)
                  == sizeof(float) * (render::STRIPS + render::FAN_CURVE_POINTS + 6), "");

extern "C" {

EMSCRIPTEN_KEEPALIVE uint8_t *aurora_controls() { return live; }
EMSCRIPTEN_KEEPALIVE uint8_t *aurora_drawn_controls() { return controls; }
EMSCRIPTEN_KEEPALIVE render::Rgb *aurora_pixels() { return frame.pixels; }
EMSCRIPTEN_KEEPALIVE render::Par *aurora_pars() { return frame.pars; }
EMSCRIPTEN_KEEPALIVE float *aurora_par_hue_places() { return frame.parHuePlaces; }
EMSCRIPTEN_KEEPALIVE uint8_t *aurora_par_hues() { return frame.parHues; }
EMSCRIPTEN_KEEPALIVE int aurora_strips_hue() { return frame.stripsHue; }
EMSCRIPTEN_KEEPALIVE float aurora_lfo() { return frame.lfo; }
EMSCRIPTEN_KEEPALIVE render::StripSpots *aurora_spots() { return frame.spots; }
EMSCRIPTEN_KEEPALIVE float *aurora_field_levels() { return frame.fieldLevels; }
EMSCRIPTEN_KEEPALIVE float *aurora_field_across() { return frame.fieldAcross; }
EMSCRIPTEN_KEEPALIVE int aurora_field_across_points() { return render::FIELD_ACROSS_POINTS; }
EMSCRIPTEN_KEEPALIVE int aurora_strip_spots_size() { return sizeof(render::StripSpots); }
EMSCRIPTEN_KEEPALIVE int aurora_spot_destination(int cc) { return render::spotDestination((uint8_t)cc); }
EMSCRIPTEN_KEEPALIVE render::FanReading *aurora_fan() { return &frame.fan; }
EMSCRIPTEN_KEEPALIVE float *aurora_bend() { return frame.bend; }
EMSCRIPTEN_KEEPALIVE float *aurora_centers() { return frame.centers; }

EMSCRIPTEN_KEEPALIVE int aurora_strip_count() { return render::STRIPS; }
EMSCRIPTEN_KEEPALIVE int aurora_par_count() { return render::PARS; }
EMSCRIPTEN_KEEPALIVE int aurora_pixels_per_strip() { return render::PIXELS; }
EMSCRIPTEN_KEEPALIVE int aurora_fan_curve_points() { return render::FAN_CURVE_POINTS; }
EMSCRIPTEN_KEEPALIVE int aurora_bend_points() { return render::BEND_POINTS; }

EMSCRIPTEN_KEEPALIVE playback::Patch *aurora_patch_buffer() { return &patchBuffer; }
EMSCRIPTEN_KEEPALIVE playback::Oneshot *aurora_oneshot_buffer() { return &oneshotBuffer; }

EMSCRIPTEN_KEEPALIVE void aurora_library_store_patch(int slot) {
  if (slot < 0 || slot > AURORA_LAST_PATCH_SLOT) return;
  library.patches[slot] = patchBuffer;
  library.filled[slot] = true;
}

EMSCRIPTEN_KEEPALIVE void aurora_library_clear_patch(int slot) {
  if (slot >= 0 && slot <= AURORA_LAST_PATCH_SLOT) library.filled[slot] = false;
}

EMSCRIPTEN_KEEPALIVE void aurora_library_store_oneshot(int index) {
  if (index < 0 || index >= AURORA_ONESHOTS) return;
  library.kit[index] = oneshotBuffer;
  library.kitFilled[index] = true;
}

EMSCRIPTEN_KEEPALIVE void aurora_library_clear_oneshot(int index) {
  if (index >= 0 && index < AURORA_ONESHOTS) library.kitFilled[index] = false;
}

EMSCRIPTEN_KEEPALIVE void aurora_library_default_oneshots(int first, int second) {
  library.defaults[0] = (uint8_t)first;
  library.defaults[1] = (uint8_t)second;
}

EMSCRIPTEN_KEEPALIVE void aurora_playback_begin(uint32_t micros) {
  if (!player) player = new playback::Playback(library, micros);
}

EMSCRIPTEN_KEEPALIVE void aurora_playback_message(int status, int first, int second, uint32_t micros) {
  switch (status & 0xF0) {
    case 0xC0: player->programChange((uint8_t)first, micros); return;
    case 0xB0: player->controlChange((uint8_t)first, (uint8_t)second); return;
    case 0x90:
      if (second) player->noteOn((uint8_t)first, micros);
      else player->noteOff((uint8_t)first, micros);
      return;
    case 0x80: player->noteOff((uint8_t)first, micros); return;
  }
  switch (status) {
    case 0xF8: player->clockTick(micros); return;
    case 0xFA: player->clockStart(micros); return;
    case 0xFB: player->clockContinue(micros); return;
    case 0xFC: player->clockStop(); return;
  }
}

EMSCRIPTEN_KEEPALIVE void aurora_playback_cut(int slot) { player->cutTo(patchBuffer, (uint8_t)slot); }
EMSCRIPTEN_KEEPALIVE void aurora_playback_patch_changed(int slot) { player->patchChanged((uint8_t)slot); }
EMSCRIPTEN_KEEPALIVE void aurora_playback_pin() { player->pin(live); }
EMSCRIPTEN_KEEPALIVE void aurora_playback_unpin() { player->unpin(); }

EMSCRIPTEN_KEEPALIVE void aurora_playback_frame(uint32_t micros) {
  frame = player->frame(micros);
  memcpy(controls, player->drawnControls(), sizeof(controls));
}

EMSCRIPTEN_KEEPALIVE float aurora_playback_beats() { return player->beats(); }
EMSCRIPTEN_KEEPALIVE int aurora_playback_slot() { return player->slot(); }
EMSCRIPTEN_KEEPALIVE float aurora_playback_oneshot_progress() { return player->oneshotProgress(); }
EMSCRIPTEN_KEEPALIVE int aurora_playback_oneshot_index() { return player->oneshotIndex(); }

EMSCRIPTEN_KEEPALIVE int aurora_palette_count() { return render::paletteCount(); }

EMSCRIPTEN_KEEPALIVE const char *aurora_palette_name(int index) {
  return render::paletteName((uint8_t)index);
}

EMSCRIPTEN_KEEPALIVE int aurora_palette_color(int palette, int hue, int saturation) {
  const render::Rgb rgb = render::paletteColor((uint8_t)palette, (uint8_t)hue, (uint8_t)saturation);
  return (rgb.r << 16) | (rgb.g << 8) | rgb.b;
}

EMSCRIPTEN_KEEPALIVE float aurora_lfo_wave(float phase, int wave) {
  return render::lfoWave(phase, (uint8_t)wave);
}

EMSCRIPTEN_KEEPALIVE float aurora_convert(int cc, int value) {
  return render::controlValue((uint8_t)cc, (uint8_t)value);
}

EMSCRIPTEN_KEEPALIVE float aurora_lfo_period_beats(int value) { return aurora_lfo_period((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE float aurora_transition_length(int value) { return aurora_transition_beats((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE float aurora_signed(int value) { return render::signedOf((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_ratio_step(int value) { return aurora_route_ratio_step((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_ratio(int value) { return aurora_route_ratio((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_once(int value) { return aurora_route_once((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_phase_step(int value) { return aurora_route_phase_step((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_arp_mode_of(int value) { return aurora_arp_mode((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_hue_layout_of(int value) { return aurora_hue_layout((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_switch_on(int value) { return aurora_switch_is_on((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_three_way(int value) { return aurora_three_way_position((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_destination_arp(int destination) { return aurora_route_arp((uint8_t)destination); }
EMSCRIPTEN_KEEPALIVE int aurora_destination_bipolar(int destination) { return aurora_route_bipolar((uint8_t)destination); }
EMSCRIPTEN_KEEPALIVE int aurora_destination_target(int destination) { return aurora_route_target((uint8_t)destination); }
EMSCRIPTEN_KEEPALIVE int aurora_destination(int target, int arp, int bipolar) {
  return aurora_route_destination((uint8_t)target, (uint8_t)arp, bipolar != 0);
}

EMSCRIPTEN_KEEPALIVE float aurora_fan_wave_at(int phase, int frequency, int strip) {
  render::Fan fan = {};
  fan.phase = render::controlValue(CC_FAN_PHASE, (uint8_t)phase);
  fan.frequency = render::controlValue(CC_FAN_FREQUENCY, (uint8_t)frequency);
  return render::fanWave(fan, (uint8_t)strip);
}

static render::Arp arpOf(int mode, int spread) {
  return { aurora_arp_mode((uint8_t)mode), render::controlValue(CC_ARP_SPREAD, (uint8_t)spread) };
}

EMSCRIPTEN_KEEPALIVE int aurora_arp_pass_length(int mode) { return render::passLength(arpOf(mode, 127)); }
EMSCRIPTEN_KEEPALIVE int aurora_arp_turns_per_pass(int mode, int spread) { return render::turnsPerPass(arpOf(mode, spread)); }
EMSCRIPTEN_KEEPALIVE int aurora_arp_reversed(int mode, int spread) { return render::reversed(arpOf(mode, spread)); }

EMSCRIPTEN_KEEPALIVE float aurora_route_turns(int ratio, int phase, float clock) {
  const render::RouteTiming timing = { 0, aurora_route_ratio((uint8_t)ratio), aurora_route_once((uint8_t)ratio),
                                       aurora_route_phase((uint8_t)phase) };
  return render::turnsOn(timing, clock, 0.0f);
}

EMSCRIPTEN_KEEPALIVE float aurora_bend_speed_ratio(int bend) {
  return render::bendSpeedRatio(render::controlValue(CC_SHAPE_BEND, (uint8_t)bend));
}

EMSCRIPTEN_KEEPALIVE int aurora_control_at_strip(int cc, int strip) {
  render::Modulation modulation;
  const float beatsPerCycle = render::controlValue(CC_LFO_RATE, controls[CC_LFO_RATE]);
  render::gatherRoutes(controls, beatsPerCycle, frame.lfo, frame.stripFanShift[strip], modulation);
  return render::routedForDisplay(controls, &modulation, (uint8_t)cc);
}

EMSCRIPTEN_KEEPALIVE int aurora_control_at_par(int cc, int par) {
  render::Modulation modulation;
  const float beatsPerCycle = render::controlValue(CC_LFO_RATE, controls[CC_LFO_RATE]);
  render::gatherRoutes(controls, beatsPerCycle, frame.lfo, 0.0f, modulation);
  return render::routedAtPar(controls, modulation, frame.lfo, (uint8_t)cc, (uint8_t)par);
}

EMSCRIPTEN_KEEPALIVE render::ArpPass *aurora_arp_pass() {
  render::Modulation modulation;
  const float beatsPerCycle = render::controlValue(CC_LFO_RATE, controls[CC_LFO_RATE]);
  render::gatherRoutes(controls, beatsPerCycle, frame.lfo, 0.0f, modulation);
  return render::firstArpPass(controls, modulation, frame.lfo, arpPass) ? &arpPass : nullptr;
}

EMSCRIPTEN_KEEPALIVE int aurora_arp_pass_marks() { return render::PASS_MARKS; }

EMSCRIPTEN_KEEPALIVE float aurora_wave_mean(int wave) {
  return render::waveMean((uint8_t)wave);
}

EMSCRIPTEN_KEEPALIVE int aurora_route_refused(int cc) { return render::routeRefused((uint8_t)cc); }

EMSCRIPTEN_KEEPALIVE uint8_t *aurora_morph_from() { return morphFrom; }
EMSCRIPTEN_KEEPALIVE uint8_t *aurora_morph_to() { return morphTo; }
EMSCRIPTEN_KEEPALIVE uint8_t *aurora_morph_switches() { return morphSwitches; }

EMSCRIPTEN_KEEPALIVE uint8_t *aurora_blend(float position, int switchesFromStart) {
  render::blendPatches(morphFrom, morphTo, position, morphSwitches, switchesFromStart != 0, morphOut);
  return morphOut;
}

EMSCRIPTEN_KEEPALIVE float *aurora_route_reach(int cc) {
  int16_t low, high;
  if (!render::routeReach(controls, (uint8_t)cc, low, high)) return nullptr;
  reach[0] = low;
  reach[1] = high;
  return reach;
}

}
