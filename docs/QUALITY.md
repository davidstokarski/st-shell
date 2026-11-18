# Quality bar & verification

"Done" means verified, not "it compiles". Before reporting a change as finished:

## Audio
- Plugin test app passes (ST Sat: `build/tests/StSatPluginTests_artefacts/Release/StSatPluginTests`):
  every mode/style/band count/oversampling factor, odd block sizes (1, 7, 63, 64, 65, 333, 2048), state round trip,
  bounded output, no NaN/Inf, CPU print.
- DSP unit tests for anything new (shell: `st_dsp_tests`, `StModTests`, `StReverbTests`).
- `auval -v aufx <PluginCode> StAu` → `AU VALIDATION SUCCEEDED`.
- No allocation/locks on the audio thread (review the diff with that question in mind).
- Latency reported to the host equals the engine's latency for every mode.

## Visual
- Render snapshots: `StSatPluginTests --snapshot <dir>` writes PNGs at 2x for several presets, scales and popovers.
  Add a job for every state you touched (see `tests/PluginTests.cpp`: `Snapshotter::Job` with `setup` / `afterOpen`).
- Open the PNGs and check against DESIGN.md §8: overlaps, clipped shadows / dark corners, hard line ends, truncated
  text, misaligned captions, stale state (e.g. a control visible that shouldn't be).
- Check 75% scale too.

## UI performance
- Display repaints only when something changed; animation via `FrameClock` (≤60 fps).
- Static parts cached (`setBufferedToImage`), backdrop cached.
- Heavy work (blur images, path building for static things) cached and invalidated on change.

## Interaction
- Every control: drag, shift-drag, double-click reset, cmd-click entry, wheel, right-click menu (native), undo/redo.
- Popovers: ×, Esc, reopen shows the correct state.
