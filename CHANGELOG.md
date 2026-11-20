# Changelog

## v0.2.2

- **Delay INSERT / SEND routing.** A new switch in the DELAY panel: INSERT keeps the delay in the slice path and the
  AMOUNT slider is the dry/wet balance; SEND runs a parallel tail that rings over the dry signal and AMOUNT becomes the
  send level. The default is INSERT, so existing sessions are unchanged.
- **Random delay ranges.** TIME, FEEDBACK and AMOUNT each use a min/max range with a live marker showing the value of
  the current slice. Older presets are migrated automatically.
- **Filter and crush visualisations.** The FILTER panel shows the live frequency response and the CRUSH panel shows
  the waveform and bit-depth grid, driven by the real DSP state.
- **Native installers.** Releases now include a Windows `.exe`, Linux `.deb` and `.AppImage`, and macOS `.pkg` and
  `.dmg`, alongside the zip with every plugin format. Linux builds moved into GitHub Actions too.

## v0.2.1

- **Ableton Live parameter clicks fixed.** One-shot edits such as the algorithm selector now go straight through
  `setParameterValue()` instead of a begin/perform/end gesture collapsed into one message batch, which Ableton
  discarded. Verified in Ableton Live and REAPER.
- **Preset browser artwork restored.** The v0.1 sidebar styling is back inside the fixed, non-resizing popup.

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
