# ST Shell

Shared foundation for the ST family of audio plugins (JUCE 9, C++20, VST3 + AU on macOS): DSP building blocks,
modulation, presets/state, and the design language that makes every ST plugin look and feel the same.

Reference plugin: [ST Sat](https://github.com/davidstokarski/st-sat). Also built on it: [ST EQ](https://github.com/davidstokarski/st-eq), [ST DC](https://github.com/davidstokarski/st-dc) (drum compressor), [ST Comp](https://github.com/davidstokarski/st-comp) (compressor), [ST Split](https://github.com/davidstokarski/st-split), [ST Sample](https://github.com/davidstokarski/st-sample) (drum sampler, the first instrument). **Agents: start with [CLAUDE.md](CLAUDE.md).**

## Docs

- [CLAUDE.md](CLAUDE.md) — rules and orientation for anyone (human or agent) building on the shell
- [docs/DESIGN.md](docs/DESIGN.md) — the visual and interaction language
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — module layers, processor/editor patterns, threading, state
- [docs/COMPONENTS.md](docs/COMPONENTS.md) — catalogue of components, painting helpers and DSP classes
- [docs/NEW_PLUGIN.md](docs/NEW_PLUGIN.md) — checklist for a new plugin
- [docs/QUALITY.md](docs/QUALITY.md) — verification bar (tests, snapshots, auval, UI performance)
- [docs/EXAMPLES.md](docs/EXAMPLES.md) — where each pattern lives in ST Sat
- [docs/RELEASING.md](docs/RELEASING.md) — universal builds and GitHub release downloads
- [CONTRIBUTING.md](CONTRIBUTING.md) — how to contribute to the shell and the plugins

## What's inside (`modules/st_shell`, namespace `st`)

| Layer | Contents |
|---|---|
| `core/` | parameter builders & formatters, meter + analyser feeds, FFT spectrum analyser |
| `dsp/` | filters, analog-matched parametric EQ bands, partitioned FFT convolver, phase-coherent crossover, upward/downward compressor, oversampler, delay, envelope follower, 8-type reverb |
| `mod/` | modulation matrix: tempo-synced LFOs, envelope followers, macros, routings saved in presets |
| `state/` | `ProcessorBase` (APVTS + undo, presets, A/B, meters, analysers, UI state), preset manager |
| `theme/` | colour tokens, Inter typeface, icons, painting helpers (knob discs, pill handles, faded lines, trays...), LookAndFeel |
| `ui/` | scaled `EditorBase` + header, knobs (ringed rims, modulation arcs), faders, buttons, dropdowns, meters, popovers, modulation strip/trails, native macOS menus, animation helpers |

## Quick start

```cmake
cmake_minimum_required(VERSION 3.24)
project(STThing VERSION 1.0.0)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../st-shell/CMakeLists.txt")
    add_subdirectory(../st-shell st-shell)
else()
    include(FetchContent)
    FetchContent_Declare(st_shell GIT_REPOSITORY https://github.com/davidstokarski/st-shell.git GIT_TAG main)
    FetchContent_MakeAvailable(st_shell)
endif()

st_add_plugin(STThing PRODUCT_NAME "ST Thing" PLUGIN_CODE Stth VERSION ${PROJECT_VERSION}
              SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp)
```

Or copy [`examples/MinimalPlugin`](examples/MinimalPlugin): a complete plugin (processor, editor with ringed knob,
popover and footer tray) in ~200 lines.

JUCE is taken from `../.st-deps/JUCE-9.0.5` when present, else `-DST_JUCE_DIR=...`, else fetched.

## Tests

```bash
cmake -S . -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DST_SHELL_BUILD_TESTS=ON
cmake --build build-tests
./build-tests/tests/st_dsp_tests_artefacts/Release/st_dsp_tests
./build-tests/tests/StModTests_artefacts/Release/StModTests
./build-tests/tests/StReverbTests_artefacts/Release/StReverbTests --quick
```

Inter typeface © The Inter Project Authors, SIL Open Font License (see `resources/fonts`).
