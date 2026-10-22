#include <stddef.h>
#include <string.h>

#include <emscripten/emscripten.h>
#include <arp.h>
#include <bend.h>
#include <cells.h>
#include <engines.h>
#include <morph.h>
#include <palettes.h>
#include <pars.h>
#include <playback.h>
#include <reading.h>
#include <render.h>
#include <routes.h>

static uint8_t live[AURORA_PATCH_CONTROLS];
static float controls[render::RENDER_CONTROL_COUNT];
static uint8_t drawnBytes[render::RENDER_CONTROL_COUNT];
static render::Frame frame;
static float reach[2];
static render::ArpPass arpPass;
static uint8_t morphFrom[AURORA_PATCH_CONTROLS];
static uint8_t morphTo[AURORA_PATCH_CONTROLS];
static uint8_t morphSwitches[AURORA_PATCH_CONTROLS];
static uint8_t morphOut[AURORA_PATCH_CONTROLS];

class EditorLibrary : public playback::Library {
 public:
  playback::Patch patches[AURORA_LAST_PATCH_SLOT + 1];
  bool filled[AURORA_LAST_PATCH_SLOT + 1] = {};
  playback::Oneshot kit[AURORA_KIT_PLACES];
  bool kitFilled[AURORA_KIT_PLACES] = {};
  uint8_t defaults[2] = { AURORA_NO_ONESHOT, AURORA_NO_ONESHOT };

  bool patch(uint8_t slot, playback::Patch &out) override {
    if (slot > AURORA_LAST_PATCH_SLOT || !filled[slot]) return false;
    out = patches[slot];
    return true;
  }

  bool oneshot(uint8_t index, playback::Oneshot &out) override {
    if (index >= AURORA_KIT_PLACES || !kitFilled[index]) return false;
    out = kit[index];
    return true;
  }

  uint8_t defaultOneshot(uint8_t place) override { return defaults[place]; }
};

static EditorLibrary library;
static playback::Patch patchBuffer;
static playback::Oneshot oneshotBuffer;
static playback::Playback *player = nullptr;

static_assert(sizeof(playback::Patch) == 4 + AURORA_PATCH_LAYERS * AURORA_PATCH_CONTROLS, "");
static_assert(sizeof(playback::Oneshot) == 1 + 2 * AURORA_PATCH_CONTROLS, "");
static_assert(sizeof(render::Par) == 4, "");
static_assert(offsetof(render::ArpPass, count) == sizeof(float) * 3, "");
static_assert(offsetof(render::ArpPass, starts) == sizeof(float) * 4, "");
static_assert(offsetof(render::ArpPass, pars)
                  == sizeof(float) * (4 + render::PASS_MARKS), "");

extern "C" {

EMSCRIPTEN_KEEPALIVE uint8_t *aurora_controls() { return live; }
EMSCRIPTEN_KEEPALIVE uint8_t *aurora_drawn_controls() { return drawnBytes; }
EMSCRIPTEN_KEEPALIVE render::Rgb *aurora_pixels() { return frame.pixels; }
EMSCRIPTEN_KEEPALIVE render::Par *aurora_pars() { return frame.pars; }
EMSCRIPTEN_KEEPALIVE float *aurora_par_hue_places() { return frame.parHuePlaces; }
EMSCRIPTEN_KEEPALIVE uint8_t *aurora_par_hues() { return frame.parHues; }
EMSCRIPTEN_KEEPALIVE int aurora_strips_hue() { return frame.stripsHue; }
EMSCRIPTEN_KEEPALIVE float aurora_lfo() { return frame.lfo; }
EMSCRIPTEN_KEEPALIVE render::StripMarks *aurora_spots() { return frame.spots; }
EMSCRIPTEN_KEEPALIVE render::StripMarks *aurora_cells() { return frame.cells; }
EMSCRIPTEN_KEEPALIVE float *aurora_field_levels() { return frame.fieldLevels; }
EMSCRIPTEN_KEEPALIVE float *aurora_curve() { return frame.curve; }
EMSCRIPTEN_KEEPALIVE int aurora_curve_shown() { return frame.curveShown; }
EMSCRIPTEN_KEEPALIVE int aurora_curve_horizontal() { return frame.curveHorizontal; }
EMSCRIPTEN_KEEPALIVE float *aurora_curve_across() { return frame.curveAcross; }
EMSCRIPTEN_KEEPALIVE float *aurora_field_across() { return frame.fieldAcross; }
EMSCRIPTEN_KEEPALIVE int aurora_field_across_points() { return render::FIELD_ACROSS_POINTS; }
EMSCRIPTEN_KEEPALIVE int aurora_strip_marks_size() { return sizeof(render::StripMarks); }
EMSCRIPTEN_KEEPALIVE int aurora_spot_destination(int control) { return render::spotDestination((uint8_t)control); }
EMSCRIPTEN_KEEPALIVE float *aurora_bend() { return frame.bend; }

EMSCRIPTEN_KEEPALIVE int aurora_strip_count() { return render::STRIPS; }
EMSCRIPTEN_KEEPALIVE int aurora_par_count() { return render::PARS; }
EMSCRIPTEN_KEEPALIVE int aurora_pixels_per_strip() { return render::PIXELS; }
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
  if (index < 0 || index >= AURORA_KIT_PLACES) return;
  library.kit[index] = oneshotBuffer;
  library.kitFilled[index] = true;
}

EMSCRIPTEN_KEEPALIVE void aurora_library_clear_oneshot(int index) {
  if (index >= 0 && index < AURORA_KIT_PLACES) library.kitFilled[index] = false;
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
      if (second) player->noteOn((uint8_t)first, (uint8_t)second, micros);
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

EMSCRIPTEN_KEEPALIVE void aurora_playback_pin() { player->pin(live); }
EMSCRIPTEN_KEEPALIVE void aurora_playback_unpin() { player->unpin(); }
EMSCRIPTEN_KEEPALIVE void aurora_playback_dial(int slot, int cut) {
  player->dial(patchBuffer, (uint8_t)slot, cut != 0);
}
EMSCRIPTEN_KEEPALIVE void aurora_playback_pin_layer(int layer, int steps) {
  player->pinLayer(morphTo, (uint8_t)layer, (uint16_t)steps);
}

EMSCRIPTEN_KEEPALIVE void aurora_playback_frame(uint32_t micros) {
  frame = player->frame(micros);
  memcpy(controls, player->drawnControls(), sizeof(controls));
  for (uint16_t i = 0; i < render::RENDER_CONTROL_COUNT; i++) {
    drawnBytes[i] = i < AURORA_PATCH_CONTROLS ? render::displayedByte((uint8_t)i, controls[i])
                                              : render::roundedControl(controls[i]);
  }
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
  const render::Rgb rgb = render::paletteColor((uint8_t)palette, (float)hue, (float)saturation);
  return (rgb.r << 16) | (rgb.g << 8) | rgb.b;
}

EMSCRIPTEN_KEEPALIVE float aurora_lfo_wave(float phase, int wave, int smooth) {
  return render::lfoWave(phase, { (uint8_t)wave, (uint8_t)smooth });
}

EMSCRIPTEN_KEEPALIVE float aurora_convert(int control, int value) {
  return render::controlValue((uint8_t)control, (float)value);
}

EMSCRIPTEN_KEEPALIVE float aurora_lfo_period_beats(int value) { return aurora_lfo_period((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE float aurora_transition_length(int value) { return aurora_transition_beats((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE float aurora_signed(int value) { return render::signedOf((float)value); }
EMSCRIPTEN_KEEPALIVE int aurora_ratio(int value) { return aurora_route_ratio((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_once(int value) { return aurora_route_once((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_source(int value) { return aurora_route_source((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_phase_step(int value) { return aurora_route_phase_step((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_arp_mode_of(int value) { return aurora_arp_mode((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_hue_layout_of(int value) { return aurora_hue_layout((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_switch_on(int value) { return aurora_switch_is_on((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_three_way(int value) { return aurora_three_way_position((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_four_way(int value) { return aurora_four_way_position((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_arp_of(int value) { return aurora_route_arp((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_arp_capable_of(int control) { return render::arpCapable((uint8_t)control); }
EMSCRIPTEN_KEEPALIVE int aurora_bipolar(int value) { return aurora_route_bipolar((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE int aurora_bipolar_capable_of(int control) { return aurora_bipolar_capable((uint8_t)control); }
EMSCRIPTEN_KEEPALIVE int aurora_group_pattern_of(int value) { return aurora_group_pattern((uint8_t)value); }
EMSCRIPTEN_KEEPALIVE float aurora_group_level(int pattern, int index, int count, int salt) {
  return render::groupLevel((uint8_t)pattern, (uint8_t)index, (uint8_t)count, (uint32_t)salt);
}

EMSCRIPTEN_KEEPALIVE float aurora_route_turns(int ratio, int cycle, int phase, float clock) {
  const render::RouteTiming timing = { 0, aurora_route_ratio((uint8_t)ratio), aurora_route_once((uint8_t)cycle),
                                       aurora_route_phase((uint8_t)phase) };
  return render::turnsOn(timing, clock, 0.0f);
}

EMSCRIPTEN_KEEPALIVE float aurora_bend_speed_ratio(int bend) {
  return render::bendSpeedRatio(render::controlValue(CONTROL_CELL_BEND, (float)bend));
}

static const render::PadLevels *padReaching(int strip, render::PadLevels &levels) {
  if (!player) return nullptr;
  const render::PadFinger finger = player->padFinger();
  if (!finger.playing || (strip >= 0 && !finger.strips[strip])) return nullptr;
  levels = { finger.x, finger.y };
  return &levels;
}

EMSCRIPTEN_KEEPALIVE int aurora_control_at_strip(int control, int strip) {
  render::Modulation modulation;
  render::PadLevels levels;
  const float beatsPerCycle = render::controlValue(CONTROL_LFO_RATE, controls[CONTROL_LFO_RATE]);
  render::gatherRoutes(controls, beatsPerCycle, frame.lfo, (int8_t)strip, padReaching(strip, levels), modulation);
  return render::routedForDisplay(controls, &modulation, (uint8_t)control);
}

EMSCRIPTEN_KEEPALIVE int aurora_control_at_par(int control, int par) {
  render::Modulation modulation;
  render::PadLevels levels;
  const float beatsPerCycle = render::controlValue(CONTROL_LFO_RATE, controls[CONTROL_LFO_RATE]);
  render::gatherRoutes(controls, beatsPerCycle, frame.lfo, -1, padReaching(-1, levels), modulation);
  return render::routedAtPar(controls, modulation, (uint8_t)control, (uint8_t)par);
}

static float routeAtStrip(int route, int strip, bool shift) {
  static render::Routes routes;
  render::readRoutes(controls, routes);
  render::PadLevels levels;
  const float beatsPerCycle = render::controlValue(CONTROL_LFO_RATE, controls[CONTROL_LFO_RATE]);
  const render::RouteContext context = { frame.lfo, beatsPerCycle, 0.0f, padReaching(strip, levels), (int8_t)strip, nullptr };
  return shift ? render::routeShiftAt(routes, (uint8_t)route, context) : render::routeAmountAt(routes, (uint8_t)route, context);
}

EMSCRIPTEN_KEEPALIVE float aurora_route_amount_at_strip(int route, int strip) { return routeAtStrip(route, strip, false); }
EMSCRIPTEN_KEEPALIVE float aurora_route_shift_at_strip(int route, int strip) { return routeAtStrip(route, strip, true); }

EMSCRIPTEN_KEEPALIVE render::ArpPass *aurora_arp_pass() {
  render::Modulation modulation;
  render::PadLevels levels;
  const float beatsPerCycle = render::controlValue(CONTROL_LFO_RATE, controls[CONTROL_LFO_RATE]);
  render::gatherRoutes(controls, beatsPerCycle, frame.lfo, -1, padReaching(-1, levels), modulation);
  return render::firstArpPass(controls, modulation, arpPass) ? &arpPass : nullptr;
}

EMSCRIPTEN_KEEPALIVE int aurora_arp_pass_marks() { return render::PASS_MARKS; }

EMSCRIPTEN_KEEPALIVE float aurora_wave_mean(int wave, int smooth) {
  return render::waveMean({ (uint8_t)wave, (uint8_t)smooth });
}

EMSCRIPTEN_KEEPALIVE int aurora_route_refused(int control) { return render::routeRefused((uint8_t)control); }
EMSCRIPTEN_KEEPALIVE int aurora_cell_control(int control) { return render::cellControl((uint8_t)control); }
EMSCRIPTEN_KEEPALIVE int aurora_route_held(int value) { return aurora_route_held((uint8_t)value); }

EMSCRIPTEN_KEEPALIVE float aurora_envelope_level(int delay, int attack, int hold, int decay, int sustain, int release,
                                                 int moves, int gate, int end, int rate, float age) {
  const render::Envelope envelope = {
    render::controlValue(CONTROL_ENVELOPE_1_DELAY, (float)delay), render::controlValue(CONTROL_ENVELOPE_1_ATTACK, (float)attack), render::controlValue(CONTROL_ENVELOPE_1_HOLD, (float)hold),
    render::controlValue(CONTROL_ENVELOPE_1_DECAY, (float)decay), render::controlValue(CONTROL_ENVELOPE_1_SUSTAIN, (float)sustain),
    render::controlValue(CONTROL_ENVELOPE_1_RELEASE, (float)release), aurora_switch_is_on((uint8_t)moves),
  };
  return render::envelopeLevelAt(envelope, render::controlValue(CONTROL_CELL_GATE, (float)gate),
                                 aurora_three_way_position((uint8_t)end),
                                 render::controlValue(CONTROL_CELL_RATE, (float)rate), age);
}

EMSCRIPTEN_KEEPALIVE uint8_t *aurora_morph_from() { return morphFrom; }
EMSCRIPTEN_KEEPALIVE uint8_t *aurora_morph_to() { return morphTo; }
EMSCRIPTEN_KEEPALIVE uint8_t *aurora_morph_switches() { return morphSwitches; }

EMSCRIPTEN_KEEPALIVE uint8_t *aurora_blend(float position, int switchesFromStart) {
  float from[AURORA_PATCH_CONTROLS], to[AURORA_PATCH_CONTROLS], switches[AURORA_PATCH_CONTROLS];
  float blended[AURORA_PATCH_CONTROLS];
  render::controlsOf(morphFrom, from);
  render::controlsOf(morphTo, to);
  render::controlsOf(morphSwitches, switches);
  render::blendPatches(from, to, position, switches, switchesFromStart != 0, blended);
  for (uint16_t i = 0; i < AURORA_PATCH_CONTROLS; i++) morphOut[i] = render::displayedByte((uint8_t)i, blended[i]);
  return morphOut;
}

EMSCRIPTEN_KEEPALIVE uint8_t *aurora_mix_layer(int layer, float position) {
  static uint8_t layers[AURORA_PATCH_LAYERS][AURORA_PATCH_CONTROLS];
  float positions[AURORA_PATCH_LAYERS] = {};
  for (uint8_t each = 0; each < AURORA_PATCH_LAYERS; each++) {
    memcpy(layers[each], each == layer ? morphTo : morphFrom, AURORA_PATCH_CONTROLS);
  }
  positions[layer] = position;
  float base[AURORA_PATCH_CONTROLS];
  static float mixed[render::RENDER_CONTROL_COUNT];
  render::controlsOf(morphFrom, base);
  render::mixLayers(base, layers, positions, mixed);
  bool used[AURORA_PATCH_CONTROLS];
  render::usedControls(morphTo, used);
  for (uint16_t i = 0; i < AURORA_PATCH_CONTROLS; i++) {
    morphOut[i] = used[i] ? render::displayedByte((uint8_t)i, mixed[i]) : morphTo[i];
  }
  for (uint8_t route = 0; route < AURORA_ROUTES && position >= 0.5f; route++) {
    const uint8_t echo = render::echoRoute((uint8_t)layer, route);
    if (!render::roundedControl(mixed[render::routeControl(echo, ROUTE_DESTINATION)])) continue;
    for (uint8_t field = 0; field < ROUTE_FIELDS; field++) {
      const uint8_t control = aurora_route_control(route, field);
      morphOut[control] = morphTo[control];
    }
  }
  return morphOut;
}

EMSCRIPTEN_KEEPALIVE float *aurora_route_reach(int control) {
  int16_t low, high;
  if (!render::routeReach(controls, (uint8_t)control, low, high)) return nullptr;
  reach[0] = low;
  reach[1] = high;
  return reach;
}

}
