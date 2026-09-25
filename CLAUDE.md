# Working on Aurora

## Code

- No comments, anywhere. The code states what it does; a comment is out of date as soon as it is written. The one exception is the tag list after each CC in `shared/aurora_protocol.h` (`// [patch][rate]`): that is data `tools/gen-cc.mjs` reads, not a comment.
- Names are descriptive words. No abbreviations unless universally obvious (MIDI, DMX, LED, RGB, CC).
- A name in code is the name on screen. When the editor's label changes, the code follows.
- American English.

## Docs

- Only facts the code cannot hold: hardware, measurements, and decided behavior that is not built yet.
- Never why, never history, never rejected alternatives. Delete what is done or no longer true.
- `docs/hardware.md`: pins, circuits, fixtures, measured facts. `docs/design.md`: decided behavior not yet built. `TODO.md`: open work.

## Generated files

- `tools/cc.js`: `node tools/gen-cc.mjs` after changing `shared/aurora_protocol.h`.
- `tools/render.js`: `node tools/build-render.mjs` after changing `shared/render/` (needs Emscripten).
- Both have `--check`.

## Build gotcha

`#include` must match the file's case exactly. macOS compiles a mismatch, but PlatformIO's dependency scan silently skips it, so header edits stop triggering rebuilds.
