#include "show.h"

#include <Arduino.h>

#include "aurora_protocol.h"
#include "controls.h"

#include <morph.h>

namespace show {

static bool blackedOut = true;
static bool showWaitingForBeat = false;

static render::Motion motion;
static render::Wall wall;
static render::Frame frame;

void select(uint8_t program) {
    if (program == PROGRAM_BLACKOUT) {
        blackedOut = true;
        showWaitingForBeat = false;
    } else if (program == PROGRAM_SHOW) {
        render::clearTails(wall);
        showWaitingForBeat = true;
    }
}

void advance(bool beatStarted, bool transportRunning) {
    if (showWaitingForBeat && (beatStarted || !transportRunning)) {
        blackedOut = false;
        showWaitingForBeat = false;
    }
}

bool blackout() { return blackedOut; }

const render::Frame &render(float quarterNotes) {
    static uint8_t composed[render::RENDER_CONTROL_COUNT];
    render::composeOneshot(controls::all(), nullptr, nullptr, composed);
    render::renderFrame(composed, quarterNotes, millis(), { 0.0f, 1.0f, false }, motion, wall, frame);
    return frame;
}

}
