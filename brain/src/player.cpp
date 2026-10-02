#include "player.h"

#include <Arduino.h>

#include "stored_library.h"

namespace player {

static StoredLibrary library;
static playback::Playback *playing = nullptr;

void begin() { playing = new playback::Playback(library, micros()); }

playback::Playback &get() { return *playing; }

void libraryChanged() { playing->patchChanged(playing->slot()); }

}
