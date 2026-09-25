#include <stddef.h>

#include <emscripten/emscripten.h>
#include <palettes.h>
#include <render.h>

// The editor's way into shared/render, compiled with it by
// tools/build-render.mjs into tools/render.js. The page writes a patch's bytes
// into `controls`, renders, and reads the one static frame back out of the
// module's memory, so nothing is allocated per frame.

static uint8_t controls[AURORA_PATCH_CC_COUNT];
static render::Frame frame;
static float reach[2];

// tools/preview.js reads FanReading as one run of floats in this order.
static_assert(offsetof(render::FanReading, curve) == sizeof(float) * render::STRIPS, "");
static_assert(offsetof(render::FanReading, turns)
                  == sizeof(float) * (render::STRIPS + render::FAN_CURVE_POINTS), "");
static_assert(sizeof(render::FanReading)
                  == sizeof(float) * (render::STRIPS + render::FAN_CURVE_POINTS + 6), "");

extern "C" {

EMSCRIPTEN_KEEPALIVE uint8_t *aurora_controls() { return controls; }
EMSCRIPTEN_KEEPALIVE render::Rgb *aurora_pixels() { return frame.pixels; }
EMSCRIPTEN_KEEPALIVE render::Wash *aurora_wash() { return &frame.wash; }
EMSCRIPTEN_KEEPALIVE render::FanReading *aurora_fan() { return &frame.fan; }
EMSCRIPTEN_KEEPALIVE float *aurora_bend() { return frame.bend; }

EMSCRIPTEN_KEEPALIVE int aurora_strips() { return render::STRIPS; }
EMSCRIPTEN_KEEPALIVE int aurora_pixels_per_strip() { return render::PIXELS; }
EMSCRIPTEN_KEEPALIVE int aurora_fan_curve_points() { return render::FAN_CURVE_POINTS; }
EMSCRIPTEN_KEEPALIVE int aurora_bend_points() { return render::BEND_POINTS; }

EMSCRIPTEN_KEEPALIVE render::Motion *aurora_motion_new() { return new render::Motion(); }

EMSCRIPTEN_KEEPALIVE void aurora_motion_copy(render::Motion *to, const render::Motion *from) {
  *to = *from;
}

EMSCRIPTEN_KEEPALIVE render::Paths *aurora_paths_new() { return new render::Paths(); }

EMSCRIPTEN_KEEPALIVE void aurora_paths_clear(render::Paths *paths) { render::clearPaths(*paths); }

EMSCRIPTEN_KEEPALIVE void aurora_render(render::Motion *motion, render::Paths *paths, float quarterNotes) {
  render::renderGenerator(controls, quarterNotes, *motion, *paths, frame);
}

EMSCRIPTEN_KEEPALIVE int aurora_palette_count() { return render::paletteCount(); }

EMSCRIPTEN_KEEPALIVE const char *aurora_palette_name(int index) {
  return render::paletteName((uint8_t)index);
}

EMSCRIPTEN_KEEPALIVE float aurora_lfo_wave(float phase, int wave) {
  return render::lfoWave(phase, (uint8_t)wave);
}

EMSCRIPTEN_KEEPALIVE float aurora_convert(int cc, int value) {
  return render::convert((uint8_t)cc, (uint8_t)value);
}

// Ramp times are stepped through the same periods as the LFO, and are not a
// CC, so they get the table's own lookup rather than convert().
EMSCRIPTEN_KEEPALIVE float aurora_lfo_period_beats(int value) {
  return aurora_lfo_period((uint8_t)value);
}

// Where a control stands on one strip this frame, as the last render's routes
// pushed it.
EMSCRIPTEN_KEEPALIVE int aurora_strip_value(int cc, int strip) {
  render::Pushes pushes;
  const float beatsPerCycle = render::convert(CC_GEN_LFO_RATE, controls[CC_GEN_LFO_RATE]);
  render::gatherRoutes(controls, beatsPerCycle, frame.lfo, frame.stripLfo[strip], pushes);
  return render::routedForDisplay(controls, &pushes, (uint8_t)cc);
}

EMSCRIPTEN_KEEPALIVE float aurora_wave_mean(int wave) {
  return render::waveMean((uint8_t)wave);
}

EMSCRIPTEN_KEEPALIVE int aurora_route_refused(int cc) { return render::routeRefused((uint8_t)cc); }

// The band a control's routes can push it across, or null if none reach it.
EMSCRIPTEN_KEEPALIVE float *aurora_route_reach(int cc) {
  int16_t low, high;
  if (!render::routeReach(controls, (uint8_t)cc, low, high)) return nullptr;
  reach[0] = low;
  reach[1] = high;
  return reach;
}

}  // extern "C"
