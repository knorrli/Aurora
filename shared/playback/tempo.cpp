#include "tempo.h"

#include "aurora_protocol.h"

namespace playback {

static const uint16_t SLOWEST_BPM = 20;
static const uint16_t FASTEST_BPM = 300;
static const uint16_t STARTUP_BPM = 120;
static const uint32_t MICROS_PER_MINUTE = 60000000UL;
static const uint32_t CLOCK_SILENCE_MICROS = 500000;
static const uint32_t MOST_TICKS_TO_CATCH_UP = 32;

Tempo::Tempo(uint32_t micros)
    : lastTickMicros(micros),
      lastClockMicros(micros),
      microsPerTick(MICROS_PER_MINUTE / STARTUP_BPM / AURORA_TICKS_PER_BEAT) {}

static uint32_t since(uint32_t micros, uint32_t earlier) {
  const uint32_t elapsed = micros - earlier;
  return (int32_t)elapsed < 0 ? 0 : elapsed;
}

void Tempo::freeRunWithoutClock(uint32_t micros) {
  if (since(micros, lastClockMicros) < CLOCK_SILENCE_MICROS) return;
  if (since(micros, lastTickMicros) > microsPerTick * MOST_TICKS_TO_CATCH_UP) lastTickMicros = micros - microsPerTick;
  while (since(micros, lastTickMicros) >= microsPerTick) {
    ticks++;
    lastTickMicros += microsPerTick;
  }
}

float Tempo::beatsAt(uint32_t micros) {
  if (!transportRunning) return frozenBeats;
  freeRunWithoutClock(micros);
  float withinTick = (float)since(micros, lastTickMicros) / (float)microsPerTick;
  if (withinTick > 1.0f) withinTick = 1.0f;
  frozenBeats = ((float)ticks + withinTick) / (float)AURORA_TICKS_PER_BEAT;
  return frozenBeats;
}

float Tempo::microsPerBeat() const { return (float)microsPerTick * (float)AURORA_TICKS_PER_BEAT; }

void Tempo::measure(uint32_t micros) {
  if (++ticksSinceBeat < AURORA_TICKS_PER_BEAT) return;
  ticksSinceBeat = 0;
  if (beatMeasured) {
    const uint32_t beatMicros = micros - lastBeatMicros;
    if (beatMicros >= MICROS_PER_MINUTE / FASTEST_BPM && beatMicros <= MICROS_PER_MINUTE / SLOWEST_BPM) {
      microsPerTick = beatMicros / AURORA_TICKS_PER_BEAT;
    }
  }
  lastBeatMicros = micros;
  beatMeasured = true;
}

void Tempo::tick(uint32_t micros) {
  lastClockMicros = micros;
  measure(micros);
  if (!transportRunning) return;
  ticks++;
  lastTickMicros = micros;
}

void Tempo::start(uint32_t micros) {
  ticks = 0;
  ticksSinceBeat = 0;
  beatMeasured = false;
  frozenBeats = 0.0f;
  lastTickMicros = micros;
  lastClockMicros = micros;
  transportRunning = true;
}

void Tempo::resume(uint32_t micros) {
  lastTickMicros = micros;
  lastClockMicros = micros;
  transportRunning = true;
}

void Tempo::stop() { transportRunning = false; }

}
