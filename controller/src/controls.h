// controls — scan the controller's physical inputs and emit MIDI.
//
// Combines keypad / faders / touchpad / switches / tap button / mic
// trigger into one module. Each input has a small state (debounce, last
// sent value) and only emits on change — keeps MIDI bus traffic low.
//
// Call `controls::begin()` from setup() and `controls::tick()` every
// loop. Emission goes out via midi_io.

#ifndef AURORA_CONTROLLER_CONTROLS_H
#define AURORA_CONTROLLER_CONTROLS_H

namespace controls {

void begin();
void tick();

} // namespace controls

#endif // AURORA_CONTROLLER_CONTROLS_H
