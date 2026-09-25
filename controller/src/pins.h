// pins.h — Nano ATmega328 pin map for the Aurora controller node.
//
// See docs/wiring.md for the full story. This file is the single source of
// truth; if it disagrees with the docs, fix the docs, and vice versa.

#ifndef AURORA_CONTROLLER_PINS_H
#define AURORA_CONTROLLER_PINS_H

// --- MIDI (hardware UART) --------------------------------------------------
// D0 = MIDI IN (RX), D1 = MIDI OUT (TX). Used implicitly by `Serial`.
// Listed here for reference only.

// --- Tempo / trigger (repurposed from original Aurora) --------------------
static const uint8_t PIN_TAP_TEMPO        = 2;  // was WS2812 data out
static const uint8_t PIN_MIC_TRIGGER      = 3;  // was external tempo pulse

// --- Touchpad mode switches (unchanged) -----------------------------------
static const uint8_t PIN_TOUCHPAD_EFFECT  = 4;
static const uint8_t PIN_HOLD_MODE        = 5;

// --- Touchpad (4-wire resistive) ------------------------------------------
static const uint8_t PIN_TOUCHPAD_XP      = 6;
static const uint8_t PIN_TOUCHPAD_YM      = 7;
static const uint8_t PIN_TOUCHPAD_YP      = A4;
static const uint8_t PIN_TOUCHPAD_XM      = A5;

// --- Keypad (PORTB bits 0..4) ---------------------------------------------
// Pins 8–12 are wired as keypad rows/columns; read collectively as `PINB`.

// --- Status LED -----------------------------------------------------------
static const uint8_t PIN_TEMPO_LED        = 13;

// --- Faders ---------------------------------------------------------------
static const uint8_t PIN_FADER_SATURATION = A0;
static const uint8_t PIN_FADER_HUE        = A1;
static const uint8_t PIN_FADER_VALUE      = A2;

// --- Reserved -------------------------------------------------------------
// A3 is currently free — reserved for future expansion.
static const uint8_t PIN_RESERVED_A3      = A3;

// --- Mode switches (analog rotary) ----------------------------------------
static const uint8_t PIN_TOUCHPAD_STRIP_MODE = A6;

#endif // AURORA_CONTROLLER_PINS_H
