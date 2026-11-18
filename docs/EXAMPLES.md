# Worked examples in ST Sat

ST Sat (`../st-sat`, https://github.com/davidstokarski/st-sat) is the reference plugin. Look here for how each
pattern is done in practice.

| Pattern | Where |
|---|---|
| CMake: sibling shell / FetchContent, `st_add_plugin`, test targets | `CMakeLists.txt`, `tests/CMakeLists.txt` |
| Parameter IDs, per-band parameter groups, formatters, choice lists | `Source/Params.h`, `Source/Params.cpp` |
| Factory presets via a small fluent helper | `Source/FactoryPresets.cpp` |
| Processor on `ProcessorBase`, chunked processing, modulation reads, mod targets | `Source/PluginProcessor.cpp` (`updateEngineSettings`, `processBlock`) |
| Oversampling change on the message thread (`suspendProcessing`) | `Source/PluginProcessor.cpp` (`applyQualityChange`) |
| Undoable multi-parameter edits (add/remove band) and remapping mod routings | `Source/PluginProcessor.cpp` (`addBand`, `removeBand`, `remapBandRoutings`) |
| Offline-testable DSP engine: oversampling → crossover → per-band chain → sum | `Source/dsp/Engine.cpp` |
| Per-band reverb with compression and cross-band ducking | `Source/dsp/Engine.cpp` (`BandProcessor::processSpace`, `Engine::process`) |
| Large style library with calibration tables + test/calibration tool | `Source/dsp/Saturation*.h/.cpp`, `tests/SaturationTests.cpp` |
| Full-window interactive display: spectrum, band columns, dashed crossovers, pill handles, mod dots, dirty-only repaint | `Source/ui/MultibandDisplay.cpp` |
| Floating control bar with buttons on its edge, ringed knobs with rim parameters, tone faders, `•••` openers | `Source/ui/Panels.cpp` (`FloatingBandPanel`) |
| Popovers built on `st::Popover` (dynamics transfer curve, reverb settings + envelope preview + duck meter) | `Source/ui/Panels.cpp` (`DynamicsPopover`, `SpacePopover`) |
| Footer tray with flat dropdowns / toggles / number boxes | `Source/ui/Panels.cpp` (`FooterBar`) |
| Editor: layout, gliding panel, cross-fades, popover placement, modulation strip/binder/trails, buffered caching | `Source/PluginEditor.cpp` |
| Whole-plugin stress tests + PNG snapshot tool (incl. opening popovers) | `tests/PluginTests.cpp` |
| Release packaging | `scripts/release.sh` |
