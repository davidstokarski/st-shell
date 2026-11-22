# Architecture

## The module

`modules/st_shell` is a JUCE module (namespace `st`). Layers are included in order and a layer may only use the ones
before it:

| Layer | Contents |
|---|---|
| `core/` | `st::params` (ranges, formatters, builders, `Ref`), `MeterSource`, `AnalyzerSource`, `SpectrumAnalyzer` |
| `dsp/` | `st::dsp`: `Biquad`, `Svf`, `TiltFilter`, `OnePole`, `DcBlocker`, `Crossover` (LR 12/24/48, ≤8 bands), `eq::` analog-matched EQ design + `EqCascade`, `PartitionedConvolver`, `UpDownCompressor`, `Oversampler` (1–16x), `DelayLine`, `EnvelopeFollower`, `Reverb` (8 types), `LinearSmoother`, utils |
| `mod/` | `st::mod::ModMatrix` (LFOs, envelope followers, macros, routings in `apvts.state`) |
| `state/` | `ProcessorBase`, `PresetManager`, `ABCompare` |
| `theme/` | `Theme` tokens, `fonts`, `icons`, `paint::` helpers, `LookAndFeel` |
| `ui/` | `EditorBase`, `HeaderBar`, `Knob`, controls, meters, `Popover`, modulation UI, `Animated`/`FrameClock`, `showMenu` |

Each layer has an aggregator (`st_core.h/.cpp`, ...). Only the module's top-level `st_shell*.cpp/.mm` files are
compiled, so **register new files in the aggregator** or they won't build. On Apple, `st_shell_menus.mm` replaces
`st_shell_menus.cpp`.

## Processor pattern (`st::ProcessorBase`)

- Gives you `apvts` (with `undoManager`), `presets`, `ab`, `inputMeter`/`outputMeter`, `preAnalyzer`/`postAnalyzer`,
  `uiState`, state save/restore, bus layout checks and `param (id)`.
- Constructor: build the layout (`st::params::make*`), bind fast refs (`st::params::Ref` or a subclass with a mod
  target index), register mod targets, register factory presets.
- `prepareToPlay`: call `prepareShell (sampleRate)`, prepare your engine, report latency.
- `processBlock`: `ScopedNoDenormals`; push input to meter/analyser; process **in chunks of ≤64 samples** so
  modulation and parameter reads are block-rate-but-smooth; push output to meter/analyser.
- Keep DSP in a plain class (ST Sat: `stsat::Engine`) that doesn't know about `AudioProcessor`, so it can be tested
  offline.

## Threading rules

| Thread | May |
|---|---|
| Audio | read atomics (`apvts` raw values, `Ref::get`), process, write meter/analyser FIFOs, try-lock (never block) |
| Message | allocate, touch ValueTrees, presets, UI, `prepare()` DSP, change oversampling (inside `suspendProcessing`) |

- Work too heavy for the audio thread (FIR design, curve analysis) runs on a worker thread that reads the
  parameters' atomics itself and publishes results lock-free (ST EQ: `FirWorker` → `PartitionedConvolver`).
- Never allocate on the audio thread. If a parameter change needs reallocation (ST Sat: oversampling quality),
  listen to it (`apvts.addParameterListener`), defer with `AsyncUpdater`, then
  `suspendProcessing (true); engine.setQuality (q); suspendProcessing (false); setLatencySamples (...)`.
- DSP setters used per block must be cheap and allocation-free (coefficient math only).
- UI → audio: through parameters (atomic) or lock-free structures. Audio → UI: atomics (`std::atomic<float>` meters),
  `MeterSource`, `AnalyzerSource`.

## State

- Everything that belongs in a preset is in `apvts.state` (parameters + child trees such as `MODULATION`).
- Editor-only, session-scoped things (scale, selected band, open panels, analyser mode) go in `uiState`.
- Presets: `.stpreset` XML in `~/Library/Audio/Presets/ST Audio/<Product>/`. Factory presets are registered in code
  with `presets.addFactoryPreset (name, category, { { "paramId", plainValue }, ... })`; unspecified params use defaults.
- Loading a preset/A-B/session calls `apvts.replaceState`; listen to state changes rather than caching values.

## Editor pattern (`st::EditorBase`)

- Design at a fixed size (ST Sat: 1040×690); the base scales the whole content with an affine transform, keeps the
  aspect ratio and persists the scale.
- Add children to `getContent()`; lay out in `layout (body)`; **call `relayout()` at the end of your constructor**
  (layout is deferred until the subclass exists).
- `header` is provided; call `header.toFront (false)` after adding a full-window display so it stays on top.
- Animate with one `st::FrameClock` (usually owned by the main display) and pass `onFrame` to the editor for panel motion.
- Cache static components: `setBufferedToImage (true)` on header, footer, control bars.

## Modulation

- `ModMatrix::addParameters (layout, config)` adds the source parameters; construct `ModMatrix (apvts, config)` as a
  processor member; `addTarget (paramId)` for every modulatable parameter.
- Audio thread per chunk: `setTransport`, `feedEnvelopeInput`, `advance (n)`, then read
  `getModulatedNormalised (target, base)` only for targets where `isTargetModulated`.
- Editor: `ModulationStrip` + `ModulationBinder` (wires every `st::Knob` registered as a target: rings, drop, depth
  drag, context menu) + `ModulationTrails` (glow). See ST Sat `PluginEditor.cpp`.

## CMake

`st_add_plugin (Target PRODUCT_NAME "ST X" PLUGIN_CODE Abcd VERSION ... SOURCES ...)` creates VST3/AU/Standalone,
links the module + fonts, sets company "ST Audio", manufacturer code `StAu`, bundle id `com.staudio.<target>`.
Instruments pass `IS_SYNTH` (AU type `aumu` / VST3 `Instrument`, MIDI in); `VST3_CATEGORIES` and `AU_TYPE` override
the defaults (ST Sample: `IS_SYNTH VST3_CATEGORIES Instrument Drum Sampler`). An instrument has no input bus, so it
overrides `isBusesLayoutSupported` and `acceptsMidi` (see ST Sample's processor).
Plugin codes must be unique per plugin. JUCE comes from `../.st-deps/JUCE-9.0.5` if present, else `ST_JUCE_DIR`, else
FetchContent. Plugins find the shell at `../st-shell` or fetch it from GitHub.
