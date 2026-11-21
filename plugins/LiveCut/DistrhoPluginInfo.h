/*
 * DISTRHO Plugin Framework (DPF)
 * Copyright (C) 2012-2021 Filipe Coelho <falktx@falktx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 This file is part of Livecut
 Copyright 2004 by Remy Muller.
 Adapted for DISTRHO Plugin Framework (DPF) by eventual-recluse.
 
 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 
 Livecut is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with Livecut; if not, visit www.gnu.org/licenses or write to the
 Free Software Foundation, Inc., 59 Temple Place, Suite 330, 
 Boston, MA 02111-1307 USA
 */

#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#include <array>

#define DISTRHO_PLUGIN_BRAND "eventual-recluse"
#define DISTRHO_PLUGIN_NAME "HyperCut"
#define DISTRHO_PLUGIN_URI "https://github.com/eventual-recluse/HyperCut"
#define DISTRHO_PLUGIN_CLAP_ID "HyperCut.eventual-recluse"

// Release shown in the UI header. Packaging derives the installer version from the git tag;
// getVersion() stays at the original LiveCut ABI version for session compatibility.
#define HYPERCUT_DISPLAY_VERSION "0.2.4"

#define DISTRHO_PLUGIN_IS_RT_SAFE 0
#define DISTRHO_PLUGIN_NUM_INPUTS 2
#define DISTRHO_PLUGIN_NUM_OUTPUTS 2
#define DISTRHO_PLUGIN_WANT_TIMEPOS 1
#define DISTRHO_PLUGIN_WANT_DIRECT_ACCESS 1 // UI reads live visualisation data from the DSP
#define DISTRHO_PLUGIN_WANT_LATENCY 1 // limiter lookahead (+ Chroma's high quality / spectral frames)
/* Chroma's MIDI mode (held notes choose the pitch classes) needs MIDI input. It is off because DPF's VST3 wrapper then
   adds 2080 MIDI CC parameters in front of ours, which would shift every VST3 parameter ID and break the automation
   of existing VST3 sessions. Set to 1 to get MIDI mode if that does not matter for you. */
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT 0

/* Session compatibility with LiveCut v1 (the original plugin name):
   - v1 used a separate VST3 controller, which gives DPF two internal parameters ahead of ours (VST3 IDs 0-1).
     Direct access removes those, so latency + programs restore exactly two, keeping every parameter ID equal.
   - No plugin state: with state, DPF switches VST2 to chunk-based saving, which old projects don't have.
     UI preferences (theme, mode, size) are stored in a per-user file instead. */
#define DISTRHO_PLUGIN_WANT_PROGRAMS 1
#define DISTRHO_PLUGIN_WANT_STATE 0

#define DISTRHO_PLUGIN_VST3_CATEGORIES "Fx|Delay|Stereo"
#define DISTRHO_PLUGIN_CLAP_FEATURES "audio-effect", "delay", "stereo"
#define DISTRHO_PLUGIN_LV2_CATEGORY "lv2:DelayPlugin"

#define DISTRHO_PLUGIN_HAS_UI 1
#define DISTRHO_UI_USE_CUSTOM 1
#define DISTRHO_UI_CUSTOM_INCLUDE_PATH "DearImGui.hpp"
#define DISTRHO_UI_CUSTOM_WIDGET_TYPE DGL_NAMESPACE::ImGuiTopLevelWidget
#define DISTRHO_UI_USER_RESIZABLE 1
#define DISTRHO_UI_DEFAULT_WIDTH 1740
#define DISTRHO_UI_DEFAULT_HEIGHT 760
#define DISTRHO_UI_URI DISTRHO_PLUGIN_URI "#UI"

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
