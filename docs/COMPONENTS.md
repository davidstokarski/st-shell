# Component catalogue

Everything here is in namespace `st` unless noted. Paths are relative to `modules/st_shell/`.

## Editor scaffolding — `ui/EditorBase.h`, `ui/Animation.h`, `ui/Popover.h`, `ui/NativeMenu.h`

| Component | What / how |
|---|---|
| `EditorBase (proc, w, h)` | Scaled, resizable editor with cached backdrop, `header`, tooltips, Cmd-Z/Cmd-Shift-Z. Override `layout (body)`; call `relayout()` at the end of your constructor. |
| `HeaderBar` | Logo tab + text buttons (undo/redo, A/B, Copy, presets, Save, settings). `setProductName`, `extendSettingsMenu`. |
| `TextPrompt::show (parent, title, text, ok, callback)` | In-window text prompt (preset save); avoids native dialogs in hosts. |
| `Popover` | Base for floating panels: shadow margin, title + subtitle, × and Esc, click-through margin. Override `layoutContent (contentBounds)` / `paintContent (g)`; use `getHeaderBounds()` for header controls; size with `Popover::totalSize (w, h)`. |
| `Animated` | `setTarget`, `tick (dt, speed)` → true while moving. Use for every visual transition. |
| `FrameClock (component, onFrame)` | vblank-driven callback, max 60 fps, timer fallback offscreen. One per editor is usually enough. |
| `showMenu (menu, target, onResult, atMouse)` | Native dark NSMenu (JUCE fallback elsewhere). Item `action`s fire; result = item ID. |

## Parameter controls — `ui/Knob.h`, `ui/Controls.h` (all derive from `ParameterControl`: gestures, undo, host menu)

| Control | Notes |
|---|---|
| `Knob (param, undo, label)` | `setSize (large/medium/small/mini)`, `setLook (classic/ringed/arc)`, `setColour`, `setBipolar`, `setRimParameter (param, label)` (rim becomes a second control), `valueToText`. Modulation via `ModulationBinder`. |
| `Knob` arc look | `Look::arc`: dark disc with a value arc in the knob colour on the disc face (Pro-style band controls, ST EQ; colour = band colour). `setRecessed (true)` puts it in a dark well; the gap is the rim zone, where `RimStyle::range` draws a thick red span and live activity in yellow. Use `Size::mini` and draw captions yourself when they must overlap the well (ST EQ `BandIsland`). |
| `Knob` gestures | Default: double-click resets, Cmd-click types a value, Alt-click resets. `Knob::setDoubleClickEntersValue (true)` (once, e.g. in the processor constructor) switches the whole plugin to Pro-style: double-click types, Cmd-click resets (ST EQ). Typed values go through the parameter's parser: `params::parseHz` accepts `1.5k` and note names (`A4`, `C#3+13`), `params::parseDb` (used by `dbAttributes`) accepts `-inf` and amplitude ratios (`2x` = +6 dB, `50%` = -6 dB). |
| `Knob` rim styles | `setRimStyle (RimStyle::spread)` (default: L/R markers, stereo spread / pan) or `RimStyle::range`: the rim value is an offset in the main parameter's units (dynamic EQ range), drawn as a span from the value; `setRimActivitySource (fn)` adds live activity (e.g. current dynamic gain) in the warning colour. |
| `Fader` | Vertical slot + pill cap; centre detent for bipolar params. |
| `ToggleChip` | Bool param. Styles: default chip, `setGreyStyle` (bar buttons), `setFlatStyle` (footer text + indicator light), icon or text, `setInverted`. |
| `Segmented` | Choice param as pills (`PRE/POST`, reverb types...). |
| `Dropdown` | Choice param. `setGreyStyle`, `setFlatStyle` + `setCaption` (footer), `setShowArrows`, `buildMenu` for categorised menus (uses `showMenu`). |
| `NumberBox` | Drag-to-edit value; `setFlatStyle` for footer "In 0.0 dB". |
| `FlatButton` | Text-only button; `setDimmedText (prefix, suffix)`, `setDropdownArrow`. Use `•••` text for popover openers. |
| `IconButton` | Icon button; `setMinimal` (header style), `setDrawBackground`. |
| `PillButton` | Small text pill toggle. |

## Meters & analysis — `ui/Meters.h`, `core/Metering.h`

`MeterSource` (audio → UI peak/RMS), `LevelMeter`, `GainChangeMeter`, `AnalyzerSource` + `SpectrumAnalyzer`
(Blackman-Harris FFT, fractional-octave smoothing, `hopSize` for smooth motion, `createPath`).
`AnalyzerSource` also keeps a non-consuming history tap (`readLatest` / `getHistoryPosition`), so any number of
readers — e.g. another instance's editor — can analyse it; `SpectrumAnalyzer::Options::nonConsuming` uses it.

## Modulation UI — `ui/ModulationStrip.h`, `ui/ModulationBinder.h`, `ui/ModulationTrails.h`, `ui/ModulationPanel.h`

`ModulationStrip` (tiles + connectors + "+", popover editors), `ModulationBinder` (attach knobs: rings, drop targets,
depth drag, context menu), `ModulationTrails` (glow overlay), `ModulationPanel` (older tabbed panel; prefer the strip).

## Painting helpers — `theme/LookAndFeel.h` (`st::paint::`)

`knobDisc`, `pillHandle`, `greyButton`, `connectorDot`, `lightTrail`, `fadedLine`, `tray`, `glass`, `panel`, `well`,
`softShadow`, `glowStroke`, `caption`. Icons: `st::icons::draw (g, Icon::..., area, colour)` — includes `pencil`,
`headphones`, `scissors`, `stereoLink`, `bars`, `chevronsRight`, `chevronsLeft`, `sidechain`. Theme token `curve`: the main response curve of
filter / EQ displays.

## DSP — `dsp/` (`st::dsp::`)

| Class | Notes |
|---|---|
| `Crossover` | Phase-coherent LR crossover, 1–8 bands, 12/24/48 dB/oct, smoothed frequencies, `getBandMagnitude` for UI. |
| `UpDownCompressor` | Upward + downward compressor/expander (`depth` −1..1), static curve for UI, meters. |
| `Oversampler` | 1–16x FIR, all factors prepared up front; switch on the message thread. |
| `eq::designAnalog / designDigital`, `EqBandSpec`, `EqShape` | Parametric EQ bands (bell, shelves, cuts, notch, band pass, tilt shelf, flat tilt, all pass; 6–120 dB/oct, resonance). One analog prototype gives the exact analog response (drawing, FIR design) and analog-matched biquads (impulse-invariant poles, least-squares / matched-z numerators): no cramping near Nyquist. `hasGain / hasSlope / hasQ`. |
| `EqCascade` | Up to 12 biquads (TDF-II, double) for one band, per-sample coefficient glide when the design changes (automation, dynamic EQ). |
| `StftProcessor` | Streaming STFT (sqrt-Hann WOLA, hop N/4, latency N) with a per-frame callback that may modify the main channels' spectra; analysis-only side channels for detectors. Spectral dynamics, de-noising, spectral gates. |
| `PartitionedConvolver` | Uniform partitioned FFT convolution, mono / diagonal / true-stereo 2x2 routing, latency = partition size. IRs written from one worker thread and swapped lock-free with a one-partition cross-fade. Linear-phase EQ, natural-phase correction, IR reverbs. |
| `Reverb` | 8 types (Room, Chamber, Plate, Hall, Cathedral, Spring, Ambience, Bloom); runs at base rate internally when fed oversampled audio; `isSilent()`. |
| `Biquad`, `Svf`, `TiltFilter`, `OnePole`, `DcBlocker`, `DelayLine`, `EnvelopeFollower`, `LinearSmoother` | Building blocks; per-sample APIs need the caller's `ScopedNoDenormals`. |

## Cross-instance and the multiview — `state/InstanceRegistry.h`, `state/SharedInstances.h`, `ui/MultiView.h`

Every ST plug-in is its own binary (own JUCE, own shell), so cross-plug-in features go through one process-wide,
plain-C registry (`st::shared::Registry`, found by name through the Objective-C runtime; layout frozen, versioned).
`InstanceRegistry::get()` is this binary's view of it: `getAllInstances()` lists every ST instance in the process
(kind, number, host track name / colour, `local` ProcessorBase* for this binary's own), `getInstances()` only this
binary's. ProcessorBase registers itself; call `unregisterInstance()` first thing in your processor's destructor.
Listeners get `instancesChanged()` (async) and `instanceRemoved (uid, local)` synchronously on the message thread
before the instance is destroyed — drop it and never call into it again.

**Multiview** (`st::MultiView`): a full-editor overlay with one row per track (grouped by the host's track name;
Logic sends it to AUs as the context name) and every ST plug-in on that track as a module side by side,
`Options::leftmostKind` first (ST EQ and ST Sat use "ST EQ"). `Options::requiredKind` shows only tracks that have a
module of that kind (with every other ST plug-in on them), `Options::orderFor` sorts rows by a key of this binary's
instances, `Options::title` names the view (ST DC: "Drum Multiview", only drum tracks, in kit order). Modules come from
`ProcessorBase::createStripView()` / `getStripWidth (height)`. A module of this binary is the real component; a
module of another binary is a remote view: its owner renders it off screen into pixels and gets mouse events
forwarded (no keyboard, menus open natively at the mouse). So keep strip views to knobs / displays / small
self-handling switches, and never capture the mouse. Hover grows a row; zoom, search, pins, track list,
sideways scrolling, `Options::glowFor` for a red module glow. Open it from `HeaderBar::setMultiViewCallback` (ST Sat)
or your own button (ST EQ's bottom bar). Tested across binaries in ST EQ's tests (they load the ST Sat AU).

## Adding a component

Put it in the right layer, register it in the aggregator, use theme tokens, add it to this table, and use it from a
plugin (or `examples/MinimalPlugin`) so it's compiled and seen.
