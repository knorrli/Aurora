#pragma once

#include <stdint.h>

namespace playback {

class Tempo {
 public:
  explicit Tempo(uint32_t micros);

  float beatsAt(uint32_t micros);
  float microsPerBeat() const;
  bool running() const { return transportRunning; }

  void tick(uint32_t micros);
  void start(uint32_t micros);
  void resume(uint32_t micros);
  void stop();

 private:
  void freeRunWithoutClock(uint32_t micros);
  void measure(uint32_t micros);

  uint32_t ticks = 0;
  uint32_t lastTickMicros;
  uint32_t lastClockMicros;
  uint32_t microsPerTick;
  uint16_t ticksSinceBeat = 0;
  uint32_t lastBeatMicros = 0;
  bool beatMeasured = false;
  float frozenBeats = 0.0f;
  bool transportRunning = true;
};

}
