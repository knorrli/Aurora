// tempo — musical position for the brain, driven by MIDI clock.
//
// Animations ask where the music *is* rather than whether a beat just
// happened. Position is derived from elapsed time every frame instead of
// being accumulated a step at a time, so a late frame never leaves an
// animation permanently behind the music, and a tempo change is a change
// of rate rather than a jump.
//
// One "animation beat" is however many clock ticks CC_TEMPO_DIVISION
// selects. The controller's subdivision rotary therefore scales every
// animation without any preset knowing it exists. Beat boundaries stay
// aligned to the musical grid, so changing division re-aligns rather than
// drifting — at the cost of a one-off jump when the rotary moves.

#ifndef AURORA_BRAIN_TEMPO_H
#define AURORA_BRAIN_TEMPO_H

#include <stdint.h>

namespace tempo {

void begin();

// Call once per frame, before anything renders, so every preset in a
// frame sees the same position.
void tick();

// Monotonic musical position in quarter notes, fractional, whatever the
// tempo division.
float quarterNotes();

// True for the one frame on which a new animation beat began.
bool pulsed();

float bpm();
bool running();

// --- Notifications from midi_in ---------------------------------------------

void onClock();
void onStart();
void onContinue();
void onStop();
void setDivision(uint8_t division);

} // namespace tempo

#endif // AURORA_BRAIN_TEMPO_H
