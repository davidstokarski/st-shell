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
| `Knob (param, undo, label)` | `setSize (large/medium/small/mini)`, `setLook (classic/ringed)`, `setColour`, `setBipolar`, `setRimParameter (param, label)` (rim becomes a second control), `valueToText`. Modulation via `ModulationBinder`. |
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

## Modulation UI — `ui/ModulationStrip.h`, `ui/ModulationBinder.h`, `ui/ModulationTrails.h`, `ui/ModulationPanel.h`

`ModulationStrip` (tiles + connectors + "+", popover editors), `ModulationBinder` (attach knobs: rings, drop targets,
depth drag, context menu), `ModulationTrails` (glow overlay), `ModulationPanel` (older tabbed panel; prefer the strip).

## Painting helpers — `theme/LookAndFeel.h` (`st::paint::`)

`knobDisc`, `pillHandle`, `greyButton`, `connectorDot`, `lightTrail`, `fadedLine`, `tray`, `glass`, `panel`, `well`,
`softShadow`, `glowStroke`, `caption`. Icons: `st::icons::draw (g, Icon::..., area, colour)`.

## DSP — `dsp/` (`st::dsp::`)

| Class | Notes |
|---|---|
| `Crossover` | Phase-coherent LR crossover, 1–8 bands, 12/24/48 dB/oct, smoothed frequencies, `getBandMagnitude` for UI. |
| `UpDownCompressor` | Upward + downward compressor/expander (`depth` −1..1), static curve for UI, meters. |
| `Oversampler` | 1–16x FIR, all factors prepared up front; switch on the message thread. |
| `Reverb` | 8 types (Room, Chamber, Plate, Hall, Cathedral, Spring, Ambience, Bloom); runs at base rate internally when fed oversampled audio; `isSilent()`. |
| `Biquad`, `Svf`, `TiltFilter`, `OnePole`, `DcBlocker`, `DelayLine`, `EnvelopeFollower`, `LinearSmoother` | Building blocks; per-sample APIs need the caller's `ScopedNoDenormals`. |

## Adding a component

Put it in the right layer, register it in the aggregator, use theme tokens, add it to this table, and use it from a
plugin (or `examples/MinimalPlugin`) so it's compiled and seen.
