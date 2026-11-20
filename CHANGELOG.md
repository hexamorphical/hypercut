# Changelog

## v0.2

- **Windows VST3 clicks fixed.** Some hosts deliver a mouse press and release in the same idle cycle. Dear ImGui
  derives clicks from the button state at `NewFrame()` time, so those clicks were lost. Presses that arrive between
  frames are now held until the next frame renders, then released, so the top dark/light toggle and the algorithm
  selector respond on the first click. Verified in REAPER.
- **Preset browser is a dropdown.** Clicking the preset name opens a searchable popup instead of sliding a sidebar
  and resizing the host window. The plugin window keeps a fixed size, and layout scaling is computed from the window
  that is actually visible.
- **HiDPI mouse coordinates.** Mouse and scroll positions are divided by the auto-scale factor when DPF auto-scaling
  is active, so pointer positions line up with the scaled UI.
- **Safer frame timing.** The ImGui delta time is clamped and never negative, avoiding huge animation jumps after a
  host stalls the UI thread.
- **Reproducible source.** The two patched submodules now point to `hexamorphical/DPF` and
  `hexamorphical/DPF-Widgets` branches that carry the fixes, so `git clone --recursive` builds the same code that this
  release ships.
- Updated Windows binaries (VST3, CLAP, LV2, VST2 and standalone).

## v0.1 beta

First public release. Six slicing algorithms, drawable trance gate, reorderable slice-aware effects, real-time
visualisers, 16 colour themes in dark and light, English and Spanish, preset browser, undo/redo and dice.
