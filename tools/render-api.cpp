#include <stddef.h>

#include <emscripten/emscripten.h>
#include <palettes.h>
#include <pars.h>
#include <render.h>
#include <routes.h>

static uint8_t controls[AURORA_PATCH_CC_COUNT];
static render::Frame frame;
static float reach[2];
static render::ArpPass arpPass;

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

EMSCRIPTEN_KEEPALIVE uint8_t *aurora_controls() { return controls; }
EMSCRIPTEN_KEEPALIVE render::Rgb *aurora_pixels() { return frame.pixels; }
EMSCRIPTEN_KEEPALIVE render::Par *aurora_pars() { return frame.pars; }
EMSCRIPTEN_KEEPALIVE float *aurora_par_hue_places() { return frame.parHuePlaces; }
EMSCRIPTEN_KEEPALIVE render::FanReading *aurora_fan() { return &frame.fan; }
EMSCRIPTEN_KEEPALIVE float *aurora_bend() { return frame.bend; }

EMSCRIPTEN_KEEPALIVE int aurora_strip_count() { return render::STRIPS; }
EMSCRIPTEN_KEEPALIVE int aurora_par_count() { return render::PARS; }
EMSCRIPTEN_KEEPALIVE int aurora_pixels_per_strip() { return render::PIXELS; }
EMSCRIPTEN_KEEPALIVE int aurora_fan_curve_points() { return render::FAN_CURVE_POINTS; }
EMSCRIPTEN_KEEPALIVE int aurora_bend_points() { return render::BEND_POINTS; }

EMSCRIPTEN_KEEPALIVE render::Motion *aurora_motion_new() { return new render::Motion(); }

EMSCRIPTEN_KEEPALIVE void aurora_motion_copy(render::Motion *to, const render::Motion *from) {
  *to = *from;
}

EMSCRIPTEN_KEEPALIVE render::Wall *aurora_wall_new() { return new render::Wall(); }

EMSCRIPTEN_KEEPALIVE void aurora_wall_clear_tails(render::Wall *wall) { render::clearTails(*wall); }

EMSCRIPTEN_KEEPALIVE void aurora_render(render::Motion *motion, render::Wall *wall,
                                       float quarterNotes) {
  render::renderFrame(controls, quarterNotes, *motion, *wall, frame);
}

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
  return render::dialedValue((uint8_t)cc, (uint8_t)value);
}

EMSCRIPTEN_KEEPALIVE float aurora_lfo_period_beats(int value) {
  return aurora_lfo_period((uint8_t)value);
}

EMSCRIPTEN_KEEPALIVE int aurora_control_at_strip(int cc, int strip) {
  render::Pushes pushes;
  const float beatsPerCycle = render::dialedValue(CC_LFO_RATE, controls[CC_LFO_RATE]);
  render::gatherRoutes(controls, beatsPerCycle, frame.lfo, frame.stripLfo[strip], pushes);
  return render::routedForDisplay(controls, &pushes, (uint8_t)cc);
}

EMSCRIPTEN_KEEPALIVE int aurora_control_at_par(int cc, int par) {
  render::Pushes pushes;
  const float beatsPerCycle = render::dialedValue(CC_LFO_RATE, controls[CC_LFO_RATE]);
  render::gatherRoutes(controls, beatsPerCycle, frame.lfo, frame.lfo, pushes);
  return render::routedAtPar(controls, pushes, frame.lfo, (uint8_t)cc, (uint8_t)par);
}

EMSCRIPTEN_KEEPALIVE render::ArpPass *aurora_arp_pass() {
  render::Pushes pushes;
  const float beatsPerCycle = render::dialedValue(CC_LFO_RATE, controls[CC_LFO_RATE]);
  render::gatherRoutes(controls, beatsPerCycle, frame.lfo, frame.lfo, pushes);
  return render::firstArpPass(controls, pushes, frame.lfo, arpPass) ? &arpPass : nullptr;
}

EMSCRIPTEN_KEEPALIVE int aurora_arp_pass_marks() { return render::PASS_MARKS; }

EMSCRIPTEN_KEEPALIVE float aurora_wave_mean(int wave) {
  return render::waveMean((uint8_t)wave);
}

EMSCRIPTEN_KEEPALIVE int aurora_route_refused(int cc) { return render::routeRefused((uint8_t)cc); }

EMSCRIPTEN_KEEPALIVE float *aurora_route_reach(int cc) {
  int16_t low, high;
  if (!render::routeReach(controls, (uint8_t)cc, low, high)) return nullptr;
  reach[0] = low;
  reach[1] = high;
  return reach;
}

}
