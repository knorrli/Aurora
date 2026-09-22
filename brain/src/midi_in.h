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

// The last byte received on each CC, indexed by CC number. Every handler
// cooks its value and discards the byte — setGeneratorCount in
// P_Generator.cpp stores round(20^(value/127)), which cannot be inverted —
// so this is the only place the brain knows what it was set to. A patch is
// these bytes; see DESIGN.md § "Patch storage".
//
// A CC nothing has sent reads 0, which is not what the wall is showing: a
// bipolar control renders centered while this still says 0. Until a default
// set is applied at boot, a snapshot taken before the brain has been driven
// records that 0 rather than what is lit.
const uint8_t *ccBytes();

} // namespace midi_in

#endif // AURORA_BRAIN_MIDI_IN_H
