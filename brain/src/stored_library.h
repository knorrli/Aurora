#pragma once

#include <playback.h>

class StoredLibrary : public playback::Library {
 public:
  bool patch(uint8_t slot, playback::Patch &out) override;
  bool oneshot(uint8_t index, playback::Oneshot &out) override;
  uint8_t defaultOneshot(uint8_t place) override;
};
