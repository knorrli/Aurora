#pragma once

#include <playback.h>

namespace player {

void begin();
playback::Playback &get();
void libraryChanged();

}
