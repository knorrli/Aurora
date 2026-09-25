# Aurora

Stage lighting for a band: five vertical LED strips (45 pixels each) standing against the back wall, and four PAR cans on the floor washing it. A Teensy 4.0, the **brain**, renders every frame from a parametric generator and is played live over MIDI, from a DAW, a hardware controller and a browser editor.

This is v2. The Arduino Nano rig it replaces ran a year on stage and is kept at the tag `aurora-nano-final`.

## The parts

- **The brain** (`brain/`): a Teensy 4.0. It drives the strips through a 74AHCT125 on pin 2 and the PARs over DMX (an M5Stack DMX unit on `Serial4`), and takes USB MIDI.
- **The renderer** (`shared/render/`): the generator, its modulation routes, the washes' color and the palettes. The brain and the editor both run this one piece of code, compiled natively for the brain and to WebAssembly for the browser.
- **The protocol** (`shared/aurora_protocol.h`): every CC, Program Change and note number, with what each means. Both firmwares and the editor read it.
- **The controller** (`controller/`): still the v1 box with its Nano firmware, which is due to be rebuilt on a second Teensy.
- **The editor** (`tools/editor.html`): builds and saves patches, drives the wall live over Web MIDI, and previews it through the same renderer.

## How it is played

A **patch** is a set of CC values: the shape, its motion, the color layer, the palette and the washes. Four far ends per patch are what the controller's faders and an accent morph toward.

| Message | Meaning |
|---|---|
| Program Change 10 | the generator — what every patch runs |
| Program Change 0 | blackout |
| Program Change 11 | each strip one flat color, for rigging |
| CCs | the patch's controls, the controller's surfaces, and eight modulation routes |
| MIDI clock | tempo; everything moves in beats |

Aurora listens on MIDI channel 1 only.

## Working on it

```bash
cd brain      && pio run -e teensy40 -t upload   # flash the brain
node tools/build-render.mjs                      # after changing shared/render/
node tools/gen-cc.mjs                            # after changing shared/aurora_protocol.h
cd tools      && python3 -m http.server          # then open http://localhost:8000/editor.html in Chrome
```

The editor needs Chrome or Edge for Web MIDI. Serve it rather than opening the file, so the MIDI permission sticks.

## Where things are written down

| For | Read |
|---|---|
| what is open and what is next | `TODO.md` |
| how it is played | `DESIGN.md` |
| the generator, the color layer and the palettes | `docs/generator.md` |
| modulation routes and the LFO | `docs/modulation.md` |
| how the system is built | `docs/architecture.md` |
| the editor | `docs/editor.md` |
| pins and circuits | `docs/wiring.md` |
| every control on the box | `docs/controls.md` |
| what has been measured | `docs/bench-facts.md` |
