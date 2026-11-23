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

## ST DC (`../st-dc`)

| Pattern | Where |
|---|---|
| Several compressor engines in one class; feedback designs solved to match the static curve | `Source/dsp/Compressor.cpp` |
| Lock-free, non-consuming level / GR history for scrolling displays | `Source/dsp/Engine.h` (`LevelHistory`), `Source/ui/Display.cpp` (`HistoryReader`) |
| Pro-style level display with a threshold handle, spectrum with a GR band, knee display | `Source/ui/Display.cpp` |
| Plugin-specific multiview (only tracks with this plug-in, custom order and title) | `Source/PluginEditor.cpp` (`toggleMultiView`) |
| Custom backdrop (procedural wood) replacing `EditorBase::paintBackground`, cached per pixel scale | `Source/PluginEditor.cpp`, `Source/ui/Style.cpp` |

## ST EQ (`../st-eq`)

| Pattern | Where |
|---|---|
| Fixed band slots with a `used` flag, undoable add / remove / copy / paste | `Source/PluginProcessor.cpp` |
| Three phase modes on one engine; latency-aligned detectors and solo | `Source/dsp/Engine.cpp` |
| Worker thread designing FIRs from parameter atomics, lock-free IR swap | `Source/dsp/FirWorker.cpp` |
| Interactive EQ display: layered image caching, multi-selection, rubber band, spectrum grab, sketch | `Source/ui/EqDisplay.cpp` |
| Band island (raised tab, recessed arc knob with dynamic-range rim), dynamics / match popovers | `Source/ui/Panels.cpp` |
| Neutral theme override for a plugin | `Source/PluginProcessor.cpp` (constructor) |
| Response-vs-analog tests per processing mode, UI paint benchmark | `tests/PluginTests.cpp` |

## ST Sample (`../st-sample`)

| Pattern | Where |
|---|---|
| Instrument plugin (`IS_SYNTH`, no input bus, sample-accurate MIDI) | `CMakeLists.txt`, `Source/PluginProcessor.cpp` |
| Non-parameter content in `apvts.state` (sample paths) reconciled into decoded data on a worker thread, graveyard so the audio thread never frees | `Source/Kit.cpp` |
| Voice engine: pool, Hermite interpolation, choke groups, click-free stealing, lock-free UI auditions | `Source/dsp/Engine.cpp`, `Source/PluginProcessor.cpp` |
| Factory presets that carry a child tree (kits) | `Source/FactoryKits.cpp` |
| File drag and drop onto many targets, native file chooser, folder stepping | `Source/PluginEditor.cpp`, `Source/ui/*` |

## ST Comp (`../st-comp`)

| Pattern | Where |
|---|---|
| Many compressor designs as data: a style table of one or two stages (detector, feed-forward / feedback, curve, release type) run by one engine | `Source/Styles.cpp`, `Source/dsp/Compressor.cpp` |
| Serial gain stages (opto leveller into a FET) with an exact static curve for the knee display | `Source/dsp/Compressor.cpp` (`staticGainDb`, `StageCurve::steadyState`) |
| Upward compression with a silence floor; signed gain change through `st::LevelHistory` | `Source/dsp/Compressor.cpp`, `Source/ui/Display.cpp` |
| Lookahead without allocation (delays sized for the maximum), host latency updated asynchronously | `Source/dsp/Engine.cpp`, `Source/PluginProcessor.cpp` (`applyPendingChanges`) |
| Side-chain filter drawn and dragged on the detector's own spectrum, in a popover; side-chain listen | `Source/ui/Display.cpp` (`SidechainDisplay`), `Source/ui/Panels.cpp` (`SidechainPopover`) |
| Mid/side and single-channel processing modes | `Source/dsp/Engine.cpp` |
| Draggable knee display (sideways = threshold, up / down = ratio) | `Source/ui/Display.cpp` (`KneeView`) |
