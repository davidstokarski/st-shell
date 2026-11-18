# ST Shell

Shared foundation for ST audio plugins (JUCE 9, C++20, VST3 + AU on macOS).
Everything a new plugin needs to look, feel and behave like the rest of the family.

## What's inside (`modules/st_shell`, namespace `st`)

| Layer | Contents |
|---|---|
| `core/` | Parameter builders & formatters (`st::params`), lock-free `MeterSource`, `AnalyzerSource` + `SpectrumAnalyzer` |
| `dsp/` | Biquad / TPT SVF / tilt / one-pole / DC filters, phase-coherent Linkwitz-Riley `Crossover` (1–8 bands, 12/24/48 dB/oct), `UpDownCompressor` (OTT-style upward + downward), `Oversampler` (1–16x, switchable without allocation), `DelayLine`, `EnvelopeFollower`, smoothing & utils (`st::dsp`) |
| `mod/` | `st::mod::ModMatrix`: LFOs (tempo sync), envelope followers, macros, drag-and-drop routings stored in the preset state |
| `state/` | `ProcessorBase` (APVTS + undo, presets, A/B, meters, analysers, session UI state), `PresetManager`, `ABCompare` |
| `theme/` | `Theme` design tokens, embedded Inter typeface, `LookAndFeel`, vector icon set, painting helpers |
| `ui/` | `EditorBase` (uniform scaling, header, tooltips, undo shortcuts), `HeaderBar` (presets, undo/redo, A/B, settings), `Knob`, `ToggleChip`, `Segmented`, `Dropdown`, `NumberBox`, `IconButton`, `PillButton`, `LevelMeter`, `GainChangeMeter`, `TextPrompt`, modulation panel |

## Using it in a plugin

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyPlugin VERSION 1.0.0)

# Use a sibling checkout (../st-shell) if present, otherwise fetch the repo.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../st-shell/CMakeLists.txt")
    add_subdirectory(../st-shell st-shell)
else()
    include(FetchContent)
    FetchContent_Declare(st_shell GIT_REPOSITORY https://github.com/davidstokarski/st-shell.git GIT_TAG main)
    FetchContent_MakeAvailable(st_shell)
endif()

st_add_plugin(MyPlugin
    PRODUCT_NAME "ST Thing"
    PLUGIN_CODE  Stth          # unique 4-char code per plugin
    VERSION      ${PROJECT_VERSION}
    SOURCES      Source/PluginProcessor.cpp Source/PluginEditor.cpp)
```

JUCE is taken from `../.st-deps/JUCE-9.0.5` when present (shared by all plugins), else
`-DST_JUCE_DIR=...`, else fetched automatically.

Processor: derive from `st::ProcessorBase`, call `prepareShell (sampleRate)` in `prepareToPlay`,
push to `inputMeter` / `outputMeter` / `preAnalyzer` / `postAnalyzer` in `processBlock`.
Editor: derive from `st::EditorBase (processor, designWidth, designHeight)`, add children to
`getContent()` and lay them out in `layout (body)`.

Conventions:
- Everything that belongs in a preset lives in `apvts.state`; editor-only state goes in `uiState`.
- Use `st::Theme::get()` tokens and `st::fonts::` — no hard-coded colours or fonts in plugins.
- Per-plugin identity = `Theme::accent` / `accent2` + the series palette.

## Tests

```bash
cmake -S . -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DST_SHELL_BUILD_TESTS=ON
cmake --build build-tests && ./build-tests/tests/st_dsp_tests_artefacts/Release/st_dsp_tests
```

Inter typeface © The Inter Project Authors, SIL Open Font License (see `resources/fonts`).
