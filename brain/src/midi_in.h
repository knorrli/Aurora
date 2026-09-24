// midi_in — the brain's only input. Everything the old hardware read from
// pins now arrives here as MIDI.
//
// USB only for now; the DIN circuit on Serial1 lands in Phase 5 and will
// call the same handlers. Listens on every channel so a bench source set
// to the wrong one still works.

#ifndef AURORA_BRAIN_MIDI_IN_H
#define AURORA_BRAIN_MIDI_IN_H

#include <stdint.h>

namespace midi_in {

void begin();
void tick();

} // namespace midi_in

#endif // AURORA_BRAIN_MIDI_IN_H
