#include "effects.h"

#include <math.h>

namespace render {

static const Rgb WHITE = { 255, 255, 255 };

static uint8_t mixed(uint8_t from, uint8_t to, float share) {
  return (uint8_t)lroundf((float)from + ((float)to - (float)from) * share);
}

static Rgb mixed(const Rgb &from, const Rgb &to, float share) {
  return { mixed(from.r, to.r, share), mixed(from.g, to.g, share), mixed(from.b, to.b, share) };
}

static void fillStrip(Frame &frame, uint8_t strip, const Rgb &color) {
  for (uint8_t pixel = 0; pixel < PIXELS; pixel++) frame.pixels[strip * PIXELS + pixel] = color;
}

void darkenOutside(Frame &frame, const bool *marked, float depth) {
  const uint8_t kept = (uint8_t)lroundf((1.0f - depth) * 255.0f);
  for (uint8_t strip = 0; strip < STRIPS; strip++) {
    if (marked[strip]) continue;
    for (uint8_t pixel = 0; pixel < PIXELS; pixel++) {
      Rgb &rgb = frame.pixels[strip * PIXELS + pixel];
      rgb = { scale8(rgb.r, kept), scale8(rgb.g, kept), scale8(rgb.b, kept) };
    }
  }
}

void blendToward(Frame &frame, const Rgb *pixels, const Par *pars, const bool *marked, float share) {
  for (uint8_t strip = 0; strip < STRIPS; strip++) {
    if (!marked[strip]) continue;
    for (uint8_t pixel = 0; pixel < PIXELS; pixel++) {
      const uint16_t at = strip * PIXELS + pixel;
      frame.pixels[at] = mixed(frame.pixels[at], pixels[at], share);
    }
  }
  for (uint8_t par = 0; par < PARS; par++) {
    frame.pars[par].color = mixed(frame.pars[par].color, pars[par].color, share);
    frame.pars[par].value = mixed(frame.pars[par].value, pars[par].value, share);
  }
}

void blackOut(Frame &frame, const bool *marked) {
  for (uint8_t strip = 0; strip < STRIPS; strip++) {
    if (marked[strip]) fillStrip(frame, strip, { 0, 0, 0 });
  }
  for (Par &par : frame.pars) par.value = 0;
}

void flashWhite(Frame &frame, const bool *marked) {
  for (uint8_t strip = 0; strip < STRIPS; strip++) {
    if (marked[strip]) fillStrip(frame, strip, WHITE);
  }
  for (Par &par : frame.pars) par = { WHITE, 255 };
}

}
