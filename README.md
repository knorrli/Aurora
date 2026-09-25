# Aurora

Stage lighting for a band: five vertical LED strips of 45 pixels against the back wall, and four PARs on the floor washing it. A Teensy 4.0, the **brain**, renders every frame from a parametric generator and is played live over MIDI.

The Arduino Nano rig it replaces is at the tag `aurora-nano-final`.

## The parts

- `brain/`: the Teensy firmware. Strips on pin 2, PARs over DMX on `Serial4`, USB MIDI in.
- `shared/render/`: the renderer. The brain runs it natively; the editor runs it compiled to WebAssembly.
- `shared/aurora_protocol.h`: every MIDI number Aurora uses.
- `tools/editor.html`: builds patches, drives the rig over Web MIDI, and previews it with the same renderer.
- `bench/dmx_bringup/`: checks the DMX link to one PAR.

## MIDI

Channel 1 only. Program Change 10 runs the generator, Program Change 0 is blackout, CCs carry every control, MIDI clock carries tempo.

## Commands

```bash
cd brain && pio run -t upload        # flash the brain
node tools/build-render.mjs          # after changing shared/render/
node tools/gen-cc.mjs                # after changing shared/aurora_protocol.h
cd tools && python3 -m http.server   # then open http://localhost:8000/editor.html in Chrome
```

## Docs

- `TODO.md`: open work
- `docs/design.md`: decided behavior that is not built yet
- `docs/hardware.md`: pins, circuits, fixtures, measurements
