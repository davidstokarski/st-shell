# ST Shell — guide for agents

This repo is the shared foundation for every **ST** audio plugin (JUCE 9, C++20, VST3 + AU on macOS).
It holds the DSP building blocks, state/preset plumbing, modulation system and — most importantly — the
**design language**. Every plugin in the family must look and feel like it was made by the same team.

The reference implementation is **ST Sat** (multiband saturation): `../st-sat` locally,
https://github.com/davidstokarski/st-sat on GitHub. When in doubt, do what ST Sat does.

## Read before you write code

| Doc | When |
|---|---|
| [docs/DESIGN.md](docs/DESIGN.md) | Before touching any UI. What "premium" means here, concretely. |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Before writing a processor/editor. Layers, threading rules, state. |
| [docs/COMPONENTS.md](docs/COMPONENTS.md) | Catalogue of shell components and helpers, with usage. |
| [docs/NEW_PLUGIN.md](docs/NEW_PLUGIN.md) | Step-by-step checklist for starting a new plugin. |
| [docs/QUALITY.md](docs/QUALITY.md) | How to verify (tests, snapshots, auval, CPU). Required before "done". |
| [docs/EXAMPLES.md](docs/EXAMPLES.md) | Where each pattern lives in ST Sat, file by file. |
| [docs/RELEASING.md](docs/RELEASING.md) | Building universal binaries and publishing the download zip. |

`examples/MinimalPlugin` is the smallest complete plugin using the shell — copy it to start.

## Non-negotiables

1. **Look**: dark, warm, hardware-inspired, modelled on the FabFilter Saturn 2 / Pro-series interaction model
   (full-window display, floating band/control bar, matte knobs with crescent pointers, red-rimmed hero knobs,
   pill handles, text-only header/footer, modulation tiles with glow trails). Never copy FabFilter names, logos,
   preset names or artwork — our brand is the "ST" mark + product name.
2. **No hard edges**: lines fade at their ends (`st::paint::fadedLine`), panels are rounded, shadows live inside
   component bounds (no dark squares behind rounded corners), nothing pops — things ease (`st::Animated`).
3. **Use tokens**: colours from `st::Theme::get()`, fonts from `st::fonts::`, icons from `st::icons::`.
   No hard-coded hex colours or system fonts in plugin code.
4. **Native menus**: always open menus with `st::showMenu (...)` (real NSMenu on macOS), never `PopupMenu::showMenuAsync`.
5. **Real-time safety**: no allocation, locks, logging or file I/O on the audio thread. Anything that must
   reallocate (e.g. oversampling factor changes) happens on the message thread inside `suspendProcessing`.
6. **UI performance**: animate with `st::FrameClock` (capped at 60 fps), repaint only when something changed,
   cache static components with `setBufferedToImage (true)`. A plugin UI must never make the host stutter.
7. **Verify visually**: render the editor to PNG with the snapshot tool and look at it before saying a UI change is done.
   Check every popover/state you touched (see QUALITY.md).
8. **Everything that belongs in a preset lives in `apvts.state`**; editor-only state goes in `ProcessorBase::uiState`.

## Build & test

```bash
# a plugin (from its repo; uses ../st-shell and ../.st-deps/JUCE-9.0.5 automatically)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build

# shell tests (DSP, modulation, reverb)
cmake -S . -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=Release -DST_SHELL_BUILD_TESTS=ON && cmake --build build-tests
./build-tests/tests/st_dsp_tests_artefacts/Release/st_dsp_tests
```

Command Line Tools are enough (no Xcode needed). `auval -v aufx <PluginCode> StAu` validates the AU.

## Layout of this repo

```
cmake/STShell.cmake        st_add_plugin(): one call creates VST3 + AU (+ Standalone) targets
modules/st_shell/          the JUCE module (namespace st)
  core/  dsp/  mod/  state/  theme/  ui/      (included in that order — lower layers never include higher ones)
  st_shell_menus.mm        native NSMenu bridge (st::showMenu)
resources/fonts/           Inter (OFL), embedded via STShellBinary
examples/MinimalPlugin/    smallest complete plugin
tests/                     DSP / modulation / reverb test apps
docs/                      the guides above
```

When you add something reusable to a plugin, ask: would the next plugin want this? If yes, put it in the shell
(right layer, registered in that layer's `st_<layer>.h/.cpp` aggregator) and document it in COMPONENTS.md.
