# Changelog

## v0.3.1

- **SQPusher chopping fix.** The block length now matches the durations of the generated cuts
  (`temp * SubDiv / 8`, not the inverted `temp * 8 / SubDiv`) and fill segments advance by their
  real length. Higher subdivisions no longer re-arm the player on every grid unit — which used to
  cut off the pre-baked fills and leave only the first 16th note of each pattern audible. Empty
  slots in the original fill table are skipped instead of stalling on the previous cut.

## v0.3

- **Freeze.** A snowflake tab centred on top of the LIVE CUTS visualiser arms Freeze; on the next bar line it records
  the last two bars of the *final output* — exactly what you just heard, with its chops and effects already baked in —
  and loops that recording verbatim instead of picking new random cuts. Every slice, pan, pitch and cut-synchrone
  effect repeats identically on each lap, and the capture buffer, phrase map, playhead and output scope are coloured by
  the recorded chops. The phrase map is locked to a fixed two-bar window, matching the output scope, so it no longer changes scale with the random phrase length; it keeps showing the exact same slices that were playing when Freeze was pressed, and the playhead follows the captured two bars wherever they fall in the phrase (including across a phrase boundary) — it never swaps in a fresh set of chops from the live cutter. Stopping the host transport silences the loop, and pressing Freeze again while stopped rejoins the live signal immediately. While it is on, a strong cool blue colour grade covers the whole interface, with a low-opacity
  procedural frost texture at the edges so everything stays readable. Press it again to rejoin the host's grid at the
  next bar line. Freeze is a new parameter appended at the end of the list, so existing sessions start with it off.
- **Freeze playback and LIVE CUTS sync fix.** Freeze now replays the exact two bars captured at the bar line, and the
  LIVE CUTS map no longer rescales to the random phrase length: it is a fixed two-bar window like the output scope below,
  so the playhead and highlighted slices stay in step with the audio being replayed, including when the captured window
  crosses a phrase boundary. A stopped DAW transport now silences the loop, and switching Freeze off while stopped
  rejoins the live signal immediately instead of waiting for a bar line that never arrives.
- **Clickable signal-flow blocks.** Every module in the single-row signal flow — including the limiter tail — now
  switches on and off with a click, without dragging it to a new position. Compact line icons sit to the left of each
  block so the chain stays on one row at any window size.
- **Even filter and crush scopes.** The FILTER and CRUSH visualisers now share the same clamped height as the rest of
  the scopes, so switching algorithms or resizing the window no longer stretches one of them out of proportion.
- **Frosted theme polish.** The freeze colour grade is cooler and the frost texture lighter so every control stays
  legible. Light mode now starts with the CRT look off unless you switch it on, the FREEZE tab lines up with the
  visualiser title, and the wordmark splits into a plain "hyper" and a gradient "Cut".

## v0.2.4

- **Pitchmap key presets.** The spectral pitchmap now uses three dropdowns — **KEY**, **SCALE** (Major, Melodic Minor,
  Pentatonic, Custom) and **MODE**. Major offers Ionian through Locrian. Melodic Minor offers Melodic Minor, Dorian #2,
  Lydian Augmented, Lydian Dominant, Mixo b6, Locrian #2, Altered and Phrygian Dominant. MODE is disabled for
  Pentatonic and Custom. The piano editor is only shown for Custom scales, where clicking a key adds or removes that
  note. Existing sessions keep their scale and sound; the new custom-scale parameter is appended.

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
