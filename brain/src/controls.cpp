#include "controls.h"

#include "aurora_protocol.h"

namespace controls {

static uint8_t values[AURORA_PATCH_CC_COUNT];

void begin() {
    for (const AuroraControlDefault &control : AURORA_CONTROL_DEFAULTS) {
        values[control.cc] = control.value;
    }
    for (uint8_t route = 0; route < AURORA_ROUTES; route++) {
        for (uint8_t field = 0; field < ROUTE_FIELDS; field++) {
            values[aurora_route_cc(route, field)] = AURORA_ROUTE_DEFAULTS[field];
        }
    }
}

void store(uint8_t cc, uint8_t value) {
    if (cc < AURORA_PATCH_CC_COUNT) values[cc] = value;
}

const uint8_t *all() { return values; }

}
