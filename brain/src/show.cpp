#include "show.h"

#include "aurora_protocol.h"
#include "controls.h"

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
    render::renderFrame(controls::all(), quarterNotes, motion, wall, frame);
    return frame;
}

}
