# ST design language

The goal: a plugin that feels as expensive as FabFilter's — specifically the Saturn 2 / Pro-series interaction
model — but darker, and unmistakably ours. Users of these plugins notice details, so details are the job.

Reference: ST Sat (`../st-sat`, screenshot in its README). Compare every new UI against it.

## 1. Composition

- **The whole window is the display.** A warm, deep-red-to-near-black gradient fills the editor
  (`EditorBase::paintBackground`, cached). The main visualisation (spectrum, bands, curves) covers the
  full width behind everything; there are no boxed "cards" or section borders.
- **Header floats over the display**: small curved logo tab top-left ("ST" + product name), then text-only
  buttons: undo / redo icons, `A/B` (inactive letter dimmed), `Copy`, `‹ preset name ›`, `Save`, gear. No bar, no boxes.
  Provided by `st::HeaderBar` (part of `EditorBase`).
- **Controls float in a translucent bar** over the lower part of the display (ST Sat: `FloatingBandPanel`):
  a short rounded bar (`Theme::glassPanel`) with grey buttons sitting *on its top edge* and knobs *hanging*
  from it, captions below the bar. The bar glides to whatever it edits (selected band) with easing.
- **Detail lives in popovers**, opened from a small `•••` next to a control: `st::Popover`. Keep the main
  surface simple; put depth (thresholds, ratios, reverb shape) one click away.
- **Bottom**: modulation tiles (`st::ModulationStrip`), then a dark footer tray (`st::paint::tray`) with
  text-only settings on the left and master values (In / Mix / Out / Pan) on the right.

## 2. Colour (always via `st::Theme::get()`)

| Token | Use |
|---|---|
| `bg0..bg4` | near-black warm surfaces, darkest to lightest |
| `displayTop`, `displayGlow` | window gradient top; selected-band column light |
| `knobLight`, `knobDark`, `buttonTop`, `buttonBottom` | the "hardware" greys of knobs, buttons, handles |
| `glassPanel` | translucent control bar |
| `text`, `textDim`, `textFaint` | three text levels; captions use `text` at ~0.9 alpha |
| `accent` (red) / `accent2` (warm orange-red) | identity: rims, selection, curves |
| `spectrum` | warm analyser line |
| `series[8]` | modulation sources (green, cyan, blue, indigo, violet, magenta, orange, yellow) |

A new plugin may change `accent`/`accent2` (and `displayTop`/`displayGlow`) for its own identity — set them once
at startup. Everything else stays shared.

## 3. Type

Inter (embedded). `st::fonts::caption()` for control captions: uppercase, semibold, tracked, ~11 px.
Values and names: `fonts::medium` 11–14 px. Never use the system font directly.

## 4. Controls — the specific look

- **Knobs** (`st::Knob`): matte grey disc, soft drop shadow, a **white crescent pointer** at the rim with a fine
  dark tick (`paint::knobDisc`). Caption below; hovering/dragging swaps the caption for the value. A faint light ring
  on hover. **Hero controls** (Drive, Level...) use `Look::ringed`: a thick red rim with white index ticks; the rim
  is a real control (`setRimParameter`) — e.g. stereo spread or pan — with L/R markers. Modulation shows as a coloured
  arc outside the knob that can be dragged to change depth, plus a coloured dot after the caption.
- **Faders** (`st::Fader`): thin dark slot + grey **pill cap** with a centre dash (`paint::pillHandle`).
- **Buttons**: grey hardware faces (`paint::greyButton`) for things that sit on the control bar (power, style, S/M, ×);
  text-only `st::FlatButton` in header/footer; `•••` flat buttons open popovers.
- **Display handles**: the same pill caps for draggable band handles; crossovers are dashed lines with small dark
  grip buttons (tent glyph).
- **Dropdowns**: grey style on the bar, flat "Caption Value ▾" style in the footer. Menus are **native** (`st::showMenu`).

## 5. Lines, light and depth

- **Lines fade at their ends.** Dividers, level lines, crossover lines, tray edges: use `paint::fadedLine` or a
  gradient. A line that stops abruptly looks cheap.
- **Highlights are tapered**: edge lights brightest in the middle, fading to the corners.
- **Shadows stay inside bounds.** Give floating components a margin (`st::Popover::margin`) so soft shadows don't get
  clipped into dark rectangles. Grey buttons keep their 1.5 px shadow inside the component.
- **Glows are subtle**: modulation trails (`ModulationTrails`) are soft pooled light, never neon.
- **Selected things are lit, not boxed**: a selected band gets light pouring down its column with soft inner edges.

## 6. Motion

- Everything eases with `st::Animated` (exponential approach) on a `st::FrameClock` (vblank, max 60 fps).
- Panels glide to their target; replaced panels cross-fade; popovers fade + slide up ~10 px.
- No motion without purpose; nothing bounces.

## 7. Interaction conventions

- Drag vertical to change, **shift = fine**, **double-click or alt-click = reset**, **cmd-click = type a value**,
  mouse wheel nudges, right-click = native context menu (with host automation items).
- Display: drag handles vertically (ST Sat: band level), drag crossovers horizontally, double-click empty space to add.
- Esc / × / click-outside closes popovers. Cmd-Z / Cmd-Shift-Z undo/redo everywhere.
- Tooltips on everything non-obvious (one plain sentence).

## 8. Checklist for any UI change

- [ ] Uses Theme tokens, `fonts::`, `icons::`; no hard-coded colours.
- [ ] No hard line ends; no clipped shadows; no overlapping controls at 100% and 75% scale.
- [ ] Hover, drag, disabled and modulated states look right.
- [ ] Menus via `st::showMenu`.
- [ ] Snapshot rendered and inspected (QUALITY.md), including popovers.
