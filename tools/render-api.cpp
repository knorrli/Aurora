#include <stddef.h>

#include <emscripten/emscripten.h>
#include <render.h>

// The editor's way into shared/render, compiled with it by
// tools/build-render.mjs into tools/render.js. The page writes a patch's bytes
// into `controls`, renders, and reads the one static frame back out of the
// module's memory, so nothing is allocated per frame.

static uint8_t controls[AURORA_PATCH_CC_COUNT];
static render::Frame frame;

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

EMSCRIPTEN_KEEPALIVE int aurora_strips() { return render::STRIPS; }
EMSCRIPTEN_KEEPALIVE int aurora_pixels_per_strip() { return render::PIXELS; }
EMSCRIPTEN_KEEPALIVE int aurora_fan_curve_points() { return render::FAN_CURVE_POINTS; }

EMSCRIPTEN_KEEPALIVE render::Motion *aurora_motion_new() { return new render::Motion(); }

EMSCRIPTEN_KEEPALIVE void aurora_motion_copy(render::Motion *to, const render::Motion *from) {
  *to = *from;
}

EMSCRIPTEN_KEEPALIVE void aurora_render(render::Motion *motion, float beats) {
  render::renderGenerator(controls, beats, *motion, frame);
}

EMSCRIPTEN_KEEPALIVE void aurora_render_strip_order() {
  render::renderStripOrder(frame.pixels);
}

EMSCRIPTEN_KEEPALIVE float aurora_pulse_wave(float phase, int wave) {
  return render::pulseWave(phase, (uint8_t)wave);
}

EMSCRIPTEN_KEEPALIVE float aurora_convert(int cc, int value) {
  return render::convert((uint8_t)cc, (uint8_t)value);
}

EMSCRIPTEN_KEEPALIVE float aurora_light_left(float dark) { return render::lightLeft(dark); }

// Ramp times are stepped through the same periods as the pulse, and are not a
// CC, so they get the table's own lookup rather than convert().
EMSCRIPTEN_KEEPALIVE float aurora_pulse_period_beats(int value) {
  return aurora_pulse_period((uint8_t)value);
}

}  // extern "C"
