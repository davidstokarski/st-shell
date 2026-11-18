# Starting a new ST plugin

1. **Create the repo** next to the others: `~/Documents/Code/st-<name>` (private GitHub repo `davidstokarski/st-<name>`).
   Copy `examples/MinimalPlugin` into it and replace its `CMakeLists.txt` shell lookup with the sibling/FetchContent
   block from `st-sat/CMakeLists.txt`. Copy `st-sat/.gitignore`.
2. **Pick identity**: `PRODUCT_NAME "ST <Name>"`, a unique 4-char `PLUGIN_CODE` (≥1 uppercase; ST Sat = `Stst`,
   example = `Stex`), `BUNDLE_SUFFIX`. Optionally set `Theme::get().accent/accent2/displayTop/displayGlow` at startup.
3. **Parameters** in `Source/Params.{h,cpp}`: IDs as constants/helpers, `createParameterLayout()` using
   `st::params::make*` and formatters, groups per section. Never rename IDs after release (sessions depend on them).
4. **Engine**: plain DSP class, `prepare / reset / setSettings / process`, no `AudioProcessor` dependency. Use
   `st::dsp` blocks. Allocation only in prepare/re-prepare.
5. **Processor**: derive `st::ProcessorBase`, bind refs, register modulation targets if you use the matrix,
   factory presets, chunked `processBlock` with meters/analysers. Message-thread handling for anything that reallocates.
6. **Editor**: derive `st::EditorBase`. Full-window display first, then floating control bar, popovers for depth,
   `ModulationStrip` + `ModulationBinder` + `ModulationTrails` if modulated, `FooterBar`-style tray. Call
   `header.toFront (false)` and `relayout()` at the end. Follow DESIGN.md.
7. **Tests**: add `tests/` like ST Sat — an offline DSP test app and a whole-plugin test app with stress tests
   (all settings, odd block sizes, state round trip, CPU) and `--snapshot <dir>` rendering every important UI state.
8. **Verify** (QUALITY.md): tests pass, `auval -v aufx <code> StAu` passes, snapshots inspected.
9. **README** with screenshot (`docs/screenshot.png`) and the download link (RELEASING.md), and a short `CLAUDE.md`
   pointing back to these docs.
10. If you built something reusable, move it into the shell and document it in COMPONENTS.md.
