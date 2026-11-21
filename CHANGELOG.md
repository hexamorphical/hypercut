# Changelog

## v0.2.3

- **Circular min/max knobs.** Every per-slice min/max range is now a rotary knob with a Serum-style modulation arc:
  drag either end of the arc to set the bounds, and watch the pointer rotate to the value of the slice playing now.
  The exact numbers remain available in a tooltip. Labels and readouts sit centred below each knob, like all other knobs.
  Related ranges are now grouped into compact rows, and the remaining knobs scale to fill each panel without gaps.
- **Live knob rotation.** Parameters that move on their own while audio plays — per-slice random values and the trance
  gate's envelope depths — now rotate their pointers instead of only changing a ring or a marker.
- **Horizontal region slider.** The pitchmap's frequency region returns to a log-frequency horizontal slider with two
  draggable edges, a live marker and exact values in a tooltip. It lines up with the analyser's spectrum axis.
- **Filter drive.** A saturation control next to RESONANCE overdrives the signal entering the per-slice filter; the
  default is clean so existing sessions sound unchanged. The crush visualiser now shows one waveform cycle.
- **Crusher mix and post lowpass.** The CRUSH panel now has a dry/wet **MIX** control and a **POST LP** filter after the
  bit/sample-rate reduction. Both default to transparent so existing sessions sound unchanged.
- **Animated segmented selectors.** The trance gate's RATE selector now uses the same segmented design as SUBDIVISION,
  and the selected highlight slides smoothly to the new value when clicked.
- **Unified theme gradients.** Toggles, knob arcs, range bands, segmented selectors, spectrum sliders and the trance
  gate envelope fill and automation line all use the same full six-stop sweep as the website, in every skin and in both
  light and dark mode, instead of each widget using its own short two-colour blend. Labelled toggle and selector fills
  are softened so the text stays readable, with no outer glow or inset shadow.
- **Gate envelope playhead.** The gate automation line is now the coloured part of the step, with a gradient bloom and a
  25% fill beneath it; the area above the curve is transparent over a pure black/white inset. Steps without attack or
  decay keep their square-wave top edge visible. The playhead is shown only as a stronger glowing dot on the envelope
  curve, with no selected-cell outline, rectangular glow, or full line across the steps.
- **Brand-consistent header logo.** The wordmark now reads **hyperCut** in the website's Space Grotesk Bold, with the
  full six-stop gradient across "hyper" and the release version (`v0.2.3`) shown underneath.

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
