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

/*
   HyperCut user interface.

   Everything is custom drawn with the Dear ImGui draw list API; ImGui is only
   used for input handling (invisible buttons) and the theme popup.
   The layout is designed on a 1740 x 760 canvas and scaled uniformly; the
   visualiser row absorbs any extra height.
*/

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <random>
#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
# include <direct.h>
#endif

#include "DistrhoUI.hpp"
#include "LiveCutControls.hpp"
#include "LiveCutLang.hpp"
#include "LiveCutVis.hpp"
#include "GateEnvelope.h"
#include "BlockKinds.h"
#include "NotoSansFonts.hpp"

#include "DearImGui/imgui_internal.h"
#if defined(DGL_USE_GLES2) || defined(DGL_USE_GLES3) || defined(DGL_USE_OPENGL3)
# include "DearImGui/imgui_impl_opengl3.h"
#else
# include "DearImGui/imgui_impl_opengl2.h"
#endif

START_NAMESPACE_DISTRHO

// implemented in LiveCut.cpp (same binary, direct access)
LiveCutVis::State* getLiveCutVisState(void* pluginInstance);

// --------------------------------------------------------------------------------------------------------------------
// Design constants

static constexpr float kBaseW = 1740.f;
static constexpr float kBaseH = 760.f;
static constexpr float kMinW  = 1264.f;
static constexpr float kMinH  = 580.f;

// --------------------------------------------------------------------------------------------------------------------
// Colour helpers

static inline ImVec4 hex(uint32_t rgb, float a = 1.f)
{
    return ImVec4(((rgb >> 16) & 0xff) / 255.f, ((rgb >> 8) & 0xff) / 255.f, (rgb & 0xff) / 255.f, a);
}

static inline ImVec4 mix(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

static inline ImVec4 withA(const ImVec4& c, float a)
{
    return ImVec4(c.x, c.y, c.z, a);
}

// The six-colour spectrum of the logo.
// Used only by the HyperCut logo mark and the "HYPER" part of the wordmark.
static const ImVec4 kLogoRainbow[6] = {
    hex(0xE65755), // red
    hex(0xF08B40), // orange
    hex(0xF3C65D), // yellow
    hex(0x7BB78B), // green
    hex(0x4B98C5), // blue
    hex(0xB85EBA), // violet
};

static inline ImU32 U32(const ImVec4& c, float alphaMul = 1.f)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w * alphaMul));
}

// --------------------------------------------------------------------------------------------------------------------
// Themes

struct ThemeDef {
    const char* name;
    uint32_t a, b, c;      // dark mode accents (gradient start, gradient end, highlight)
    uint32_t la, lb, lc;   // light mode accents
    bool darkOnAccent;     // dark text on accent fills (for pale colours in dark mode)
    const uint32_t* stripes; // the palette's colours in order, for the selector dots
    int stripeLen;
};

// The colours of each palette in order, drawn as a row of dots in the theme selector
// (short palettes are interpolated to >= 5 dots).
static const uint32_t kStripesSpectrum[]  = { 0xE65755, 0xF08B40, 0xF3C65D, 0x7BB78B, 0x4B98C5, 0xB85EBA };
static const uint32_t kStripesSunset[]    = { 0xE66562, 0xFEF2F1, 0xEF8C41, 0xE9B968, 0xB85BB8, 0xD7A8CD, 0x852D6F };
static const uint32_t kStripesCandy[]     = { 0xFDF5F5, 0xF4D0CA, 0x95C8E4, 0xAAC3D5, 0xD6D4DC };
static const uint32_t kStripesTwilight[]  = { 0x3259BA, 0xD4B5F1, 0xE6498A };
static const uint32_t kStripesSaffron[]   = { 0xFDF5F3, 0xF4C75C, 0xB65DB8, 0x342B32 };
static const uint32_t kStripesOrchid[]    = { 0x7BB88C, 0xFCF7F8, 0xB95DBB };
static const uint32_t kStripesArcade[]    = { 0xE54C7F, 0xF3C65D, 0x65B1E5 };
static const uint32_t kStripesMarigold[]  = { 0xF4C75C, 0xB85BB8, 0xFDF5F5, 0xB85BB8, 0xF4C75C };
static const uint32_t kStripesNebula[]    = { 0xE6879A, 0xFDF5F3, 0xB059B2, 0x342B32, 0x3359B5 };
static const uint32_t kStripesMint[]      = { 0x4C9E6B, 0x8EC69D, 0xFDFEFD, 0xB8B3B7, 0x2F2B35 };
static const uint32_t kStripesGraphite[]  = { 0x2F2B35, 0xB8B3B7, 0xFAF7F9, 0xB75CB8 };
static const uint32_t kStripesLagoon[]    = { 0xE44B88, 0x65ABA1, 0x4A98C6 };
static const uint32_t kStripesFern[]      = { 0xFEFEFE, 0x78B58A, 0x322A30 };
static const uint32_t kStripesBlossom[]   = { 0xBA5ABC, 0xF9DAEE, 0xD4A5CD };
static const uint32_t kStripesAmber[]     = { 0x97693E, 0xEE9740, 0xF3C961, 0xF6D994, 0xFEFAF5, 0xB8B3B7, 0x362E31 };
static const uint32_t kStripesSage[]      = { 0x342B32, 0xB8B3B7, 0xFDFEFD, 0x7BB88C, 0xFDFEFD, 0xB8B3B7, 0x342B32 };

// Accent colours of each theme; Spectrum (the logo's six colours) is the default.
// Light-mode variants are darkened versions of the same colours so they stay legible on white.
static const ThemeDef kThemes[] = {
    { "Spectrum", 0xE65755, 0x7BB78B, 0x4B98C5,   0xBB1E1B, 0x3B9B55, 0x3479A2,   false, kStripesSpectrum, 6 }, // the logo's colours (default)
    { "Sunset",   0xE66562, 0xEF8C41, 0xB85BB8,   0xB9211D, 0xC65F11, 0x9B3B9B,   false, kStripesSunset,   7 },
    { "Candy",    0xF4D0CA, 0x95C8E4, 0xD6D4DC,   0xB13925, 0x2B7EAB, 0x6D6969,   true,  kStripesCandy,    5 },
    { "Twilight", 0x3259BA, 0xD4B5F1, 0xE6498A,   0x2D51A9, 0x6E22B4, 0xBC1A5D,   false, kStripesTwilight, 3 },
    { "Saffron",  0xF4C75C, 0xB65DB8, 0xFDF5F3,   0xC9910E, 0x993B9B, 0x6D6969,   true,  kStripesSaffron,  4 },
    { "Orchid",   0x7BB88C, 0xB95DBB, 0xFCF7F8,   0x3B9B56, 0x993B9B, 0x6D6969,   false, kStripesOrchid,   3 },
    { "Arcade",   0xE54C7F, 0xF3C65D, 0x65B1E5,   0xBB1B50, 0xC7900F, 0x1F79B7,   false, kStripesArcade,   3 },
    { "Marigold", 0xF4C75C, 0xB85BB8, 0xFDF5F5,   0xC9910E, 0x9B3B9B, 0x6D6969,   true,  kStripesMarigold, 5 },
    { "Nebula",   0xE6879A, 0xB059B2, 0x3359B5,   0xB12541, 0x993B9B, 0x2F52A7,   false, kStripesNebula,   5 },
    { "Mint",     0x79B78B, 0xA3D8B1, 0xD0D0D0,   0x3B9B57, 0x3B9B54, 0x6D6969,   true,  kStripesMint,     5 },
    { "Graphite", 0xB75CB8, 0xC0BBBE, 0xFAF7F9,   0x9A3B9B, 0x6D6969, 0x6D6969,   true,  kStripesGraphite, 4 },
    { "Lagoon",   0xE44B88, 0x65ABA1, 0x4A98C6,   0xBA1C5B, 0x3B9B8E, 0x3379A3,   false, kStripesLagoon,   3 },
    { "Fern",     0x78B58A, 0xB4D3BE, 0xFEFEFE,   0x3B9B57, 0x3B9B5A, 0x6D6969,   true,  kStripesFern,     3 },
    { "Blossom",  0xBA5ABC, 0xF9DAEE, 0xD4A5CD,   0x993B9B, 0xB81E82, 0x9B3B8D,   true,  kStripesBlossom,  3 },
    { "Amber",    0xA26B3A, 0xEE9740, 0xF3C961,   0x9E6838, 0xC56B12, 0xC7920F,   false, kStripesAmber,    7 },
    { "Sage",     0x79B88C, 0xB8B3B7, 0xF0EFF0,   0x3B9B58, 0x6D6969, 0x6D6969,   true,  kStripesSage,     7 },
};
static constexpr int kNumThemes = sizeof(kThemes) / sizeof(kThemes[0]);

struct Palette {
    bool dark;
    ImVec4 bgTop, bgBot, panelTop, panelBot, panelBorder, inset, insetBorder;
    ImVec4 text, textDim, textFaint, track, knobTop, knobBot, knobRim, shadow, onAccent;
    ImVec4 a, b, c, rec;
    ImVec4 gradStops[6];  // full sweep for the rainbow theme, otherwise { a, b }
    int numStops = 2;

    void build(int theme, bool isDark)
    {
        dark = isDark;
        const ThemeDef& t = kThemes[std::max(0, std::min(kNumThemes - 1, theme))];
        if (dark)
        {
            bgTop       = hex(0x151826);
            bgBot       = hex(0x0A0B11);
            panelTop    = hex(0x1A1E2D, 0.92f);
            panelBot    = hex(0x121521, 0.92f);
            panelBorder = hex(0xFFFFFF, 0.07f);
            inset       = hex(0x07080D, 0.75f);
            insetBorder = hex(0xFFFFFF, 0.05f);
            text        = hex(0xE9EBF3);
            textDim     = hex(0x8D94AA);
            textFaint   = hex(0x4A5068);
            track       = hex(0xFFFFFF, 0.08f);
            knobTop     = hex(0x30364C);
            knobBot     = hex(0x151826);
            knobRim     = hex(0xFFFFFF, 0.13f);
            shadow      = hex(0x000000, 0.55f);
            onAccent    = t.darkOnAccent ? hex(0x0B0C12) : hex(0xFFFFFF);
            a = hex(t.a); b = hex(t.b); c = hex(t.c);
            rec = hex(0xFF4D5E);
        }
        else
        {
            bgTop       = hex(0xF4F5F9);
            bgBot       = hex(0xE2E5EE);
            panelTop    = hex(0xFFFFFF, 0.96f);
            panelBot    = hex(0xF5F6FA, 0.96f);
            panelBorder = hex(0x141928, 0.09f);
            inset       = hex(0xE8EBF2, 1.f);
            insetBorder = hex(0x141928, 0.06f);
            text        = hex(0x161925);
            textDim     = hex(0x5F6780);
            textFaint   = hex(0xA6ADBF);
            track       = hex(0x141928, 0.10f);
            knobTop     = hex(0xFFFFFF);
            knobBot     = hex(0xD7DBE5);
            knobRim     = hex(0x000000, 0.10f);
            shadow      = hex(0x1E2840, 0.20f);
            onAccent    = hex(0xFFFFFF);
            a = hex(t.la); b = hex(t.lb); c = hex(t.lc);
            rec = hex(0xE11D48);
        }

        // The present rainbow sweeps through all six stripes; every other theme is a plain a->b blend.
        if (theme == 0)
        {
            for (int i = 0; i < 6; ++i) gradStops[i] = kLogoRainbow[i];
            numStops = 6;
        }
        else
        {
            gradStops[0] = a; gradStops[1] = b; numStops = 2;
        }
    }

    ImVec4 grad(float t) const
    {
        t = std::max(0.f, std::min(1.f, t));
        if (numStops < 2) return gradStops[0];
        const float u = t * static_cast<float>(numStops - 1);
        int i = static_cast<int>(u);
        if (i >= numStops - 1) i = numStops - 2;
        return mix(gradStops[i], gradStops[i + 1], u - static_cast<float>(i));
    }

    // distinct but harmonious colour for a block index
    ImVec4 blockColor(int index) const
    {
        if (index < 0)
            return textFaint;
        const float t = std::fmod(index * 0.381966f, 1.f); // golden-ratio walk over the gradient
        const ImVec4 g = (t < 0.5f) ? mix(a, b, t * 2.f) : mix(b, c, (t - 0.5f) * 2.f);
        return g;
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Parameter mapping / formatting

enum MapType { kMapLin, kMapLog, kMapPow, kMapInt };
enum FmtType { kFmtPct, kFmtMs, kFmtHz, kFmtBits, kFmtInt, kFmtSemis, kFmtPan, kFmtFloat, kFmtBars, kFmtDb, kFmtSec, kFmtSt, kFmtCents, kFmtDiv };

struct ParamSpec {
    MapType map;
    float skew;
    FmtType fmt;
};

static ParamSpec specFor(uint32_t i)
{
    switch (i)
    {
    case LVC_FADE:     return { kMapPow, 3.f, kFmtMs };
    case LVC_MINAMP: case LVC_MAXAMP: return { kMapLin, 1.f, kFmtPct };
    case LVC_MINPAN: case LVC_MAXPAN: return { kMapLin, 1.f, kFmtPan };
    case LVC_MINPITCH: case LVC_MAXPITCH: return { kMapLin, 1.f, kFmtSemis };
    case LVC_DUTY: case LVC_FILLDUTY: return { kMapLin, 1.f, kFmtPct };
    case LVC_MAXPHRS: case LVC_MINPHRS: return { kMapInt, 1.f, kFmtBars };
    case LVC_MAXREP: case LVC_MINREP: return { kMapInt, 1.f, kFmtInt };
    case LVC_SPEED:    return { kMapLin, 1.f, kFmtFloat };
    case LVC_MINBITS: case LVC_MAXBITS: return { kMapLin, 1.f, kFmtBits };
    case LVC_MINFREQ: case LVC_MAXFREQ: return { kMapLog, 1.f, kFmtHz };
    case LVC_MINDELAY: case LVC_MAXDELAY: return { kMapPow, 2.f, kFmtMs };
    case LVC_SEED:     return { kMapInt, 1.f, kFmtInt };
    case LVC_LIMCEIL:  return { kMapLin, 1.f, kFmtDb };
    case LVC_LIMREL:   return { kMapLog, 1.f, kFmtMs };
    case LVC_RVDECAYMIN: case LVC_RVDECAYMAX: return { kMapLog, 1.f, kFmtSec };
    case LVC_LPFREQ: case LVC_HPFREQ: case LVC_LPMIN: case LVC_HPMAX:
    case LVC_MASTERLPFREQ: case LVC_MASTERHPFREQ: return { kMapLog, 1.f, kFmtHz };
    case LVC_CHPITCH:  return { kMapInt, 1.f, kFmtSt };
    case LVC_CHTUNE:   return { kMapLin, 1.f, kFmtCents };
    case LVC_CHGAIN: case LVC_CHGATETHR: case LVC_MAKEUP: return { kMapLin, 1.f, kFmtDb };
    case LVC_CHLOW: case LVC_CHHIGH: return { kMapLog, 1.f, kFmtHz };
    case LVC_CHPOS: case LVC_CHKEY: case LVC_CHSCALE: case LVC_TRPOS: return { kMapInt, 1.f, kFmtInt };
    case LVC_DLYTIMEL: case LVC_DLYTIMER: return { kMapInt, 1.f, kFmtDiv };
    case LVC_DLYMSL: case LVC_DLYMSR: return { kMapLog, 1.f, kFmtMs };
    case LVC_DLYMODE: case LVC_DLYSYNC: case LVC_DLYSTEREO: case LVC_DLYTUNE: return { kMapInt, 1.f, kFmtInt };
    default:           return { kMapLin, 1.f, kFmtPct };
    }
}

static float toNorm(uint32_t i, float v)
{
    const float mn = controlLimits[i].first, mx = controlLimits[i].second;
    const ParamSpec sp = specFor(i);
    float n;
    switch (sp.map)
    {
    case kMapLog: n = std::log(std::max(v, mn) / mn) / std::log(mx / mn); break;
    case kMapPow: n = std::pow(std::max(0.f, (v - mn) / (mx - mn)), 1.f / sp.skew); break;
    default:      n = (v - mn) / (mx - mn); break;
    }
    return std::max(0.f, std::min(1.f, n));
}

static float fromNorm(uint32_t i, float n)
{
    n = std::max(0.f, std::min(1.f, n));
    const float mn = controlLimits[i].first, mx = controlLimits[i].second;
    const ParamSpec sp = specFor(i);
    switch (sp.map)
    {
    case kMapLog: return mn * std::pow(mx / mn, n);
    case kMapPow: return mn + (mx - mn) * std::pow(n, sp.skew);
    case kMapInt: return std::round(mn + (mx - mn) * n);
    default:      return mn + (mx - mn) * n;
    }
}

static void formatValue(uint32_t i, float v, char* buf, size_t sz)
{
    switch (specFor(i).fmt)
    {
    case kFmtPct:   std::snprintf(buf, sz, "%.0f%%", v * 100.f); break;
    case kFmtMs:    std::snprintf(buf, sz, v < 10.f ? "%.2f ms" : "%.1f ms", v); break;
    case kFmtHz:    if (v < 1000.f) std::snprintf(buf, sz, "%.0f Hz", v); else std::snprintf(buf, sz, "%.2f kHz", v / 1000.f); break;
    case kFmtBits:  std::snprintf(buf, sz, "%.1f bit", v); break;
    case kFmtInt:   std::snprintf(buf, sz, "%d", static_cast<int>(std::round(v))); break;
    case kFmtBars:  std::snprintf(buf, sz, "%d", static_cast<int>(std::round(v))); break;
    case kFmtSemis: std::snprintf(buf, sz, "%+.1f st", v / 100.f); break;
    case kFmtFloat: std::snprintf(buf, sz, "%.3f", v); break;
    case kFmtDb:    std::snprintf(buf, sz, "%.1f dB", v); break;
    case kFmtSec:   std::snprintf(buf, sz, v < 10.f ? "%.1fs" : "%.0fs", v); break;
    case kFmtSt:    std::snprintf(buf, sz, "%+d st", static_cast<int>(std::round(v))); break;
    case kFmtCents: std::snprintf(buf, sz, "%+.0f ct", v); break;
    case kFmtDiv:   std::snprintf(buf, sz, "%s", delayDiv(v).name); break;
    case kFmtPan:
        if (std::fabs(v) < 0.005f) std::snprintf(buf, sz, "C");
        else std::snprintf(buf, sz, "%s%.0f", v < 0.f ? "L" : "R", std::fabs(v) * 100.f);
        break;
    }
}

// --------------------------------------------------------------------------------------------------------------------

static constexpr int kNumAlgos = kAlgoCount;
static const char* const kAlgoNames[kNumAlgos] = { "CUTPROC 11", "WARPCUT", "SQPUSHER", "EUCLID", "RATCHET", "GATE" };
static const char* const kAlgoDescriptions[kNumAlgos] = {
    "Classic jungle cutter. Blocks of repeated cuts with stutter fills toward the end of each phrase.",
    "Accelerating / decelerating cut rolls. Blocks are cut straight, or warped into speeding rolls.",
    "Squarepusher-style pre-baked cut sequences, with fills chosen by activity.",
    "Retriggers each half / whole bar on a euclidean rhythm (3-3-2 and friends). Busier in the last bar.",
    "Build-up rolls: retriggers per beat grow across the phrase. Pitch range = riser, pan range = ping-pong.",
    "Trance gate: the audio stays in place, steps are only muted. Random patterns, or click the steps to draw your own.",
};
// identity color per algorithm (independent of the theme), dark / light mode variants
static const uint32_t kAlgoColorsDark[kNumAlgos]  = { 0xFFA94D, 0x38D9F5, 0xF472B6, 0x5EE38A, 0xA78BFA, 0xFACC15 };
static const uint32_t kAlgoColorsLight[kNumAlgos] = { 0xE07800, 0x0891B2, 0xDB2777, 0x16A34A, 0x7C3AED, 0xB45309 };

// --------------------------------------------------------------------------------------------------------------------
// Per-user UI preferences (theme, mode, window size).
// Kept out of the plugin state on purpose: plugin state would change how VST2 hosts save projects and
// break sessions made with LiveCut v1. These are look-and-feel settings, so one file per user is enough.

struct UIPrefs {
    int theme = 0;
    bool dark = true;
    uint width = 0, height = 0;
    std::string author; // last author typed when saving a preset
    bool crt = true;    // CRT look on the visualisers
    std::vector<std::string> favourites; // preset keys ("factory:Name" / "user:Name")
    std::vector<std::string> hidden;     // factory presets the user deleted (keys)
    int lang = -1;      // UiLang; -1 = not chosen yet (follows the system language)
};

// first start: Spanish when the system language is Spanish
static int systemLanguage()
{
    for (const char* var : { "LC_ALL", "LC_MESSAGES", "LANG", "LANGUAGE" })
        if (const char* v = std::getenv(var))
            if (v[0] != '\0')
                return (std::strncmp(v, "es", 2) == 0) ? kLangEs : kLangEn;
    return kLangEn;
}

static std::string prefsDir()
{
   #if defined(_WIN32)
    const char* base = std::getenv("APPDATA");
    return base != nullptr ? std::string(base) + "\\HyperCut" : std::string();
   #elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return home != nullptr ? std::string(home) + "/Library/Preferences/HyperCut" : std::string();
   #else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        if (xdg[0] != '\0')
            return std::string(xdg) + "/HyperCut";
    const char* home = std::getenv("HOME");
    return home != nullptr ? std::string(home) + "/.config/HyperCut" : std::string();
   #endif
}

static std::string prefsFile()
{
    const std::string dir = prefsDir();
   #if defined(_WIN32)
    return dir.empty() ? dir : dir + "\\ui.conf";
   #else
    return dir.empty() ? dir : dir + "/ui.conf";
   #endif
}

static UIPrefs loadPrefs()
{
    UIPrefs p;
    const std::string path = prefsFile();
    if (path.empty())
        return p;
    if (FILE* f = std::fopen(path.c_str(), "r"))
    {
        char line[192];
        while (std::fgets(line, sizeof(line), f) != nullptr)
        {
            if (std::strncmp(line, "author=", 7) == 0 || std::strncmp(line, "fav=", 4) == 0 || std::strncmp(line, "hide=", 5) == 0)
            {
                const char* eq = std::strchr(line, '=');
                std::string v(eq + 1);
                while (! v.empty() && (v.back() == '\n' || v.back() == '\r')) v.pop_back();
                if (line[0] == 'a') p.author = v;
                else if (line[0] == 'f') p.favourites.push_back(v);
                else p.hidden.push_back(v);
                continue;
            }
            int iv = 0;
            uint w = 0, h = 0;
            char sv[16] = {};
            if (std::sscanf(line, "theme=%d", &iv) == 1)
                p.theme = std::max(0, std::min(kNumThemes - 1, iv));
            else if (std::sscanf(line, "mode=%15s", sv) == 1)
                p.dark = std::strcmp(sv, "light") != 0;
            else if (std::sscanf(line, "crt=%d", &iv) == 1)
                p.crt = iv != 0;
            else if (std::sscanf(line, "lang=%15s", sv) == 1)
                p.lang = std::strcmp(sv, "es") == 0 ? kLangEs : kLangEn;
            else if (std::sscanf(line, "size=%ux%u", &w, &h) == 2)
            {
                p.width = w;
                p.height = h;
            }
        }
        std::fclose(f);
    }
    return p;
}

static void savePrefs(const UIPrefs& p)
{
    const std::string dir = prefsDir();
    if (dir.empty())
        return;
   #if defined(_WIN32)
    _mkdir(dir.c_str());
   #else
    mkdir(dir.c_str(), 0755);
   #endif
    if (FILE* f = std::fopen(prefsFile().c_str(), "w"))
    {
        std::fprintf(f, "theme=%d\nmode=%s\ncrt=%d\n", p.theme, p.dark ? "dark" : "light", p.crt ? 1 : 0);
        if (p.lang >= 0)
            std::fprintf(f, "lang=%s\n", p.lang == kLangEs ? "es" : "en");
        if (p.width > 0 && p.height > 0)
            std::fprintf(f, "size=%ux%u\n", p.width, p.height);
        if (! p.author.empty())
            std::fprintf(f, "author=%s\n", p.author.c_str());
        for (const std::string& k : p.favourites)
            std::fprintf(f, "fav=%s\n", k.c_str());
        for (const std::string& k : p.hidden)
            std::fprintf(f, "hide=%s\n", k.c_str());
        std::fclose(f);
    }
}

// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
// Presets
//
// A preset is a complete set of parameter values. Factory presets are listed here as overrides of the defaults.
// User presets are plain text files ("symbol=value" per line, plus name= and author=) in
// <prefs dir>/presets/*.lcpreset, so they are
// readable, diffable and survive new parameters being added (missing values fall back to the default).
// Algorithm values: 0 CutProc11, 0.5 Euclid, 1 WarpCut, 1.5 Ratchet, 2 SQPusher (see algoFromParam); gate=1 selects
// the trance gate instead (gatepat1 / gatepat2: step bit masks, bit 0 = step 1).

struct FactoryPreset {
    const char* name;
    const char* category;
    const char* data;   // "symbol=value;symbol=value" (symbol without the "livecut_" prefix)
    const char* author; // nullptr = "HyperCut"
};

static const FactoryPreset kFactoryPresets[] = {
    { "Init", "Basic", "" },
    // ---- by Hexamorphical ----
    { "Jungle Classic", "Jungle",
      "fade=0;minpan=-0.00052267313;maxpan=-0.00052267313;minpitch=-2400;maxpitch=2400;crusher=1"
      ";minbits=20.0380764;minfreq=2475.34351;mindelay=1;maxdelay=4.45629597;limiter=1"
      ";limceil=-0.856194496;mincut=1;transient=1;trsustain=-0.487063766",
      "Hexamorphical" },
    { "Instant Colorbass", "FX",
      "cutproc=0.5;fade=0;minpan=-0.00286000967;maxpan=-0.00286000967;activity=1;minbits=14.3492155"
      ";minfreq=4772.875;reso=1;euslice=4;filter=0;lpmin=20;rvchance=1;rvsendmin=0"
      ";rvdecaymin=0.200000003;masterhpfreq=30.2712231;rvmode=1;fxorder=12;chroma=1;chpos=0;chkey=3"
      ";chcolor=1.56420016;chgain=10.9368019;chlow=251.031387;chhigh=20000;chmorph=1"
      ";chmorphamt=0.832600117;chgatethr=-83.2320099",
      "Hexamorphical" },
    { "Kickbass-Fills", "Rolls",
      "cutproc=1;reso=0.50795877;limiter=1;limceil=0;mincut=1;fillmode=1;fillcycle=0;fillbars=0"
      ";hpmax=2659.40625;transient=1",
      "Hexamorphical" },
    { "Little Bits", "FX",
      "minpan=0.0424896479;maxpan=0.0424896479;duty=0.57569927;crusher=1;minbits=11.6534796"
      ";minfreq=852.505005;feedback=0.899999976;mindelay=1;maxdelay=7.07895756;reso=0.968221068"
      ";limceil=0;lpmin=336.128571;reverb=1;rvsendmin=0.701431453;rvsendmax=1;rvdecaymin=0.72362709"
      ";rvdecaymax=2.54552794;rvmode=1;fxorder=14;chroma=1;chscale=1;chcolor=1.57285166;chlow=20"
      ";chhigh=20000;chmorphamt=1;chgatethr=-38.2487411;transient=1;trsustain=-1",
      "Hexamorphical" },
    { "Lowpass Rainbow", "FX",
      "crusher=1;minbits=11.6534796;minfreq=852.505005;feedback=0.899999976;mindelay=1"
      ";maxdelay=7.07895756;reso=0.968221068;lpmin=336.128571;fxorder=14;chroma=1;chpos=0;chscale=1"
      ";chlow=20;chhigh=20000;route=-1;transient=1;trpos=1",
      "Hexamorphical" },
    { "Lush Pad Gater", "Gate",
      "cutproc=1;fade=12.0214281;minpan=0.0499122143;maxpan=0.0499122143;minpitch=119.789307"
      ";maxpitch=119.789307;duty=0.94685781;maxphrs=5;maxrep=2;minrep=1;stutter=0.820256889"
      ";area=0.502915382;straight=0.22785604;regular=0.696587205;ritard=0.603043318;speed=0.90705812"
      ";activity=0.401636899;crusher=1;minfreq=2980.49561;comb=1;type=1;feedback=0.275582701;seed=11"
      ";reso=1;mincut=1;eudensity=0.368411243;euchance=0.718687773;raintensity=0.811035573"
      ";racurve=0.280992955;euslice=4;lpmin=476.33844;reverb=1;rvchance=0.303105265;rvsendmin=0"
      ";rvsendmax=1;rvdecaymin=0.200000003;rvdecaymax=0.711017489;rvdamp=0;chroma=1;chlow=20"
      ";chhigh=20000;route=333;makeup=-5.68665934;gate=1;gatemode=1;gatedensity=0.25310123;gatevary=0"
      ";gatepat1=16351;gatepat2=16191;gateenv1=61680;gateenv2=61680;gateenv3=3840;gateenv4=61455"
      ";gateenv5=240;gateenv6=61440;gateenv7=3;gatetension1=60596;gatetension2=33151;gatetension3=64641"
      ";gatetie1=71;gatelp=1;dlymode=1;dlytimel=8;dlymsl=55.0101814;dlymix=0.216811731;dlytone=0",
      "Hexamorphical" },
    { "Skrill Vocals", "Glitch",
      "subdiv=3;fade=0;minpan=-0.0209413767;maxpan=-0.0209413767;duty=0.495138437;fillduty=0.507528186"
      ";maxrep=0;crusher=1;minbits=21.8020306;minfreq=4638.41699;seed=16;reso=1;limiter=1;limceil=-12"
      ";limrel=10;mincut=1;raintensity=0.699999988;filter=0;lpmin=713.919739;rvsize=0.800000012"
      ";masterhpfreq=57.0603104;rvmode=1;chroma=1;chkey=9;chscale=10;chcolor=1.06726384;chpitch=12"
      ";chlow=20;chhigh=20000;trpos=3;makeup=9.70075035;dlymode=1;dlytimel=8;dlymix=0.24159424"
      ";dlytone=1",
      "Hexamorphical" },
    { "Weird SFX", "FX",
      "minpan=0.0424896479;maxpan=0.0424896479;duty=0.57569927;minbits=21.2756653;minfreq=4798.16504"
      ";comb=1;type=1;feedback=0.899999976;mindelay=1;maxdelay=26.6393318;reso=0.968221068;limiter=1"
      ";limceil=-12;limrel=10;lpmin=336.128571;reverb=1;rvsendmin=0.701431453;rvsendmax=1"
      ";rvdecaymin=0.72362709;rvdecaymax=2.54552794;rvmode=1;fxorder=14;chscale=1;chcolor=1.57285166"
      ";chlow=20;chhigh=20000;chmorphamt=1;chgatethr=-38.2487411;route=535;transient=1;trsustain=-1"
      ";makeup=11.5688629",
      "Hexamorphical" },
};
static constexpr int kNumFactoryPresets = sizeof(kFactoryPresets) / sizeof(kFactoryPresets[0]);

static int symbolIndex(const char* sym)
{
    for (int i = 0; i < LVC_CONTROL_NR; ++i)
        if (std::strcmp(LVC_SYMBOLS[i], sym) == 0)
            return i;
    return -1;
}

static void factoryValues(const FactoryPreset& fp, float* out)
{
    for (int i = 0; i < LVC_CONTROL_NR; ++i)
        out[i] = LVC_DEFAULTS[i];
    std::string data(fp.data);
    size_t pos = 0;
    while (pos < data.size())
    {
        const size_t end = std::min(data.find(';', pos), data.size());
        const std::string item = data.substr(pos, end - pos);
        const size_t eq = item.find('=');
        if (eq != std::string::npos)
        {
            const int idx = symbolIndex(("livecut_" + item.substr(0, eq)).c_str());
            if (idx >= 0)
                out[idx] = limit<float>(static_cast<float>(std::atof(item.substr(eq + 1).c_str())),
                                        controlLimits[idx].first, controlLimits[idx].second);
        }
        pos = end + 1;
    }
}

static std::string presetsDir()
{
    const std::string dir = prefsDir();
   #if defined(_WIN32)
    return dir.empty() ? dir : dir + "\\presets";
   #else
    return dir.empty() ? dir : dir + "/presets";
   #endif
}

static std::string presetPath(const std::string& name)
{
    std::string safe;
    for (char c : name)
        safe += (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-' || c == '_' || c == '.') ? c : '_';
   #if defined(_WIN32)
    return presetsDir() + "\\" + safe + ".lcpreset";
   #else
    return presetsDir() + "/" + safe + ".lcpreset";
   #endif
}

static bool readPresetFile(const std::string& path, std::string& name, std::string& author, std::string& category, float* out)
{
    FILE* f = std::fopen(path.c_str(), "r");
    if (f == nullptr)
        return false;
    bool seen[LVC_CONTROL_NR] = {};
    for (int i = 0; i < LVC_CONTROL_NR; ++i)
        out[i] = LVC_DEFAULTS[i];
    char line[256];
    while (std::fgets(line, sizeof(line), f) != nullptr)
    {
        std::string l(line);
        while (! l.empty() && (l.back() == '\n' || l.back() == '\r'))
            l.pop_back();
        if (l.empty() || l[0] == '#')
            continue;
        const size_t eq = l.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = l.substr(0, eq), val = l.substr(eq + 1);
        if (key == "name")
        {
            name = val;
            continue;
        }
        if (key == "author")
        {
            author = val;
            continue;
        }
        if (key == "category")
        {
            category = val;
            continue;
        }
        const int idx = symbolIndex(key.c_str());
        if (idx >= 0)
        {
            out[idx] = limit<float>(static_cast<float>(std::atof(val.c_str())), controlLimits[idx].first, controlLimits[idx].second);
            seen[idx] = true;
        }
    }
    std::fclose(f);
    if (seen[LVC_LPFREQ] && ! seen[LVC_LPMIN]) out[LVC_LPMIN] = out[LVC_LPFREQ];
    if (seen[LVC_HPFREQ] && ! seen[LVC_HPMAX]) out[LVC_HPMAX] = out[LVC_HPFREQ];
    return true;
}

static bool writePresetFile(const std::string& name, const std::string& author, const std::string& category,
                            const float* values)
{
    const std::string pdir = prefsDir(), dir = presetsDir();
    if (dir.empty())
        return false;
   #if defined(_WIN32)
    _mkdir(pdir.c_str());
    _mkdir(dir.c_str());
   #else
    mkdir(pdir.c_str(), 0755);
    mkdir(dir.c_str(), 0755);
   #endif
    FILE* f = std::fopen(presetPath(name).c_str(), "w");
    if (f == nullptr)
        return false;
    std::fprintf(f, "# HyperCut preset\nname=%s\n", name.c_str());
    if (! author.empty())
        std::fprintf(f, "author=%s\n", author.c_str());
    if (! category.empty())
        std::fprintf(f, "category=%s\n", category.c_str());
    for (int i = 0; i < LVC_CONTROL_NR; ++i)
        std::fprintf(f, "%s=%.9g\n", LVC_SYMBOLS[i], static_cast<double>(values[i]));
    std::fclose(f);
    return true;
}

static std::vector<std::string> listPresetFiles()
{
    std::vector<std::string> files;
    const std::string dir = presetsDir();
    if (DIR* d = opendir(dir.c_str()))
    {
        while (dirent* e = readdir(d))
        {
            const std::string n(e->d_name);
            if (n.size() > 9 && n.compare(n.size() - 9, 9, ".lcpreset") == 0)
               #if defined(_WIN32)
                files.push_back(dir + "\\" + n);
               #else
                files.push_back(dir + "/" + n);
               #endif
        }
        closedir(d);
    }
    return files;
}

// --------------------------------------------------------------------------------------------------------------------

struct ViewZoom {
    float time = 1.f;    // horizontal zoom factor
    float amp = 1.f;     // vertical (amplitude) zoom factor
    float offset = 0.f;  // left edge of the view, 0..1 of the content (pannable views only)
};

// --------------------------------------------------------------------------------------------------------------------

class HyperCutUI : public UI
{
    // parameter mirror
    float pv[LVC_CONTROL_NR];

    // visualisation
    LiveCutVis::State* vis = nullptr;
    LiveCutVis::Phrase phrase {};

    // theme
    int themeIndex = 0;
    bool darkMode = true;
    Palette P;

    // fonts
    ImGuiContext* context = nullptr;
    ImFont* fBody = nullptr;
    ImFont* fSmall = nullptr;
    ImFont* fBold = nullptr;
    ImFont* fTitle = nullptr;
    float fontBuiltScale = 1.f;
    float fontAdj = 1.f;
    bool fontRebuildPending = false;
    float fontWantedScale = 1.f;

    // layout scale
    float s = 1.f;
    double hostScale = 1.0;

    // interaction state
    float dragNorm = 0.f;
    int rangeHandle = 0; // 0 = min, 1 = max
    bool resizing = false;
    ImVec2 resizeStartMouse, resizeStartSize;
    bool cursorIsResize = false;
    float algoAnim = -1.f;
    int gatePaintValue = -1; // trance gate drawing: 1 = opening steps, 0 = muting, -1 = not drawing
    int gateDragMode = 0;    // trance gate sequencer: 0 none, 1 painting steps, 2 right-drag envelope
    int gateDragStep = -1, gateDragStart = 0, gatePaintLast = -1;
    float gateDragStartF = 0.f;
    float gateDragY = 0.f, gateWheelAcc = 0.f;
    int framesSinceOpen = 0;
    bool sizeDirty = false;

    // visualiser zoom + envelope lanes (per window, not saved)
    ViewZoom zBuf, zMap, zScope;

    float transMeter = 0.f; // transient shaper gain meter, held for display

    // CRT look of the visualisers: each screen is copied to a texture in the middle of ImGui's rendering and drawn
    // back curved, with phosphor ghosting, bloom, scanlines and grain (plain OpenGL 1.1 calls, no shaders)
    struct CrtScreen { GLuint tex = 0; int w = 0, h = 0; bool history = false; };
    static constexpr int kCrtMax = 4;
    CrtScreen crtScreens[kCrtMax];
    ImRect crtRects[kCrtMax];
    float crtRound[kCrtMax] {};
    int crtCount = 0;
    GLuint crtNoise = 0;
    bool crtEnabled = true;
    uint32_t crtSeed = 0x9E3779B9u;

    // Spectral Pitchmap: moved level per pitch class, smoothed for the keyboard glow
    float chGlow[12] {};
    // spectrum view: per-note levels smoothed for display, region edge being dragged (0 = low, 1 = high)
    float chDispDry[LiveCutVis::kChromaNotes] {}, chDispTuned[LiveCutVis::kChromaNotes] {};
    int chRegionDrag = -1;

    // signal-flow ribbon: drag + animation state of the movable row (slicer, four effects, Chroma)
    static constexpr int kFlowRow = kFlowItems;
    int flowDrag = -1;
    int flowPreview[kFlowRow] = { kRouteSlice, kFxTransient, 0, 1, 2, 3, 4 };
    float flowX[kFlowRow] = {};
    bool flowReady = false;
    ImVec2 flowPressPos;
    bool flowMoved = false;
    bool showEnv[6] = { true, false, false, false, false, false }; // amp, bits, rate, comb, lowpass, reverb send
    ImVec2 envPts[6][2048];

    // antialiased output view scratch buffers
    static constexpr int kScopePts = 2048;
    float sX[kScopePts], sTop[kScopePts], sBot[kScopePts], sGTop[kScopePts], sGBot[kScopePts], sEnvY[kScopePts], sEnvB[kScopePts];
    ImU32 sFill[kScopePts], sLine[kScopePts], sGCol[kScopePts], sEnvC[kScopePts];
    uint8_t sFlags[kScopePts];
    int16_t sBlk[kScopePts];
    bool sBypass[kScopePts];
    ImVec2 contourPts[kScopePts];

    // undo / redo
    struct UndoChange { uint32_t idx; float before, after; };
    struct UndoEntry { std::vector<UndoChange> changes; double time; bool wheel; };
    float committed[LVC_CONTROL_NR];
    bool touched[LVC_CONTROL_NR] = {};
    bool wheelEdit = false;
    std::vector<UndoEntry> undoStack, redoStack;

    // presets
    struct PresetEntry {
        std::string name;
        std::string author;
        std::string category;
        std::string path; // empty for factory presets
        float values[LVC_CONTROL_NR];
    };
    std::vector<PresetEntry> presets;
    int currentPreset = 0;
    std::string currentName = "Init";
    float presetRef[LVC_CONTROL_NR];
    char saveName[64] = {};
    char saveAuthor[64] = {};
    char saveCategory[48] = {};
    char presetSearch[64] = {};
    std::vector<std::string> favourites, hiddenFactory; // preset keys, remembered per user
    bool favOnly = false;      // list only favourites
    int deleteArmed = -1;      // preset whose delete button was clicked once (a second click deletes)
    int presetSortCol = -1;   // -1 = default order (factory first), 0 name, 1 author, 2 category
    bool presetSortDesc = false;
    std::string authorName; // remembered between saves (per user)
    static constexpr const char* kFactoryAuthor = "HyperCut";

    // preset sidebar: slides out on the right; the window grows by its width so every control stays in view
    static constexpr float kSideW = 440.f; // design width
    bool sideOpen = false;
    float sideAnim = 0.f;    // 0 = closed .. 1 = open
    uint sideBaseW = 0;      // window width without the sidebar
    float sidePx = 0.f;      // sidebar width in pixels (fixed while it is out)
    int sideNoResize = 0;    // frames the host has not followed a resize request
    bool saveFocus = false;

    std::mt19937 rng { std::random_device{}() };
    uint lastSavedW = 0, lastSavedH = 0;

public:
    HyperCutUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, false)
    {
        hostScale = getScaleFactor();

        const uint minW = static_cast<uint>(kMinW * hostScale);
        const uint minH = static_cast<uint>(kMinH * hostScale);
        setGeometryConstraints(minW, minH, false, false);

        if (! d_isEqual(hostScale, 1.0))
            setSize(static_cast<uint>(DISTRHO_UI_DEFAULT_WIDTH * hostScale),
                    static_cast<uint>(DISTRHO_UI_DEFAULT_HEIGHT * hostScale));

        for (int32_t i = 0; i < LVC_CONTROL_NR; i++)
            pv[i] = LVC_DEFAULTS[i];

        vis = getLiveCutVisState(getPluginInstancePointer());

        std::memcpy(committed, pv, sizeof(committed));
        std::memcpy(presetRef, pv, sizeof(presetRef));
        rescanPresets();

        // user preferences: theme, mode and last window size
        {
            const UIPrefs prefs = loadPrefs();
            themeIndex = prefs.theme;
            darkMode = prefs.dark;
            authorName = prefs.author;
            crtEnabled = prefs.crt;
            uiLang() = prefs.lang >= 0 ? prefs.lang : systemLanguage();
            favourites = prefs.favourites;
            hiddenFactory = prefs.hidden;
            rescanPresets();
            lastSavedW = prefs.width;
            lastSavedH = prefs.height;
            if (prefs.width >= kMinW * hostScale && prefs.height >= kMinH * hostScale
                && prefs.width < 8192 && prefs.height < 8192)
                setSize(prefs.width, prefs.height);
        }

        context = ImGui::GetCurrentContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigWindowsMoveFromTitleBarOnly = true;

        P.build(themeIndex, darkMode);
        s = computeScale();
        buildFonts(s);
        applyImGuiStyle();
    }

protected:
    // ----------------------------------------------------------------------------------------------------------------
    // DSP/Plugin callbacks

    ~HyperCutUI() override
    {
        for (int i = 0; i < kCrtMax; ++i)
            if (crtScreens[i].tex != 0)
                glDeleteTextures(1, &crtScreens[i].tex);
        if (crtNoise != 0)
            glDeleteTextures(1, &crtNoise);
    }

    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= LVC_CONTROL_NR)
            return;
        pv[index] = value;
        if (! touched[index])
            committed[index] = value; // host change (automation, preset, session load): not an undo step
        repaint();
    }

    // the single placeholder program changes nothing (it only keeps VST3 parameter IDs compatible with v1)
    void programLoaded(uint32_t) override {}

    // ----------------------------------------------------------------------------------------------------------------
    // Undo / redo
    //
    // Every change made from this UI goes through setParam(), which marks the parameter as touched. Once the mouse
    // is released (or right away for clicks, presets and dice), the touched parameters that really changed become
    // one history entry. Host automation is not recorded: it only moves the "committed" baseline.


    void commitUndo(bool force = false)
    {
        if (! force && (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right)))
            return;

        std::vector<UndoChange> changes;
        for (uint32_t i = 0; i < LVC_CONTROL_NR; ++i)
        {
            if (! touched[i])
                continue;
            touched[i] = false;
            if (pv[i] != committed[i])
                changes.push_back({ i, committed[i], pv[i] });
            committed[i] = pv[i];
        }
        const bool wheel = wheelEdit;
        wheelEdit = false;
        if (changes.empty())
            return;

        const double now = ImGui::GetTime();
        // consecutive mouse-wheel steps on the same control become one step
        if (wheel && ! undoStack.empty() && undoStack.back().wheel && now - undoStack.back().time < 0.6
            && undoStack.back().changes.size() == changes.size())
        {
            bool same = true;
            for (size_t k = 0; k < changes.size(); ++k)
                same = same && undoStack.back().changes[k].idx == changes[k].idx;
            if (same)
            {
                for (size_t k = 0; k < changes.size(); ++k)
                    undoStack.back().changes[k].after = changes[k].after;
                undoStack.back().time = now;
                redoStack.clear();
                return;
            }
        }

        undoStack.push_back({ changes, now, wheel });
        if (undoStack.size() > 200)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
    }

    void applyHistory(const UndoEntry& e, bool useBefore)
    {
        for (const UndoChange& c : e.changes)
        {
            const float v = useBefore ? c.before : c.after;
            editParameter(c.idx, true);
            pv[c.idx] = v;
            setParameterValue(c.idx, v);
            editParameter(c.idx, false);
            committed[c.idx] = v;
            touched[c.idx] = false;
        }
    }

    void undo()
    {
        commitUndo(true);
        if (undoStack.empty())
            return;
        UndoEntry e = undoStack.back();
        undoStack.pop_back();
        applyHistory(e, true);
        redoStack.push_back(e);
    }

    void redo()
    {
        commitUndo(true);
        if (redoStack.empty())
            return;
        UndoEntry e = redoStack.back();
        redoStack.pop_back();
        applyHistory(e, false);
        undoStack.push_back(e);
    }

    // set many parameters as one gesture + one undo step
    void applyValues(const float* values)
    {
        for (uint32_t i = 0; i < LVC_CONTROL_NR; ++i)
            if (values[i] != pv[i])
                setParamGesture(i, values[i]);
        commitUndo(true);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Presets


    void rescanPresets()
    {
        presets.clear();
        std::vector<PresetEntry> user;
        for (const std::string& path : listPresetFiles())
        {
            PresetEntry e;
            e.path = path;
            if (readPresetFile(path, e.name, e.author, e.category, e.values))
            {
                if (e.name.empty())
                    e.name = path.substr(path.find_last_of("/\\") + 1);
                user.push_back(e);
            }
        }
        auto userHas = [&](const char* name) {
            for (const PresetEntry& u : user)
                if (u.name == name) return true;
            return false;
        };
        for (int i = 0; i < kNumFactoryPresets; ++i)
        {
            if (isHidden(std::string("factory:") + kFactoryPresets[i].name))
                continue; // deleted by the user (factory presets can only be hidden)
            if (userHas(kFactoryPresets[i].name))
                continue; // the user's own preset of that name takes its place
            PresetEntry e;
            e.name = kFactoryPresets[i].name;
            e.author = kFactoryPresets[i].author != nullptr ? kFactoryPresets[i].author : kFactoryAuthor;
            e.category = kFactoryPresets[i].category;
            factoryValues(kFactoryPresets[i], e.values);
            presets.push_back(e);
        }
        std::sort(user.begin(), user.end(), [](const PresetEntry& x, const PresetEntry& y) {
            std::string a = x.name, b = y.name;
            for (char& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            for (char& c : b) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return a < b;
        });
        for (const PresetEntry& e : user)
            presets.push_back(e);

        // keep pointing at the same preset by name
        currentPreset = -1;
        for (size_t i = 0; i < presets.size(); ++i)
            if (presets[i].name == currentName)
                currentPreset = static_cast<int>(i);
    }

    void loadPreset(int index)
    {
        if (index < 0 || index >= static_cast<int>(presets.size()))
            return;
        applyValues(presets[index].values);
        currentPreset = index;
        currentName = presets[index].name;
        std::memcpy(presetRef, presets[index].values, sizeof(presetRef));
    }

    void savePreset(const std::string& name)
    {
        if (name.empty())
            return;
        if (writePresetFile(name, saveAuthor, saveCategory, pv))
        {
            authorName = saveAuthor;
            persistPrefs();
            currentName = name;
            std::memcpy(presetRef, pv, sizeof(presetRef));
            rescanPresets();
        }
    }

    // a preset's identity for favourites / hiding (names can repeat between factory and user presets)
    static std::string presetKey(const PresetEntry& pe) { return (pe.path.empty() ? "factory:" : "user:") + pe.name; }
    bool isFavourite(const PresetEntry& pe) const
    {
        return std::find(favourites.begin(), favourites.end(), presetKey(pe)) != favourites.end();
    }
    bool isHidden(const std::string& key) const
    {
        return std::find(hiddenFactory.begin(), hiddenFactory.end(), key) != hiddenFactory.end();
    }
    void toggleFavourite(const PresetEntry& pe)
    {
        const std::string k = presetKey(pe);
        auto it = std::find(favourites.begin(), favourites.end(), k);
        if (it != favourites.end()) favourites.erase(it);
        else favourites.push_back(k);
        persistPrefs();
    }

    // user presets: the file is deleted. Factory presets are built in: they are hidden instead (can be restored).
    void deletePreset(int index)
    {
        if (index < 0 || index >= static_cast<int>(presets.size()))
            return;
        const PresetEntry& pe = presets[static_cast<size_t>(index)];
        if (pe.path.empty())
        {
            hiddenFactory.push_back(presetKey(pe));
            persistPrefs();
        }
        else
        {
            std::remove(pe.path.c_str());
        }
        if (pe.name == currentName)
            currentName += " (deleted)";
        rescanPresets();
    }

    void restoreHiddenPresets()
    {
        hiddenFactory.clear();
        persistPrefs();
        rescanPresets();
    }

    bool presetModified() const
    {
        for (int i = 0; i < LVC_CONTROL_NR; ++i)
            if (std::fabs(pv[i] - presetRef[i]) > 1e-4f * std::max(1.f, std::fabs(presetRef[i])))
                return true;
        return false;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Randomizers (musically sensible ranges, each applied as one undo step)

    float rnd(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }
    float rndLog(float a, float b) { return std::exp(rnd(std::log(a), std::log(b))); }
    bool chance(float p) { return rnd(0.f, 1.f) < p; }
    int pick(const float* weights, int n)
    {
        float total = 0.f;
        for (int i = 0; i < n; ++i) total += weights[i];
        float r = rnd(0.f, total);
        for (int i = 0; i < n; ++i)
        {
            if (r < weights[i]) return i;
            r -= weights[i];
        }
        return n - 1;
    }

    void randSequence(float* v)
    {
        static const float subdivWeights[7] = { 1.f, 4.f, 2.f, 3.f, 0.4f, 1.f, 0.6f }; // 6 8 12 16 18 24 32
        v[LVC_SUBDIV] = static_cast<float>(pick(subdivWeights, 7));
        static const float minCutWeights[3] = { 6.f, 2.5f, 1.5f };
        v[LVC_MINCUT] = static_cast<float>(pick(minCutWeights, 3));
        const float mn = std::round(rnd(1.f, 2.99f));
        v[LVC_MINPHRS] = mn;
        v[LVC_MAXPHRS] = std::min(8.f, mn + std::round(rnd(0.f, 4.f)));
    }

    void randAlgorithmParams(float* v, int algo)
    {
        switch (algo)
        {
        case kAlgoCutProc11:
            v[LVC_STUTTER] = rnd(0.3f, 1.f);
            v[LVC_AREA] = rnd(0.2f, 1.f);
            v[LVC_MINREP] = std::round(rnd(0.f, 1.f));
            v[LVC_MAXREP] = std::round(rnd(v[LVC_MINREP], 3.f));
            break;
        case kAlgoWarpCut:
            v[LVC_STRAIGHT] = rnd(0.1f, 0.8f);
            v[LVC_REGULAR] = rnd(0.2f, 0.9f);
            v[LVC_RITARD] = rnd(0.2f, 0.8f);
            v[LVC_SPEED] = rnd(0.7f, 0.98f);
            break;
        case kAlgoSQPusher:
            v[LVC_ACTIVITY] = rnd(0.2f, 1.f);
            break;
        case kAlgoEuclid:
            v[LVC_EUDENSITY] = rnd(0.15f, 0.8f);
            v[LVC_EUCHANCE] = rnd(0.5f, 1.f);
            v[LVC_EUSLICE] = std::round(rnd(0.f, 4.f));
            break;
        case kAlgoGate:
            if (v[LVC_GATEMODE] > 0.5f)
            {
                // pattern mode: a new drawn pattern, denser on the beats
                const int n = std::min(LVC_GATE_MAX_STEPS, static_cast<int>(LVC_SUBDIV_OPTIONS[limit<int>(static_cast<int>(v[LVC_SUBDIV] + 0.5f), 0, 6)]));
                const float density = rnd(0.35f, 0.8f);
                uint32_t m = 0;
                for (int st = 0; st < n; ++st)
                    if (chance((st * 4) % n == 0 ? 0.85f : density))
                        m |= 1u << st;
                if (m == 0)
                    m = 1u;
                // repeat it over the steps the current subdivision does not use, so a change of SubDiv still has steps
                for (int st = n; st < LVC_GATE_MAX_STEPS; ++st)
                    if ((m >> (st % n)) & 1u)
                        m |= 1u << st;
                v[LVC_GATEPAT1] = static_cast<float>(m & 0xFFFFu);
                v[LVC_GATEPAT2] = static_cast<float>((m >> 16) & 0xFFFFu);
            }
            else
            {
                v[LVC_GATEDENSITY] = rnd(0.35f, 0.85f);
                v[LVC_GATEVARY] = rnd(0.f, 0.5f);
            }
            break;
        default:
            v[LVC_RAINTENSITY] = rnd(0.3f, 1.f);
            v[LVC_RACURVE] = rnd(0.f, 1.f);
            break;
        }
    }

    void randShape(float* v)
    {
        if (chance(0.6f)) { v[LVC_MINAMP] = 1.f; v[LVC_MAXAMP] = 1.f; }
        else { v[LVC_MINAMP] = rnd(0.5f, 1.f); v[LVC_MAXAMP] = rnd(v[LVC_MINAMP], 1.f); }
        const float width = chance(0.3f) ? 0.f : rnd(0.1f, 0.8f);
        v[LVC_MINPAN] = -width;
        v[LVC_MAXPAN] = width;
        const float pr = rnd(0.f, 1.f);
        if (pr < 0.6f) { v[LVC_MINPITCH] = 0.f; v[LVC_MAXPITCH] = 0.f; }
        else if (pr < 0.85f) { const float d = rnd(50.f, 300.f); v[LVC_MINPITCH] = -d; v[LVC_MAXPITCH] = d; }
        else { v[LVC_MINPITCH] = rnd(-1200.f, 0.f); v[LVC_MAXPITCH] = rnd(0.f, 1200.f); }
        v[LVC_DUTY] = chance(0.6f) ? 1.f : rnd(0.6f, 1.f);
        v[LVC_FILLDUTY] = chance(0.5f) ? 1.f : rnd(0.4f, 1.f);
        v[LVC_FADE] = rndLog(0.5f, 15.f);
        v[LVC_SEED] = std::round(rnd(1.f, 16.f));
    }

    void randCrusher(float* v)
    {
        v[LVC_MINBITS] = rnd(3.f, 12.f);
        v[LVC_MAXBITS] = rnd(v[LVC_MINBITS], 16.f);
        v[LVC_MINFREQ] = rndLog(2000.f, 20000.f);
        v[LVC_MAXFREQ] = rndLog(v[LVC_MINFREQ], 44100.f);
    }

    void randComb(float* v)
    {
        if (v[LVC_DLYMODE] > 0.5f)
        {
            // echo: musical note values, moderate feedback
            static const float divs[6] = { 2.f, 3.f, 5.f, 6.f, 8.f, 9.f }; // 1/16 1/16D 1/8 1/8D 1/4 1/4D
            static const float divWeights[6] = { 1.f, 1.5f, 2.f, 3.f, 2.f, 1.f };
            v[LVC_DLYTIMEL] = divs[pick(divWeights, 6)];
            v[LVC_DLYTIMER] = divs[pick(divWeights, 6)];
            static const float stereoWeights[3] = { 1.f, 1.5f, 2.5f };
            v[LVC_DLYSTEREO] = static_cast<float>(pick(stereoWeights, 3));
            v[LVC_FEEDBACK] = rnd(0.25f, 0.75f);
            v[LVC_DLYMIX] = rnd(0.2f, 0.5f);
            v[LVC_DLYTONE] = rnd(0.1f, 0.7f);
            return;
        }
        v[LVC_TYPE] = chance(0.5f) ? 1.f : 0.f;
        v[LVC_FEEDBACK] = rnd(0.2f, 0.85f);
        v[LVC_MINDELAY] = rndLog(1.f, 20.f);
        v[LVC_MAXDELAY] = rndLog(v[LVC_MINDELAY], 40.f);
    }

    void randFilter(float* v)
    {
        v[LVC_LPMIN]  = rndLog(500.f, 8000.f);
        v[LVC_LPFREQ] = rndLog(v[LVC_LPMIN], 20000.f);
        if (chance(0.5f))
        {
            v[LVC_HPFREQ] = rndLog(20.f, 150.f);
            v[LVC_HPMAX]  = rndLog(v[LVC_HPFREQ], std::min(800.f, v[LVC_LPMIN] * 0.5f));
        }
        else
        {
            v[LVC_HPFREQ] = 20.f; // lowpass only
            v[LVC_HPMAX] = 20.f;
        }
        v[LVC_RESO] = rnd(0.f, 0.7f);
    }

    void randReverb(float* v)
    {
        v[LVC_RVCHANCE] = rnd(0.12f, 0.6f);
        v[LVC_RVSENDMIN] = rnd(0.2f, 0.6f);
        v[LVC_RVSENDMAX] = rnd(v[LVC_RVSENDMIN], 1.f);
        v[LVC_RVDECAYMIN] = rndLog(0.5f, 3.f);
        v[LVC_RVDECAYMAX] = rndLog(v[LVC_RVDECAYMIN], 8.f);
        v[LVC_RVSIZE] = rnd(0.3f, 0.95f);
        v[LVC_RVDAMP] = rnd(0.2f, 0.8f);
    }

    void randMaster(float* v)
    {
        v[LVC_LIMCEIL] = rnd(-3.f, -0.1f);
        v[LVC_LIMREL] = rndLog(30.f, 400.f);
        v[LVC_MIX] = chance(0.6f) ? 1.f : rnd(0.5f, 1.f);
        v[LVC_MASTERHPFREQ] = chance(0.5f) ? rndLog(30.f, 250.f) : 20.f;
        v[LVC_MASTERLPFREQ] = chance(0.5f) ? rndLog(4000.f, 18000.f) : 20000.f;
    }

    // key, region and engine stay: they belong to the track, not to the sound
    void randChroma(float* v)
    {
        static const int scales[] = { 0, 1, 4, 5, 6, 7, 9, 10, 11, 12, 14, 16, 17, 18, 19, 21 };
        v[LVC_CHSCALE] = static_cast<float>(scales[static_cast<int>(rnd(0.f, 15.999f))]);
        v[LVC_CHCOLOR] = rnd(0.7f, 1.8f);
        static const float shifts[] = { -12.f, -7.f, -5.f, 5.f, 7.f, 12.f };
        v[LVC_CHPITCH] = chance(0.65f) ? 0.f : shifts[static_cast<int>(rnd(0.f, 5.999f))];
        v[LVC_CHGAIN] = 0.f;
        v[LVC_CHMORPH] = chance(0.25f) ? 1.f : 0.f;
        v[LVC_CHMORPHAMT] = rnd(0.2f, 0.7f);
        v[LVC_CHGATE] = chance(0.25f) ? 1.f : 0.f;
        v[LVC_CHGATETHR] = rnd(-70.f, -40.f);
    }

    enum DiceTarget { kDiceAll, kDiceSequence, kDiceAlgorithm, kDiceShape, kDiceEffects, kDiceCrusher, kDiceComb, kDiceFilter, kDiceReverb, kDiceMaster, kDiceChroma };

    void randomize(DiceTarget target)
    {
        float v[LVC_CONTROL_NR];
        std::memcpy(v, pv, sizeof(v));
        switch (target)
        {
        case kDiceAll:
        {
            // everything except arrangement / safety settings (fill mode, limiter, dry/wet)
            const int algo = static_cast<int>(rnd(0.f, kAlgoCount - 0.001f));
            selectAlgo(v, algo);
            randSequence(v);
            randAlgorithmParams(v, algo);
            randShape(v);
            v[LVC_CRUSHER] = chance(0.3f) ? 1.f : 0.f;
            v[LVC_COMB] = chance(0.25f) ? 1.f : 0.f;
            v[LVC_FILTER] = chance(0.35f) ? 1.f : 0.f;
            v[LVC_REVERB] = chance(0.25f) ? 1.f : 0.f;
            randReverb(v);
            randCrusher(v);
            randComb(v);
            randFilter(v);
            break;
        }
        case kDiceSequence:  randSequence(v); break;
        case kDiceAlgorithm: randAlgorithmParams(v, algoFromParams(pv)); break;
        case kDiceShape:     randShape(v); break;
        case kDiceEffects:   randCrusher(v); randComb(v); randFilter(v); break;
        case kDiceCrusher:   randCrusher(v); break;
        case kDiceComb:      randComb(v); break;
        case kDiceFilter:    randFilter(v); break;
        case kDiceReverb:    randReverb(v); break;
        case kDiceMaster:    randMaster(v); break;
        case kDiceChroma:    randChroma(v); break;
        }
        for (int i = 0; i < LVC_CONTROL_NR; ++i)
            v[i] = limit<float>(v[i], controlLimits[i].first, controlLimits[i].second);
        applyValues(v);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Dice widget: a die face that spins when rolled. Returns true when clicked.

    bool dice(ImDrawList* dl, const char* id, ImVec2 c, float size)
    {
        ImGui::PushID(id);
        ImGui::SetCursorScreenPos(ImVec2(c.x - size * 0.5f, c.y - size * 0.5f));
        const bool clicked = ImGui::InvisibleButton("##dice", ImVec2(size, size));
        const bool hov = ImGui::IsItemHovered();
        ImGuiStorage* st = ImGui::GetStateStorage();
        const ImGuiID kTime = ImGui::GetID("t"), kFace = ImGui::GetID("f");
        if (clicked)
        {
            st->SetFloat(kTime, static_cast<float>(ImGui::GetTime()));
            st->SetInt(kFace, 1 + static_cast<int>(rng() % 6));
        }
        ImGui::PopID();
        if (hov)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            tooltip("Randomize");
        }

        const float age = static_cast<float>(ImGui::GetTime()) - st->GetFloat(kTime, -10.f);
        const float spin = age < 0.4f ? (1.f - age / 0.4f) : 0.f;
        const float ang = spin * spin * IM_PI * 1.5f;
        const int face = spin > 0.f ? 1 + static_cast<int>(age * 30.f) % 6 : st->GetInt(kFace, 5);
        const float h = size * 0.29f * (1.f + 0.12f * spin), rr = h * 0.32f;
        const float ca = std::cos(ang), sa = std::sin(ang);
        auto R = [&](float x, float y) { return ImVec2(c.x + x * ca - y * sa, c.y + x * sa + y * ca); };

        // rounded square as a convex polygon
        ImVec2 pts[24];
        int n = 0;
        const float cx[4] = { h - rr, -(h - rr), -(h - rr), h - rr }, cy[4] = { h - rr, h - rr, -(h - rr), -(h - rr) };
        for (int k = 0; k < 4; ++k)
            for (int j = 0; j < 6; ++j)
            {
                const float t = (k * 0.5f + j / 10.f) * IM_PI;
                pts[n++] = R(cx[k] + std::cos(t) * rr, cy[k] + std::sin(t) * rr);
            }
        const bool lit = hov || spin > 0.f;
        if (lit)
        {
            const int v0 = dl->VtxBuffer.Size;
            dl->AddConvexPolyFilled(pts, n, U32(P.a, 0.9f));
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, dl->VtxBuffer.Size, ImVec2(c.x - h, c.y - h),
                                                          ImVec2(c.x + h, c.y + h), U32(P.a), U32(P.b));
        }
        else
        {
            dl->AddConvexPolyFilled(pts, n, U32(P.text, 0.06f));
        }
        dl->AddPolyline(pts, n, U32(lit ? withA(P.onAccent, 0.6f) : P.textDim), ImDrawFlags_Closed, std::max(1.f, 1.3f * s));

        static const float pipX[6][6] = { {0}, {-1, 1}, {-1, 0, 1}, {-1, 1, -1, 1}, {-1, 1, 0, -1, 1}, {-1, 1, -1, 1, -1, 1} };
        static const float pipY[6][6] = { {0}, {-1, 1}, {-1, 0, 1}, {-1, -1, 1, 1}, {-1, -1, 0, 1, 1}, {-1, -1, 0, 0, 1, 1} };
        const float off = h * 0.5f, pr = std::max(1.f, h * 0.15f);
        for (int k = 0; k < face; ++k)
            dl->AddCircleFilled(R(pipX[face - 1][k] * off, pipY[face - 1][k] * off), pr,
                                U32(lit ? P.onAccent : P.text, lit ? 1.f : 0.85f), 10);
        return clicked;
    }

    // small die in the top-right corner of a panel
    bool panelDice(ImDrawList* dl, const char* id, ImVec2 panelA, ImVec2 panelB)
    {
        return dice(dl, id, ImVec2(panelB.x - 26.f * s, panelA.y + 21.f * s), 22.f * s);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Algorithm identity: color + glyph

    ImVec4 algoColor(int algo) const
    {
        algo = std::max(0, std::min(kNumAlgos - 1, algo));
        return hex(P.dark ? kAlgoColorsDark[algo] : kAlgoColorsLight[algo]);
    }

    // color for a fractional position along the selector (animates between neighbours)
    ImVec4 algoColorAt(float pos) const
    {
        const int i0 = std::max(0, std::min(kNumAlgos - 1, static_cast<int>(std::floor(pos))));
        const int i1 = std::min(kNumAlgos - 1, i0 + 1);
        return mix(algoColor(i0), algoColor(i1), std::max(0.f, std::min(1.f, pos - i0)));
    }

    // glyph centered on c, fitting in a (2r x 2r) box
    void drawAlgoIcon(ImDrawList* dl, int algo, ImVec2 c, float r, ImU32 col)
    {
        const float th = std::max(1.f, r * 0.2f);
        auto P2 = [&](float x, float y) { return ImVec2(c.x + x * r, c.y + y * r); };
        switch (algo)
        {
        case kAlgoCutProc11: // scissors
        {
            const float rr = 0.27f;
            dl->AddCircle(P2(-0.48f, 0.62f), rr * r, col, 14, th);
            dl->AddCircle(P2(0.48f, 0.62f), rr * r, col, 14, th);
            dl->AddLine(P2(-0.33f, 0.40f), P2(0.55f, -0.95f), col, th);
            dl->AddLine(P2(0.33f, 0.40f), P2(-0.55f, -0.95f), col, th);
            dl->AddCircleFilled(P2(0.f, -0.13f), th * 0.7f, col, 8);
            break;
        }
        case kAlgoWarpCut: // accelerating roll: slices getting closer together
        {
            static const float xs[6] = { -0.92f, -0.36f, 0.08f, 0.42f, 0.66f, 0.84f };
            for (int k = 0; k < 6; ++k)
            {
                const float hh = 0.85f - k * 0.07f;
                dl->AddLine(P2(xs[k], -hh), P2(xs[k], hh), col, th * (k < 3 ? 1.1f : 0.85f));
            }
            break;
        }
        case kAlgoSQPusher: // irregular square wave
        {
            const ImVec2 pts[] = { P2(-1.f, 0.5f), P2(-0.55f, 0.5f), P2(-0.55f, -0.6f), P2(-0.1f, -0.6f), P2(-0.1f, 0.5f),
                                   P2(0.18f, 0.5f), P2(0.18f, -0.6f), P2(0.45f, -0.6f), P2(0.45f, 0.5f), P2(1.f, 0.5f) };
            dl->AddPolyline(pts, IM_ARRAYSIZE(pts), col, 0, th);
            break;
        }
        case kAlgoEuclid: // euclidean necklace E(3,8)
        {
            const float rad = 0.72f;
            dl->AddCircle(c, rad * r, (col & ~IM_COL32_A_MASK) | (((col >> IM_COL32_A_SHIFT) & 0xFF) / 3 << IM_COL32_A_SHIFT),
                          24, std::max(1.f, th * 0.6f));
            for (int k = 0; k < 8; ++k)
            {
                const bool hit = k == 0 || k == 3 || k == 6;
                const float ang = -IM_PI * 0.5f + k * IM_PI / 4.f;
                const ImVec2 p(c.x + std::cos(ang) * rad * r, c.y + std::sin(ang) * rad * r);
                dl->AddCircleFilled(p, hit ? r * 0.26f : r * 0.1f, col, 10);
            }
            break;
        }
        case kAlgoGate: // gated pulses with a gap
        {
            static const float xs[5][2] = { { -1.f, -0.62f }, { -0.42f, -0.04f }, { 0.16f, 0.2f }, { 0.4f, 0.62f }, { 0.78f, 1.f } };
            static const bool open[5] = { true, true, false, true, true };
            for (int k = 0; k < 5; ++k)
            {
                if (open[k])
                    dl->AddRectFilled(P2(xs[k][0], -0.7f), P2(xs[k][1], 0.7f), col, r * 0.12f);
                else
                    dl->AddLine(P2(xs[k][0] - 0.12f, 0.7f), P2(xs[k][1] + 0.12f, 0.7f), col, th * 0.8f);
            }
            break;
        }
        case kAlgoRatchet: // growing saw teeth (build-up)
        default:
        {
            const ImVec2 pts[] = { P2(-1.f, 0.7f), P2(-0.4f, 0.25f), P2(-0.4f, 0.7f), P2(0.2f, -0.2f), P2(0.2f, 0.7f),
                                   P2(0.85f, -0.85f), P2(0.85f, 0.7f) };
            dl->AddPolyline(pts, IM_ARRAYSIZE(pts), col, 0, th);
            break;
        }
        }
    }


    // small flag drawn into [a, b]: Spain (red-yellow-red, 1:2:1) or the United Kingdom (Union Jack) for English
    void drawFlag(ImDrawList* dl, int lang, ImVec2 a, ImVec2 b, float alpha) const
    {
        auto col = [&](uint32_t rgb) { return U32(hex(rgb), alpha); };
        const float w = b.x - a.x, h = b.y - a.y;
        dl->PushClipRect(a, b, true);
        if (lang == kLangEs)
        {
            dl->AddRectFilled(a, b, col(0xAA151B));
            dl->AddRectFilled(ImVec2(a.x, a.y + h * 0.25f), ImVec2(b.x, a.y + h * 0.75f), col(0xF1BF00));
        }
        else
        {
            const ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
            dl->AddRectFilled(a, b, col(0x012169));
            // saltires: white, then the thinner red
            dl->AddLine(a, b, col(0xFFFFFF), h * 0.30f);
            dl->AddLine(ImVec2(a.x, b.y), ImVec2(b.x, a.y), col(0xFFFFFF), h * 0.30f);
            dl->AddLine(a, b, col(0xC8102E), h * 0.10f);
            dl->AddLine(ImVec2(a.x, b.y), ImVec2(b.x, a.y), col(0xC8102E), h * 0.10f);
            // cross: white border, red centre
            dl->AddRectFilled(ImVec2(a.x, c.y - h * 0.18f), ImVec2(b.x, c.y + h * 0.18f), col(0xFFFFFF));
            dl->AddRectFilled(ImVec2(c.x - h * 0.18f, a.y), ImVec2(c.x + h * 0.18f, b.y), col(0xFFFFFF));
            dl->AddRectFilled(ImVec2(a.x, c.y - h * 0.10f), ImVec2(b.x, c.y + h * 0.10f), col(0xC8102E));
            dl->AddRectFilled(ImVec2(c.x - h * 0.10f, a.y), ImVec2(c.x + h * 0.10f, b.y), col(0xC8102E));
        }
        dl->PopClipRect();
        (void)w;
        dl->AddRect(a, b, U32(P.shadow, 0.5f * alpha), 1.5f * s, 0, 1.f);
    }

    void persistPrefs()
    {
        UIPrefs p;
        p.theme = themeIndex;
        p.dark = darkMode;
        p.width = lastSavedW;
        p.height = lastSavedH;
        p.author = authorName;
        p.crt = crtEnabled;
        p.lang = uiLang();
        p.favourites = favourites;
        p.hidden = hiddenFactory;
        savePrefs(p);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Parameter helpers

    void setParam(uint32_t i, float v)
    {
        v = limit<float>(v, controlLimits[i].first, controlLimits[i].second);
        if (v == pv[i])
            return;
        pv[i] = v;
        touched[i] = true;
        setParameterValue(i, v);
    }

    void setParamGesture(uint32_t i, float v)
    {
        editParameter(i, true);
        setParam(i, v);
        editParameter(i, false);
    }

    // the gate has its own switch; the other algorithms are values of LVC_CUTPROC
    void selectAlgo(int algo)
    {
        if (algo == kAlgoGate)
        {
            setParamGesture(LVC_GATE, 1.f);
            return;
        }
        setParamGesture(LVC_GATE, 0.f);
        setParamGesture(LVC_CUTPROC, paramFromAlgo(algo));
    }

    static void selectAlgo(float* v, int algo)
    {
        v[LVC_GATE] = algo == kAlgoGate ? 1.f : 0.f;
        if (algo != kAlgoGate)
            v[LVC_CUTPROC] = paramFromAlgo(algo);
    }

    // ---- trance gate pattern ----

    int gateSteps() const
    {
        return static_cast<int>(LVC_SUBDIV_OPTIONS[limit<int>(static_cast<int>(pv[LVC_SUBDIV] + 0.5f), 0, 6)]);
    }

    uint32_t gateDrawn() const { return gatePatternFromParams(pv[LVC_GATEPAT1], pv[LVC_GATEPAT2]); }

    // what the steps look like now: the drawn pattern, or in random mode the bar the gate is playing
    uint32_t gateShown() const
    {
        if (pv[LVC_GATEMODE] > 0.5f || vis == nullptr)
            return gateDrawn();
        return vis->gateMask.load(std::memory_order_relaxed);
    }

    // store a drawn pattern; drawing always switches the gate to pattern mode (random off)
    void gateSetPattern(uint32_t mask)
    {
        setParamGesture(LVC_GATEPAT1, static_cast<float>(mask & 0xFFFFu));
        setParamGesture(LVC_GATEPAT2, static_cast<float>((mask >> 16) & 0xFFFFu));
        setParamGesture(LVC_GATEMODE, 1.f);
    }

    // click / drag painting of steps, shared by the LIVE CUTS view and the algorithm panel.
    // The first step clicked decides whether the drag opens or mutes; the whole drag is one undo step.
    uint32_t gateTiesDrawn() const { return gatePatternFromParams(pv[LVC_GATETIE1], pv[LVC_GATETIE2]); }

    // ties in effect: only the drawn pattern has ties
    uint32_t gateTiesShown() const { return pv[LVC_GATEMODE] > 0.5f ? gateTiesDrawn() : 0u; }

    void gateSetTies(uint32_t t)
    {
        setParamGesture(LVC_GATETIE1, static_cast<float>(t & 0xFFFFu));
        setParamGesture(LVC_GATETIE2, static_cast<float>((t >> 16) & 0xFFFFu));
    }

    // Painting. A click opens / mutes one step on its own (muting breaks its ties). Dragging on from there paints
    // every step passed over; when opening, the painted steps are tied into one continuous slice.
    void gatePaint(int step, bool pressed)
    {
        if (step < 0 || step >= LVC_GATE_MAX_STEPS)
            return;
        uint32_t m = gateShown();
        uint32_t ties = gateTiesShown();
        const uint32_t m0 = m, t0 = ties;
        if (pressed)
        {
            gatePaintValue = ((m >> step) & 1u) ? 0 : 1;
            gatePaintLast = step;
            if (gatePaintValue) m |= 1u << step;
            else
            {
                m &= ~(1u << step);
                ties &= ~(1u << step);
                if (step > 0) ties &= ~(1u << (step - 1));
            }
        }
        else
        {
            if (gatePaintValue < 0 || step == gatePaintLast)
                return;
            const int lo = std::min(step, gatePaintLast), hi = std::max(step, gatePaintLast);
            for (int k = lo; k <= hi; ++k)
            {
                if (gatePaintValue)
                {
                    m |= 1u << k;
                    if (k < hi) ties |= 1u << k; // run on into the next painted step
                }
                else
                {
                    m &= ~(1u << k);
                    ties &= ~(1u << k);
                    if (k > 0) ties &= ~(1u << (k - 1));
                }
            }
            gatePaintLast = step;
        }
        if (m != m0 || pv[LVC_GATEMODE] <= 0.5f)
            gateSetPattern(m);
        if (ties != t0)
            gateSetTies(ties);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Fonts

    // layout scale from the current window size (preset browser is a dropdown popup now)
    float computeScale(float /*sideVisible*/ = 0.f) const
    {
        const float w = static_cast<float>(getWidth());
        const float h = static_cast<float>(getHeight());
        return std::max(0.5f, std::min(4.f, std::min(w / kBaseW, h / kBaseH)));
    }

    void buildFonts(float scale)
    {
        static const ImWchar ranges[] = { 0x0020, 0x00FF, 0x2013, 0x2026, 0 };

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();

        auto add = [&](const unsigned char* data, unsigned int size, float px) -> ImFont* {
            ImFontConfig cfg;
            cfg.FontDataOwnedByAtlas = false;
            cfg.OversampleH = 3;
            cfg.OversampleV = 1;
            cfg.PixelSnapH = false;
            return io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(data), static_cast<int>(size),
                                                  std::round(px * scale), &cfg, ranges);
        };

        fBody  = add(NotoSansMedium_data, NotoSansMedium_size, 13.5f);
        fSmall = add(NotoSansMedium_data, NotoSansMedium_size, 11.f);
        fBold  = add(NotoSansBold_data,   NotoSansBold_size,   12.f);
        fTitle = add(NotoSansBold_data,   NotoSansBold_size,   24.f);
        io.Fonts->Build();
        io.FontDefault = fBody;

        fontBuiltScale = scale;
        fontAdj = 1.f;
    }

    // runs before the ImGui frame is started, with the GL context current
    void onDisplay() override
    {
        if (fontRebuildPending && context != nullptr)
        {
            fontRebuildPending = false;
            ImGui::SetCurrentContext(context);
           #if defined(DGL_USE_GLES2) || defined(DGL_USE_GLES3) || defined(DGL_USE_OPENGL3)
            ImGui_ImplOpenGL3_DestroyFontsTexture();
            buildFonts(fontWantedScale);
            ImGui_ImplOpenGL3_CreateFontsTexture();
           #else
            ImGui_ImplOpenGL2_DestroyFontsTexture();
            buildFonts(fontWantedScale);
            ImGui_ImplOpenGL2_CreateFontsTexture();
           #endif
        }
        UI::onDisplay();
    }

    void applyImGuiStyle()
    {
        ImGuiStyle& st = ImGui::GetStyle();
        st.WindowPadding = ImVec2(0, 0);
        st.WindowBorderSize = 0.f;
        st.PopupRounding = 12.f * s;
        st.PopupBorderSize = 1.f;
        st.AntiAliasedLines = true;
        st.AntiAliasedFill = true;
        st.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
        st.Colors[ImGuiCol_PopupBg] = P.dark ? hex(0x1A1E2D, 0.98f) : hex(0xFFFFFF, 0.98f);
        st.Colors[ImGuiCol_Border] = P.panelBorder;
        st.Colors[ImGuiCol_Text] = P.text;
        st.Colors[ImGuiCol_NavHighlight] = ImVec4(0, 0, 0, 0);
        st.Colors[ImGuiCol_FrameBg] = P.inset;
        st.Colors[ImGuiCol_FrameBgHovered] = mix(P.inset, P.a, 0.12f);
        st.Colors[ImGuiCol_FrameBgActive] = mix(P.inset, P.a, 0.18f);
        st.Colors[ImGuiCol_TextSelectedBg] = withA(P.a, 0.4f);
        st.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        st.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        st.Colors[ImGuiCol_ScrollbarGrab] = withA(P.text, 0.15f);
        st.Colors[ImGuiCol_ScrollbarGrabHovered] = withA(P.text, 0.25f);
        st.Colors[ImGuiCol_ScrollbarGrabActive] = withA(P.a, 0.6f);
        st.ScrollbarSize = 8.f * s;
        st.ScrollbarRounding = 4.f * s;
        st.WindowRounding = 8.f * s;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Drawing primitives

    float fs(ImFont* f) const { return f->FontSize * fontAdj; }

    // both translate (see LiveCutLang.hpp), so labels are measured and drawn in the current language
    ImVec2 textSize(ImFont* f, const char* t) const
    {
        return f->CalcTextSizeA(fs(f), FLT_MAX, 0.f, tr(t));
    }

    void text(ImDrawList* dl, ImFont* f, ImVec2 pos, ImU32 col, const char* t)
    {
        dl->AddText(f, fs(f), ImVec2(std::round(pos.x), std::round(pos.y)), col, tr(t));
    }

    // ImGui::SetTooltip in the current language ("%s" tooltips translate their argument)
    static void tooltip(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        if (std::strcmp(fmt, "%s") == 0)
        {
            const char* arg = va_arg(ap, const char*);
            va_end(ap);
            ImGui::SetTooltip("%s", tr(arg));
            return;
        }
        ImGui::SetTooltipV(tr(fmt), ap);
        va_end(ap);
    }

    void textCentered(ImDrawList* dl, ImFont* f, ImVec2 center, ImU32 col, const char* t)
    {
        const ImVec2 sz = textSize(f, t);
        text(dl, f, ImVec2(center.x - sz.x * 0.5f, center.y - sz.y * 0.5f), col, t);
    }

    // Wordmark: "HYPER" takes the six-stripe palette (one colour per letter), "CUT" follows the theme
    void logoText(ImDrawList* dl, ImFont* f, ImVec2 pos, ImU32 cutCol) const
    {
        static const char hyper[6] = "HYPER";
        static const char cut[4]   = "CUT";
        const float size = fs(f), scale = size / f->FontSize;
        float x = pos.x;
        for (int i = 0; i < 5; ++i)
        {
            char ch[2] = { hyper[i], '\0' };
            dl->AddText(f, size, ImVec2(x, pos.y), U32(kLogoRainbow[i]), ch);
            x += f->GetCharAdvance(static_cast<ImWchar>(static_cast<unsigned char>(hyper[i]))) * scale;
        }
        for (int i = 0; i < 3; ++i)
        {
            char ch[2] = { cut[i], '\0' };
            dl->AddText(f, size, ImVec2(x, pos.y), cutCol, ch);
            x += f->GetCharAdvance(static_cast<ImWchar>(static_cast<unsigned char>(cut[i]))) * scale;
        }
    }

    void textRight(ImDrawList* dl, ImFont* f, ImVec2 rightTop, ImU32 col, const char* t)
    {
        const ImVec2 sz = textSize(f, t);
        text(dl, f, ImVec2(rightTop.x - sz.x, rightTop.y), col, t);
    }

    static void gradRect(ImDrawList* dl, ImVec2 a, ImVec2 b, const ImVec4& c0, const ImVec4& c1,
                         float rounding, bool horizontal = false, ImDrawFlags flags = 0)
    {
        const int v0 = dl->VtxBuffer.Size;
        dl->AddRectFilled(a, b, U32(c0), rounding, flags);
        const int v1 = dl->VtxBuffer.Size;
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, v1, a, horizontal ? ImVec2(b.x, a.y) : ImVec2(a.x, b.y),
                                                      U32(withA(c0, 1.f)), U32(withA(c1, 1.f)));
    }

    // Full multi-stop sweep (used by the rainbow theme); falls back to a plain two-colour blend.
    void gradRectTheme(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding) const
    {
        if (P.numStops <= 2)
        {
            gradRect(dl, a, b, P.gradStops[0], P.gradStops[1], rounding, true);
            return;
        }
        const float w = b.x - a.x, h = b.y - a.y;
        if (w <= 0.f || h <= 0.f) return;
        const float rad = std::min(rounding, std::min(w, h) * 0.5f);

        // Draw one properly rounded shape first, so capsule ends keep their silhouette.
        const int v0 = dl->VtxBuffer.Size;
        dl->AddRectFilled(a, b, U32(P.gradStops[0]), rad);
        const int v1 = dl->VtxBuffer.Size;
        for (int i = v0; i < v1; ++i)
        {
            ImDrawVert& v = dl->VtxBuffer[i];
            v.col = U32(P.grad((v.pos.x - a.x) / w));
        }

        // Refine the straight middle into per-stop slices; the arc ends already carry
        // enough vertices for a smooth sweep on their own.
        const float ml = a.x + rad, mr = b.x - rad;
        if (mr > ml)
        {
            const int segs = P.numStops - 1;
            for (int i = 0; i < segs; ++i)
            {
                const ImVec2 s0(ml + (mr - ml) * i / segs, a.y);
                const ImVec2 s1(ml + (mr - ml) * (i + 1) / segs, b.y);
                gradRect(dl, s0, s1, P.grad((s0.x - a.x) / w), P.grad((s1.x - a.x) / w), 0.f, true);
            }
        }
    }

    static void gradCircle(ImDrawList* dl, ImVec2 c, float r, const ImVec4& top, const ImVec4& bot)
    {
        const int v0 = dl->VtxBuffer.Size;
        dl->AddCircleFilled(c, r, U32(top), 48);
        const int v1 = dl->VtxBuffer.Size;
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, v1, ImVec2(c.x, c.y - r), ImVec2(c.x, c.y + r),
                                                      U32(withA(top, 1.f)), U32(withA(bot, 1.f)));
    }

    void glow(ImDrawList* dl, ImVec2 a, ImVec2 b, const ImVec4& col, float rounding, float strength = 1.f)
    {
        for (int i = 1; i <= 6; ++i)
        {
            const float g = i * 2.f * s;
            dl->AddRectFilled(ImVec2(a.x - g, a.y - g), ImVec2(b.x + g, b.y + g),
                              U32(col, 0.045f * strength * (7 - i) / 6.f), rounding + g);
        }
    }

    // panel with title; returns content top-left y
    float panel(ImDrawList* dl, ImVec2 a, ImVec2 b, const char* title, const char* subtitle = nullptr, int subAlgo = -1)
    {
        // drop shadow
        for (int i = 1; i <= 4; ++i)
        {
            const float g = i * 2.5f * s;
            dl->AddRectFilled(ImVec2(a.x - g * 0.5f, a.y + g * 0.6f), ImVec2(b.x + g * 0.5f, b.y + g),
                              U32(P.shadow, 0.10f), 14.f * s + g);
        }
        gradRect(dl, a, b, P.panelTop, P.panelBot, 14.f * s);
        dl->AddRect(a, b, U32(P.panelBorder), 14.f * s, 0, 1.f);
        // top sheen
        dl->AddLine(ImVec2(a.x + 14.f * s, a.y + 1.f), ImVec2(b.x - 14.f * s, a.y + 1.f),
                    U32(P.dark ? hex(0xFFFFFF, 0.06f) : hex(0xFFFFFF, 0.9f)), 1.f);

        if (title != nullptr)
        {
            const float x = a.x + 16.f * s, y = a.y + 14.f * s;
            const float dotR = 3.f * s;
            const ImVec2 dc(x + dotR, y + fs(fBold) * 0.5f + 1.f * s);
            dl->AddCircleFilled(dc, dotR + 2.f * s, U32(P.a, 0.18f), 16);
            dl->AddCircleFilled(dc, dotR, U32(P.grad(0.3f)), 16);
            text(dl, fBold, ImVec2(x + dotR * 2.f + 8.f * s, y), U32(P.textDim), title);
            if (subtitle != nullptr)
            {
                float tx = x + dotR * 2.f + 8.f * s + textSize(fBold, title).x + 8.f * s;
                if (subAlgo >= 0)
                {
                    const ImVec4 ac = algoColor(subAlgo);
                    drawAlgoIcon(dl, subAlgo, ImVec2(tx + 6.f * s, y + fs(fBold) * 0.5f + 1.f * s), 6.f * s, U32(ac));
                    text(dl, fBold, ImVec2(tx + 17.f * s, y), U32(ac), subtitle);
                }
                else
                {
                    text(dl, fBold, ImVec2(tx, y), U32(P.grad(0.5f)), subtitle);
                }
            }
        }
        return a.y + 44.f * s;
    }

    void inset(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding)
    {
        dl->AddRectFilled(a, b, U32(P.inset), rounding);
        dl->AddRect(a, b, U32(P.insetBorder), rounding, 0, 1.f);
    }

    void chip(ImDrawList* dl, ImVec2 rightTop, const char* label, const ImVec4& col, float& outLeft)
    {
        const ImVec2 sz = textSize(fSmall, label);
        const float padX = 8.f * s, h = sz.y + 6.f * s;
        const ImVec2 a(rightTop.x - sz.x - padX * 2.f, rightTop.y);
        const ImVec2 b(rightTop.x, rightTop.y + h);
        dl->AddRectFilled(a, b, U32(col, 0.14f), h * 0.5f);
        dl->AddRect(a, b, U32(col, 0.30f), h * 0.5f, 0, 1.f);
        text(dl, fSmall, ImVec2(a.x + padX, a.y + 3.f * s), U32(col), label);
        outLeft = a.x - 6.f * s;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Widgets

    // Rotary knob. Drag vertically (shift = fine), wheel, double-click resets.
    // filled ring segment between radii r0 and r1, angles a0..a1 (a triangle strip, so any angle works)
    static void arcBand(ImDrawList* dl, ImVec2 c, float r0, float r1, float a0, float a1, ImU32 col)
    {
        const int segs = std::max(2, static_cast<int>((a1 - a0) / (IM_PI * 2.f) * 64.f) + 2);
        const ImVec2 uv = dl->_Data->TexUvWhitePixel;
        dl->PrimReserve(segs * 6, (segs + 1) * 2);
        const ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
        for (int i = 0; i <= segs; ++i)
        {
            const float a = a0 + (a1 - a0) * i / segs;
            const float ca = std::cos(a), sa = std::sin(a);
            dl->PrimWriteVtx(ImVec2(c.x + ca * r1, c.y + sa * r1), uv, col);
            dl->PrimWriteVtx(ImVec2(c.x + ca * r0, c.y + sa * r0), uv, col);
        }
        for (int i = 0; i < segs; ++i)
        {
            const ImDrawIdx q = static_cast<ImDrawIdx>(base + i * 2);
            dl->PrimWriteIdx(q); dl->PrimWriteIdx(static_cast<ImDrawIdx>(q + 1)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(q + 2));
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(q + 1)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(q + 3)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(q + 2));
        }
    }

    // modNorm >= 0: modulation drawn on the knob (0..1 of the knob's travel: how far it is pulled now); liveText replaces the value
    // text while the knob is not hovered or dragged
    void knob(ImDrawList* dl, uint32_t idx, const char* label, ImVec2 center, float radius, bool enabled = true,
              bool bipolar = false, float hitWidth = 0.f, float modNorm = -1.f, const char* liveText = nullptr)
    {
        ImGui::PushID(static_cast<int>(idx));
        const float hitW = hitWidth > 0.f ? std::max(radius * 2.f, hitWidth) : std::max(radius * 2.f, 56.f * s);
        const ImVec2 hitA(center.x - hitW * 0.5f, center.y - radius);
        ImGui::SetCursorScreenPos(hitA);
        ImGui::InvisibleButton("##knob", ImVec2(hitW, radius * 2.f + 32.f * s));

        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsItemActivated())
        {
            editParameter(idx, true);
            dragNorm = toNorm(idx, pv[idx]);
        }

        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            setParam(idx, LVC_DEFAULTS[idx]);
            dragNorm = toNorm(idx, pv[idx]);
        }
        else if (active && (io.MouseDelta.y != 0.f || io.MouseDelta.x != 0.f))
        {
            const float sens = (io.KeyShift ? 0.0008f : 0.0045f) / s;
            dragNorm = std::max(0.f, std::min(1.f, dragNorm + (io.MouseDelta.x - io.MouseDelta.y) * sens));
            setParam(idx, fromNorm(idx, dragNorm));
        }
        else if (hovered && ! active && io.MouseWheel != 0.f)
        {
            const ParamSpec sp = specFor(idx);
            float n = toNorm(idx, pv[idx]);
            wheelEdit = true;
            if (sp.map == kMapInt)
            {
                const float range = controlLimits[idx].second - controlLimits[idx].first;
                setParamGesture(idx, pv[idx] + (io.MouseWheel > 0.f ? 1.f : -1.f) * (range > 0.f ? 1.f : 0.f));
            }
            else
            {
                n += io.MouseWheel * (io.KeyShift ? 0.005f : 0.03f);
                setParamGesture(idx, fromNorm(idx, n));
            }
        }

        if (ImGui::IsItemDeactivated())
            editParameter(idx, false);

        // ---- drawing ----
        const float alpha = enabled ? 1.f : 0.4f;
        const float norm = toNorm(idx, pv[idx]);
        const float a0 = IM_PI * 0.75f, a1 = IM_PI * 2.25f;
        const float av = a0 + (a1 - a0) * norm;
        const float trackR = radius;
        const float thick = std::max(2.f, 3.5f * s);

        // track
        dl->PathArcTo(center, trackR, a0, a1, 48);
        dl->PathStroke(U32(P.track, alpha), 0, thick);

        // value arc (gradient left -> right)
        float vs = a0, ve = av;
        if (bipolar)
        {
            const float mid = (a0 + a1) * 0.5f;
            vs = std::min(mid, av);
            ve = std::max(mid, av);
        }
        if (ve - vs > 0.01f)
        {
            if (hovered || active)
            {
                const int g0 = dl->VtxBuffer.Size;
                dl->PathArcTo(center, trackR, vs, ve, 40);
                dl->PathStroke(U32(P.a, 0.22f * alpha), 0, thick * 3.f);
                ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, g0, dl->VtxBuffer.Size,
                    ImVec2(center.x - radius, center.y), ImVec2(center.x + radius, center.y), U32(P.a), U32(P.b));
            }
            const int v0 = dl->VtxBuffer.Size;
            dl->PathArcTo(center, trackR, vs, ve, 40);
            dl->PathStroke(U32(P.a, alpha), 0, thick);
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, v0, dl->VtxBuffer.Size,
                ImVec2(center.x - radius, center.y), ImVec2(center.x + radius, center.y), U32(P.a), U32(P.b));
        }

        // modulation: the reachable range (0 .. the knob's setting) as a soft glowing band, the part applied right now
        // as a brighter band with bloom, and a glowing point riding on it
        if (modNorm >= 0.f)
        {
            const float rOut = trackR + thick * 0.55f, rIn = trackR - thick * 2.7f, rMid = (rOut + rIn) * 0.5f;
            const float am = a0 + (a1 - a0) * std::min(1.f, modNorm);
            const ImVec4 mc = mix(P.c, P.text, P.dark ? 0.15f : 0.f);
            if (av - a0 > 0.01f)
            {
                // range: faint halo + band
                arcBand(dl, center, rIn - thick * 1.0f, rOut + thick * 1.0f, a0, av, U32(mc, 0.09f * alpha));
                arcBand(dl, center, rIn, rOut, a0, av, U32(mc, 0.22f * alpha));
            }
            if (am - a0 > 0.01f)
            {
                // live: filled band, then bloom layers from wide and faint to a hot core
                arcBand(dl, center, rIn, rOut, a0, am, U32(mc, 0.55f * alpha));
                static const float widths[4] = { 5.2f, 3.3f, 1.9f, 0.95f };
                static const float alphas[4] = { 0.11f, 0.19f, 0.36f, 1.0f };
                for (int k = 0; k < 4; ++k)
                {
                    dl->PathArcTo(center, rMid, a0, am, 40);
                    dl->PathStroke(U32(k == 3 ? mix(mc, P.text, 0.35f) : mc, alphas[k] * alpha), 0, thick * widths[k]);
                }
            }
            // glowing point (only when the knob has some depth)
            if (av - a0 > 0.01f)
            {
                const ImVec2 mp(center.x + std::cos(am) * rMid, center.y + std::sin(am) * rMid);
                for (int k = 0; k < 4; ++k)
                    dl->AddCircleFilled(mp, thick * (3.0f - 0.6f * k), U32(mc, (0.10f + 0.14f * k) * alpha), 16);
                dl->AddCircleFilled(mp, thick * 0.65f, U32(mix(mc, P.text, 0.6f), alpha), 12);
            }
        }

        // body
        const float bodyR = radius * 0.70f;
        dl->AddCircleFilled(ImVec2(center.x, center.y + 2.f * s), bodyR + 1.f * s, U32(P.shadow, 0.8f * alpha), 40);
        gradCircle(dl, center, bodyR, withA(P.knobTop, alpha), withA(P.knobBot, alpha));
        dl->AddCircle(center, bodyR, U32(P.knobRim, alpha), 40, 1.f);

        // pointer
        const ImVec2 dir(std::cos(av), std::sin(av));
        const ImVec2 p0(center.x + dir.x * bodyR * 0.30f, center.y + dir.y * bodyR * 0.30f);
        const ImVec2 p1(center.x + dir.x * bodyR * 0.82f, center.y + dir.y * bodyR * 0.82f);
        dl->AddLine(p0, p1, U32(P.text, alpha), std::max(1.5f, 2.2f * s));

        // end dot on arc
        const ImVec2 dot(center.x + dir.x * trackR, center.y + dir.y * trackR);
        dl->AddCircleFilled(dot, thick * 0.9f, U32(mix(P.a, P.b, norm), alpha), 16);
        if (active)
            dl->AddCircleFilled(dot, thick * 2.0f, U32(P.b, 0.25f), 16);

        // label + value
        char buf[32];
        formatValue(idx, pv[idx], buf, sizeof(buf));
        const bool showLive = liveText != nullptr && ! active && ! hovered;
        if (showLive)
            std::snprintf(buf, sizeof(buf), "%s", liveText);
        const float ly = center.y + radius + 7.f * s;
        textCentered(dl, fSmall, ImVec2(center.x, ly + fs(fSmall) * 0.5f), U32(P.textDim, alpha), label);
        textCentered(dl, fSmall, ImVec2(center.x, ly + fs(fSmall) * 1.55f),
                     (active || hovered) ? U32(P.grad(0.6f)) : (showLive ? U32(P.c, alpha) : U32(P.text, 0.85f * alpha)), buf);

        if (hovered || active)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        ImGui::PopID();
    }

    // Horizontal dual-handle range slider for a min/max parameter pair.
    // multiply the alpha of everything drawn since vertex v0 (used to dim disabled controls)
    static void fadeVerts(ImDrawList* dl, int v0, float factor)
    {
        for (int i = v0; i < dl->VtxBuffer.Size; ++i)
        {
            ImU32& c = dl->VtxBuffer[i].col;
            const ImU32 a = static_cast<ImU32>(((c >> IM_COL32_A_SHIFT) & 0xFF) * factor);
            c = (c & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
        }
    }

    void rangeSlider(ImDrawList* dl, uint32_t idxMin, uint32_t idxMax, const char* label, ImVec2 a, float width,
                     bool bipolar = false, bool enabled = true)
    {
        const int fadeFrom = dl->VtxBuffer.Size;
        struct FadeGuard {
            ImDrawList* dl; int v0; bool on;
            ~FadeGuard() { if (on) fadeVerts(dl, v0, 0.45f); }
        } fadeGuard { dl, fadeFrom, ! enabled };
        ImGui::PushID(static_cast<int>(idxMin * 100 + idxMax));
        const float labelH = fs(fSmall) + 6.f * s;
        const float trackY = a.y + labelH + 9.f * s;
        const float handleR = 7.f * s;
        const float x0 = a.x + handleR, x1 = a.x + width - handleR;

        // label row
        char bmin[24], bmax[24], val[64];
        formatValue(idxMin, pv[idxMin], bmin, sizeof(bmin));
        formatValue(idxMax, pv[idxMax], bmax, sizeof(bmax));
        if (std::strcmp(bmin, bmax) == 0)
            std::snprintf(val, sizeof(val), "%s", bmin);
        else
            std::snprintf(val, sizeof(val), "%s  \xe2\x80\x93  %s", bmin, bmax); // en dash (UTF-8)

        ImGui::SetCursorScreenPos(ImVec2(a.x, trackY - 11.f * s));
        ImGui::InvisibleButton("##range", ImVec2(width, 22.f * s));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        const ImGuiIO& io = ImGui::GetIO();

        float nMin = toNorm(idxMin, pv[idxMin]);
        float nMax = toNorm(idxMax, pv[idxMax]);

        auto mouseNorm = [&]() { return std::max(0.f, std::min(1.f, (io.MousePos.x - x0) / (x1 - x0))); };

        if (ImGui::IsItemActivated())
        {
            const float m = mouseNorm();
            const float lo = std::min(nMin, nMax), hi = std::max(nMin, nMax);
            if (std::fabs(hi - lo) < 1e-4f)
                rangeHandle = (m < lo) ? 0 : 1;
            else
                rangeHandle = (std::fabs(m - nMin) <= std::fabs(m - nMax)) ? 0 : 1;
            editParameter(idxMin, true);
            editParameter(idxMax, true);
        }

        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            setParam(idxMin, LVC_DEFAULTS[idxMin]);
            setParam(idxMax, LVC_DEFAULTS[idxMax]);
        }
        else if (active)
        {
            const float m = mouseNorm();
            if (rangeHandle == 0)
                setParam(idxMin, fromNorm(idxMin, std::min(m, nMax)));
            else
                setParam(idxMax, fromNorm(idxMax, std::max(m, nMin)));
        }

        if (ImGui::IsItemDeactivated())
        {
            editParameter(idxMin, false);
            editParameter(idxMax, false);
        }

        nMin = toNorm(idxMin, pv[idxMin]);
        nMax = toNorm(idxMax, pv[idxMax]);

        // label + value text
        text(dl, fSmall, a, U32(P.textDim), label);
        textRight(dl, fSmall, ImVec2(a.x + width, a.y), active || hovered ? U32(P.grad(0.6f)) : U32(P.text, 0.85f), val);

        // track
        const float th = 4.f * s;
        dl->AddRectFilled(ImVec2(x0 - handleR * 0.5f, trackY - th * 0.5f), ImVec2(x1 + handleR * 0.5f, trackY + th * 0.5f),
                          U32(P.track), th);

        // integer tick marks
        if (specFor(idxMin).map == kMapInt)
        {
            const int steps = static_cast<int>(controlLimits[idxMin].second - controlLimits[idxMin].first);
            for (int k = 0; k <= steps && steps > 0; ++k)
            {
                const float x = x0 + (x1 - x0) * k / static_cast<float>(steps);
                dl->AddCircleFilled(ImVec2(x, trackY + 9.f * s), 1.3f * s, U32(P.textFaint), 8);
            }
        }
        if (bipolar)
        {
            const float xm = (x0 + x1) * 0.5f;
            dl->AddLine(ImVec2(xm, trackY - 7.f * s), ImVec2(xm, trackY + 7.f * s), U32(P.textFaint), 1.f);
        }

        // fill
        const float lo = std::min(nMin, nMax), hi = std::max(nMin, nMax);
        const float fx0 = x0 + (x1 - x0) * lo, fx1 = x0 + (x1 - x0) * hi;
        if (hovered || active)
            dl->AddRectFilled(ImVec2(fx0, trackY - th * 1.5f), ImVec2(std::max(fx1, fx0 + 1.f), trackY + th * 1.5f),
                              U32(P.a, 0.18f), th * 1.5f);
        gradRect(dl, ImVec2(fx0, trackY - th * 0.5f), ImVec2(std::max(fx1, fx0 + 1.f), trackY + th * 0.5f),
                 P.grad(lo), P.grad(hi), th * 0.5f, true);

        // handles
        for (int h = 0; h < 2; ++h)
        {
            const float n = h == 0 ? nMin : nMax;
            const ImVec2 c(x0 + (x1 - x0) * n, trackY);
            const bool hot = active && rangeHandle == h;
            dl->AddCircleFilled(ImVec2(c.x, c.y + 1.5f * s), handleR, U32(P.shadow), 24);
            gradCircle(dl, c, handleR, P.knobTop, P.knobBot);
            dl->AddCircle(c, handleR, U32(P.grad(n), hot ? 1.f : 0.85f), 24, hot ? 2.2f * s : 1.6f * s);
            if (hot)
                dl->AddCircleFilled(c, handleR * 2.f, U32(P.grad(n), 0.15f), 24);
        }

        if (hovered || active)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        ImGui::PopID();
    }

    // Segmented control for an enumerated/integer parameter
    void segmented(ImDrawList* dl, uint32_t idx, const char* const* labels, int count, ImVec2 a, ImVec2 b,
                   bool enabled = true, const bool* marks = nullptr)
    {
        ImGui::PushID(static_cast<int>(idx) + 5000);
        const float rounding = (b.y - a.y) * 0.32f;
        inset(dl, a, b, rounding);
        const float pad = 3.f * s;
        const float w = (b.x - a.x - pad * 2.f) / count;
        const int cur = static_cast<int>(std::round(pv[idx] - controlLimits[idx].first));
        const float alpha = enabled ? 1.f : 0.5f;

        for (int i = 0; i < count; ++i)
        {
            const ImVec2 ia(a.x + pad + w * i, a.y + pad);
            const ImVec2 ib(ia.x + w, b.y - pad);
            ImGui::PushID(i);
            ImGui::SetCursorScreenPos(ia);
            if (ImGui::InvisibleButton("##seg", ImVec2(w, ib.y - ia.y)))
                setParamGesture(idx, controlLimits[idx].first + static_cast<float>(i));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();

            if (i == cur)
            {
                gradRect(dl, ia, ib, P.grad(count > 1 ? i / float(count - 1) * 0.6f : 0.f),
                         P.grad(count > 1 ? 0.4f + i / float(count - 1) * 0.6f : 1.f), rounding - pad * 0.5f, true);
                textCentered(dl, fBold, ImVec2((ia.x + ib.x) * 0.5f, (ia.y + ib.y) * 0.5f), U32(P.onAccent, alpha),
                             labels[i]);
            }
            else
            {
                if (hovered)
                    dl->AddRectFilled(ia, ib, U32(P.text, 0.06f), rounding - pad * 0.5f);
                textCentered(dl, fBold, ImVec2((ia.x + ib.x) * 0.5f, (ia.y + ib.y) * 0.5f),
                             U32(hovered ? P.text : P.textDim, alpha), labels[i]);
            }
            if (marks != nullptr && marks[i])
                dl->AddCircleFilled(ImVec2((ia.x + ib.x) * 0.5f, ib.y - 3.5f * s), 1.8f * s,
                                    i == cur ? U32(P.onAccent, alpha) : U32(P.c, alpha), 8);
            if (hovered)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        ImGui::PopID();
    }

    // Pill on/off switch for a boolean parameter
    bool toggle(ImDrawList* dl, uint32_t idx, ImVec2 a)
    {
        ImGui::PushID(static_cast<int>(idx) + 9000);
        const float w = 38.f * s, h = 21.f * s;
        ImGui::SetCursorScreenPos(a);
        if (ImGui::InvisibleButton("##tog", ImVec2(w, h)))
            setParamGesture(idx, pv[idx] > 0.5f ? 0.f : 1.f);
        const bool hovered = ImGui::IsItemHovered();
        const bool on = pv[idx] > 0.5f;
        const ImVec2 b(a.x + w, a.y + h);

        if (on)
        {
            glow(dl, a, b, P.a, h * 0.5f, 0.6f);
            gradRect(dl, a, b, P.a, P.b, h * 0.5f, true);
        }
        else
        {
            dl->AddRectFilled(a, b, U32(P.track, hovered ? 1.6f : 1.f), h * 0.5f);
            dl->AddRect(a, b, U32(P.panelBorder), h * 0.5f, 0, 1.f);
        }
        const float r = h * 0.5f - 3.f * s;
        const ImVec2 c(on ? b.x - h * 0.5f : a.x + h * 0.5f, a.y + h * 0.5f);
        dl->AddCircleFilled(ImVec2(c.x, c.y + 1.f * s), r, U32(P.shadow), 20);
        dl->AddCircleFilled(c, r, on ? U32(P.onAccent) : U32(P.dark ? hex(0xC9CEDB) : hex(0xFFFFFF)), 20);
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::PopID();
        return on;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Sections

    void drawBackground(ImDrawList* dl, float W, float H)
    {
        dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(W, H), U32(P.bgTop), U32(P.bgTop), U32(P.bgBot), U32(P.bgBot));

        // soft accent glows, horizontally centered at the top and bottom edges
        const float ga = P.dark ? 0.018f : 0.022f;
        const float cx = W * 0.5f;
        if (P.numStops > 2)
        {
            // rainbow: one glow per stripe so the background reads as the full palette
            for (int k = 0; k < P.numStops; ++k)
            {
                const float r = (140.f + k * 26.f) * s;
                dl->AddCircleFilled(ImVec2(cx, -40.f * s), r, U32(P.gradStops[k], ga), 64);
                dl->AddCircleFilled(ImVec2(cx, H + 60.f * s), r, U32(P.gradStops[P.numStops - 1 - k], ga * 0.8f), 64);
            }
        }
        else
        {
            for (int i = 0; i < 14; ++i)
            {
                const float r = (120.f + i * 34.f) * s;
                dl->AddCircleFilled(ImVec2(cx, -40.f * s), r, U32(P.a, ga), 64);
                dl->AddCircleFilled(ImVec2(cx, H + 60.f * s), r, U32(P.b, ga * 0.8f), 64);
            }
        }
    }

    // small square icon button frame; returns hovered
    bool iconButtonFrame(ImDrawList* dl, ImVec2 a, ImVec2 b, bool hov, bool enabled = true)
    {
        const float r = 9.f * s;
        dl->AddRectFilled(a, b, U32(P.text, ! enabled ? 0.02f : (hov ? 0.10f : 0.05f)), r);
        dl->AddRect(a, b, U32(P.panelBorder), r, 0, 1.f);
        return hov;
    }

    void drawUndoIcon(ImDrawList* dl, ImVec2 c, bool redoIcon, ImU32 col, float scale = 1.f)
    {
        const float r = 6.f * scale * s, th = 1.6f * s;
        const float m = redoIcon ? -1.f : 1.f; // mirror for redo
        ImVec2 pts[16];
        const float a0 = IM_PI * 1.05f, a1 = IM_PI * 2.3f;
        for (int k = 0; k < 16; ++k)
        {
            const float t = a0 + (a1 - a0) * k / 15.f;
            pts[k] = ImVec2(c.x + m * std::cos(t) * r, c.y + 1.5f * scale * s + std::sin(t) * r);
        }
        dl->AddPolyline(pts, 16, col, 0, th);
        // arrow head at the start of the arc, pointing back along it
        const ImVec2 p = pts[0];
        const ImVec2 d(m * std::sin(a0), -std::cos(a0));
        const ImVec2 n(-d.y, d.x);
        const float h4 = 4.f * scale * s, h1 = 1.f * scale * s, h3 = 3.5f * scale * s;
        dl->AddTriangleFilled(ImVec2(p.x + d.x * h4, p.y + d.y * h4),
                              ImVec2(p.x - d.x * h1 + n.x * h3, p.y - d.y * h1 + n.y * h3),
                              ImVec2(p.x - d.x * h1 - n.x * h3, p.y - d.y * h1 - n.y * h3), col);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Logo sparks: every audible chop kicks a bar of the logo and throws sparks out of it. Faster chops throw them
    // harder and twinkle them faster; the bloom around the mark follows the chop speed.

    struct Spark
    {
        float x, y, vx, vy;
        float life, maxLife; // seconds left / total
        float size, phase, spin;
        int col;
    };
    static constexpr int kMaxSparks = 240;
    Spark sparks[kMaxSparks] {};
    int numSparks = 0;
    uint32_t lastChopCount = 0;
    bool chopSeen = false;
    float chopEnergy = 0.f;   // 0 (slow / stopped) .. 1 (fast stutters), smoothed
    float logoKick[6] {};
    float sparkCarry = 0.f;
    int lastKickBar = -1;
    float sparkTime = 0.f;

    void spawnSpark(float x, float y, float vx, float vy, float life, float size, int col)
    {
        int slot = numSparks;
        if (numSparks < kMaxSparks)
            ++numSparks;
        else
        {
            slot = 0; // pool full: recycle the spark closest to dying
            for (int i = 1; i < kMaxSparks; ++i)
                if (sparks[i].life < sparks[slot].life) slot = i;
        }
        Spark& p = sparks[slot];
        p.x = x; p.y = y; p.vx = vx; p.vy = vy;
        p.life = p.maxLife = life;
        p.size = size;
        p.phase = rnd(0.f, 6.2832f);
        p.spin = rnd(-2.5f, 2.5f);
        p.col = col;
    }

    void updateLogoSparks(float x, float cy, float bw, float gap, const float* heights)
    {
        const float dt = std::max(0.f, std::min(0.05f, ImGui::GetIO().DeltaTime));
        sparkTime += dt;

        // new chops since the last frame
        int chops = 0;
        float amp = 1.f, chopSec = 0.f;
        if (vis != nullptr)
        {
            const uint32_t c = vis->chopCount.load(std::memory_order_acquire);
            if (chopSeen)
                chops = static_cast<int>(std::min<uint32_t>(c - lastChopCount, 8u));
            chopSeen = true;
            lastChopCount = c;
            chopSec = vis->chopSeconds.load(std::memory_order_relaxed);
            amp = limit<float>(vis->chopAmp.load(std::memory_order_relaxed), 0.2f, 1.f);
        }

        // speed: 2 chops/s (half notes at 120) = 0 .. 32 chops/s (1/64 stutters) = 1
        if (chops > 0 && chopSec > 1e-4f)
        {
            const float target = limit<float>(std::log2(1.f / chopSec / 2.f) / 4.f, 0.f, 1.f);
            chopEnergy += (target - chopEnergy) * (target > chopEnergy ? 0.6f : 0.25f);
        }
        else
            chopEnergy *= std::exp(-dt * 1.2f); // no chops: calm down
        for (int i = 0; i < 6; ++i)
            logoKick[i] *= std::exp(-dt * (9.f + 10.f * chopEnergy));

        for (int n = 0; n < chops; ++n)
        {
            int bar = static_cast<int>(rnd(0.f, 5.999f));
            if (bar == lastKickBar) bar = (bar + 1 + static_cast<int>(rnd(0.f, 4.999f))) % 6;
            lastKickBar = bar;
            logoKick[bar] = std::max(logoKick[bar], amp);

            // fewer but faster sparks per chop as the chops speed up (keeps the density musical)
            sparkCarry += (6.5f - 4.5f * chopEnergy) * (0.4f + 0.6f * amp);
            const float bh = 30.f * s * heights[bar];
            const float bx = x + bar * (bw + gap) + bw * 0.5f;
            const float speed = (55.f + 150.f * chopEnergy) * s;
            while (sparkCarry >= 1.f)
            {
                sparkCarry -= 1.f;
                // out of the bar's ends and sides, leaning right over the wordmark
                const float along = rnd(-0.5f, 0.5f);
                const float py = cy + along * bh;
                const float ang = rnd(0.f, 6.2832f);
                const float v = speed * rnd(0.35f, 1.f);
                float vx = std::cos(ang) * v + rnd(10.f, 45.f) * s;
                float vy = std::sin(ang) * v * 0.8f + along * 50.f * s;
                const int col = chance(0.75f) ? bar : (bar + (chance(0.5f) ? 1 : 5)) % 6;
                spawnSpark(bx + rnd(-0.5f, 0.5f) * bw, py, vx, vy,
                           rnd(0.45f, 1.05f) * (1.1f - 0.35f * chopEnergy),
                           rnd(0.9f, 1.8f) * s * (0.8f + 0.4f * amp), col);
            }
        }

        // drift: drag, a little buoyancy, and a slight shimmer sideways
        const float drag = std::exp(-dt * 2.6f);
        for (int i = 0; i < numSparks;)
        {
            Spark& p = sparks[i];
            p.life -= dt;
            if (p.life <= 0.f)
            {
                p = sparks[--numSparks];
                continue;
            }
            p.vx *= drag;
            p.vy = p.vy * drag - 14.f * s * dt;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            ++i;
        }
    }

    void drawLogoSparks(ImDrawList* dl)
    {
        if (numSparks == 0)
            return;
        const ImVec4 white = hex(0xFFFFFF);
        const float twinkleHz = 9.f + 16.f * chopEnergy;
        const float haloA = P.dark ? 1.f : 0.7f;
        // halos first, so every spark's bloom sits under all the cores
        for (int i = 0; i < numSparks; ++i)
        {
            const Spark& p = sparks[i];
            const float t = p.life / p.maxLife;                         // 1 -> 0
            const float a = std::min(1.f, (1.f - t) * 12.f) * t * t;     // quick fade-in, soft fade-out
            const float tw = 0.6f + 0.4f * std::sin(p.phase + sparkTime * twinkleHz);
            const ImVec4& c = kLogoRainbow[p.col];
            const float r = p.size;
            dl->AddCircleFilled(ImVec2(p.x, p.y), r * 5.2f, U32(c, 0.05f * a * tw * haloA), 14);
            dl->AddCircleFilled(ImVec2(p.x, p.y), r * 3.2f, U32(c, 0.11f * a * tw * haloA), 12);
            dl->AddCircleFilled(ImVec2(p.x, p.y), r * 1.9f, U32(c, 0.26f * a * haloA), 10);
        }
        for (int i = 0; i < numSparks; ++i)
        {
            const Spark& p = sparks[i];
            const float t = p.life / p.maxLife;
            const float a = std::min(1.f, (1.f - t) * 12.f) * t;
            const float tw = 0.5f + 0.5f * std::sin(p.phase + sparkTime * twinkleHz);
            const ImVec4 core = P.dark ? mix(kLogoRainbow[p.col], white, 0.7f) : kLogoRainbow[p.col];
            const ImVec2 c(p.x, p.y);
            const float r = p.size;
            dl->AddCircleFilled(c, r * 0.9f, U32(core, a), 8);

            // four-point star glint, rotating slowly, flaring with the twinkle
            const float L = r * (2.2f + 3.2f * tw * t), w = r * 0.42f;
            const float ang = p.phase + p.spin * (p.maxLife - p.life);
            const float ca = std::cos(ang), sa = std::sin(ang);
            const ImU32 rc = U32(core, a * (0.35f + 0.65f * tw));
            for (int k = 0; k < 2; ++k)
            {
                const float dx = k == 0 ? ca : -sa, dy = k == 0 ? sa : ca; // ray direction
                const float nx = -dy, ny = dx;                             // its normal
                dl->AddQuadFilled(ImVec2(c.x + dx * L, c.y + dy * L), ImVec2(c.x + nx * w, c.y + ny * w),
                                  ImVec2(c.x - dx * L, c.y - dy * L), ImVec2(c.x - nx * w, c.y - ny * w), rc);
            }
        }
    }

    void drawHeader(ImDrawList* dl, float W, float pad)
    {
        const float top = pad, h = 52.f * s;
        const float cy = top + h * 0.5f;

        // ---- logo: six bars in the "present" rainbow palette; HYPER is coloured, CUT uses the theme ----
        {
            const float x = pad + 4.f * s;
            const float bw = 4.f * s, gap = 3.f * s;
            const float heights[6] = { 0.45f, 0.9f, 0.6f, 1.f, 0.75f, 0.35f };
            updateLogoSparks(x, cy, bw, gap, heights);
            for (int i = 0; i < 6; ++i)
            {
                // each chop kicks one bar: it jumps, brightens and blooms
                const float k = logoKick[i];
                const float bh = 30.f * s * heights[i] * (1.f + 0.32f * k);
                const float bx = x + i * (bw + gap);
                const ImVec2 ba(bx, cy - bh * 0.5f), bb(bx + bw, cy + bh * 0.5f);
                const float bloom = std::min(1.f, 0.85f * k + 0.35f * chopEnergy);
                if (bloom > 0.01f)
                    for (int g = 1; g <= 5; ++g)
                    {
                        const float e = g * 1.6f * s;
                        dl->AddRectFilled(ImVec2(ba.x - e, ba.y - e), ImVec2(bb.x + e, bb.y + e),
                                          U32(kLogoRainbow[i], (P.dark ? 0.10f : 0.07f) * bloom * (6 - g) / 5.f), bw * 0.5f + e);
                    }
                const ImVec4 bc = mix(kLogoRainbow[i], hex(0xFFFFFF), P.dark ? 0.45f * k : 0.15f * k);
                dl->AddRectFilled(ba, bb, U32(bc), bw * 0.5f);
            }

            const float tx = x + 6 * (bw + gap) + 10.f * s;
            const float ty = cy - fs(fTitle) * 0.5f - 6.f * s;
            logoText(dl, fTitle, ImVec2(tx, ty), U32(P.text));
            const ImVec2 tsz = textSize(fTitle, "HYPERCUT");
            text(dl, fSmall, ImVec2(tx + 1.f * s, ty + tsz.y - 2.f * s), U32(P.textDim), "BEAT SLICER  \xc2\xb7  v2");
            drawLogoSparks(dl);
        }

        // ---- algorithm selector (centered) ----
        {
            const float bw = 96.f * s, bh = 40.f * s, gap = 4.f * s, padIn = 4.f * s;
            const float totalW = bw * kNumAlgos + gap * (kNumAlgos - 1) + padIn * 2.f;
            const ImVec2 ca(W * 0.5f - totalW * 0.5f, cy - bh * 0.5f - padIn);
            const ImVec2 cb(ca.x + totalW, cy + bh * 0.5f + padIn);
            inset(dl, ca, cb, 14.f * s);

            const int cur = algoFromParams(pv);
            const float dt = ImGui::GetIO().DeltaTime;
            if (algoAnim < 0.f)
                algoAnim = static_cast<float>(cur);
            algoAnim += (cur - algoAnim) * std::min(1.f, dt * 16.f);
            if (std::fabs(algoAnim - cur) < 0.001f)
                algoAnim = static_cast<float>(cur);

            const ImVec2 ha(ca.x + padIn + algoAnim * (bw + gap), ca.y + padIn);
            const ImVec2 hb(ha.x + bw, ha.y + bh);
            const ImVec4 hc = algoColorAt(algoAnim);
            glow(dl, ha, hb, hc, 11.f * s, 1.4f);
            gradRect(dl, ha, hb, withA(hc, P.dark ? 0.30f : 0.20f), withA(hc, P.dark ? 0.14f : 0.09f), 11.f * s);
            dl->AddRect(ha, hb, U32(hc, 0.85f), 11.f * s, 0, 1.3f * s);
            dl->AddLine(ImVec2(ha.x + 10.f * s, ha.y + 1.f), ImVec2(hb.x - 10.f * s, ha.y + 1.f), U32(hex(0xFFFFFF), P.dark ? 0.18f : 0.6f), 1.f);

            for (int i = 0; i < kNumAlgos; ++i)
            {
                const ImVec2 ba(ca.x + padIn + i * (bw + gap), ca.y + padIn);
                ImGui::PushID(i + 700);
                ImGui::SetCursorScreenPos(ba);
                if (ImGui::InvisibleButton("##algo", ImVec2(bw, bh)))
                {
                    selectAlgo(i);
                    commitUndo(true);
                }
                const bool hov = ImGui::IsItemHovered();
                ImGui::PopID();
                if (hov)
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    if (i != cur)
                        dl->AddRectFilled(ba, ImVec2(ba.x + bw, ba.y + bh), U32(P.text, 0.05f), 11.f * s);
                }
                const float sel = 1.f - std::min(1.f, std::fabs(algoAnim - i));
                const ImVec4 tc = mix(hov ? P.text : P.textDim, P.text, sel);
                const ImVec4 ic = algoColor(i);
                // icon + name, centered as a pair
                const float ir = 5.5f * s, igap = 5.f * s;
                const ImVec2 tsz = textSize(fBold, kAlgoNames[i]);
                const float px = ba.x + bw * 0.5f - (ir * 2.f + igap + tsz.x) * 0.5f;
                drawAlgoIcon(dl, i, ImVec2(px + ir, ba.y + bh * 0.5f), ir, U32(ic, 0.6f + 0.4f * std::max(sel, hov ? 1.f : 0.f)));
                text(dl, fBold, ImVec2(px + ir * 2.f + igap, ba.y + bh * 0.5f - tsz.y * 0.5f), U32(tc), kAlgoNames[i]);
            }
        }

        // ---- right side: tools row above the preset bar ----
        {
            const float blockW = 300.f * s;
            const float bx0 = W - pad - blockW, bx1 = W - pad;
            const float rowH = 20.f * s;
            const float rcy = top + rowH * 0.5f;
            const float k = 0.72f; // icon scale for the small row
            float x = bx1;

            text(dl, fSmall, ImVec2(bx0 + 2.f * s, rcy - fs(fSmall) * 0.5f - 1.f * s), U32(P.textFaint), "PRESET");

            // dark / light toggle
            const float bh = rowH;
            const ImVec2 ma(x - 28.f * s, top), mb(x, top + bh);
            ImGui::SetCursorScreenPos(ma);
            if (ImGui::InvisibleButton("##mode", mb - ma))
            {
                darkMode = ! darkMode;
                P.build(themeIndex, darkMode);
                applyImGuiStyle();
                persistPrefs();
            }
            const bool mhov = ImGui::IsItemHovered();
            if (mhov)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                tooltip(darkMode ? "Light mode" : "Dark mode");
            }
            iconButtonFrame(dl, ma, mb, mhov);
            const ImVec2 mc((ma.x + mb.x) * 0.5f, rcy);
            if (darkMode)
            {
                dl->AddCircleFilled(mc, 7.5f * k * s, U32(P.text), 32);
                dl->AddCircleFilled(ImVec2(mc.x + 4.f * k * s, mc.y - 3.f * k * s), 6.5f * k * s,
                                    U32(mix(P.bgTop, P.text, mhov ? 0.10f : 0.05f)), 32);
            }
            else
            {
                dl->AddCircleFilled(mc, 4.5f * k * s, U32(P.text), 24);
                for (int r = 0; r < 8; ++r)
                {
                    const float ang = r * IM_PI / 4.f;
                    dl->AddLine(ImVec2(mc.x + std::cos(ang) * 7.f * k * s, mc.y + std::sin(ang) * 7.f * k * s),
                                ImVec2(mc.x + std::cos(ang) * 10.f * k * s, mc.y + std::sin(ang) * 10.f * k * s),
                                U32(P.text), 1.4f * s);
                }
            }
            x = ma.x - 4.f * s;

            // theme selector (swatch + chevron)
            const float tw = 44.f * s;
            const ImVec2 ta(x - tw, top), tb(x, top + bh);
            ImGui::SetCursorScreenPos(ta);
            if (ImGui::InvisibleButton("##theme", tb - ta))
                ImGui::OpenPopup("##themes");
            const bool thov = ImGui::IsItemHovered();
            if (thov)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                tooltip("Theme: %s", tr(kThemes[themeIndex].name));
            }
            iconButtonFrame(dl, ta, tb, thov);
            gradRectTheme(dl, ImVec2(ta.x + 7.f * s, rcy - 5.f * s), ImVec2(ta.x + 25.f * s, rcy + 5.f * s), 5.f * s);
            const ImVec2 cc(tb.x - 10.f * s, rcy);
            dl->AddTriangleFilled(ImVec2(cc.x - 3.5f * s, cc.y - 1.5f * s), ImVec2(cc.x + 3.5f * s, cc.y - 1.5f * s),
                                  ImVec2(cc.x, cc.y + 2.5f * s), U32(P.textDim));
            ImGui::SetNextWindowPos(ImVec2(bx1, top + bh + 6.f * s), ImGuiCond_Always, ImVec2(1.f, 0.f));
            drawThemePopup();
            x = ta.x - 10.f * s;

            // redo, undo
            for (int i = 0; i < 2; ++i)
            {
                const bool isRedo = i == 0;
                const ImVec2 ua(x - 28.f * s, top), ub(x, top + bh);
                const bool enabled = isRedo ? ! redoStack.empty() : ! undoStack.empty();
                ImGui::PushID(i + 900);
                ImGui::SetCursorScreenPos(ua);
                const bool clicked = ImGui::InvisibleButton("##undo", ub - ua);
                const bool hov = ImGui::IsItemHovered() && enabled;
                ImGui::PopID();
                if (clicked && enabled)
                    isRedo ? redo() : undo();
                if (hov)
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    tooltip(isRedo ? "Redo (Ctrl+Shift+Z)" : "Undo (Ctrl+Z)");
                }
                iconButtonFrame(dl, ua, ub, hov, enabled);
                drawUndoIcon(dl, (ua + ub) * 0.5f, isRedo, U32(enabled ? (hov ? P.text : P.textDim) : P.textFaint), k);
                x = ua.x - 4.f * s;
            }

            // language: English and Spanish flags, the active one lit
            x -= 6.f * s;
            for (int lang = kNumLangs - 1; lang >= 0; --lang)
            {
                const float fw = 26.f * s;
                const ImVec2 la(x - fw, top), lb(x, top + bh);
                ImGui::PushID(lang + 950);
                ImGui::SetCursorScreenPos(la);
                const bool clicked = ImGui::InvisibleButton("##lang", lb - la);
                const bool hov = ImGui::IsItemHovered();
                ImGui::PopID();
                const bool active = uiLang() == lang;
                if (clicked && ! active)
                {
                    uiLang() = lang;
                    persistPrefs();
                }
                if (hov)
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    ImGui::SetTooltip("%s", lang == kLangEs ? "Espa\xc3\xb1ol" : "English"); // each in its own language
                }
                const ImVec2 fa(la.x + 3.f * s, rcy - 7.f * s), fb(lb.x - 3.f * s, rcy + 7.f * s);
                drawFlag(dl, lang, fa, fb, active ? 1.f : (hov ? 0.75f : 0.4f));
                if (active)
                    dl->AddRect(ImVec2(fa.x - 2.f * s, fa.y - 2.f * s), ImVec2(fb.x + 2.f * s, fb.y + 2.f * s), U32(P.text, 0.9f), 3.f * s, 0, 1.2f * s);
                x = la.x - 2.f * s;
            }

            // preset bar spanning the block
            drawPresetBar(dl, ImVec2(bx0, top + rowH + 3.f * s), 29.f * s, blockW);
        }
    }

    // [<] [ preset name v ] [>] [save] [dice]
    void drawPresetBar(ImDrawList* dl, ImVec2 a, float h, float width)
    {
        const float arrowW = 22.f * s, gap = 4.f * s;
        const float nameW = width - arrowW * 2.f - gap * 3.f - 4.f * s - h * 2.f;
        const float barRight = a.x + width;
        float x = a.x;
        const float cy = a.y + h * 0.5f;

        auto arrow = [&](const char* id, bool left) {
            const ImVec2 ba(x, a.y), bb(x + arrowW, a.y + h);
            ImGui::SetCursorScreenPos(ba);
            const bool clicked = ImGui::InvisibleButton(id, bb - ba);
            const bool hov = ImGui::IsItemHovered();
            if (hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            iconButtonFrame(dl, ba, bb, hov);
            const ImVec2 c = (ba + bb) * 0.5f;
            const float d = left ? -1.f : 1.f;
            dl->AddTriangleFilled(ImVec2(c.x + d * 3.5f * s, c.y), ImVec2(c.x - d * 2.5f * s, c.y - 4.5f * s),
                                  ImVec2(c.x - d * 2.5f * s, c.y + 4.5f * s), U32(hov ? P.text : P.textDim));
            x += arrowW + gap;
            return clicked;
        };

        const int n = static_cast<int>(presets.size());
        if (arrow("##prevpreset", true) && n > 0)
            loadPreset(currentPreset < 0 ? 0 : (currentPreset + n - 1) % n);

        // name box (opens the preset browser dropdown)
        {
            const ImVec2 na(x, a.y), nb(x + nameW, a.y + h);
            ImGui::SetCursorScreenPos(na);
            const bool popupOpen = ImGui::IsPopupOpen("##presetbrowser");
            if (ImGui::InvisibleButton("##presetname", nb - na))
                ImGui::OpenPopup("##presetbrowser");
            const bool hov = ImGui::IsItemHovered();
            if (hov)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                tooltip(popupOpen ? "Close the preset browser" : "Browse presets");
            }
            inset(dl, na, nb, 9.f * s);
            if (hov || popupOpen)
                dl->AddRect(na, nb, U32(P.a, 0.5f), 9.f * s, 0, 1.f);
            std::string label = currentName;
            const bool modified = presetModified();
            // icon of the current algorithm
            const int curAlgo = algoFromParams(pv);
            drawAlgoIcon(dl, curAlgo, ImVec2(na.x + 16.f * s, cy), 6.f * s, U32(algoColor(curAlgo)));
            // ellipsize to fit
            const float maxW = nameW - 52.f * s;
            while (label.size() > 3 && textSize(fBold, (label + (modified ? " *" : "")).c_str()).x > maxW)
                label = label.substr(0, label.size() - 4) + "...";
            if (modified)
                label += " *";
            text(dl, fBold, ImVec2(na.x + 28.f * s, cy - fs(fBold) * 0.5f - 1.f * s), U32(P.text), label.c_str());
            // chevron (dropdown)
            {
                const ImVec2 cc(nb.x - 12.f * s, cy);
                const float ang = popupOpen ? IM_PI : 0.f;
                const float ca = std::cos(ang), sa = std::sin(ang);
                auto R = [&](float px, float py) { return ImVec2(cc.x + px * ca - py * sa, cc.y + px * sa + py * ca); };
                dl->AddTriangleFilled(R(0.f, 3.f * s), R(-4.f * s, -2.f * s), R(4.f * s, -2.f * s),
                                      U32(popupOpen ? P.b : P.textDim));
            }

            if (ImGui::BeginPopup("##presetbrowser"))
            {
                ImGui::SetNextItemWidth(300.f * s);
                ImGui::InputTextWithHint("##presetsearch", tr("Search presets"), presetSearch, sizeof(presetSearch));
                ImGui::Separator();
                if (ImGui::BeginChild("##presetlist", ImVec2(420.f * s, 320.f * s), false))
                {
                    const std::string query = lowerCase(presetSearch);
                    bool any = false;
                    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
                    {
                        const PresetEntry& pe = presets[static_cast<size_t>(i)];
                        if (! query.empty() && ! presetMatches(pe, query))
                            continue;
                        any = true;
                        const bool selected = (i == currentPreset);
                        std::string row = pe.name;
                        if (! pe.author.empty()) row += "  —  " + pe.author;
                        if (! pe.category.empty()) row += "  [" + pe.category + "]";
                        if (ImGui::Selectable(row.c_str(), selected))
                        {
                            loadPreset(i);
                            ImGui::CloseCurrentPopup();
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    if (! any)
                        ImGui::TextDisabled("%s", tr("No presets found"));
                }
                ImGui::EndChild();
                ImGui::EndPopup();
            }

            x += nameW + gap;
        }

        if (arrow("##nextpreset", false) && n > 0)
            loadPreset(currentPreset < 0 ? 0 : (currentPreset + 1) % n);

        // save
        {
            x += 4.f * s;
            const ImVec2 sa(x, a.y), sb(x + h, a.y + h);
            ImGui::SetCursorScreenPos(sa);
            if (ImGui::InvisibleButton("##savepreset", sb - sa))
            {
                const bool curUser = currentPreset < 0 || currentPreset >= static_cast<int>(presets.size())
                                   || ! presets[currentPreset].path.empty();
                std::snprintf(saveName, sizeof(saveName), "%s", curUser ? currentName.c_str() : "");
                // re-saving a user preset keeps its author; otherwise the last author used
                const bool user = currentPreset >= 0 && currentPreset < static_cast<int>(presets.size())
                                && ! presets[currentPreset].path.empty();
                std::snprintf(saveAuthor, sizeof(saveAuthor), "%s",
                              user && ! presets[currentPreset].author.empty() ? presets[currentPreset].author.c_str()
                                                                             : authorName.c_str());
                // the category of whatever was loaded is a good guess for its variation
                const bool any = currentPreset >= 0 && currentPreset < static_cast<int>(presets.size());
                std::snprintf(saveCategory, sizeof(saveCategory), "%s", any ? presets[currentPreset].category.c_str() : "");
                saveFocus = true;
                ImGui::OpenPopup("##savepopup");
            }
            const bool hov = ImGui::IsItemHovered();
            if (hov)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                tooltip("Save preset");
            }
            iconButtonFrame(dl, sa, sb, hov);
            // floppy disk
            const ImVec2 c = (sa + sb) * 0.5f;
            const float r = 7.f * s;
            const ImU32 col = U32(hov ? P.text : P.textDim);
            dl->AddRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 2.f * s, 0, 1.5f * s);
            dl->AddRectFilled(ImVec2(c.x - r * 0.5f, c.y - r), ImVec2(c.x + r * 0.45f, c.y - r * 0.3f), col, 1.f * s);
            dl->AddRectFilled(ImVec2(c.x - r * 0.55f, c.y + r * 0.15f), ImVec2(c.x + r * 0.55f, c.y + r), col, 1.f * s);
            ImGui::SetNextWindowPos(ImVec2(barRight, sb.y + 6.f * s), ImGuiCond_Always, ImVec2(1.f, 0.f));
            drawSavePopup();
            x += h + gap;
        }

        // dice: randomize everything (except fill mode, limiter and dry/wet)
        if (dice(dl, "globaldice", ImVec2(x + h * 0.5f, cy), h))
            randomize(kDiceAll);
    }

    // ---- preset sidebar ----

    float sideEase() const
    {
        const float t = std::max(0.f, std::min(1.f, sideAnim));
        return t * t * (3.f - 2.f * t); // smoothstep
    }

    void setSidebar(bool open)
    {
        sideOpen = open;
        sideAnim = 0.f;
        (void)open;
    }

    // preset browser is now a dropdown popup; window resizing/sidebar animation disabled
    float updateSidebar()
    {
        sideAnim = 0.f;
        sideNoResize = 0;
        return 0.f;
    }

    // width the window would have without the sidebar (what the size preference stores)
    uint widthWithoutSidebar() const
    {
        return getWidth();
    }

    static std::string lowerCase(std::string v)
    {
        for (char& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return v;
    }

    // every word of the search has to appear in the name, author or category
    bool presetMatches(const PresetEntry& pe, const std::string& query) const
    {
        const std::string hay = lowerCase(pe.name + " " + pe.author + " " + pe.category);
        size_t pos = 0;
        while (pos < query.size())
        {
            while (pos < query.size() && query[pos] == ' ') ++pos;
            const size_t end = std::min(query.find(' ', pos), query.size());
            if (end > pos && hay.find(query.substr(pos, end - pos)) == std::string::npos)
                return false;
            pos = end;
        }
        return true;
    }

    // clip a string to a pixel width with "..."
    std::string fitText(ImFont* f, std::string t, float maxW) const
    {
        t = tr(t.c_str());
        if (textSize(f, t.c_str()).x <= maxW)
            return t;
        while (t.size() > 1 && textSize(f, (t + "...").c_str()).x > maxW)
            t.pop_back();
        return t + "...";
    }

    // five-pointed star (filled or outlined)
    void drawStar(ImDrawList* dl, ImVec2 c, float r, ImU32 col, bool filled)
    {
        ImVec2 pts[10];
        for (int k = 0; k < 10; ++k)
        {
            const float ang = -IM_PI * 0.5f + k * IM_PI / 5.f;
            const float rr = (k & 1) ? r * 0.45f : r;
            pts[k] = ImVec2(c.x + std::cos(ang) * rr, c.y + std::sin(ang) * rr);
        }
        if (filled)
            for (int k = 0; k < 10; ++k)
                dl->AddTriangleFilled(c, pts[k], pts[(k + 1) % 10], col);
        else
            dl->AddPolyline(pts, 10, col, ImDrawFlags_Closed, std::max(1.f, 1.2f * s));
    }

    void drawTrash(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
    {
        const float th = std::max(1.f, 1.3f * s);
        dl->AddLine(ImVec2(c.x - r, c.y - r * 0.7f), ImVec2(c.x + r, c.y - r * 0.7f), col, th);           // lid
        dl->AddLine(ImVec2(c.x - r * 0.3f, c.y - r), ImVec2(c.x + r * 0.3f, c.y - r), col, th);           // handle
        dl->AddRect(ImVec2(c.x - r * 0.7f, c.y - r * 0.5f), ImVec2(c.x + r * 0.7f, c.y + r), col, 1.5f * s, 0, th);
        dl->AddLine(ImVec2(c.x, c.y - r * 0.2f), ImVec2(c.x, c.y + r * 0.7f), col, th);
    }

    // (preset browser dropdown is inlined in drawPresetBar for stability)

    static void dl_x(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
    {
        dl->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 1.6f);
        dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r), col, 1.6f);
    }

    void drawSavePopup()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f * s, 12.f * s));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f * s, 6.f * s));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.f * s);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f * s, 8.f * s));
        if (ImGui::BeginPopup("##savepopup", ImGuiWindowFlags_NoMove))
        {
            ImDrawList* pdl = ImGui::GetWindowDrawList();
            const float w = 240.f * s;
            text(pdl, fSmall, ImGui::GetCursorScreenPos(), U32(P.textDim), "SAVE PRESET AS");
            ImGui::Dummy(ImVec2(w, fs(fSmall) + 2.f * s));
            if (saveFocus)
            {
                ImGui::SetKeyboardFocusHere();
                saveFocus = false;
            }
            ImGui::PushItemWidth(w);
            bool enter = ImGui::InputText("##savename", saveName, sizeof(saveName), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopItemWidth();
            text(pdl, fSmall, ImGui::GetCursorScreenPos(), U32(P.textDim), "AUTHOR");
            ImGui::Dummy(ImVec2(w, fs(fSmall) + 2.f * s));
            ImGui::PushItemWidth(w);
            enter = ImGui::InputTextWithHint("##saveauthor", tr("optional"), saveAuthor, sizeof(saveAuthor),
                                             ImGuiInputTextFlags_EnterReturnsTrue) || enter;
            ImGui::PopItemWidth();
            text(pdl, fSmall, ImGui::GetCursorScreenPos(), U32(P.textDim), "CATEGORY");
            ImGui::Dummy(ImVec2(w, fs(fSmall) + 2.f * s));
            ImGui::PushItemWidth(w);
            enter = ImGui::InputTextWithHint("##savecat", tr("optional"), saveCategory, sizeof(saveCategory),
                                             ImGuiInputTextFlags_EnterReturnsTrue) || enter;
            ImGui::PopItemWidth();
            // quick picks: the categories already in use
            {
                std::vector<std::string> cats;
                for (const PresetEntry& pe : presets)
                    if (! pe.category.empty() && std::find(cats.begin(), cats.end(), pe.category) == cats.end())
                        cats.push_back(pe.category);
                std::sort(cats.begin(), cats.end());
                const ImVec2 o = ImGui::GetCursorScreenPos();
                float px = o.x, py = o.y;
                const float ph = 20.f * s;
                for (size_t k = 0; k < cats.size(); ++k)
                {
                    const float pw = textSize(fSmall, cats[k].c_str()).x + 16.f * s;
                    if (px + pw > o.x + w) { px = o.x; py += ph + 4.f * s; }
                    const ImVec2 ca(px, py), cb(px + pw, py + ph);
                    ImGui::SetCursorScreenPos(ca);
                    ImGui::PushID(static_cast<int>(k) + 300);
                    if (ImGui::InvisibleButton("##cat", cb - ca))
                        std::snprintf(saveCategory, sizeof(saveCategory), "%s", cats[k].c_str());
                    const bool hv = ImGui::IsItemHovered();
                    ImGui::PopID();
                    const bool cur = cats[k] == saveCategory;
                    pdl->AddRectFilled(ca, cb, U32(cur ? P.a : P.text, cur ? 0.25f : (hv ? 0.10f : 0.05f)), ph * 0.5f);
                    textCentered(pdl, fSmall, (ca + cb) * 0.5f, U32(cur || hv ? P.text : P.textDim), cats[k].c_str());
                    if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    px += pw + 4.f * s;
                }
                ImGui::SetCursorScreenPos(ImVec2(o.x, cats.empty() ? o.y : py + ph + 8.f * s));
                ImGui::Dummy(ImVec2(w, 0.f));
            }

            bool exists = false;
            for (int i = 0; i < static_cast<int>(presets.size()); ++i)
                exists = exists || (! presets[i].path.empty() && presets[i].name == saveName);

            const ImVec2 ba = ImGui::GetCursorScreenPos();
            const float bw = 90.f * s, bh = 28.f * s;
            const bool valid = saveName[0] != '\0';
            if (exists)
                text(pdl, fSmall, ImVec2(ba.x, ba.y + 7.f * s), U32(P.c), "Overwrites existing");
            ImGui::SetCursorScreenPos(ImVec2(ba.x + w - bw, ba.y));
            const bool clicked = ImGui::InvisibleButton("##dosave", ImVec2(bw, bh));
            const bool hov = ImGui::IsItemHovered();
            const ImVec2 sa(ba.x + w - bw, ba.y), sb(sa.x + bw, sa.y + bh);
            if (valid)
                gradRectTheme(pdl, sa, sb, 8.f * s);
            else
                pdl->AddRectFilled(sa, sb, U32(P.track), 8.f * s);
            if (hov && valid)
                pdl->AddRect(sa, sb, U32(P.text, 0.6f), 8.f * s, 0, 1.f);
            textCentered(pdl, fBold, (sa + sb) * 0.5f, U32(valid ? P.onAccent : P.textFaint), "SAVE");
            if ((clicked || enter) && valid)
            {
                savePreset(saveName);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(4);
    }

    void drawThemePopup()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f * s, 8.f * s));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 12.f * s);
        if (ImGui::BeginPopup("##themes", ImGuiWindowFlags_NoMove))
        {
            ImDrawList* pdl = ImGui::GetWindowDrawList();
            const float w = 200.f * s, rh = 34.f * s;

            text(pdl, fSmall, ImGui::GetCursorScreenPos() + ImVec2(8.f * s, 2.f * s), U32(P.textDim), "THEME");
            ImGui::Dummy(ImVec2(w, 22.f * s));

            for (int i = 0; i < kNumThemes; ++i)
            {
                const ImVec2 a = ImGui::GetCursorScreenPos();
                ImGui::PushID(i);
                if (ImGui::InvisibleButton("##t", ImVec2(w, rh)))
                {
                    themeIndex = i;
                    P.build(themeIndex, darkMode);
                    applyImGuiStyle();
                    persistPrefs();
                }
                const bool hov = ImGui::IsItemHovered();
                ImGui::PopID();

                if (hov || i == themeIndex)
                    pdl->AddRectFilled(a, ImVec2(a.x + w, a.y + rh), U32(P.text, i == themeIndex ? 0.08f : 0.05f), 8.f * s);

                const ThemeDef& t = kThemes[i];
                // one dot per palette colour (at least five)
                const int dots = std::max(5, t.stripeLen);
                const float dotD = 7.f * s, dotGap = 2.f * s, pitch = dotD + dotGap;
                const float cyDot = a.y + rh * 0.5f;
                for (int k = 0; k < dots; ++k)
                {
                    const float u = t.stripeLen > 1 ? static_cast<float>(k) / (dots - 1) * (t.stripeLen - 1) : 0.f;
                    const int j = std::min(static_cast<int>(u), t.stripeLen - 1);
                    const int jn = std::min(j + 1, t.stripeLen - 1);
                    const ImVec4 col = mix(hex(t.stripes[j]), hex(t.stripes[jn]), u - static_cast<float>(j));
                    pdl->AddCircleFilled(ImVec2(a.x + 10.f * s + dotD * 0.5f + k * pitch, cyDot),
                                         dotD * 0.5f, U32(col), 10);
                }
                text(pdl, fBody, ImVec2(a.x + 10.f * s + dots * pitch + 2.f * s,
                                        a.y + rh * 0.5f - fs(fBody) * 0.5f - 1.f * s), U32(P.text), t.name);
                if (i == themeIndex)
                {
                    const ImVec2 c(a.x + w - 18.f * s, a.y + rh * 0.5f);
                    pdl->AddLine(ImVec2(c.x - 5.f * s, c.y), ImVec2(c.x - 1.5f * s, c.y + 4.f * s), U32(P.b), 2.f * s);
                    pdl->AddLine(ImVec2(c.x - 1.5f * s, c.y + 4.f * s), ImVec2(c.x + 6.f * s, c.y - 4.f * s), U32(P.b), 2.f * s);
                }
            }

            // divider + dark mode switch
            const ImVec2 d = ImGui::GetCursorScreenPos();
            pdl->AddLine(ImVec2(d.x + 6.f * s, d.y + 6.f * s), ImVec2(d.x + w - 6.f * s, d.y + 6.f * s), U32(P.panelBorder), 1.f);
            ImGui::Dummy(ImVec2(w, 12.f * s));
            const ImVec2 r = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##darkrow", ImVec2(w, rh)))
            {
                darkMode = ! darkMode;
                P.build(themeIndex, darkMode);
                applyImGuiStyle();
                persistPrefs();
            }
            if (ImGui::IsItemHovered())
                pdl->AddRectFilled(r, ImVec2(r.x + w, r.y + rh), U32(P.text, 0.05f), 8.f * s);
            text(pdl, fBody, ImVec2(r.x + 12.f * s, r.y + rh * 0.5f - fs(fBody) * 0.5f - 1.f * s), U32(P.text), "Dark mode");
            {
                const float tw = 34.f * s, th = 19.f * s;
                const ImVec2 ta(r.x + w - tw - 10.f * s, r.y + rh * 0.5f - th * 0.5f), tb(ta.x + tw, ta.y + th);
                if (darkMode) gradRectTheme(pdl, ta, tb, th * 0.5f);
                else pdl->AddRectFilled(ta, tb, U32(P.track, 1.5f), th * 0.5f);
                const float cr = th * 0.5f - 3.f * s;
                pdl->AddCircleFilled(ImVec2(darkMode ? tb.x - th * 0.5f : ta.x + th * 0.5f, ta.y + th * 0.5f), cr,
                                     IM_COL32_WHITE, 20);
            }
            // CRT look of the visualisers
            {
                const ImVec2 cr0 = ImGui::GetCursorScreenPos();
                if (ImGui::InvisibleButton("##crtrow", ImVec2(w, rh)))
                {
                    crtEnabled = ! crtEnabled;
                    for (CrtScreen& sc : crtScreens) sc.history = false;
                    persistPrefs();
                }
                if (ImGui::IsItemHovered())
                    pdl->AddRectFilled(cr0, ImVec2(cr0.x + w, cr0.y + rh), U32(P.text, 0.05f), 8.f * s);
                text(pdl, fBody, ImVec2(cr0.x + 12.f * s, cr0.y + rh * 0.5f - fs(fBody) * 0.5f - 1.f * s), U32(P.text), "CRT screens");
                const float tw = 34.f * s, th = 19.f * s;
                const ImVec2 ta(cr0.x + w - tw - 10.f * s, cr0.y + rh * 0.5f - th * 0.5f), tb(ta.x + tw, ta.y + th);
                if (crtEnabled) gradRectTheme(pdl, ta, tb, th * 0.5f);
                else pdl->AddRectFilled(ta, tb, U32(P.track, 1.5f), th * 0.5f);
                const float cr = th * 0.5f - 3.f * s;
                pdl->AddCircleFilled(ImVec2(crtEnabled ? tb.x - th * 0.5f : ta.x + th * 0.5f, ta.y + th * 0.5f), cr,
                                     IM_COL32_WHITE, 20);
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
    }

    // ---- zoom handling shared by all visualisers ----

    struct MiniButton { bool clicked = false, hovered = false; ImVec2 a, b; };

    MiniButton miniButton(const char* id, ImVec2 a, ImVec2 b)
    {
        MiniButton m;
        m.a = a; m.b = b;
        ImGui::SetCursorScreenPos(a);
        m.clicked = ImGui::InvisibleButton(id, b - a);
        m.hovered = ImGui::IsItemHovered();
        if (m.hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        return m;
    }

    // "- x2.0 +" cluster in the bottom-right corner of a view: input part (call before zoomInput)
    struct ZoomButtons { MiniButton minus, plus, label; };

    ZoomButtons zoomButtons(const char* id, ImVec2 viewB, ViewZoom& z, float minT, float maxT)
    {
        ImGui::PushID(id);
        const float bh = 18.f * s, bw = 18.f * s, lw = 42.f * s;
        const float y = viewB.y - bh - 6.f * s;
        const float xr = viewB.x - 6.f * s;
        ZoomButtons zb;
        zb.plus  = miniButton("##zp", ImVec2(xr - bw, y), ImVec2(xr, y + bh));
        zb.label = miniButton("##zl", ImVec2(xr - bw - lw, y), ImVec2(xr - bw, y + bh));
        zb.minus = miniButton("##zm", ImVec2(xr - bw * 2.f - lw, y), ImVec2(xr - bw - lw, y + bh));
        if (zb.plus.clicked)  z.time = std::min(maxT, z.time * 2.f);
        if (zb.minus.clicked) z.time = std::max(minT, z.time * 0.5f);
        if (zb.label.clicked) z = ViewZoom();
        ImGui::PopID();
        return zb;
    }

    // drawing part (call after the view content so it sits on top)
    void drawZoomButtons(ImDrawList* dl, const ZoomButtons& zb, const ViewZoom& z)
    {
        const ImVec2 a = zb.minus.a, b = zb.plus.b;
        const float r = (b.y - a.y) * 0.5f;
        dl->AddRectFilled(a, b, U32(P.inset, 0.9f), r);
        dl->AddRect(a, b, U32(P.panelBorder), r, 0, 1.f);
        auto sym = [&](const MiniButton& m, bool plus) {
            const ImVec2 c = (m.a + m.b) * 0.5f;
            const ImU32 col = U32(m.hovered ? P.text : P.textDim);
            dl->AddLine(ImVec2(c.x - 4.f * s, c.y), ImVec2(c.x + 4.f * s, c.y), col, 1.5f * s);
            if (plus)
                dl->AddLine(ImVec2(c.x, c.y - 4.f * s), ImVec2(c.x, c.y + 4.f * s), col, 1.5f * s);
        };
        sym(zb.minus, false);
        sym(zb.plus, true);
        char buf[24];
        if (std::fabs(z.amp - 1.f) > 0.01f)
            std::snprintf(buf, sizeof(buf), "%.3gx|%.2gx", z.time, z.amp);
        else
            std::snprintf(buf, sizeof(buf), "%.3gx", z.time);
        const bool zoomed = std::fabs(z.time - 1.f) > 0.01f || std::fabs(z.amp - 1.f) > 0.01f;
        textCentered(dl, fSmall, (zb.label.a + zb.label.b) * 0.5f,
                     U32(zb.label.hovered ? P.text : (zoomed ? P.grad(0.6f) : P.textDim)), buf);
    }

    // wheel = time zoom, shift+wheel = amplitude zoom, drag = pan (pannable views), double-click = reset
    bool zoomInput(const char* id, ImVec2 a, ImVec2 b, ViewZoom& z, float minT, float maxT, bool pannable,
                   bool doubleClickReset = true)
    {
        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton(id, b - a);
        const bool hov = ImGui::IsItemHovered();
        const bool act = ImGui::IsItemActive();
        const ImGuiIO& io = ImGui::GetIO();
        const float w = std::max(1.f, b.x - a.x);

        if (doubleClickReset && hov && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            z = ViewZoom();
            return hov;
        }

        if (hov)
        {
            if (io.KeyShift && (io.MouseWheel != 0.f || io.MouseWheelH != 0.f))
            {
                const float wheel = io.MouseWheel != 0.f ? io.MouseWheel : io.MouseWheelH;
                z.amp = std::max(0.5f, std::min(16.f, z.amp * std::pow(1.25f, wheel)));
            }
            else if (io.MouseWheel != 0.f)
            {
                const float m = std::max(0.f, std::min(1.f, (io.MousePos.x - a.x) / w));
                const float anchor = z.offset + m / z.time;
                z.time = std::max(minT, std::min(maxT, z.time * std::pow(1.25f, io.MouseWheel)));
                if (pannable)
                    z.offset = anchor - m / z.time;
            }
        }

        if (act && pannable && io.MouseDelta.x != 0.f)
            z.offset -= io.MouseDelta.x / w / z.time;
        if (pannable)
            z.offset = std::max(0.f, std::min(std::max(0.f, 1.f - 1.f / z.time), z.offset));
        else
            z.offset = 0.f;

        if (pannable && (act || (hov && z.time > 1.001f)))
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        return hov || act;
    }

    void zoomHint(ImDrawList* dl, ImVec2 a, ImVec2 b, bool pannable, const char* custom = nullptr)
    {
        const char* hint = custom != nullptr ? custom : pannable ? "wheel: zoom  \xc2\xb7  shift+wheel: height  \xc2\xb7  drag: pan  \xc2\xb7  double-click: reset"
                                    : "wheel: zoom  \xc2\xb7  shift+wheel: height  \xc2\xb7  double-click: reset";
        const ImVec2 ts = textSize(fSmall, hint);
        const ImVec2 ha(a.x + 6.f * s, b.y - ts.y - 9.f * s);
        if (ha.x + ts.x + 12.f * s > b.x - 90.f * s)
            return; // not enough room next to the zoom buttons
        dl->AddRectFilled(ha, ImVec2(ha.x + ts.x + 12.f * s, ha.y + ts.y + 4.f * s), U32(P.inset, 0.85f), 6.f * s);
        text(dl, fSmall, ImVec2(ha.x + 6.f * s, ha.y + 2.f * s), U32(P.textDim), hint);
    }

    // small toggle pill used for the envelope lanes; returns its width
    float envPill(ImDrawList* dl, const MiniButton& m, const char* label, const ImVec4& col, bool on)
    {
        const float r = (m.b.y - m.a.y) * 0.5f;
        dl->AddRectFilled(m.a, m.b, U32(P.inset, 0.9f), r);
        dl->AddRect(m.a, m.b, on ? U32(col, 0.8f) : U32(P.panelBorder), r, 0, 1.f);
        dl->AddCircleFilled(ImVec2(m.a.x + 9.f * s, (m.a.y + m.b.y) * 0.5f), 3.f * s, on ? U32(col) : U32(P.textFaint), 12);
        text(dl, fSmall, ImVec2(m.a.x + 16.f * s, m.a.y + (m.b.y - m.a.y - fs(fSmall)) * 0.5f - 0.5f * s),
             U32(on ? P.text : (m.hovered ? P.textDim : P.textFaint)), label);
        return m.b.x - m.a.x;
    }

    float pillWidth(const char* label) const { return textSize(fSmall, label).x + 24.f * s; }

    // ---- capture buffer visualiser ----
    void drawBufferView(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        const float top = panel(dl, a, b, "CAPTURE BUFFER");
        const ImVec2 ia(a.x + 14.f * s, top), ib(b.x - 14.f * s, b.y - 14.f * s);
        inset(dl, ia, ib, 10.f * s);
        crtAdd(ia, ib, 10.f * s);

        if (vis == nullptr)
        {
            textCentered(dl, fBody, (ia + ib) * 0.5f, U32(P.textDim), "Visualisation unavailable in this host");
            return;
        }

        const float rec = vis->recPos.load(std::memory_order_relaxed);
        const float play = vis->playPos.load(std::memory_order_relaxed);
        const float cutEnd = vis->cutEnd.load(std::memory_order_relaxed);
        const float dutyEnd = vis->dutyEnd.load(std::memory_order_relaxed);
        const float cutStart = vis->cutStart.load(std::memory_order_relaxed);
        const int rep = vis->cutRepeat.load(std::memory_order_relaxed);
        const int nIn = vis->cutsInBlock.load(std::memory_order_relaxed);
        const int curCut = vis->currentCut.load(std::memory_order_relaxed);
        const bool active = play >= 0.f && nIn > 0;

        // info chips
        {
            char buf[48];
            float x = b.x - 16.f * s;
            const float y = a.y + 12.f * s;
            if (active)
            {
                const bool gateNow = algoFromParams(pv) == kAlgoGate;
                std::snprintf(buf, sizeof(buf), tr(gateNow ? "STEP %d / %d" : "REPEAT %d / %d"), rep + 1, nIn);
                chip(dl, ImVec2(x, y), buf, P.b, x);
                if (curCut >= 0 && curCut < phrase.numCuts && std::fabs(phrase.cuts[curCut].cents) > 1.f)
                {
                    std::snprintf(buf, sizeof(buf), "%+.1f st", phrase.cuts[curCut].cents / 100.f);
                    chip(dl, ImVec2(x, y), buf, P.c, x);
                }
            }
            else
            {
                chip(dl, ImVec2(x, y), "IDLE", P.textDim, x);
            }
        }

        // ---- input: envelope toggle, zoom buttons, then the view itself ----
        const MiniButton envBtn = miniButton("##bufenv", ImVec2(ia.x + 6.f * s, ia.y + 6.f * s),
                                             ImVec2(ia.x + 6.f * s + pillWidth("ENVELOPE"), ia.y + 24.f * s));
        if (envBtn.clicked)
            showEnv[0] = ! showEnv[0];
        const ZoomButtons zb = zoomButtons("bufzoom", ib, zBuf, 1.f, 64.f);
        const bool viewHot = zoomInput("##bufview", ia, ib, zBuf, 1.f, 64.f, true);

        const float pad = 8.f * s;
        const float x0 = ia.x + pad, x1 = ib.x - pad;
        const float w = x1 - x0;
        const float midY = (ia.y + ib.y) * 0.5f;
        const float hh = (ib.y - ia.y) * 0.5f - pad - 6.f * s;
        const float off = zBuf.offset, zt = zBuf.time;
        auto X = [&](float t) { return x0 + (t - off) * zt * w; };

        dl->PushClipRect(ImVec2(ia.x + 1.f, ia.y), ImVec2(ib.x - 1.f, ib.y), true);

        // cut region
        if (active)
        {
            const float ce = X(cutEnd), de = X(dutyEnd), cs = X(cutStart);
            gradRect(dl, ImVec2(cs, ia.y + 4.f * s), ImVec2(std::max(cs + 1.f, de), ib.y - 4.f * s),
                     withA(P.a, 0.10f), withA(P.b, 0.10f), 6.f * s, true);
            if (ce > de + 1.f)
            {
                // duty-cycle "off" part: hatched
                dl->PushClipRect(ImVec2(std::max(de, ia.x), ia.y), ImVec2(std::min(ce, ib.x), ib.y), true);
                for (float hx = de - (ib.y - ia.y); hx < ce; hx += 7.f * s)
                    dl->AddLine(ImVec2(hx, ib.y), ImVec2(hx + (ib.y - ia.y), ia.y), U32(P.textDim, 0.12f), 1.f);
                dl->PopClipRect();
            }
            dl->AddLine(ImVec2(ce, ia.y + 4.f * s), ImVec2(ce, ib.y - 4.f * s), U32(P.b, 0.45f), 1.f);
        }

        // center line
        dl->AddLine(ImVec2(x0, midY), ImVec2(x1, midY), U32(P.textFaint, 0.5f), 1.f);

        // waveform (L up, R down) over the visible part of the buffer
        {
            const int B = LiveCutVis::kBufferBins;
            const float binStart = off * B;
            const float binCount = B / zt;
            const float colW = std::max(2.f, 3.f * s);
            const int ncols = std::max(1, std::min(static_cast<int>(w / colW), static_cast<int>(binCount)));
            const float cw = w / ncols;
            for (int c = 0; c < ncols; ++c)
            {
                const int b0 = static_cast<int>(binStart + c * binCount / ncols);
                const int b1 = std::max(b0 + 1, static_cast<int>(binStart + (c + 1) * binCount / ncols));
                float pl = 0.f, pr = 0.f;
                for (int k = b0; k < b1 && k < B; ++k)
                {
                    pl = std::max(pl, vis->bufferPeakL[k]);
                    pr = std::max(pr, vis->bufferPeakR[k]);
                }
                const float t = (b0 + 0.5f) / B;
                const float x = x0 + c * cw;
                const float hl = std::max(1.f * s, std::pow(std::min(1.f, pl * zBuf.amp), 0.65f) * hh);
                const float hr = std::max(1.f * s, std::pow(std::min(1.f, pr * zBuf.amp), 0.65f) * hh);
                const bool inCut = active && t >= cutStart && t <= dutyEnd;
                const bool played = active && t >= cutStart && t <= play;
                const float al = ! active ? 0.35f : (played ? 1.f : (inCut ? 0.75f : 0.35f));
                const ImVec4 colr = P.grad(t);
                const float gapPx = cw > 3.f * s ? 1.f * s : 0.5f * s;
                dl->AddRectFilled(ImVec2(x, midY - hl), ImVec2(x + cw - gapPx, midY - 0.5f * s), U32(colr, al), 1.f * s);
                dl->AddRectFilled(ImVec2(x, midY + 0.5f * s), ImVec2(x + cw - gapPx, midY + hr), U32(colr, al * 0.8f), 1.f * s);
            }
        }

        if (active)
        {
            // envelope of the current cut: amp x fade-in/out, zero during the duty-cycle gap
            if (showEnv[0] && dutyEnd > cutStart)
            {
                const float amp = vis->cutAmp.load(std::memory_order_relaxed);
                const float F = std::max(1e-4f, vis->fadeFrac.load(std::memory_order_relaxed));
                const float ey0 = ib.y - pad, ey1 = ia.y + pad + 26.f * s; // 0 .. 1
                ImVec2 pts[260];
                int n = 0;
                const int steps = 256;
                for (int k = 0; k <= steps && n < 256; ++k)
                {
                    // denser sampling near the edges where the fades are
                    const float lin = k / float(steps);
                    const float u = 0.5f - 0.5f * std::cos(lin * IM_PI);
                    const float e = amp * (1.f - std::exp(-5.f * u / F)) * (1.f - std::exp(5.f * (u - 1.f) / F));
                    pts[n++] = ImVec2(X(cutStart + u * (dutyEnd - cutStart)), ey0 + (ey1 - ey0) * std::max(0.f, std::min(1.f, e)));
                }
                pts[n++] = ImVec2(X(cutEnd), ey0);
                dl->AddPolyline(pts, n, U32(P.shadow), 0, 4.f * s);
                dl->AddPolyline(pts, n, U32(P.text, 0.9f), 0, 1.6f * s);
                dl->AddCircleFilled(pts[0], 2.5f * s, U32(P.text), 10);
            }

            // record head
            if (rec < 1.f)
            {
                const float rx = X(rec);
                dl->AddRectFilled(ImVec2(rx, ia.y), ImVec2(ib.x, ib.y), U32(P.inset, 0.5f));
                dl->AddLine(ImVec2(rx, ia.y + 4.f * s), ImVec2(rx, ib.y - 4.f * s), U32(P.rec), 1.5f * s);
                const char* lbl = "REC";
                const ImVec2 ls = textSize(fSmall, lbl);
                const ImVec2 la(rx + 4.f * s, ia.y + 6.f * s);
                dl->AddRectFilled(la, ImVec2(la.x + ls.x + 10.f * s, la.y + ls.y + 4.f * s), U32(P.rec, 0.9f), 4.f * s);
                text(dl, fSmall, ImVec2(la.x + 5.f * s, la.y + 2.f * s), IM_COL32_WHITE, lbl);
            }

            // play head
            const float px = X(play);
            for (int g = 3; g >= 1; --g)
                dl->AddLine(ImVec2(px, ia.y + 4.f * s), ImVec2(px, ib.y - 4.f * s), U32(P.b, 0.08f), g * 4.f * s);
            dl->AddLine(ImVec2(px, ia.y + 4.f * s), ImVec2(px, ib.y - 4.f * s), U32(P.text), 1.5f * s);
            dl->AddTriangleFilled(ImVec2(px - 5.f * s, ib.y - 4.f * s), ImVec2(px + 5.f * s, ib.y - 4.f * s),
                                  ImVec2(px, ib.y - 10.f * s), U32(P.text));
        }
        else
        {
            const bool playing = vis->playing.load(std::memory_order_relaxed);
            textCentered(dl, fBody, ImVec2((ia.x + ib.x) * 0.5f, midY), U32(P.textDim),
                         playing ? "Waiting for the next bar..." : "Start host playback to begin slicing");
        }

        // overview strip when zoomed: where the view sits inside the buffer
        if (zt > 1.001f)
        {
            const float sy = ia.y + 2.f * s;
            dl->AddRectFilled(ImVec2(x0, sy), ImVec2(x1, sy + 3.f * s), U32(P.track), 1.5f * s);
            dl->AddRectFilled(ImVec2(x0 + off * w, sy), ImVec2(x0 + (off + 1.f / zt) * w, sy + 3.f * s), U32(P.b, 0.8f), 1.5f * s);
        }

        dl->PopClipRect();

        envPill(dl, envBtn, "ENVELOPE", P.text, showEnv[0]);
        if (viewHot)
            zoomHint(dl, ia, ib, true);
        drawZoomButtons(dl, zb, zBuf);
    }

    // ---- live cuts visualiser ----
    void drawCutsView(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        const float top = panel(dl, a, b, "LIVE CUTS");
        const ImVec2 ia(a.x + 14.f * s, top), ib(b.x - 14.f * s, b.y - 14.f * s);

        if (vis == nullptr)
        {
            inset(dl, ia, ib, 10.f * s);
            textCentered(dl, fBody, (ia + ib) * 0.5f, U32(P.textDim), "Visualisation unavailable in this host");
            return;
        }

        LiveCutVis::Phrase tmp;
        if (vis->readPhrase(tmp))
            phrase = tmp;

        const int curBlock = vis->currentBlock.load(std::memory_order_relaxed);
        const int curCut = vis->currentCut.load(std::memory_order_relaxed);
        const float unitPos = vis->unitPos.load(std::memory_order_relaxed);
        const int total = std::max(1, phrase.totalUnits);
        const int subdiv = std::max(1, phrase.subdiv);
        const bool fillMode = vis->fillMode.load(std::memory_order_relaxed);
        const bool fillActive = vis->fillActive.load(std::memory_order_relaxed);
        const int barInCycle = vis->barInCycle.load(std::memory_order_relaxed);
        const int cycleBars = std::max(1, vis->cycleBars.load(std::memory_order_relaxed));
        const int fillBars = vis->fillBars.load(std::memory_order_relaxed);
        const bool dryNow = fillMode && ! fillActive;

        // chips
        {
            char buf[48];
            float x = b.x - 16.f * s;
            const float y = a.y + 12.f * s;
            if (fillMode)
            {
                if (fillActive)
                    std::snprintf(buf, sizeof(buf), tr("FILL %d / %d"), barInCycle - (cycleBars - fillBars) + 1, fillBars);
                else
                    std::snprintf(buf, sizeof(buf), trn("FILL IN %d BAR%s", cycleBars - fillBars - barInCycle), cycleBars - fillBars - barInCycle,
                                  cycleBars - fillBars - barInCycle == 1 ? "" : "S");
                chip(dl, ImVec2(x, y), buf, fillActive ? P.c : P.textDim, x);
            }
            if (pv[LVC_SLICER] <= 0.5f)
                chip(dl, ImVec2(x, y), "SLICER OFF", P.rec, x);
            if (algoFromParams(pv) == kAlgoGate)
                chip(dl, ImVec2(x, y), pv[LVC_GATEMODE] > 0.5f ? "GATE PATTERN" : "GATE RANDOM", algoColor(kAlgoGate), x);
            std::snprintf(buf, sizeof(buf), trn("%d BAR PHRASE", std::max(0, phrase.totalUnits / subdiv)), std::max(0, phrase.totalUnits / subdiv));
            chip(dl, ImVec2(x, y), buf, P.a, x);
            const bool playing = vis->playing.load(std::memory_order_relaxed);
            std::snprintf(buf, sizeof(buf), "%s %.1f BPM", playing ? "\xe2\x80\xa2" : "", vis->bpm.load(std::memory_order_relaxed));
            chip(dl, ImVec2(x, y), buf[0] == ' ' ? buf + 1 : buf, playing ? P.b : P.textDim, x);
            if (curBlock >= 0 && curBlock < phrase.numBlocks)
            {
                std::snprintf(buf, sizeof(buf), trn("%d CUTS", phrase.blocks[curBlock].numCuts), phrase.blocks[curBlock].numCuts);
                chip(dl, ImVec2(x, y), buf, P.blockColor(curBlock), x);
            }
        }

        const float gapY = 10.f * s;
        const float laneH = (ib.y - ia.y - gapY);
        // the gate's step sequencer gets more room than the phrase map: its cells are drawn into
        const ImVec2 pa = ia, pb(ib.x, ia.y + laneH * (algoFromParams(pv) == kAlgoGate ? 0.6f : 0.42f));
        const ImVec2 sa(ia.x, pb.y + gapY), sb = ib;

        // ===== phrase map (trance gate: a one-bar step sequencer instead) =====
        inset(dl, pa, pb, 10.f * s);
        if (algoFromParams(pv) == kAlgoGate)
            drawGateSequencer(dl, pa, pb, dryNow);
        else
        {
            const ZoomButtons zb = zoomButtons("mapzoom", pb, zMap, 1.f, 16.f);
            const bool viewHot = zoomInput("##mapview", pa, pb, zMap, 1.f, 16.f, false);

            const float pad = 10.f * s;
            const float x0 = pa.x + pad, x1 = pb.x - pad;
            const float w = x1 - x0;
            const float rulerH = 14.f * s;
            const float y0 = pa.y + pad, y1 = pb.y - rulerH - 4.f * s;
            const float midY = (y0 + y1) * 0.5f;
            const float maxH = (y1 - y0);

            // zoomed: follow the playhead
            const float viewUnits = total / zMap.time;
            const float start = zMap.time > 1.001f
                ? std::max(0.f, std::min(total - viewUnits, unitPos - viewUnits * 0.35f)) : 0.f;
            const float uw = w / viewUnits;
            auto X = [&](float u) { return x0 + (u - start) * uw; };

            dl->PushClipRect(ImVec2(pa.x + 1.f, pa.y), ImVec2(pb.x - 1.f, pb.y), true);

            // ruler
            const int u0 = std::max(0, static_cast<int>(std::floor(start)));
            const int u1 = std::min(total, static_cast<int>(std::ceil(start + viewUnits)));
            for (int u = u0; u <= u1; ++u)
            {
                const float x = X(static_cast<float>(u));
                const bool bar = (u % subdiv) == 0;
                if (bar)
                {
                    dl->AddLine(ImVec2(x, y0 - 2.f * s), ImVec2(x, pb.y - 4.f * s), U32(P.textFaint, 0.45f), 1.f);
                    if (u < total)
                    {
                        char bb[16];
                        std::snprintf(bb, sizeof(bb), "%d", u / subdiv + 1);
                        text(dl, fSmall, ImVec2(x + 4.f * s, pb.y - rulerH - 2.f * s), U32(P.textFaint), bb);
                    }
                }
                else if (uw > 4.f * s)
                {
                    dl->AddLine(ImVec2(x, pb.y - 6.f * s), ImVec2(x, pb.y - 3.f * s), U32(P.textFaint, 0.6f), 1.f);
                }
            }

            if (phrase.numBlocks == 0)
                textCentered(dl, fSmall, ImVec2((x0 + x1) * 0.5f, midY), U32(P.textFaint), "no phrase yet");

            // blocks and cuts
            const float dim = dryNow ? 0.35f : 1.f;
            for (int bi = 0; bi < phrase.numBlocks; ++bi)
            {
                const LiveCutVis::Block& blk = phrase.blocks[bi];
                const ImVec4 bc = P.blockColor(bi);
                const float bx0 = X(static_cast<float>(blk.startUnit));
                const float bx1 = X(static_cast<float>(blk.startUnit + blk.units));
                if (bx1 < pa.x || bx0 > pb.x)
                    continue;
                const bool isCurBlock = bi == curBlock;

                dl->AddRectFilled(ImVec2(bx0 + 1.f, y0), ImVec2(bx1 - 1.f, y1), U32(bc, (isCurBlock ? 0.10f : 0.05f) * dim), 5.f * s);
                dl->AddRect(ImVec2(bx0 + 1.f, y0), ImVec2(bx1 - 1.f, y1), U32(bc, (isCurBlock ? 0.45f : 0.18f) * dim), 5.f * s, 0, 1.f);

                float cx = bx0;
                for (int ci = 0; ci < blk.numCuts; ++ci)
                {
                    const int gi = blk.firstCut + ci;
                    if (gi < 0 || gi >= phrase.numCuts)
                        break;
                    const LiveCutVis::Cut& cut = phrase.cuts[gi];
                    const float cw = cut.units * uw;
                    const float h = maxH * (0.30f + 0.62f * std::min(1.f, std::max(0.f, cut.amp)));
                    const float cy0 = midY - h * 0.5f, cy1 = midY + h * 0.5f;
                    const float inner = std::min(1.5f * s, cw * 0.15f);
                    const float dx1 = cx + std::max(1.f, cw * std::min(1.f, std::max(0.f, cut.duty)));

                    const bool isCur = gi == curCut;
                    const bool past = isCurBlock ? gi < curCut : bi < curBlock;
                    const float al = (isCur ? 1.f : (past ? 0.62f : 0.28f)) * dim;

                    if (isCur && ! dryNow)
                        glow(dl, ImVec2(cx + inner, cy0), ImVec2(cx + cw - inner, cy1), bc, 3.f * s, 1.4f);

                    gradRect(dl, ImVec2(cx + inner, cy0), ImVec2(std::max(cx + inner + 1.f, dx1 - inner), cy1),
                             withA(mix(bc, P.text, isCur ? 0.25f : 0.f), al), withA(bc, al), 3.f * s);
                    if (cw * (1.f - cut.duty) > 2.f)
                        dl->AddRect(ImVec2(dx1 - inner, cy0), ImVec2(cx + cw - inner, cy1), U32(bc, al * 0.6f), 3.f * s, 0, 1.f);
                    if (isCur)
                        dl->AddRect(ImVec2(cx + inner, cy0), ImVec2(cx + cw - inner, cy1), U32(P.text, 0.9f * dim), 3.f * s, 0, 1.5f * s);

                    if (std::fabs(cut.cents) > 1.f && cw > 6.f * s)
                    {
                        const float py = midY - (cut.cents / 2400.f) * h * 0.45f;
                        dl->AddLine(ImVec2(cx + inner + 2.f * s, py), ImVec2(cx + cw - inner - 2.f * s, py),
                                    U32(P.onAccent, 0.8f * al), 1.5f * s);
                    }
                    // reverb throw: ripple in the top-right corner, size by send level
                    if (cut.verb > 0.001f && cw > 7.f * s)
                    {
                        const ImVec2 rc(cx + cw - inner - 5.f * s, cy0 + 5.f * s);
                        const ImU32 rcol = U32(P.onAccent, 0.95f * std::max(al, 0.4f));
                        dl->AddCircleFilled(rc, 1.8f * s, rcol, 10);
                        dl->AddCircle(rc, (2.8f + 2.2f * cut.verb) * s, rcol, 14, 1.f * s);
                    }
                    // euclid slice: this cut replays audio from later in the block
                    if (cut.offset > 0.01f && cw > 14.f * s && blk.kind != kBlockGate) // gate steps play in place
                    {
                        char ob[16];
                        std::snprintf(ob, sizeof(ob), "+%d", static_cast<int>(cut.offset + 0.5f));
                        text(dl, fSmall, ImVec2(cx + inner + 3.f * s, cy0 + 1.f * s), U32(P.onAccent, 0.9f * al), ob);
                    }
                    if (std::fabs(cut.pan) > 0.02f && cw > 8.f * s)
                    {
                        const float pxm = cx + cw * 0.5f + cut.pan * (cw * 0.5f - inner - 3.f * s);
                        dl->AddCircleFilled(ImVec2(pxm, cy1 - 4.f * s), 1.8f * s, U32(P.onAccent, 0.85f * al), 10);
                    }
                    cx += cw;
                }
            }

            // playhead
            if (phrase.numBlocks > 0)
            {
                const float px = X(std::min(static_cast<float>(total), unitPos));
                dl->AddLine(ImVec2(px, y0 - 4.f * s), ImVec2(px, y1 + 4.f * s), U32(P.text, 0.9f), 1.5f * s);
                dl->AddTriangleFilled(ImVec2(px - 5.f * s, y0 - 6.f * s), ImVec2(px + 5.f * s, y0 - 6.f * s),
                                      ImVec2(px, y0), U32(P.text));
            }

            if (dryNow)
            {
                char buf[48];
                std::snprintf(buf, sizeof(buf), trn("DRY  \xc2\xb7  fill starts in %d bar%s", cycleBars - fillBars - barInCycle), cycleBars - fillBars - barInCycle,
                              cycleBars - fillBars - barInCycle == 1 ? "" : "s");
                const ImVec2 ts = textSize(fBold, buf);
                const ImVec2 c((pa.x + pb.x) * 0.5f, (pa.y + pb.y) * 0.5f);
                dl->AddRectFilled(ImVec2(c.x - ts.x * 0.5f - 12.f * s, c.y - ts.y * 0.5f - 6.f * s),
                                  ImVec2(c.x + ts.x * 0.5f + 12.f * s, c.y + ts.y * 0.5f + 6.f * s), U32(P.inset, 0.92f), 8.f * s);
                textCentered(dl, fBold, c, U32(P.textDim), buf);
            }

            dl->PopClipRect();
            if (viewHot)
                zoomHint(dl, pa, pb, false);
            drawZoomButtons(dl, zb, zMap);
        }

        // ===== scrolling output + envelopes =====
        inset(dl, sa, sb, 10.f * s);
        {
            // envelope lane toggles (top right)
            static const char* const envNames[6] = { "AMP", "BITS", "RATE", "COMB", "LPF", "VERB" };
            const ImVec4 envCols[6] = { P.text, P.c, P.rec, P.b, P.a, mix(P.b, P.text, 0.45f) };
            MiniButton envBtns[6];
            {
                float x = sb.x - 6.f * s;
                for (int k = 5; k >= 0; --k)
                {
                    const float pw = pillWidth(envNames[k]);
                    ImGui::PushID(k + 400);
                    envBtns[k] = miniButton("##envlane", ImVec2(x - pw, sa.y + 5.f * s), ImVec2(x, sa.y + 23.f * s));
                    ImGui::PopID();
                    if (envBtns[k].clicked)
                        showEnv[k] = ! showEnv[k];
                    x -= pw + 4.f * s;
                }
            }
            const ZoomButtons zb = zoomButtons("scopezoom", sb, zScope, 0.5f, 32.f);
            const bool viewHot = zoomInput("##scopeview", sa, sb, zScope, 0.5f, 32.f, false);

            const float pad = 8.f * s;
            const float x0 = sa.x + pad, x1 = sb.x - pad;
            const float w = x1 - x0;
            const float midY = (sa.y + sb.y) * 0.5f;
            const float hh = (sb.y - sa.y) * 0.5f - pad;
            const float envTop = sa.y + 28.f * s, envBot = sb.y - pad;

            dl->PushClipRect(ImVec2(sa.x + 1.f, sa.y), ImVec2(sb.x - 1.f, sb.y), true);

            const int N = LiveCutVis::kScopeColumns;
            const float colsPerView = std::min(static_cast<float>(N), (N / float(LiveCutVis::kScopeBars)) * 2.f / zScope.time);
            const uint32_t wr = vis->scopeWrite.load(std::memory_order_acquire);
            // one point per pixel (at most), drawn as antialiased bands and contours
            const int npx = std::max(2, std::min(std::min(static_cast<int>(w / std::max(1.f, 1.f * s)), kScopePts - 1),
                                                 static_cast<int>(colsPerView)));
            const float pxw = w / (npx - 1);
            const float firstCol = N - colsPerView;
            const float ampZ = zScope.amp;

            dl->AddLine(ImVec2(x0, midY), ImVec2(x1, midY), U32(P.textFaint, 0.35f), 1.f);

            auto shape = [ampZ](float v) {
                const float a2 = std::min(1.f, std::fabs(v) * ampZ);
                return (v < 0.f ? -1.f : 1.f) * std::pow(a2, 0.7f);
            };

            int n = 0;
            const ImU32 ghostCol = U32(P.textDim, P.dark ? 0.10f : 0.14f);
            for (int p = 0; p < npx; ++p)
            {
                const int c0 = static_cast<int>(firstCol + (p - 0.5f) * colsPerView / (npx - 1));
                const int c1 = std::max(c0 + 1, static_cast<int>(firstCol + (p + 0.5f) * colsPerView / (npx - 1)));
                float mn = 1e9f, mx = -1e9f, in = 0.f;
                float env = 0.f, bits = 0.f, rate = 0.f, comb = 0.f, lpf = 0.f, verb = 0.f;
                int cnt = 0;
                uint8_t flags = 0;
                int blk = -1;
                bool silent = true, bypass = true;
                for (int c = std::max(0, c0); c < std::min(N, c1); ++c)
                {
                    // column c in [0,N): oldest at 0, newest at N-1
                    const int64_t abs = static_cast<int64_t>(wr) - N + c;
                    if (abs < 0)
                        continue;
                    const int ri = static_cast<int>(abs % N);
                    ++cnt;
                    mn = std::min(mn, vis->scopeMin[ri]);
                    mx = std::max(mx, vis->scopeMax[ri]);
                    in = std::max(in, vis->scopeIn[ri]);
                    env += vis->scopeEnv[ri];
                    bits += vis->scopeBits[ri];
                    rate += vis->scopeRate[ri];
                    comb += vis->scopeComb[ri];
                    lpf += std::fabs(vis->scopeLP[ri]);
                    verb = std::max(verb, vis->scopeVerb[ri]);
                    const uint8_t f = vis->scopeFlags[ri];
                    flags |= f;
                    if (vis->scopeBlock[ri] >= 0 && blk < 0)
                        blk = vis->scopeBlock[ri];
                    if (! (f & LiveCutVis::kFlagSilent))
                        silent = false;
                    if (! (f & LiveCutVis::kFlagBypass))
                        bypass = false;
                }
                if (cnt == 0)
                    continue;

                const float x = x0 + p * pxw;
                if (mx < mn) { mx = 0.f; mn = 0.f; }
                const float ih = std::pow(std::min(1.f, in * ampZ), 0.7f) * hh;
                const ImVec4 bc = bypass ? P.textDim : P.blockColor(blk);
                const float al = bypass ? 0.45f : (blk < 0 ? 0.3f : (silent ? 0.35f : 0.95f));

                sX[n] = x;
                sGTop[n] = midY - ih;
                sGBot[n] = midY + ih;
                sGCol[n] = ghostCol;
                sTop[n] = std::min(midY - shape(mx) * hh, midY - 0.5f);
                sBot[n] = std::max(midY - shape(mn) * hh, midY + 0.5f);
                sFill[n] = U32(bc, al * 0.62f);
                sLine[n] = U32(bc, al);
                sFlags[n] = flags;
                sBlk[n] = static_cast<int16_t>(blk);
                sBypass[n] = bypass;

                const float inv = 1.f / cnt;
                const float vals[6] = {
                    std::min(1.f, env * inv),
                    std::max(0.f, std::min(1.f, (bits * inv - 1.f) / 31.f)),
                    std::max(0.f, std::min(1.f, std::log(std::max(441.f, rate * inv) / 441.f) / std::log(100.f))),
                    std::max(0.f, std::min(1.f, comb * inv / 50.f)),
                    std::max(0.f, std::min(1.f, std::log(std::max(20.f, lpf * inv) / 20.f) / std::log(1000.f))),
                    std::min(1.f, verb),
                };
                for (int k = 0; k < 6; ++k)
                    envPts[k][n] = ImVec2(x, envBot + (envTop - envBot) * vals[k]);
                ++n;
            }
            const int envN = n;

            // bar / beat grid
            for (int i = 0; i < n; ++i)
            {
                if (sFlags[i] & LiveCutVis::kFlagBar)
                    dl->AddLine(ImVec2(sX[i], sa.y + 4.f * s), ImVec2(sX[i], sb.y - 4.f * s), U32(P.textFaint, 0.5f), 1.f);
                else if (sFlags[i] & LiveCutVis::kFlagBeat)
                    dl->AddLine(ImVec2(sX[i], sb.y - 10.f * s), ImVec2(sX[i], sb.y - 4.f * s), U32(P.textFaint, 0.6f), 1.f);
            }

            // input ghost, amplitude-envelope fill, output band + antialiased edges
            fillBand(dl, sX, sGTop, sGBot, sGCol, n);
            if (showEnv[0] && n >= 2)
            {
                const ImU32 ef = U32(P.text, 0.05f);
                for (int i = 0; i < n; ++i) { sEnvY[i] = envPts[0][i].y; sEnvB[i] = envBot; sEnvC[i] = ef; }
                fillBand(dl, sX, sEnvY, sEnvB, sEnvC, n);
            }
            fillBand(dl, sX, sTop, sBot, sFill, n);
            contour(dl, sX, sTop, sLine, n, std::max(1.f, 1.2f * s));
            contour(dl, sX, sBot, sLine, n, std::max(1.f, 1.2f * s));

            // block / cut markers
            for (int i = 0; i < n; ++i)
            {
                if (sBypass[i])
                    continue;
                const float x = sX[i];
                if (sFlags[i] & LiveCutVis::kFlagBlockStart)
                {
                    const ImVec4 bc = P.blockColor(sBlk[i]);
                    dl->AddLine(ImVec2(x, sa.y + 2.f * s), ImVec2(x, sb.y - 2.f * s), U32(bc, 0.9f), 1.5f * s);
                    dl->AddTriangleFilled(ImVec2(x, sa.y + 2.f * s), ImVec2(x + 7.f * s, sa.y + 2.f * s),
                                          ImVec2(x, sa.y + 9.f * s), U32(bc));
                }
                else if (sFlags[i] & LiveCutVis::kFlagCutStart)
                {
                    dl->AddLine(ImVec2(x, sa.y + 10.f * s), ImVec2(x, sb.y - 10.f * s), U32(P.text, 0.22f), 1.f);
                }
            }

            // envelope lines on top of the waveform
            const bool fxOn[6] = { true, pv[LVC_CRUSHER] > 0.5f, pv[LVC_CRUSHER] > 0.5f, pv[LVC_COMB] > 0.5f, pv[LVC_FILTER] > 0.5f,
                                   pv[LVC_REVERB] > 0.5f };
            for (int k = 5; k >= 0; --k)
            {
                if (! showEnv[k] || envN < 2)
                    continue;
                dl->AddPolyline(envPts[k], envN, U32(P.shadow, 0.8f), 0, 3.5f * s);
                dl->AddPolyline(envPts[k], envN, U32(envCols[k], fxOn[k] ? 0.95f : 0.35f), 0, 1.5f * s);
            }

            // lane label
            {
                char lbl[48];
                const float bars = 2.f / zScope.time;
                if (bars >= 0.999f)
                    std::snprintf(lbl, sizeof(lbl), trn("OUTPUT  \xc2\xb7  LAST %.3g BAR%s", bars > 1.001f ? 2 : 1), bars, bars > 1.001f ? "S" : "");
                else
                {
                    const float beats = bars * std::max(1.f, vis->beatsPerBar.load(std::memory_order_relaxed));
                    std::snprintf(lbl, sizeof(lbl), trn("OUTPUT  \xc2\xb7  LAST %.3g BEAT%s", beats > 1.001f ? 2 : 1), beats, beats > 1.001f ? "S" : "");
                }
                const ImVec2 ls = textSize(fSmall, lbl);
                const ImVec2 la(sa.x + 6.f * s, sa.y + 5.f * s);
                dl->AddRectFilled(la, ImVec2(la.x + ls.x + 12.f * s, la.y + ls.y + 4.f * s), U32(P.inset, 0.85f), 6.f * s);
                text(dl, fSmall, ImVec2(la.x + 6.f * s, la.y + 2.f * s), U32(P.textDim), lbl);
            }

            // "now" edge
            dl->AddRectFilledMultiColor(ImVec2(x1 - 30.f * s, sa.y), ImVec2(sb.x, sb.y), U32(P.b, 0.f), U32(P.b, 0.12f),
                                        U32(P.b, 0.12f), U32(P.b, 0.f));

            dl->PopClipRect();

            for (int k = 0; k < 6; ++k)
                envPill(dl, envBtns[k], envNames[k], envCols[k], showEnv[k]);
            if (viewHot)
                zoomHint(dl, sa, sb, false);
            drawZoomButtons(dl, zb, zScope);
        }
    }

    // ---- control panels ----

    // ---- subdivision helpers (meter aware) ----

    enum SubdivKind { kSubStraight = 0, kSubTriplet, kSubOther };

    // classify subdivision option i under the current time signature and build a musical label
    SubdivKind subdivInfo(int i, char* label, size_t sz) const
    {
        const float bpb = vis != nullptr ? vis->beatsPerBar.load(std::memory_order_relaxed) : 4.f;
        const float unit = vis != nullptr ? vis->beatUnit.load(std::memory_order_relaxed) : 4.f;
        const double quarters = std::max(0.25, bpb * 4.0 / std::max(1.f, unit));
        const double pq = LVC_SUBDIV_OPTIONS[i] / quarters; // units per quarter note

        static const double straight[6] = { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
        for (double v : straight)
            if (std::fabs(pq - v) < 1e-3)
            {
                std::snprintf(label, sz, "1/%d", static_cast<int>(4.0 * v + 0.5));
                return kSubStraight;
            }
        static const double triplet[5] = { 0.75, 1.5, 3.0, 6.0, 12.0 };
        for (double v : triplet)
            if (std::fabs(pq - v) < 1e-3)
            {
                std::snprintf(label, sz, "1/%dT", static_cast<int>(4.0 * v / 1.5 + 0.5));
                return kSubTriplet;
            }
        std::snprintf(label, sz, tr("%ld/bar"), LVC_SUBDIV_OPTIONS[i]);
        return kSubOther;
    }

    // option of the wanted kind whose rate is closest (in octaves) to `target` units per bar; -1 if none
    int nearestSubdiv(SubdivKind want, double target) const
    {
        int best = -1;
        double bestDist = 1e9;
        char tmp[16];
        for (int i = 0; i < 7; ++i)
        {
            if (subdivInfo(i, tmp, sizeof(tmp)) != want)
                continue;
            const double d = std::fabs(std::log2(LVC_SUBDIV_OPTIONS[i] / target));
            if (d < bestDist - 1e-6)
            {
                bestDist = d;
                best = i;
            }
        }
        return best;
    }

    // small pill button; returns true when clicked
    bool pillButton(ImDrawList* dl, const char* id, const char* label, ImVec2 a, ImVec2 b, bool lit, bool enabled = true)
    {
        ImGui::SetCursorScreenPos(a);
        const bool clicked = ImGui::InvisibleButton(id, b - a) && enabled;
        const bool hov = ImGui::IsItemHovered() && enabled;
        const float rr = (b.y - a.y) * 0.5f;
        if (lit)
        {
            glow(dl, a, b, P.c, rr, 0.7f);
            gradRectTheme(dl, a, b, rr);
            textCentered(dl, fBold, (a + b) * 0.5f, U32(P.onAccent), label);
        }
        else
        {
            dl->AddRectFilled(a, b, U32(P.text, hov ? 0.10f : 0.05f), rr);
            dl->AddRect(a, b, U32(hov ? withA(P.c, 0.6f) : P.panelBorder), rr, 0, 1.f);
            textCentered(dl, fBold, (a + b) * 0.5f, U32(enabled ? (hov ? P.text : P.textDim) : P.textFaint), label);
        }
        if (hov)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        return clicked;
    }

    void drawSequencePanel(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        float y = panel(dl, a, b, "SEQUENCE");
        if (panelDice(dl, "seqdice", a, b))
            randomize(kDiceSequence);
        // the auto slicer itself: off lets the input run straight through the effects
        toggle(dl, LVC_SLICER, ImVec2(b.x - 26.f * s - 11.f * s - 10.f * s - 38.f * s, a.y + 11.f * s));
        if (ImGui::IsItemHovered())
            tooltip(pv[LVC_SLICER] > 0.5f ? "Auto slicer on: click to let the input run straight through the effects"
                                                    : "Auto slicer off: the input runs straight through the effects");
        const float x0 = a.x + 16.f * s, x1 = b.x - 16.f * s;
        const float w = x1 - x0;

        // ---- subdivision + triplet switch ----
        const int cur = std::max(0, std::min(6, static_cast<int>(pv[LVC_SUBDIV] + 0.5f)));
        char curLabel[16];
        const SubdivKind curKind = subdivInfo(cur, curLabel, sizeof(curLabel));

        text(dl, fSmall, ImVec2(x0, y + 2.f * s), U32(P.textDim), "SUBDIVISION");
        {
            const float bw = 72.f * s, bh = 20.f * s;
            const ImVec2 ba(x1 - bw, y - 2.f * s), bb(x1, y - 2.f * s + bh);
            const int target = curKind == kSubTriplet
                ? nearestSubdiv(kSubStraight, LVC_SUBDIV_OPTIONS[cur] / 1.5)
                : nearestSubdiv(kSubTriplet, LVC_SUBDIV_OPTIONS[cur] * (curKind == kSubStraight ? 1.5 : 1.0));
            if (pillButton(dl, "##triplet", "TRIPLET", ba, bb, curKind == kSubTriplet, target >= 0) && target >= 0)
                setParamGesture(LVC_SUBDIV, static_cast<float>(target));
        }
        y += fs(fSmall) + 9.f * s;

        static const char* const subdivs[7] = { "6", "8", "12", "16", "18", "24", "32" };
        bool marks[7];
        char tmp[16];
        for (int i = 0; i < 7; ++i)
            marks[i] = subdivInfo(i, tmp, sizeof(tmp)) == kSubTriplet;
        segmented(dl, LVC_SUBDIV, subdivs, 7, ImVec2(x0, y), ImVec2(x1, y + 28.f * s), true, marks);
        y += 28.f * s + 6.f * s;

        {
            char cap[48];
            if (curKind == kSubStraight)
                std::snprintf(cap, sizeof(cap), tr("%s notes"), curLabel);
            else if (curKind == kSubTriplet)
                std::snprintf(cap, sizeof(cap), tr("%s  \xc2\xb7  triplets"), curLabel);
            else
                std::snprintf(cap, sizeof(cap), "%s", curLabel);
            text(dl, fSmall, ImVec2(x0, y), U32(curKind == kSubTriplet ? P.c : P.textDim), cap);
            // legend for the triplet dots
            const char* leg = "\xe2\x80\xa2 = triplet";
            textRight(dl, fSmall, ImVec2(x1, y), U32(P.textFaint), leg);
        }
        y += fs(fSmall) + 14.f * s;

        // ---- shortest cut ----
        text(dl, fSmall, ImVec2(x0, y), U32(P.textDim), "SHORTEST CUT");
        y += fs(fSmall) + 7.f * s;
        static const char* const mincut[3] = { "ANY", "1/16", "1/8" };
        segmented(dl, LVC_MINCUT, mincut, 3, ImVec2(x0, y), ImVec2(x1, y + 26.f * s));
        y += 26.f * s + 16.f * s;

        rangeSlider(dl, LVC_MINPHRS, LVC_MAXPHRS, "PHRASE (BARS)", ImVec2(x0, y), w);
        y += 50.f * s;

        // ---- fill mode ----
        dl->AddLine(ImVec2(x0, y - 6.f * s), ImVec2(x1, y - 6.f * s), U32(P.panelBorder), 1.f);
        const bool fillOn = pv[LVC_FILLMODE] > 0.5f;
        y += 2.f * s;
        fxHeader(dl, x0, x1, y, "FILL MODE", LVC_FILLMODE);
        y += 28.f * s;

        const float half = (w - 8.f * s) * 0.5f;
        text(dl, fSmall, ImVec2(x0, y), U32(P.textDim, fillOn ? 1.f : 0.6f), "EVERY (BARS)");
        text(dl, fSmall, ImVec2(x0 + half + 8.f * s, y), U32(P.textDim, fillOn ? 1.f : 0.6f), "SLICE LAST");
        y += fs(fSmall) + 5.f * s;
        static const char* const cycles[3] = { "8", "16", "32" };
        static const char* const lengths[4] = { "1", "2", "4", "8" };
        segmented(dl, LVC_FILLCYCLE, cycles, 3, ImVec2(x0, y), ImVec2(x0 + half, y + 24.f * s), fillOn);
        segmented(dl, LVC_FILLBARS, lengths, 4, ImVec2(x0 + half + 8.f * s, y), ImVec2(x1, y + 24.f * s), fillOn);
        y += 24.f * s + 10.f * s;

        // cycle map: one cell per bar, fill bars highlighted, current bar marked
        {
            const int cycle = static_cast<int>(LVC_FILLCYCLE_OPTIONS[std::max(0, std::min(2, static_cast<int>(pv[LVC_FILLCYCLE] + 0.5f)))]);
            const int fbars = std::min(cycle, static_cast<int>(LVC_FILLBARS_OPTIONS[std::max(0, std::min(3, static_cast<int>(pv[LVC_FILLBARS] + 0.5f)))]));
            const int cur = (vis != nullptr && vis->fillMode.load(std::memory_order_relaxed))
                          ? vis->barInCycle.load(std::memory_order_relaxed) : -1;
            const float ch = std::min(12.f * s, b.y - 14.f * s - y);
            if (ch > 3.f * s)
            {
                const float cw = w / cycle;
                for (int i = 0; i < cycle; ++i)
                {
                    const ImVec2 ca(x0 + i * cw + 0.75f * s, y), cb(x0 + (i + 1) * cw - 0.75f * s, y + ch);
                    const bool isFill = i >= cycle - fbars;
                    const float al = fillOn ? 1.f : 0.4f;
                    if (isFill)
                        gradRect(dl, ca, cb, withA(P.b, al), withA(P.c, al), 2.f * s, true);
                    else
                        dl->AddRectFilled(ca, cb, U32(P.track, 1.2f), 2.f * s);
                    if (i == cur && fillOn)
                        dl->AddRect(ImVec2(ca.x - 1.f * s, ca.y - 1.f * s), ImVec2(cb.x + 1.f * s, cb.y + 1.f * s), U32(P.text), 2.f * s, 0, 1.5f * s);
                }
            }
        }
    }

    void drawAlgorithmPanel(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        const int algo = algoFromParams(pv);
        float y = panel(dl, a, b, "ALGO", kAlgoNames[algo], algo);
        if (panelDice(dl, "algodice", a, b))
            randomize(kDiceAlgorithm);
        const float x0 = a.x + 16.f * s, x1 = b.x - 16.f * s;
        const float w = x1 - x0;

        // description
        {
            const char* desc = tr(kAlgoDescriptions[algo]);
            const ImVec2 sz = fSmall->CalcTextSizeA(fs(fSmall), FLT_MAX, w, desc);
            dl->AddText(fSmall, fs(fSmall), ImVec2(x0, y - 4.f * s), U32(P.textDim), desc, nullptr, w);
            y += sz.y + 12.f * s;
        }

        const float r = 18.f * s;
        const float row = r * 2.f + 40.f * s;
        switch (algo)
        {
        case kAlgoCutProc11:
            knob(dl, LVC_STUTTER, "STUTTER", ImVec2(x0 + w * 0.25f, y + r), r);
            knob(dl, LVC_AREA, "AREA", ImVec2(x0 + w * 0.75f, y + r), r);
            y += row;
            rangeSlider(dl, LVC_MINREP, LVC_MAXREP, "REPEATS", ImVec2(x0, y), w);
            y += 46.f * s;
            break;
        case kAlgoWarpCut:
            knob(dl, LVC_STRAIGHT, "STRAIGHT", ImVec2(x0 + w * 0.25f, y + r), r);
            knob(dl, LVC_REGULAR, "REGULAR", ImVec2(x0 + w * 0.75f, y + r), r);
            y += row;
            knob(dl, LVC_RITARD, "RITARD", ImVec2(x0 + w * 0.25f, y + r), r);
            knob(dl, LVC_SPEED, "SPEED", ImVec2(x0 + w * 0.75f, y + r), r);
            y += row;
            break;
        case kAlgoSQPusher:
        {
            const float rb = 24.f * s;
            knob(dl, LVC_ACTIVITY, "ACTIVITY", ImVec2(x0 + w * 0.5f, y + rb), rb);
            y += rb * 2.f + 42.f * s;
            break;
        }
        case kAlgoEuclid:
        {
            knob(dl, LVC_EUDENSITY, "DENSITY", ImVec2(x0 + w * 0.25f, y + r), r);
            knob(dl, LVC_EUCHANCE, "CHANCE", ImVec2(x0 + w * 0.75f, y + r), r);
            y += row;
            text(dl, fSmall, ImVec2(x0, y), U32(P.textDim), "SLICE");
            textRight(dl, fSmall, ImVec2(x1, y), U32(P.textFaint), "which hit's audio repeats");
            y += fs(fSmall) + 6.f * s;
            static const char* const slices[5] = { "1", "2", "3", "4", "RND" };
            segmented(dl, LVC_EUSLICE, slices, 5, ImVec2(x0, y), ImVec2(x1, y + 24.f * s));
            y += 24.f * s + 12.f * s;
            break;
        }
        case kAlgoGate:
        {
            // gate clock = the slicer's subdivision, offered here with musical names (straight and triplet rates)
            {
                text(dl, fSmall, ImVec2(x0, y), U32(P.textDim), "RATE");
                textRight(dl, fSmall, ImVec2(x1, y), U32(P.textFaint), "length of one step");
                y += fs(fSmall) + 6.f * s;
                int opts[7], nOpts = 0;
                char labels[7][16];
                for (int i = 0; i < 7; ++i)
                    if (subdivInfo(i, labels[nOpts], sizeof(labels[0])) != kSubOther)
                        opts[nOpts++] = i;
                const int cur = limit<int>(static_cast<int>(pv[LVC_SUBDIV] + 0.5f), 0, 6);
                const float bh = 24.f * s, gap = 3.f * s;
                const float bw = (w - gap * (nOpts - 1)) / std::max(1, nOpts);
                for (int k = 0; k < nOpts; ++k)
                {
                    const ImVec2 ba(x0 + k * (bw + gap), y), bb(ba.x + bw, y + bh);
                    ImGui::PushID(k + 860);
                    if (pillButton(dl, "##gaterate", labels[k], ba, bb, opts[k] == cur))
                    {
                        setParamGesture(LVC_SUBDIV, static_cast<float>(opts[k]));
                        commitUndo(true);
                    }
                    ImGui::PopID();
                }
                y += bh + 10.f * s;
            }
            static const char* const modes[2] = { "RANDOM", "PATTERN" };
            segmented(dl, LVC_GATEMODE, modes, 2, ImVec2(x0, y), ImVec2(x1, y + 24.f * s));
            y += 24.f * s + 10.f * s;
            const bool random = pv[LVC_GATEMODE] <= 0.5f;
            knob(dl, LVC_GATEDENSITY, "DENSITY", ImVec2(x0 + w * 0.25f, y + r), r, random);
            knob(dl, LVC_GATEVARY, "VARIATION", ImVec2(x0 + w * 0.75f, y + r), r, random);
            y += row;

            // what the gate envelope shapes; while it plays, the inner arcs show how far each one is pulled now
            text(dl, fSmall, ImVec2(x0, y - 6.f * s), U32(P.textDim), "ENVELOPE SHAPES");
            textRight(dl, fSmall, ImVec2(x1, y - 6.f * s), U32(P.textFaint), "depth");
            y += fs(fSmall) + 2.f * s;
            {
                const bool live = vis != nullptr && phrase.numBlocks > 0;
                const float env = live ? limit<float>(vis->gateEnv.load(std::memory_order_relaxed), 0.f, 1.f) : 1.f;
                const float vol = pv[LVC_GATEVOL], lp = pv[LVC_GATELP], hp = pv[LVC_GATEHP];
                char tv[24], tl[24], th[24];
                std::snprintf(tv, sizeof(tv), "%.0f%%", (1.f - vol * (1.f - env)) * 100.f);
                auto hz = [](float f, char* b, size_t n) {
                    if (f < 1000.f) std::snprintf(b, n, "%.0f Hz", f);
                    else std::snprintf(b, n, "%.1f kHz", f / 1000.f);
                };
                hz(gateLowpassHz(lp, env), tl, sizeof(tl));
                hz(gateHighpassHz(hp, env), th, sizeof(th));
                const float k = 1.f - env;
                knob(dl, LVC_GATEVOL, "VOLUME", ImVec2(x0 + w / 6.f, y + r), r, true, false, w / 3.f,
                     live ? vol * k : 0.f, live && vol > 0.f ? tv : nullptr);
                knob(dl, LVC_GATELP, "LOWPASS", ImVec2(x0 + w * 0.5f, y + r), r, true, false, w / 3.f,
                     live ? lp * k : 0.f, live && lp > 0.f ? tl : nullptr);
                knob(dl, LVC_GATEHP, "HIGHPASS", ImVec2(x0 + w * 5.f / 6.f, y + r), r, true, false, w / 3.f,
                     live ? hp * k : 0.f, live && hp > 0.f ? th : nullptr);
            }
            y += row;
            textCentered(dl, fSmall, ImVec2((x0 + x1) * 0.5f, y), U32(P.textFaint), "draw the steps in the LIVE CUTS view");
            y += fs(fSmall) + 10.f * s;
            break;
        }
        default:
            knob(dl, LVC_RAINTENSITY, "INTENSITY", ImVec2(x0 + w * 0.25f, y + r), r);
            knob(dl, LVC_RACURVE, "CURVE", ImVec2(x0 + w * 0.75f, y + r), r);
            y += row;
            break;
        }

        // live monitor in the remaining space
        const float mb = b.y - 14.f * s;
        if (mb - y > 60.f * s)
            drawAlgoMonitor(dl, ImVec2(x0 - 4.f * s, y), ImVec2(x1 + 4.f * s, mb), algo);
    }

    // ---- trance gate: one-bar step sequencer (replaces the phrase map in LIVE CUTS) ----
    // Always exactly one bar, steps at fixed places, everything edited in place:
    //   click                 open or mute one step (drawing switches the gate to pattern mode; in random mode the
    //                         cells show the bar that is playing)
    //   drag                  paint: opened steps are tied into one continuous slice
    //   wheel / right-drag    the step's envelope: up = attack (fade-in), down = decay (fade-out, pluck)
    //   shift + drag          bend the envelope's curve, like an automation curve
    // Tied steps play as one slice and are drawn as one block.

    float gateStepMs(int n) const
    {
        const float bpm = vis != nullptr ? vis->bpm.load(std::memory_order_relaxed) : 120.f;
        const float bpb = vis != nullptr ? vis->beatsPerBar.load(std::memory_order_relaxed) : 4.f;
        const float unit = vis != nullptr ? vis->beatUnit.load(std::memory_order_relaxed) : 4.f;
        const float barMs = 60000.f / std::max(1.f, bpm) * bpb * 4.f / std::max(1.f, unit);
        return barMs / std::max(1, n);
    }

    // a step's envelope as one bipolar amount: > 0 attack level, < 0 decay level, 0 none
    int gateShape(int st) const
    {
        const int a = gateEnvLevel(pv, st, false), d = gateEnvLevel(pv, st, true);
        return a > 0 ? a : -d;
    }

    void gateSetShape(int st, int v)
    {
        v = limit<int>(v, -(kGateEnvLevels - 1), kGateEnvLevels - 1);
        const uint32_t idx = LVC_GATEENV1 + st / 2;
        float nv = gateEnvWithLevel(pv[idx], st, false, std::max(0, v));
        nv = gateEnvWithLevel(nv, st, true, std::max(0, -v));
        if (nv != pv[idx])
            setParamGesture(idx, nv);
    }

    // write envelope amounts and curve tensions of n steps (nullptr = leave as is)
    void gateSetSteps(const int* shapes, const float* tensions, int n)
    {
        float ev[LVC_GATE_ENV_PARAMS], tv[LVC_GATE_TENSION_PARAMS];
        for (int k = 0; k < LVC_GATE_ENV_PARAMS; ++k) ev[k] = pv[LVC_GATEENV1 + k];
        for (int k = 0; k < LVC_GATE_TENSION_PARAMS; ++k) tv[k] = pv[LVC_GATETENSION1 + k];
        for (int st = 0; st < n; ++st)
        {
            if (shapes != nullptr)
            {
                ev[st / 2] = gateEnvWithLevel(ev[st / 2], st, false, std::max(0, shapes[st]));
                ev[st / 2] = gateEnvWithLevel(ev[st / 2], st, true, std::max(0, -shapes[st]));
            }
            if (tensions != nullptr)
                tv[st / 2] = gateTensionWith(tv[st / 2], st, tensions[st]);
        }
        for (int k = 0; k < LVC_GATE_ENV_PARAMS; ++k)
            if (ev[k] != pv[LVC_GATEENV1 + k]) setParamGesture(LVC_GATEENV1 + k, ev[k]);
        for (int k = 0; k < LVC_GATE_TENSION_PARAMS; ++k)
            if (tv[k] != pv[LVC_GATETENSION1 + k]) setParamGesture(LVC_GATETENSION1 + k, tv[k]);
    }

    void drawGateSequencer(ImDrawList* dl, ImVec2 a, ImVec2 b, bool dryNow)
    {
        const int n = std::min(LVC_GATE_MAX_STEPS, gateSteps());
        const bool random = pv[LVC_GATEMODE] <= 0.5f;
        const uint32_t m = gateShown();
        const float pad = 10.f * s;
        const float x0 = a.x + pad, x1 = b.x - pad;
        const ImVec4 gc = algoColor(kAlgoGate);
        const uint32_t tieMask = gateTiesShown();

        int shape[LVC_GATE_MAX_STEPS];
        float tension[LVC_GATE_MAX_STEPS];
        for (int st = 0; st < LVC_GATE_MAX_STEPS; ++st)
        {
            shape[st] = gateShape(st);
            tension[st] = gateTension(pv, st);
        }

        // ---- header: info on the left, pattern tools on the right ----
        const float hh = 20.f * s;
        const float hy = a.y + 8.f * s;
        float toolsLeft = x1;
        {
            const uint32_t used = n >= 32 ? 0xFFFFFFFFu : ((1u << n) - 1u);
            static const char* const labels[6] = { "CLEAR", "FILL", "INVERT", "FLAT", "<", ">" };
            static const char* const tips[6] = { "Mute every step", "Open every step (untied)", "Swap open and muted steps",
                                                 "Remove every step's envelope and curve",
                                                 "Shift steps and envelopes one step earlier", "Shift steps and envelopes one step later" };
            float bx = x1;
            for (int k = 5; k >= 0; --k)
            {
                const float bw = std::max(26.f * s, textSize(fBold, labels[k]).x + 18.f * s);
                const ImVec2 ba(bx - bw, hy), bb(bx, hy + hh);
                bx -= bw + 4.f * s;
                ImGui::PushID(k + 820);
                const bool clicked = pillButton(dl, "##gatetool", labels[k], ba, bb, false);
                if (ImGui::IsItemHovered())
                    tooltip("%s", tips[k]);
                ImGui::PopID();
                if (! clicked)
                    continue;
                if (k >= 4)
                {
                    // rotate the pattern with the envelopes, so each step keeps its shape
                    auto src = [&](int i) { return k == 4 ? (i + 1) % n : (i + n - 1) % n; };
                    uint32_t next = 0, nextTies = 0;
                    int ns[LVC_GATE_MAX_STEPS];
                    float nt[LVC_GATE_MAX_STEPS];
                    for (int i = 0; i < n; ++i)
                    {
                        if ((m >> src(i)) & 1u) next |= 1u << i;
                        // a tie moves with its step, except one that would wrap over the bar line
                        if (i + 1 < n && ((tieMask >> src(i)) & 1u) && src(i) + 1 < n && src(i + 1) == src(i) + 1)
                            nextTies |= 1u << i;
                        ns[i] = shape[src(i)];
                        nt[i] = tension[src(i)];
                    }
                    gateSetPattern((m & ~used) | next);
                    gateSetTies((tieMask & ~used) | nextTies);
                    gateSetSteps(ns, nt, n);
                }
                else if (k == 3)
                {
                    int zero[LVC_GATE_MAX_STEPS] = {};
                    float zf[LVC_GATE_MAX_STEPS] = {};
                    gateSetSteps(zero, zf, n);
                }
                else
                {
                    const uint32_t st = m & used;
                    const uint32_t next = k == 0 ? 0u : (k == 1 ? used : (~st & used));
                    gateSetPattern((m & ~used) | next);
                    gateSetTies(tieMask & ~used); // new steps start untied
                }
                commitUndo(true);
            }
            toolsLeft = bx;
        }

        // ---- the steps ----
        const float rulerH = 14.f * s;
        const float gy0 = hy + hh + 8.f * s, gy1 = b.y - pad - rulerH;
        if (gy1 - gy0 < 8.f * s)
            return;
        const float H = gy1 - gy0;
        const float cw = (x1 - x0) / n;
        const float gap = std::min(3.f * s, cw * 0.12f);

        ImGui::SetCursorScreenPos(ImVec2(x0, gy0));
        ImGui::InvisibleButton("##gateseq", ImVec2(x1 - x0, gy1 - gy0),
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive(), activated = ImGui::IsItemActivated();
        ImGuiIO& io = ImGui::GetIO();
        int hoverStep = -1;
        {
            const ImVec2 mp = io.MousePos;
            if ((hov || act) && mp.x >= x0 && mp.x < x1)
                hoverStep = std::max(0, std::min(n - 1, static_cast<int>((mp.x - x0) / cw)));

            if (activated && hoverStep >= 0)
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                {
                    gateDragMode = 2; // envelope drag on this step
                    gateDragStep = hoverStep;
                    gateDragStart = shape[hoverStep];
                    gateDragY = mp.y;
                }
                else if (io.KeyShift)
                {
                    gateDragMode = 3; // bend this step's curve
                    gateDragStep = hoverStep;
                    gateDragStartF = tension[hoverStep];
                    gateDragY = mp.y;
                }
                else
                {
                    gateDragMode = 1; // painting open / muted steps
                    gatePaint(hoverStep, true);
                }
            }
            else if (act && gateDragMode == 1 && hoverStep >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                gatePaint(hoverStep, false);
            }
            else if (act && gateDragMode == 2 && ImGui::IsMouseDown(ImGuiMouseButton_Right))
            {
                // relative: every 10 px up = one attack level more (or one decay level less)
                const int v = gateDragStart + static_cast<int>(std::round((gateDragY - mp.y) / (10.f * s)));
                gateSetShape(gateDragStep, v);
                shape[gateDragStep] = gateShape(gateDragStep);
                hoverStep = gateDragStep;
            }
            else if (act && gateDragMode == 3 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                // relative: 100 px of travel sweeps from straight to fully bent
                const float t = limit<float>(gateDragStartF + (gateDragY - mp.y) / (100.f * s), -1.f, 1.f);
                const uint32_t idx = LVC_GATETENSION1 + gateDragStep / 2;
                const float nv = gateTensionWith(pv[idx], gateDragStep, t);
                if (nv != pv[idx])
                    setParamGesture(idx, nv);
                tension[gateDragStep] = gateTension(pv, gateDragStep);
                hoverStep = gateDragStep;
            }
            if (! act)
                gateDragMode = 0;

            // wheel: up = attack, down = decay (touchpads scroll in fractions, so they are accumulated)
            if (hov && hoverStep >= 0 && io.MouseWheel != 0.f && ! act)
            {
                gateWheelAcc += io.MouseWheel;
                const int steps = static_cast<int>(gateWheelAcc);
                if (steps != 0)
                {
                    gateWheelAcc -= static_cast<float>(steps);
                    gateSetShape(hoverStep, shape[hoverStep] + steps);
                    shape[hoverStep] = gateShape(hoverStep);
                    wheelEdit = true;
                }
            }
            if (hoverStep >= 0)
                ImGui::SetMouseCursor(gateDragMode >= 2 || io.KeyShift ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_Hand);
        }

        // info text: the hovered step, or how to edit
        {
            char buf[160];
            if (hoverStep >= 0)
            {
                const int v = shape[hoverStep];
                const float f = GateEnvFraction(std::abs(v));
                char tb[32];
                if (std::fabs(tension[hoverStep]) < 0.005f)
                    std::snprintf(tb, sizeof(tb), "%s", tr("straight"));
                else
                    std::snprintf(tb, sizeof(tb), tr("curve %+.0f%%"), tension[hoverStep] * 100.f);
                const bool tiedNow = ((tieMask >> hoverStep) & 1u) || (hoverStep > 0 && ((tieMask >> (hoverStep - 1)) & 1u));
                if (v == 0)
                    std::snprintf(buf, sizeof(buf), tr("STEP %d  \xc2\xb7  %s%s  \xc2\xb7  no envelope (wheel to add)"), hoverStep + 1,
                                  tr(((m >> hoverStep) & 1u) ? "OPEN" : "MUTED"), tr(tiedNow ? ", TIED" : ""));
                else
                    std::snprintf(buf, sizeof(buf), tr("STEP %d  \xc2\xb7  %s %.0f%% = %.1f ms  \xc2\xb7  %s"), hoverStep + 1,
                                  tr(v > 0 ? "ATTACK" : "DECAY"), f * 100.f, f * gateStepMs(n), tb);
            }
            else
            {
                char rate[16];
                subdivInfo(limit<int>(static_cast<int>(pv[LVC_SUBDIV] + 0.5f), 0, 6), rate, sizeof(rate));
                std::snprintf(buf, sizeof(buf), tr("%d STEPS  \xc2\xb7  %s%s  \xc2\xb7  click: open / mute  \xc2\xb7  drag: tie  \xc2\xb7  "
                              "wheel / right-drag: attack up, decay down  \xc2\xb7  shift+drag: curve"), n, rate, tr(random ? "  \xc2\xb7  RANDOM" : ""));
            }
            const float room = toolsLeft - 8.f * s - x0;
            if (room > 30.f * s)
                text(dl, fSmall, ImVec2(x0, hy + hh * 0.5f - fs(fSmall) * 0.5f - 0.5f * s),
                     U32(hoverStep >= 0 ? P.text : (random ? P.textDim : gc)), fitText(fSmall, buf, room).c_str());
        }

        // playhead: position inside the bar (only while the phrase runs on this grid)
        float barPos = -1.f;
        if (vis != nullptr && phrase.numBlocks > 0 && phrase.subdiv == n && ! dryNow)
        {
            const float up = vis->unitPos.load(std::memory_order_relaxed);
            barPos = std::fmod(std::max(0.f, up), static_cast<float>(n));
        }
        const int playing = barPos >= 0.f ? std::min(n - 1, static_cast<int>(barPos)) : -1;
        const float duty = limit<float>(pv[LVC_DUTY], 0.f, 1.f);
        const float dim = dryNow ? 0.4f : 1.f;
        const float al = (random ? 0.7f : 0.9f) * dim;
        const int perBeat = std::max(1, n / 4);
        const bool beatsEven = n % 4 == 0;
        const float rr = std::min(4.f * s, cw * 0.3f);

        // tied steps (drawn by dragging) are one slice: one block, one gradient, no gaps
        bool open[LVC_GATE_MAX_STEPS], tied[LVC_GATE_MAX_STEPS];
        for (int st = 0; st < n; ++st) open[st] = ((m >> st) & 1u) != 0;
        for (int st = 0; st < n; ++st) tied[st] = st + 1 < n && open[st] && open[st + 1] && ((tieMask >> st) & 1u) != 0;
        auto cellA = [&](int st) { return ImVec2(x0 + st * cw + ((st > 0 && tied[st - 1]) ? 0.f : gap * 0.5f), gy0); };
        auto cellB = [&](int st) { return ImVec2(x0 + (st + 1) * cw - (tied[st] ? 0.f : gap * 0.5f), gy1); };

        for (int st = 0; st < n; )
        {
            // a run: one step, or open steps tied together
            int end = st;
            while (tied[end]) ++end;
            const ImVec2 ra = cellA(st), rb = cellB(end);
            const bool beat = beatsEven ? st % perBeat == 0 : (st * 4) % n == 0;
            dl->AddRectFilled(ra, rb, U32(P.track, ! open[st] && beat ? 1.6f : 1.f), rr);

            if (open[st])
            {
                // one gradient across the whole run; each step's envelope cuts its part of it
                auto colAt = [&](float x) { return withA(mix(P.b, P.a, (x - ra.x) / std::max(1.f, rb.x - ra.x)), al); };
                bool anyShaped = false;
                for (int k = st; k <= end; ++k) anyShaped = anyShaped || shape[k] != 0;
                if (end > st)
                    gradRect(dl, ra, rb, withA(P.b, 0.12f * dim), withA(P.a, 0.12f * dim), rr, true); // run background
                // the slice ends after the gate length of its last step
                const ImVec2 la = cellA(end), lb = cellB(end);
                const float gateEnd = la.x + std::max(3.f * s, (lb.x - la.x) * duty);
                if (! anyShaped)
                {
                    const ImVec2 fb(gateEnd, rb.y);
                    gradRect(dl, ra, fb, colAt(ra.x), colAt(fb.x), rr, true);
                }
                else
                {
                    for (int k = st; k <= end; ++k)
                    {
                        const ImVec2 ca = cellA(k), cb = cellB(k);
                        const float gw = k < end ? (cb.x - ca.x) : std::max(3.f * s, (cb.x - ca.x) * duty);
                        if (shape[k] == 0)
                        {
                            dl->AddRectFilledMultiColor(ca, ImVec2(ca.x + gw, cb.y), U32(colAt(ca.x)), U32(colAt(ca.x + gw)),
                                                        U32(colAt(ca.x + gw)), U32(colAt(ca.x)));
                            continue;
                        }
                        const float fa = shape[k] > 0 ? GateEnvFraction(shape[k]) : 0.f;
                        const float fd = shape[k] < 0 ? GateEnvFraction(-shape[k]) : 0.f;
                        // the step's own part of the gate (a step tied on into the next is open to its end)
                        const float tEnd = k < end ? 1.f : duty;
                        // one seamless mesh under the curve, coloured by the run's gradient
                        const int K = std::max(4, std::min(40, static_cast<int>(gw / (1.5f * s))));
                        ImVec2 pts[41];
                        float xs[41], ys[41], yb[41];
                        ImU32 cs[41];
                        for (int q = 0; q <= K; ++q)
                        {
                            const float xq = ca.x + gw * q / K;
                            pts[q] = ImVec2(xq, cb.y - H * GateEnvelope(tEnd * q / K, fa, fd, tension[k]));
                            xs[q] = xq; ys[q] = pts[q].y; yb[q] = cb.y; cs[q] = U32(colAt(xq));
                        }
                        fillBand(dl, xs, ys, yb, cs, K + 1);
                        dl->AddPolyline(pts, K + 1, U32(P.text, 0.6f * dim), 0, std::max(1.f, 1.2f * s));
                    }
                }
                if (duty < 0.98f || anyShaped)
                    dl->AddRect(ra, rb, U32(P.a, 0.3f * dim), rr, 0, 1.f); // the whole slice behind a short gate
            }
            else if (shape[st] != 0)
            {
                // muted step: its envelope stays visible as a faint outline (it comes back when opened)
                const float fa = shape[st] > 0 ? GateEnvFraction(shape[st]) : 0.f;
                const float fd = shape[st] < 0 ? GateEnvFraction(-shape[st]) : 0.f;
                ImVec2 pts[17];
                for (int q = 0; q <= 16; ++q)
                    pts[q] = ImVec2(ra.x + (rb.x - ra.x) * q / 16.f, rb.y - H * GateEnvelope(q / 16.f, fa, fd, tension[st]));
                dl->AddPolyline(pts, 17, U32(P.textFaint, 0.6f), 0, 1.f);
            }
            st = end + 1;
        }

        // per-step marks on top: playing, hovered, ruler
        for (int st = 0; st < n; ++st)
        {
            const ImVec2 ca = cellA(st), cb = cellB(st);
            if (st == playing)
            {
                if (open[st])
                    glow(dl, ca, cb, P.b, rr, 1.1f);
                dl->AddRect(ca, cb, U32(P.text, 0.95f), rr, 0, 1.5f * s);
            }
            else if (st == hoverStep)
            {
                dl->AddRectFilled(ca, cb, U32(P.text, 0.06f), rr);
                dl->AddRect(ca, cb, U32(P.text, 0.55f), rr, 0, 1.f * s);
            }
            const bool beat = beatsEven ? st % perBeat == 0 : (st * 4) % n == 0;
            const float rx = x0 + (st + 0.5f) * cw;
            if (beat)
            {
                char bn[8];
                std::snprintf(bn, sizeof(bn), "%d", beatsEven ? st / perBeat + 1 : st + 1);
                textCentered(dl, fSmall, ImVec2(rx, gy1 + rulerH * 0.5f + 2.f * s), U32(st == playing ? P.text : P.textDim), bn);
            }
            else if (cw > 10.f * s)
                dl->AddCircleFilled(ImVec2(rx, gy1 + rulerH * 0.5f + 2.f * s), 1.3f * s, U32(P.textFaint, 0.8f), 8);
        }

        // moving playhead line
        if (barPos >= 0.f)
        {
            const float px = x0 + barPos * cw;
            dl->AddLine(ImVec2(px, gy0 - 3.f * s), ImVec2(px, gy1 + 3.f * s), U32(P.text, 0.8f), 1.5f * s);
        }

        if (dryNow)
        {
            const char* msg = "DRY  \xc2\xb7  waiting for the fill";
            const ImVec2 ts = textSize(fBold, msg);
            const ImVec2 c((x0 + x1) * 0.5f, (gy0 + gy1) * 0.5f);
            dl->AddRectFilled(ImVec2(c.x - ts.x * 0.5f - 12.f * s, c.y - ts.y * 0.5f - 6.f * s),
                              ImVec2(c.x + ts.x * 0.5f + 12.f * s, c.y + ts.y * 0.5f + 6.f * s), U32(P.inset, 0.92f), 8.f * s);
            textCentered(dl, fBold, c, U32(P.textDim), msg);
        }
    }

    // monitor detail for a gate bar: open steps filled (width = duty), muted ones empty
    void drawGateSteps(ImDrawList* dl, const LiveCutVis::Block& blk, int rep, ImVec2 a, ImVec2 b)
    {
        const int n = std::max(1, static_cast<int>(blk.numCuts));
        const float cw = (b.x - a.x) / n;
        for (int ci = 0; ci < n; ++ci)
        {
            const int gi = blk.firstCut + ci;
            if (gi < 0 || gi >= phrase.numCuts)
                break;
            const LiveCutVis::Cut& c = phrase.cuts[gi];
            const ImVec2 ca(a.x + ci * cw + 1.f * s, a.y), cb(a.x + (ci + 1) * cw - 1.f * s, b.y);
            dl->AddRectFilled(ca, cb, U32(P.track), 3.f * s);
            if (c.duty > 0.f)
            {
                const ImVec2 fb(ca.x + std::max(2.f * s, (cb.x - ca.x) * std::min(1.f, c.duty)), cb.y);
                const float al = ci == rep ? 1.f : (ci < rep ? 0.65f : 0.4f);
                gradRect(dl, ca, fb, withA(P.b, al), withA(P.a, al), 3.f * s);
                if (ci == rep)
                    glow(dl, ca, fb, P.b, 3.f * s, 1.3f);
            }
            if (ci == rep)
                dl->AddRect(ca, cb, U32(P.text), 3.f * s, 0, 1.5f * s);
        }
    }

    // ---- algorithm monitor: what the cutter decided, block by block ----

    ImVec4 kindColor(int kind, int info0 = 0) const
    {
        switch (kind)
        {
        case kBlockNormal:      return P.a;
        case kBlockStutter:     return P.c;
        case kBlockStraight:    return mix(P.textDim, P.a, 0.25f);
        case kBlockRoll:        return P.b;
        case kBlockAccel:       return P.c;
        case kBlockRitard:      return P.rec;
        case kBlockQuavers:     return P.a;
        case kBlockSemiquavers: return P.b;
        case kBlockFill:        return P.c;
        case kBlockEuclid:      return P.b;
        case kBlockRatchet:     return P.grad(std::min(1.f, std::log2(static_cast<float>(std::max(1, info0))) / 4.f));
        case kBlockGate:        return P.b;
        default:                return P.textDim;
        }
    }

    void blockDescription(const LiveCutVis::Block& blk, char* buf, size_t sz) const
    {
        switch (blk.kind)
        {
        case kBlockNormal:      std::snprintf(buf, sz, tr("%d-UNIT CUT  x%d"), blk.info[0], blk.info[1]); break;
        case kBlockStutter:     std::snprintf(buf, sz, tr("STUTTER 1/%d UNIT  x%d"), blk.info[0], blk.info[1]); break;
        case kBlockStraight:    std::snprintf(buf, sz, tr("STRAIGHT  %d UNIT%s"), blk.units, blk.units == 1 ? "" : "S"); break;
        case kBlockRoll:        std::snprintf(buf, sz, tr("ROLL  x%d"), blk.info[0]); break;
        case kBlockAccel:       std::snprintf(buf, sz, tr("ACCEL ROLL  x%d"), blk.info[0]); break;
        case kBlockRitard:      std::snprintf(buf, sz, tr("RITARD ROLL  x%d"), blk.info[0]); break;
        case kBlockQuavers:     std::snprintf(buf, sz, trn("%d QUAVER%s", blk.info[0]), blk.info[0], blk.info[0] == 1 ? "" : "S"); break;
        case kBlockSemiquavers: std::snprintf(buf, sz, tr("%d SEMIQUAVERS"), blk.info[0]); break;
        case kBlockFill:        std::snprintf(buf, sz, tr("FILL #%d  %d/%d"), blk.info[0] + 1, blk.info[1] + 1, blk.info[2]); break;
        case kBlockEuclid:      std::snprintf(buf, sz, "E(%d,%d)  ROT %d", blk.info[0], blk.info[1], blk.info[2]); break;
        case kBlockRatchet:     std::snprintf(buf, sz, tr("x%d PER BEAT"), blk.info[0]); break;
        case kBlockGate:
        {
            // counted from the cuts: drawn edits change the bar while it plays
            int open = 0;
            for (int ci = 0; ci < blk.numCuts; ++ci)
            {
                const int gi = blk.firstCut + ci;
                if (gi >= 0 && gi < phrase.numCuts && phrase.cuts[gi].duty > 0.f)
                    ++open;
            }
            std::snprintf(buf, sz, tr("%s  %d/%d OPEN"), tr(blk.info[2] ? "PATTERN" : "RANDOM"), open, blk.numCuts);
            break;
        }
        default:                std::snprintf(buf, sz, "-"); break;
        }
        // SHORTEST CUT may have merged the algorithm's cuts: show what is actually played
        int planned = 0;
        switch (blk.kind)
        {
        case kBlockNormal: case kBlockStutter: planned = blk.info[1]; break;
        case kBlockRoll: case kBlockAccel: case kBlockRitard: case kBlockSemiquavers: case kBlockRatchet: planned = blk.info[0]; break;
        default: break;
        }
        if (planned > blk.numCuts && blk.numCuts > 0)
        {
            const size_t len = std::strlen(buf);
            std::snprintf(buf + len, sz - len, " > %d", blk.numCuts);
        }
    }

    void drawAlgoMonitor(ImDrawList* dl, ImVec2 a, ImVec2 b, int algo)
    {
        inset(dl, a, b, 9.f * s);
        const float pad = 8.f * s;
        const float x0 = a.x + pad, x1 = b.x - pad, w = x1 - x0;
        float y = a.y + 6.f * s;

        text(dl, fSmall, ImVec2(x0, y), U32(P.textFaint), "LIVE");
        if (vis == nullptr)
        {
            textCentered(dl, fSmall, (a + b) * 0.5f, U32(P.textFaint), "unavailable in this host");
            return;
        }

        const int curBlock = vis->currentBlock.load(std::memory_order_relaxed);
        const int rep = vis->cutRepeat.load(std::memory_order_relaxed);
        const float unitPos = vis->unitPos.load(std::memory_order_relaxed);
        const int total = std::max(1, phrase.totalUnits);
        const int subdiv = std::max(1, phrase.subdiv);
        const bool haveBlock = curBlock >= 0 && curBlock < phrase.numBlocks;
        const LiveCutVis::Block* cb = haveBlock ? &phrase.blocks[curBlock] : nullptr;

        // readout of the current decision
        {
            char buf[64];
            if (cb != nullptr)
                blockDescription(*cb, buf, sizeof(buf));
            else
                std::snprintf(buf, sizeof(buf), "waiting for a phrase");
            textRight(dl, fBold, ImVec2(x1, y - 1.f * s), U32(cb != nullptr ? kindColor(cb->kind, cb->info[0]) : P.textFaint), buf);
        }
        y += fs(fSmall) + 8.f * s;

        // ---- phrase timeline: one segment per block, coloured by decision ----
        const bool ratchet = algo == kAlgoRatchet;
        const float th = ratchet ? 26.f * s : 16.f * s;
        const float uw = w / total;
        dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + th), U32(P.track), 3.f * s);

        if (algo == kAlgoCutProc11)
        {
            // stutters can only start in the last AREA bars of the phrase
            const float zone = std::min(static_cast<float>(total), pv[LVC_AREA] * subdiv);
            if (zone > 0.f)
            {
                const float zx = x0 + (total - zone) * uw;
                dl->PushClipRect(ImVec2(zx, y), ImVec2(x1, y + th), true);
                for (float hx = zx - th; hx < x1; hx += 5.f * s)
                    dl->AddLine(ImVec2(hx, y + th), ImVec2(hx + th, y), U32(P.c, 0.35f), 1.f);
                dl->PopClipRect();
                dl->AddLine(ImVec2(zx, y - 3.f * s), ImVec2(zx, y + th + 3.f * s), U32(P.c, 0.8f), 1.f);
            }
        }
        if (ratchet)
        {
            // planned build-up (faint) behind the actual blocks
            static const int options[8] = { 1, 2, 3, 4, 6, 8, 12, 16 };
            const double exponent = 3.0 + (0.35 - 3.0) * pv[LVC_RACURVE];
            const int steps = std::max(1, total / std::max(1, subdiv / 4));
            for (int i = 0; i < steps; ++i)
            {
                const double prog = i / double(steps);
                int idx = static_cast<int>(pv[LVC_RAINTENSITY] * std::pow(prog, exponent) * 7.0 + 0.5);
                if (i == steps - 1)
                    idx = std::max(static_cast<int>(pv[LVC_RAINTENSITY] * 7.0 + 0.5), pv[LVC_RAINTENSITY] > 0.f ? 1 : 0);
                const float t = std::log2(static_cast<float>(options[std::min(7, idx)])) / 4.f;
                const float bh = th * (0.15f + 0.85f * t);
                dl->AddRectFilled(ImVec2(x0 + i * w / steps + 1.f * s, y + th - bh), ImVec2(x0 + (i + 1) * w / steps - 1.f * s, y + th),
                                  U32(P.text, 0.07f), 2.f * s);
            }
        }

        for (int bi = 0; bi < phrase.numBlocks; ++bi)
        {
            const LiveCutVis::Block& blk = phrase.blocks[bi];
            const float bx0 = x0 + blk.startUnit * uw, bx1 = x0 + (blk.startUnit + blk.units) * uw;
            const ImVec4 kc = kindColor(blk.kind, blk.info[0]);
            const bool isCur = bi == curBlock;
            float top = y;
            if (ratchet && blk.kind == kBlockRatchet)
                top = y + th - th * (0.15f + 0.85f * std::min(1.f, std::log2(static_cast<float>(std::max(1, static_cast<int>(blk.info[0])))) / 4.f));
            dl->AddRectFilled(ImVec2(bx0 + 0.75f * s, top), ImVec2(std::max(bx0 + 1.5f * s, bx1 - 0.75f * s), y + th),
                              U32(kc, isCur ? 1.f : 0.55f), 3.f * s);
            if (isCur)
                dl->AddRect(ImVec2(bx0 + 0.75f * s, top - 1.f * s), ImVec2(bx1 - 0.75f * s, y + th + 1.f * s), U32(P.text), 3.f * s, 0, 1.5f * s);
        }
        if (phrase.numBlocks > 0)
        {
            const float px = x0 + std::min(1.f, unitPos / total) * w;
            dl->AddLine(ImVec2(px, y - 3.f * s), ImVec2(px, y + th + 3.f * s), U32(P.text), 1.5f * s);
        }
        y += th + 10.f * s;

        // ---- current block in detail ----
        const float detailH = std::min(34.f * s, b.y - y - fs(fSmall) - 14.f * s);
        if (cb != nullptr && detailH > 12.f * s)
        {
            if (cb->kind == kBlockGate)
                drawGateSteps(dl, *cb, rep, ImVec2(x0, y), ImVec2(x1, y + detailH));
            else if (cb->kind == kBlockEuclid || cb->kind == kBlockStraight)
                drawEuclidSteps(dl, *cb, rep, ImVec2(x0, y), ImVec2(x1, y + detailH));
            else
                drawBlockCuts(dl, *cb, rep, ImVec2(x0, y), ImVec2(x1, y + detailH));
            y += detailH + 8.f * s;
        }

        // ---- legend ----
        if (y + fs(fSmall) < b.y - 4.f * s)
        {
            static const int legends[kAlgoCount][4] = {
                { kBlockNormal, kBlockStutter, -1, -1 },
                { kBlockStraight, kBlockRoll, kBlockAccel, kBlockRitard },
                { kBlockQuavers, kBlockSemiquavers, kBlockFill, -1 },
                { kBlockStraight, kBlockEuclid, -1, -1 },
                { kBlockRatchet, -1, -1, -1 },
                { kBlockGate, -1, -1, -1 },
            };
            static const char* const names[kNumBlockKinds] = {
                "CUT", "STUTTER", "STRAIGHT", "ROLL", "ACCEL", "RITARD", "QUAVERS", "SEMIQ.", "FILL", "EUCLID", "RATCHET", "GATE BAR"
            };
            float lx = x0;
            const float ly = b.y - 6.f * s - fs(fSmall);
            for (int k = 0; k < 4; ++k)
            {
                const int kind = legends[algo][k];
                if (kind < 0)
                    break;
                const ImVec4 kc = kind == kBlockRatchet ? P.grad(0.6f) : kindColor(kind);
                dl->AddCircleFilled(ImVec2(lx + 3.f * s, ly + fs(fSmall) * 0.5f + 1.f * s), 3.f * s, U32(kc), 12);
                text(dl, fSmall, ImVec2(lx + 9.f * s, ly), U32(P.textDim), names[kind]);
                lx += textSize(fSmall, names[kind]).x + 20.f * s;
            }
            if (algo == kAlgoCutProc11)
            {
                dl->AddLine(ImVec2(lx, ly + 2.f * s), ImVec2(lx + 6.f * s, ly + fs(fSmall)), U32(P.c, 0.8f), 1.f);
                text(dl, fSmall, ImVec2(lx + 10.f * s, ly), U32(P.textDim), "ZONE");
            }
        }
    }

    // the cuts of a block as boxes (width = length, fill = duty), the playing one highlighted
    void drawBlockCuts(ImDrawList* dl, const LiveCutVis::Block& blk, int rep, ImVec2 a, ImVec2 b)
    {
        float totalUnits = 0.f;
        for (int ci = 0; ci < blk.numCuts; ++ci)
        {
            const int gi = blk.firstCut + ci;
            if (gi >= 0 && gi < phrase.numCuts)
                totalUnits += phrase.cuts[gi].units;
        }
        if (totalUnits <= 0.f)
            return;
        const float w = b.x - a.x;
        const ImVec4 kc = kindColor(blk.kind, blk.info[0]);
        float x = a.x;
        for (int ci = 0; ci < blk.numCuts; ++ci)
        {
            const int gi = blk.firstCut + ci;
            if (gi < 0 || gi >= phrase.numCuts)
                break;
            const LiveCutVis::Cut& c = phrase.cuts[gi];
            const float cw = c.units / totalUnits * w;
            const float inner = std::min(1.5f * s, cw * 0.2f);
            const bool isCur = ci == rep;
            const float al = isCur ? 1.f : (ci < rep ? 0.6f : 0.25f);
            const float h = (b.y - a.y) * (0.45f + 0.55f * std::min(1.f, c.amp));
            const float y0 = b.y - h;
            const float dx = x + std::max(1.f, cw * std::min(1.f, c.duty));
            dl->AddRectFilled(ImVec2(x + inner, y0), ImVec2(std::max(x + inner + 1.f, dx - inner), b.y), U32(kc, al), 2.f * s);
            if (isCur)
            {
                glow(dl, ImVec2(x + inner, y0), ImVec2(x + cw - inner, b.y), kc, 2.f * s, 1.2f);
                dl->AddRect(ImVec2(x + inner, y0), ImVec2(x + cw - inner, b.y), U32(P.text), 2.f * s, 0, 1.5f * s);
            }
            x += cw;
        }
    }

    // euclid: the block's steps, hits filled, the playing hit glowing and the slice it replays marked
    void drawEuclidSteps(ImDrawList* dl, const LiveCutVis::Block& blk, int rep, ImVec2 a, ImVec2 b)
    {
        const int n = std::max(1, static_cast<int>(blk.units));
        const float cw = (b.x - a.x) / n;
        const float arcH = 9.f * s;
        const float y0 = a.y + arcH, y1 = b.y;
        // hits from the cut lengths
        std::vector<int> hits;
        std::vector<int> sources;
        float pos = 0.f;
        for (int ci = 0; ci < blk.numCuts; ++ci)
        {
            const int gi = blk.firstCut + ci;
            if (gi < 0 || gi >= phrase.numCuts)
                break;
            hits.push_back(static_cast<int>(pos + 0.5f));
            sources.push_back(static_cast<int>(phrase.cuts[gi].offset + 0.5f));
            pos += phrase.cuts[gi].units;
        }
        for (int i = 0; i < n; ++i)
        {
            const ImVec2 ca(a.x + i * cw + 1.f * s, y0), cbb(a.x + (i + 1) * cw - 1.f * s, y1);
            bool isHit = false;
            for (int hpos : hits) isHit = isHit || hpos == i;
            if (isHit)
                gradRect(dl, ca, cbb, withA(P.b, 0.55f), withA(P.a, 0.55f), 3.f * s);
            else
                dl->AddRectFilled(ca, cbb, U32(P.track), 3.f * s);
        }
        if (rep >= 0 && rep < static_cast<int>(hits.size()))
        {
            const int h = hits[rep], src = sources[rep];
            const ImVec2 ca(a.x + h * cw + 1.f * s, y0), cbb(a.x + (h + 1) * cw - 1.f * s, y1);
            glow(dl, ca, cbb, P.b, 3.f * s, 1.4f);
            gradRect(dl, ca, cbb, P.b, P.a, 3.f * s);
            dl->AddRect(ca, cbb, U32(P.text), 3.f * s, 0, 1.5f * s);
            // the slice being replayed
            const ImVec2 sa(a.x + src * cw + 1.f * s, y0), sb(a.x + (src + 1) * cw - 1.f * s, y1);
            dl->AddRect(sa, sb, U32(P.c), 3.f * s, 0, 2.f * s);
            if (src != h)
            {
                // arc from the source slice to the playing hit
                const float xs = (sa.x + sb.x) * 0.5f, xh = (ca.x + cbb.x) * 0.5f;
                ImVec2 pts[12];
                for (int k = 0; k < 12; ++k)
                {
                    const float t = k / 11.f;
                    pts[k] = ImVec2(xs + (xh - xs) * t, y0 - 1.f * s - std::sin(t * IM_PI) * (arcH - 2.f * s));
                }
                dl->AddPolyline(pts, 12, U32(P.c), 0, 1.5f * s);
                dl->AddTriangleFilled(ImVec2(xh, y0 - 0.5f * s), ImVec2(xh - 3.f * s, y0 - 5.f * s), ImVec2(xh + 3.f * s, y0 - 5.f * s), U32(P.c));
            }
        }
    }

    void drawShapePanel(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        float y = panel(dl, a, b, "CUT SHAPE");
        if (panelDice(dl, "shapedice", a, b))
            randomize(kDiceShape);
        const float x0 = a.x + 16.f * s, x1 = b.x - 16.f * s;
        const float w = x1 - x0;

        rangeSlider(dl, LVC_MINAMP, LVC_MAXAMP, "AMPLITUDE", ImVec2(x0, y), w);
        y += 50.f * s;
        rangeSlider(dl, LVC_MINPAN, LVC_MAXPAN, "PAN", ImVec2(x0, y), w, true);
        y += 50.f * s;
        rangeSlider(dl, LVC_MINPITCH, LVC_MAXPITCH, "PITCH", ImVec2(x0, y), w, true);
        y += 56.f * s;

        const float r = 16.f * s;
        const float cw = w / 2.f;
        const float row = r * 2.f + 38.f * s;
        knob(dl, LVC_DUTY, "DUTY", ImVec2(x0 + cw * 0.5f, y + r), r, true, false, cw);
        knob(dl, LVC_FILLDUTY, "FILL DUTY", ImVec2(x0 + cw * 1.5f, y + r), r, true, false, cw);
        knob(dl, LVC_FADE, "FADE", ImVec2(x0 + cw * 0.5f, y + row + r), r, true, false, cw);
        knob(dl, LVC_SEED, "SEED", ImVec2(x0 + cw * 1.5f, y + row + r), r, true, false, cw);
    }

    void fxHeader(ImDrawList* dl, float x0, float x1, float y, const char* title, int toggleParam)
    {
        const bool on = toggleParam < 0 || pv[toggleParam] > 0.5f;
        text(dl, fBold, ImVec2(x0, y + 2.f * s), on ? U32(P.text) : U32(P.textDim), title);
        if (toggleParam >= 0)
            toggle(dl, static_cast<uint32_t>(toggleParam), ImVec2(x1 - 38.f * s, y));
    }

    // Filter, crusher and delay each have their own panel, in signal-flow order. Their per-slice values are drawn at
    // random inside the ranges set here.

    void drawFilterPanel(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        const float top = panel(dl, a, b, "FILTER");
        if (panelDice(dl, "filterdice", a, b))
            randomize(kDiceFilter);
        const float x0 = a.x + 16.f * s, x1 = b.x - 16.f * s, w = x1 - x0;
        const bool on = pv[LVC_FILTER] > 0.5f;
        float y = top - 4.f * s;
        fxHeader(dl, x0, x1, y, "PER SLICE", LVC_FILTER);
        y += 30.f * s;
        rangeSlider(dl, LVC_LPMIN, LVC_LPFREQ, "LOWPASS", ImVec2(x0, y), w, false, on);
        y += 46.f * s;
        rangeSlider(dl, LVC_HPFREQ, LVC_HPMAX, "HIGHPASS", ImVec2(x0, y), w, false, on);
        y += 50.f * s;
        const float r = 19.f * s;
        const float ky = std::min(y + 20.f * s + r, b.y - 14.f * s - r - 32.f * s);
        knob(dl, LVC_RESO, "RESO", ImVec2((x0 + x1) * 0.5f, ky), r, on, false, w);
    }

    void drawCrusherPanel(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        const float top = panel(dl, a, b, "CRUSH");
        if (panelDice(dl, "crushdice", a, b))
            randomize(kDiceCrusher);
        const float x0 = a.x + 16.f * s, x1 = b.x - 16.f * s, w = x1 - x0;
        const bool on = pv[LVC_CRUSHER] > 0.5f;
        float y = top - 4.f * s;
        fxHeader(dl, x0, x1, y, "PER SLICE", LVC_CRUSHER);
        y += 30.f * s;
        rangeSlider(dl, LVC_MINBITS, LVC_MAXBITS, "BITS", ImVec2(x0, y), w, false, on);
        y += 46.f * s;
        rangeSlider(dl, LVC_MINFREQ, LVC_MAXFREQ, "RATE", ImVec2(x0, y), w, false, on);
    }

    // delay module, ECHO mode: stereo mode, time (note values or ms), feedback / mix / tone
    // ---- delay TUNE: the note grid is the pitchmap's key / scale (its TUNE moves A4) ----
    int tunedNote(double seconds) const
    {
        int m = 69;
        lvcTunedPeriod(seconds, chromaMask(), lvcTuneRefA4(pv[LVC_CHTUNE]), &m);
        return m;
    }

    // echo knob text when tuned: the note for pitched times (up to 50 ms = 20 Hz), the snapped time beyond that
    void tunedTimeText(float ms, char* buf, size_t n) const
    {
        int m = 69;
        const double sec = lvcTunedPeriod(ms * 0.001, chromaMask(), lvcTuneRefA4(pv[LVC_CHTUNE]), &m);
        if (sec <= 0.0505)
            noteName(m, buf, n);
        else if (sec < 1.0)
            std::snprintf(buf, n, "%.0f ms", sec * 1000.0);
        else
            std::snprintf(buf, n, "%.2f s", sec);
    }

    void drawEchoControls(ImDrawList* dl, float x0, float x1, float y, bool on)
    {
        const float w = x1 - x0;
        static const char* const st[3] = { "MONO", "L / R", "PING" };
        segmented(dl, LVC_DLYSTEREO, st, 3, ImVec2(x0, y), ImVec2(x1, y + 22.f * s), on);
        if (ImGui::IsMouseHoveringRect(ImVec2(x0, y), ImVec2(x1, y + 22.f * s)))
            tooltip("Mono: same time on both sides  \xc2\xb7  L / R: own time per side  \xc2\xb7  Ping: repeats bounce between the sides");
        y += 30.f * s;

        // time: synced note values, free ms, or free ms tuned to the scale (SYNC and TUNE light one at a time)
        const bool sync = pv[LVC_DLYSYNC] > 0.5f;
        const bool tuned = ! sync && pv[LVC_DLYTUNE] > 0.5f;
        {
            const float pg = 5.f * s, pw = (w - pg) * 0.5f;
            if (pillButton(dl, "##dlysync", "SYNC", ImVec2(x0, y), ImVec2(x0 + pw, y + 18.f * s), sync))
            {
                setParamGesture(LVC_DLYSYNC, sync ? 0.f : 1.f);
                commitUndo(true);
            }
            if (ImGui::IsItemHovered())
                tooltip(sync ? "Synced to the host tempo (note values): click for free time in ms"
                             : "Free time in ms: click to sync to the host tempo");
            if (pillButton(dl, "##dlytune", "TUNE", ImVec2(x1 - pw, y), ImVec2(x1, y + 18.f * s), tuned))
            {
                if (tuned)
                    setParamGesture(LVC_DLYTUNE, 0.f);
                else
                {
                    setParamGesture(LVC_DLYTUNE, 1.f);
                    setParamGesture(LVC_DLYSYNC, 0.f); // tuned times are free times
                }
                commitUndo(true);
            }
            if (ImGui::IsItemHovered())
                tooltip("Tune: the free times snap to notes of the pitchmap's key and scale (time = 1 / pitch).\n"
                        "Short times with high feedback ring at that note; set the scale in SPECTRAL PITCHMAP.");
        }
        y += 26.f * s;

        const bool stereo = static_cast<int>(pv[LVC_DLYSTEREO] + 0.5f) == 1;
        const float r = 17.f * s;
        const uint32_t il = sync ? LVC_DLYTIMEL : LVC_DLYMSL, ir = sync ? LVC_DLYTIMER : LVC_DLYMSR;
        char noteL[24] = "", noteR[24] = "";
        if (tuned)
        {
            tunedTimeText(pv[LVC_DLYMSL], noteL, sizeof(noteL));
            tunedTimeText(pv[LVC_DLYMSR], noteR, sizeof(noteR));
        }
        knob(dl, il, stereo ? "LEFT" : "TIME", ImVec2(x0 + w * 0.25f, y + r), r, on, false, w * 0.5f, -1.f, tuned ? noteL : nullptr);
        knob(dl, ir, "RIGHT", ImVec2(x0 + w * 0.75f, y + r), r, on && stereo, false, w * 0.5f, -1.f, tuned ? noteR : nullptr);
        y += r * 2.f + 40.f * s;

        const float r2 = 15.f * s;
        knob(dl, LVC_FEEDBACK, "FEEDBACK", ImVec2(x0 + w / 6.f, y + r2), r2, on, false, w / 3.f);
        knob(dl, LVC_DLYMIX, "MIX", ImVec2(x0 + w * 0.5f, y + r2), r2, on, false, w / 3.f);
        knob(dl, LVC_DLYTONE, "TONE", ImVec2(x0 + w * 5.f / 6.f, y + r2), r2, on, false, w / 3.f);
    }

    void drawDelayPanel(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        const float top = panel(dl, a, b, "DELAY");
        if (panelDice(dl, "combdice", a, b))
            randomize(kDiceComb);
        const float x0 = a.x + 16.f * s, x1 = b.x - 16.f * s, w = x1 - x0;
        const bool on = pv[LVC_COMB] > 0.5f;
        const bool echo = pv[LVC_DLYMODE] > 0.5f;
        float y = top - 4.f * s;
        fxHeader(dl, x0, x1, y, echo ? "ON" : "PER SLICE", LVC_COMB);
        y += 30.f * s;
        // mode: the per-slice comb, or the synced stereo echo
        static const char* const modes[2] = { "COMB", "ECHO" };
        segmented(dl, LVC_DLYMODE, modes, 2, ImVec2(x0, y), ImVec2(x1, y + 24.f * s));
        y += 32.f * s;
        if (echo)
        {
            drawEchoControls(dl, x0, x1, y, on);
            return;
        }
        // comb delay: feed-forward or feedback
        const bool tuned = pv[LVC_DLYTUNE] > 0.5f;
        text(dl, fSmall, ImVec2(x0, y + 2.f * s), U32(P.textDim, on ? 1.f : 0.6f), "TYPE");
        static const char* const types[2] = { "FF", "FB" };
        const float tuneW = std::max(textSize(fBold, "TUNE").x + 16.f * s, w * 0.36f);
        segmented(dl, LVC_TYPE, types, 2, ImVec2(x0, y + 16.f * s), ImVec2(x1 - tuneW - 6.f * s, y + 40.f * s), on);
        if (pillButton(dl, "##combtune", "TUNE", ImVec2(x1 - tuneW, y + 16.f * s), ImVec2(x1, y + 40.f * s), tuned))
        {
            setParamGesture(LVC_DLYTUNE, tuned ? 0.f : 1.f);
            commitUndo(true);
        }
        if (ImGui::IsItemHovered())
            tooltip("Tune: each slice's comb delay snaps to a note of the pitchmap's key and scale,\n"
                    "so the comb rings in key (FB rings the most). Set the scale in SPECTRAL PITCHMAP.");
        y += 52.f * s;
        rangeSlider(dl, LVC_MINDELAY, LVC_MAXDELAY, "TIME", ImVec2(x0, y), w, false, on);
        y += 50.f * s;
        if (tuned)
        {
            // the notes the comb can ring at: the delay range read as pitches (long delay = low note)
            char lo[16], hi[16], buf[48];
            const float t0 = std::min(pv[LVC_MINDELAY], pv[LVC_MAXDELAY]), t1 = std::max(pv[LVC_MINDELAY], pv[LVC_MAXDELAY]);
            noteName(tunedNote(t1 * 0.001), lo, sizeof(lo));
            noteName(tunedNote(t0 * 0.001), hi, sizeof(hi));
            if (std::strcmp(lo, hi) == 0)
                std::snprintf(buf, sizeof(buf), "%s", lo);
            else
                std::snprintf(buf, sizeof(buf), "%s \xe2\x80\x93 %s", lo, hi);
            textCentered(dl, fSmall, ImVec2((x0 + x1) * 0.5f, y - 8.f * s), U32(P.c, on ? 0.95f : 0.5f), buf);
        }
        const float r = 19.f * s;
        const float ky = std::min(y + 20.f * s + r, b.y - 14.f * s - r - 32.f * s);
        knob(dl, LVC_FEEDBACK, "FEEDBACK", ImVec2((x0 + x1) * 0.5f, ky), r, on, false, w);
    }

    // ---- antialiased band: filled strip (per-vertex colour) between two contours ----
    void fillBand(ImDrawList* dl, const float* xs, const float* y0, const float* y1, const ImU32* cols, int n)
    {
        if (n < 2)
            return;
        const ImVec2 uv = dl->_Data->TexUvWhitePixel;
        dl->PrimReserve((n - 1) * 6, n * 2);
        const ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
        for (int i = 0; i < n; ++i)
        {
            dl->PrimWriteVtx(ImVec2(xs[i], y0[i]), uv, cols[i]);
            dl->PrimWriteVtx(ImVec2(xs[i], y1[i]), uv, cols[i]);
        }
        for (int i = 0; i < n - 1; ++i)
        {
            const ImDrawIdx q = static_cast<ImDrawIdx>(base + i * 2);
            dl->PrimWriteIdx(q); dl->PrimWriteIdx(static_cast<ImDrawIdx>(q + 1)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(q + 2));
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(q + 1)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(q + 3)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(q + 2));
        }
    }

    // antialiased contour, split into runs of the same colour
    void contour(ImDrawList* dl, const float* xs, const float* ys, const ImU32* cols, int n, float thickness)
    {
        int start = 0;
        for (int i = 1; i <= n; ++i)
        {
            if (i == n || cols[i] != cols[start])
            {
                const int end = std::min(n - 1, i); // include the boundary point so runs connect
                const int cnt = end - start + 1;
                if (cnt >= 2)
                {
                    for (int k = 0; k < cnt; ++k)
                        contourPts[k] = ImVec2(xs[start + k], ys[start + k]);
                    dl->AddPolyline(contourPts, cnt, cols[start], 0, thickness);
                }
                start = i;
            }
        }
    }

    // Transient shaper: attack (the hit), pump (the dip right after it), sustain (the tail), with a gain meter
    void drawTransientPanel(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        const float top = panel(dl, a, b, "TRANSIENT");
        const float x0 = a.x + 16.f * s, x1 = b.x - 16.f * s;
        const float w = x1 - x0;
        const bool on = pv[LVC_TRANSIENT] > 0.5f;

        float y = top - 4.f * s;
        fxHeader(dl, x0, x1, y, "SHAPER", LVC_TRANSIENT);
        y += 28.f * s;

        // gain change right now: bipolar meter, boost to the right, cut to the left (+-18 dB)
        {
            const float g = (vis != nullptr && on) ? vis->transientGain.load(std::memory_order_relaxed) : 0.f;
            transMeter = std::fabs(g) > std::fabs(transMeter) ? g : transMeter * std::exp(-ImGui::GetIO().DeltaTime * 6.f);
            const float mh = 6.f * s, cx = (x0 + x1) * 0.5f;
            dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + mh), U32(P.track), mh * 0.5f);
            const float t = std::max(-1.f, std::min(1.f, transMeter / 18.f));
            if (std::fabs(t) > 0.01f)
            {
                const float xe = cx + t * w * 0.5f;
                gradRect(dl, ImVec2(std::min(cx, xe), y), ImVec2(std::max(cx, xe), y + mh),
                         t > 0.f ? P.b : P.rec, t > 0.f ? P.c : P.a, mh * 0.5f, true);
            }
            dl->AddLine(ImVec2(cx, y - 2.f * s), ImVec2(cx, y + mh + 2.f * s), U32(P.textFaint), 1.f);
            char buf[24];
            std::snprintf(buf, sizeof(buf), "%+.1f dB", transMeter);
            textCentered(dl, fSmall, ImVec2(cx, y + mh + 10.f * s), U32(on ? P.textDim : P.textFaint), buf);
            y += mh + 24.f * s;
        }

        // three knobs, stacked
        const float avail = b.y - 12.f * s - y;
        const float rowH = avail / 3.f;
        const float r = std::max(12.f * s, std::min(18.f * s, (rowH - 34.f * s) * 0.5f));
        static const uint32_t idx[3] = { LVC_TRATTACK, LVC_TRPUMP, LVC_TRSUSTAIN };
        static const char* const lbl[3] = { "ATTACK", "PUMP", "SUSTAIN" };
        for (int k = 0; k < 3; ++k)
        {
            const float cy = y + rowH * k + (rowH - 2.f * r - 32.f * s) * 0.5f + r;
            knob(dl, idx[k], lbl[k], ImVec2((x0 + x1) * 0.5f, cy), r, on, k != 1, w);
            if (ImGui::IsItemHovered())
                tooltip(k == 0 ? "Boost (right) or cut (left) the hit of each transient"
                                 : k == 1 ? "Dip the level right after each hit: the hit stands out without getting louder"
                                          : "Boost (right) or cut (left) the tail after each hit");
        }
    }

    void drawReverbPanel(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        float y = panel(dl, a, b, "REVERB");
        if (panelDice(dl, "reverbdice", a, b))
            randomize(kDiceReverb);
        const float x0 = a.x + 16.f * s, x1 = b.x - 16.f * s;
        const float w = x1 - x0;
        const bool on = pv[LVC_REVERB] > 0.5f;

        y -= 4.f * s;
        fxHeader(dl, x0, x1, y, "PER SLICE", LVC_REVERB);
        y += 20.f * s;

        // send adds a tail after the gate; insert puts the reverb in the slice path and cuts its
        // tail when the next slice plays
        text(dl, fSmall, ImVec2(x0, y + 2.f * s), U32(P.textDim, on ? 1.f : 0.6f), "MODE");
        static const char* const rvModes[2] = { "SEND", "INSERT" };
        segmented(dl, LVC_RVMODE, rvModes, 2, ImVec2(x0, y + 14.f * s), ImVec2(x1, y + 38.f * s), on);
        y += 42.f * s;

        // chance that a slice is thrown into the reverb
        const float rc = 20.f * s;
        knob(dl, LVC_RVCHANCE, "CHANCE", ImVec2(x0 + w * 0.5f, y + rc), rc, on, false, w);
        y += rc * 2.f + 40.f * s;

        rangeSlider(dl, LVC_RVSENDMIN, LVC_RVSENDMAX, "SEND", ImVec2(x0, y), w, false, on);
        y += 46.f * s;
        rangeSlider(dl, LVC_RVDECAYMIN, LVC_RVDECAYMAX, "DECAY", ImVec2(x0, y), w, false, on);
        y += 50.f * s;

        const float r = 16.f * s;
        const float ky = std::min(y + r, b.y - 14.f * s - r - 30.f * s);
        knob(dl, LVC_RVSIZE, "SIZE", ImVec2(x0 + w * 0.25f, ky), r, on, false, w * 0.5f);
        knob(dl, LVC_RVDAMP, "DAMP", ImVec2(x0 + w * 0.75f, ky), r, on, false, w * 0.5f);
    }

    void drawMasterPanel(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        const float top = panel(dl, a, b, "MASTER");
        if (panelDice(dl, "masterdice", a, b))
            randomize(kDiceMaster);
        const float x0 = a.x + 16.f * s, x1 = b.x - 16.f * s;
        const float w = x1 - x0;
        const float r = 18.f * s;
        const bool limOn = pv[LVC_LIMITER] > 0.5f;

        // limiter
        float y = top - 4.f * s;
        fxHeader(dl, x0, x1, y, "LIMITER", LVC_LIMITER);
        knob(dl, LVC_LIMCEIL, "CEILING", ImVec2(x0 + w * 0.25f, y + 30.f * s + r), r, limOn, false, w * 0.5f);
        knob(dl, LVC_LIMREL, "RELEASE", ImVec2(x0 + w * 0.75f, y + 30.f * s + r), r, limOn, false, w * 0.5f);
        y += 30.f * s + r * 2.f + 36.f * s;

        // gain reduction meter
        {
            const float gr = (vis != nullptr && limOn) ? vis->limiterGR.load(std::memory_order_relaxed) : 0.f;
            char buf[24];
            std::snprintf(buf, sizeof(buf), gr > 0.05f ? "-%.1f dB" : "0.0 dB", gr);
            text(dl, fSmall, ImVec2(x0, y), U32(P.textFaint), "GAIN REDUCTION");
            y += fs(fSmall) + 5.f * s;
            const float mh = 8.f * s;
            const ImVec2 ma(x0, y), mb(x1, y + mh);
            dl->AddRectFilled(ma, mb, U32(P.track), mh * 0.5f);
            const float t = std::min(1.f, gr / 12.f);
            if (t > 0.002f)
            {
                const ImVec2 fa(mb.x - (mb.x - ma.x) * t, ma.y);
                gradRect(dl, fa, mb, P.c, P.rec, mh * 0.5f, true);
            }
            for (int db = 3; db < 12; db += 3)
            {
                const float tx = mb.x - (mb.x - ma.x) * db / 12.f;
                dl->AddLine(ImVec2(tx, ma.y + 1.f), ImVec2(tx, mb.y - 1.f), U32(P.panelBorder), 1.f);
            }
            y += mh + 4.f * s;
            textRight(dl, fSmall, ImVec2(x1, y), U32(gr > 0.05f ? P.text : P.textDim, limOn ? 1.f : 0.5f), buf);
            y += fs(fSmall) + 6.f * s;
        }
        dl->AddLine(ImVec2(x0, y), ImVec2(x1, y), U32(P.panelBorder), 1.f);

        // master filters: fully bypassed at the open extremes (HP left, LP right)
        {
            text(dl, fSmall, ImVec2(x0, y + 2.f * s), U32(P.textDim), "FILTERS");
            const float colW = w * 0.5f;
            const float fr = 16.f * s;
            const float ky = y + 20.f * s + fr;
            knob(dl, LVC_MASTERHPFREQ, "HIGHPASS", ImVec2(x0 + colW * 0.5f, ky), fr, true, false, colW);
            knob(dl, LVC_MASTERLPFREQ, "LOWPASS", ImVec2(x0 + colW * 1.5f, ky), fr, true, false, colW);
            y += 20.f * s + fr * 2.f + 37.f * s;
        }
        dl->AddLine(ImVec2(x0, y), ImVec2(x1, y), U32(P.panelBorder), 1.f);

        // makeup gain (after the limiter) and dry / wet, side by side in the remaining space
        {
            const float rb = 19.f * s;
            const float areaTop = y + 2.f * s, areaBot = b.y - 10.f * s;
            const float blockH = rb * 2.f + 34.f * s;
            const float cy = areaTop + std::max(0.f, (areaBot - areaTop - blockH) * 0.5f) + rb + 4.f * s;
            const float colW = w * 0.5f;
            knob(dl, LVC_MAKEUP, "MAKEUP", ImVec2(x0 + colW * 0.5f, cy), rb, true, false, colW);
            if (ImGui::IsItemHovered())
                tooltip("Output gain after the limiter, to bring the level back up.\n"
                                  "A boost here can go past the limiter's ceiling.");
            knob(dl, LVC_MIX, "DRY / WET", ImVec2(x0 + colW * 1.5f, cy), rb, true, false, colW);
        }
    }

    // ---- Spectral Pitchmap panel ----

    // DRY = what the region holds, TUNED = what was moved (fixed hues so the legend always means the same)
    ImVec4 chromaDryCol() const  { return P.dark ? hex(0xB86FD8) : hex(0x8A3FAE); }
    ImVec4 chromaTuneCol() const { return P.dark ? hex(0x4FB3E8) : hex(0x1F78B4); }

    // pitch classes the pitchmap is using: what the DSP reports, or the key / scale when there is no live state
    uint16_t chromaMask() const
    {
        if (vis != nullptr)
            return static_cast<uint16_t>(vis->chromaMask.load(std::memory_order_relaxed));
        return lvcScalePitchClasses(static_cast<int>(pv[LVC_CHSCALE] + 0.5f), static_cast<int>(pv[LVC_CHKEY] + 0.5f));
    }

    void setScale(int scale, int key)
    {
        setParamGesture(LVC_CHSCALE, static_cast<float>(((scale % LVC_NUM_SCALES) + LVC_NUM_SCALES) % LVC_NUM_SCALES));
        setParamGesture(LVC_CHKEY, static_cast<float>(((key % 12) + 12) % 12));
    }

    // one octave keyboard: click a key to set the root; in-key notes lit, held MIDI notes ringed
    void drawKeyboard(ImDrawList* dl, ImVec2 a, ImVec2 b, bool on)
    {
        static const int whitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static const int blackPc[5] = { 1, 3, 6, 8, 10 };
        static const int blackAfter[5] = { 0, 1, 3, 4, 5 }; // white key index the black key sits after
        const float ww = (b.x - a.x) / 7.f, bkw = ww * 0.62f, bkh = (b.y - a.y) * 0.6f;
        const uint16_t mask = chromaMask();
        const uint32_t held = vis != nullptr ? vis->chromaHeld.load(std::memory_order_relaxed) : 0u;
        const int key = static_cast<int>(pv[LVC_CHKEY] + 0.5f);
        const bool midi = DISTRHO_PLUGIN_WANT_MIDI_INPUT && pv[LVC_CHMIDI] > 0.5f;
        const float alpha = on ? 1.f : 0.55f;

        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton("##chkeys", b - a);
        const bool hov = ImGui::IsItemHovered();
        int hoverPc = -1;
        if (hov)
        {
            const ImVec2 m = ImGui::GetIO().MousePos;
            if (m.y < a.y + bkh)
                for (int k = 0; k < 5; ++k)
                {
                    const float cx = a.x + ww * (blackAfter[k] + 1);
                    if (m.x >= cx - bkw * 0.5f && m.x <= cx + bkw * 0.5f) hoverPc = blackPc[k];
                }
            if (hoverPc < 0)
                hoverPc = whitePc[std::max(0, std::min(6, static_cast<int>((m.x - a.x) / ww)))];
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            char tip[160];
            std::snprintf(tip, sizeof(tip), tr("Root: %s (a blue line marks notes the pitchmap is moving sound to)%s"), LVC_KEY_NAMES[hoverPc],
                          tr(midi ? "  (MIDI mode: held notes choose the notes)" : ""));
            tooltip("%s", tip);
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && hoverPc >= 0)
            setParamGesture(LVC_CHKEY, static_cast<float>(hoverPc));

        // where the pitchmap is moving material to right now: a glow along the bottom of the key
        {
            float lv[12] = {};
            if (vis != nullptr)
                for (int n = 0; n < LiveCutVis::kChromaNotes; ++n)
                    lv[n % 12] = std::max(lv[n % 12], vis->chromaTuned[n]);
            const float fall = std::exp(-std::max(0.f, std::min(0.1f, ImGui::GetIO().DeltaTime)) * 5.f);
            for (int pc = 0; pc < 12; ++pc)
            {
                const float t = lv[pc] > 1e-6f ? std::max(0.f, std::min(1.f, (20.f * std::log10(lv[pc]) + 60.f) / 60.f)) : 0.f;
                chGlow[pc] = std::max(t, chGlow[pc] * fall);
            }
        }
        const float rr = 4.f * s;
        auto drawKey = [&](ImVec2 ka, ImVec2 kb, int pc, bool black) {
            const bool in = (mask >> pc) & 1u;
            const bool isRoot = pc == key && ! (midi && held != 0);
            const ImVec4 base = black ? (P.dark ? hex(0x1B1E2A) : hex(0x30343F)) : (P.dark ? hex(0xD6DAE4) : hex(0xFFFFFF));
            const ImDrawFlags rf = ImDrawFlags_RoundCornersBottom;
            if (isRoot)
                gradRect(dl, ka, kb, withA(P.a, alpha), withA(P.b, alpha), rr, false, rf);
            else
                dl->AddRectFilled(ka, kb, U32(base, alpha), rr, rf);
            // out-of-key notes are shaded so the scale reads at a glance
            if (! in)
                dl->AddRectFilled(ka, kb, U32(black ? hex(0x000000) : hex(0x1A1D29), black ? 0.35f : (P.dark ? 0.55f : 0.30f)), rr, rf);
            if (pc == hoverPc)
                dl->AddRectFilled(ka, kb, U32(black ? hex(0xFFFFFF) : hex(0x000000), 0.12f), rr, rf);
            dl->AddRect(ka, kb, U32(hex(0x000000), P.dark ? 0.55f : 0.18f), rr, rf, 1.f);
            // in-key dot at the bottom of the key, held MIDI notes underlined
            const ImVec2 mc((ka.x + kb.x) * 0.5f, kb.y - 8.f * s);
            if (in && ! isRoot)
                dl->AddCircleFilled(mc, 2.6f * s, U32(black ? mix(P.a, hex(0xFFFFFF), 0.3f) : P.a, alpha), 12);
            if (isRoot)
                dl->AddCircleFilled(mc, 2.6f * s, U32(P.onAccent, alpha), 12);
            if ((held >> pc) & 1u || chGlow[pc] > 0.01f)
            {
                const float g = ((held >> pc) & 1u) ? 1.f : chGlow[pc];
                dl->AddRectFilled(ImVec2(ka.x + 2.f * s, kb.y - 4.f * s), ImVec2(kb.x - 2.f * s, kb.y - 1.f * s),
                                  U32(chromaTuneCol(), 0.25f + 0.75f * g), 1.5f * s);
            }
        };
        for (int k = 0; k < 7; ++k)
            drawKey(ImVec2(a.x + ww * k + 0.5f, a.y), ImVec2(a.x + ww * (k + 1) - 0.5f, b.y), whitePc[k], false);
        for (int k = 0; k < 5; ++k)
        {
            const float cx = a.x + ww * (blackAfter[k] + 1);
            drawKey(ImVec2(cx - bkw * 0.5f, a.y), ImVec2(cx + bkw * 0.5f, a.y + bkh), blackPc[k], true);
        }
    }

    void drawScalePopup()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f * s, 8.f * s));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
        if (ImGui::BeginPopup("##chscales", ImGuiWindowFlags_NoMove))
        {
            ImDrawList* pdl = ImGui::GetWindowDrawList();
            const float cw = 150.f * s, rh = 24.f * s;
            const int rows = (LVC_NUM_SCALES + 1) / 2;
            const int cur = static_cast<int>(pv[LVC_CHSCALE] + 0.5f);
            text(pdl, fSmall, ImGui::GetCursorScreenPos() + ImVec2(8.f * s, 2.f * s), U32(P.textDim), "SCALE");
            ImGui::Dummy(ImVec2(cw * 2.f, 22.f * s));
            const ImVec2 o = ImGui::GetCursorScreenPos();
            for (int i = 0; i < LVC_NUM_SCALES; ++i)
            {
                const ImVec2 ra(o.x + (i / rows) * cw, o.y + (i % rows) * rh);
                ImGui::SetCursorScreenPos(ra);
                ImGui::PushID(i);
                const bool clicked = ImGui::InvisibleButton("##sc", ImVec2(cw - 4.f * s, rh));
                const bool hov = ImGui::IsItemHovered();
                ImGui::PopID();
                if (clicked)
                {
                    setParamGesture(LVC_CHSCALE, static_cast<float>(i));
                    ImGui::CloseCurrentPopup();
                }
                const ImVec2 rb(ra.x + cw - 4.f * s, ra.y + rh);
                if (i == cur)
                    pdl->AddRectFilled(ra, rb, U32(P.a, 0.16f), 6.f * s);
                else if (hov)
                    pdl->AddRectFilled(ra, rb, U32(P.text, 0.06f), 6.f * s);
                text(pdl, fBody, ImVec2(ra.x + 8.f * s, ra.y + rh * 0.5f - fs(fBody) * 0.5f - 1.f * s),
                     U32(i == cur || hov ? P.text : P.textDim), LVC_SCALES[i].name);
            }
            ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + rows * rh));
            ImGui::Dummy(ImVec2(cw * 2.f, 2.f * s));
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
    }

    // [<] scale name [>]: arrows / wheel step through the scales, click opens the list,
    // shift-click switches to the relative scale (major <-> minor, same notes)
    void drawScaleSelector(ImDrawList* dl, ImVec2 a, ImVec2 b, bool on)
    {
        const int cur = static_cast<int>(pv[LVC_CHSCALE] + 0.5f);
        const int key = static_cast<int>(pv[LVC_CHKEY] + 0.5f);
        const float h = b.y - a.y, aw = h;
        if (pillButton(dl, "##chprev", "<", a, ImVec2(a.x + aw, b.y), false, on))
            setParamGesture(LVC_CHSCALE, static_cast<float>((cur + LVC_NUM_SCALES - 1) % LVC_NUM_SCALES));
        if (pillButton(dl, "##chnext", ">", ImVec2(b.x - aw, a.y), b, false, on))
            setParamGesture(LVC_CHSCALE, static_cast<float>((cur + 1) % LVC_NUM_SCALES));

        const ImVec2 na(a.x + aw + 4.f * s, a.y), nb(b.x - aw - 4.f * s, b.y);
        ImGui::SetCursorScreenPos(na);
        const bool clicked = ImGui::InvisibleButton("##chscale", nb - na);
        const bool hov = ImGui::IsItemHovered();
        const ImGuiIO& io = ImGui::GetIO();
        if (clicked)
        {
            const LvcScale& sc = LVC_SCALES[cur];
            if (io.KeyShift && sc.relative >= 0)
                setScale(sc.relative, key + sc.relShift);
            else
                ImGui::OpenPopup("##chscales");
        }
        if (hov && io.MouseWheel != 0.f)
        {
            wheelEdit = true;
            setParamGesture(LVC_CHSCALE, static_cast<float>((cur + (io.MouseWheel > 0.f ? LVC_NUM_SCALES - 1 : 1)) % LVC_NUM_SCALES));
        }
        inset(dl, na, nb, h * 0.32f);
        if (hov)
        {
            dl->AddRectFilled(na, nb, U32(P.text, 0.05f), h * 0.32f);
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            tooltip(LVC_SCALES[cur].relative >= 0 ? "Click: all scales  \xc2\xb7  wheel: step  \xc2\xb7  shift-click: relative scale"
                                                            : "Click: all scales  \xc2\xb7  wheel: step");
        }
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%s %s", LVC_KEY_NAMES[key], tr(LVC_SCALES[cur].name));
        dl->PushClipRect(na, nb, true);
        textCentered(dl, fBold, (na + nb) * 0.5f, U32(on ? P.text : P.textDim), buf);
        dl->PopClipRect();
        ImGui::SetNextWindowPos(ImVec2(na.x, nb.y + 4.f * s));
        drawScalePopup();
    }

    static void noteName(int note, char* buf, size_t sz)
    {
        std::snprintf(buf, sz, "%s%d", LVC_KEY_NAMES[((note % 12) + 12) % 12], static_cast<int>(std::floor(note / 12.0)) - 1);
    }

    // ---- pitchmap spectrum (top row): what the region holds and where it was moved; region edges drag ----
    void drawPitchmapView(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        const float top = panel(dl, a, b, "PITCHMAP");
        const bool on = pv[LVC_CHROMA] > 0.5f;
        const int key = static_cast<int>(pv[LVC_CHKEY] + 0.5f);
        const int scale = static_cast<int>(pv[LVC_CHSCALE] + 0.5f);
        {
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%s %s", LVC_KEY_NAMES[key], tr(LVC_SCALES[scale].name));
            for (char* c = buf; *c; ++c) *c = static_cast<char>(std::toupper(static_cast<unsigned char>(*c)));
            float x = b.x - 16.f * s;
            chip(dl, ImVec2(x, a.y + 12.f * s), on ? buf : "OFF", on ? P.b : P.textDim, x);
        }
        const ImVec2 ia(a.x + 14.f * s, top), ib(b.x - 14.f * s, b.y - 14.f * s);
        inset(dl, ia, ib, 10.f * s);
        crtAdd(ia, ib, 10.f * s);

        const float fMin = 30.f, fMax = 16000.f;
        const float lf = std::log(fMax / fMin);
        const float x0 = ia.x + 10.f * s, x1 = ib.x - 10.f * s, w = x1 - x0;
        const float yTop = ia.y + 36.f * s, yBot = ib.y - 20.f * s, hh = yBot - yTop; // below the legend and the edge tabs
        auto X = [&](float f) { return x0 + std::log(std::max(f, 1.f) / fMin) / lf * w; };
        auto F = [&](float x) { return fMin * std::exp((x - x0) / w * lf); };
        const float refHz = 440.f * std::pow(2.f, pv[LVC_CHTUNE] / 1200.f);
        auto noteHz = [&](int n) { return refHz * std::pow(2.f, (n - 69) / 12.f); };
        const uint16_t mask = chromaMask();

        // ---- input: region edges are draggable, double-click resets them ----
        const float lo = pv[LVC_CHLOW], hi = pv[LVC_CHHIGH];
        const float xLo = X(std::min(lo, hi)), xHi = X(std::max(lo, hi));
        ImGui::SetCursorScreenPos(ia);
        ImGui::InvisibleButton("##chspec", ib - ia);
        const bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
        const ImGuiIO& io = ImGui::GetIO();
        // the edge nearest the cursor is the one a click grabs (anywhere in the view, not only on the handle);
        // it is highlighted while hovering
        const int nearEdge = std::fabs(io.MousePos.x - xLo) <= std::fabs(io.MousePos.x - xHi) ? 0 : 1;
        const int hotEdge = hov && chRegionDrag < 0 ? nearEdge : -1;
        if (ImGui::IsItemActivated())
        {
            chRegionDrag = nearEdge;
            editParameter(chRegionDrag == 0 ? LVC_CHLOW : LVC_CHHIGH, true);
        }
        if (hov && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (chRegionDrag >= 0)
                editParameter(chRegionDrag == 0 ? LVC_CHLOW : LVC_CHHIGH, false);
            chRegionDrag = -2; // ignore the rest of this press
            setParamGesture(LVC_CHLOW, LVC_DEFAULTS[LVC_CHLOW]);
            setParamGesture(LVC_CHHIGH, LVC_DEFAULTS[LVC_CHHIGH]);
        }
        else if (act && chRegionDrag >= 0)
        {
            // keep the edges a few semitones apart
            const float f = std::max(20.f, std::min(20000.f, F(io.MousePos.x)));
            if (chRegionDrag == 0)
                setParam(LVC_CHLOW, std::min(f, pv[LVC_CHHIGH] / 1.2f));
            else
                setParam(LVC_CHHIGH, std::max(f, pv[LVC_CHLOW] * 1.2f));
        }
        if (ImGui::IsItemDeactivated() || (chRegionDrag != -1 && ! act))
        {
            if (chRegionDrag >= 0)
                editParameter(chRegionDrag == 0 ? LVC_CHLOW : LVC_CHHIGH, false);
            chRegionDrag = -1;
        }
        if (hotEdge >= 0 || chRegionDrag >= 0)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        dl->PushClipRect(ImVec2(ia.x + 1.f, ia.y + 1.f), ImVec2(ib.x - 1.f, ib.y - 1.f), true);

        // region: lit inside, dimmed outside
        dl->AddRectFilled(ImVec2(xLo, yTop - 6.f * s), ImVec2(xHi, yBot), U32(P.a, P.dark ? 0.07f : 0.06f), 4.f * s);
        dl->AddRectFilled(ImVec2(ia.x, ia.y), ImVec2(xLo, ib.y), U32(P.inset, 0.35f));
        dl->AddRectFilled(ImVec2(xHi, ia.y), ImVec2(ib.x, ib.y), U32(P.inset, 0.35f));

        // the scale: a faint line on each in-key note, stronger on the root
        for (int n = 12; n < 128; ++n)
        {
            const float f = noteHz(n);
            if (f < fMin || f > fMax || ! ((mask >> (n % 12)) & 1u)) continue;
            const float x = X(f);
            const bool root = n % 12 == key;
            dl->AddLine(ImVec2(x, yTop), ImVec2(x, yBot), U32(root ? P.c : P.text, root ? 0.16f : 0.05f), 1.f);
        }

        // levels, smoothed for display
        const float fall = std::exp(-std::max(0.f, std::min(0.1f, io.DeltaTime)) * 5.f);
        const float semiPx = w / lf * std::log(2.f) / 12.f;
        const float barW = std::max(1.5f * s, std::min(5.f * s, semiPx * 0.55f));
        auto heightOf = [&](float lvl) { // -60 .. 0 dB
            return lvl > 1e-6f ? std::max(0.f, std::min(1.f, (20.f * std::log10(lvl) + 60.f) / 60.f)) : 0.f;
        };
        for (int n = 0; n < LiveCutVis::kChromaNotes; ++n)
        {
            const float vd = vis != nullptr ? vis->chromaDry[n] : 0.f;
            const float vt = vis != nullptr ? vis->chromaTuned[n] : 0.f;
            chDispDry[n] = std::max(vd, chDispDry[n] * fall);
            chDispTuned[n] = std::max(vt, chDispTuned[n] * fall);
            const float f = noteHz(n);
            if (f < fMin || f > fMax) continue;
            const float x = X(f);
            const float td = heightOf(chDispDry[n]), tt = heightOf(chDispTuned[n]);
            if (td > 0.f)
                dl->AddRectFilled(ImVec2(x - barW * 0.5f, yBot - td * hh), ImVec2(x + barW * 0.5f, yBot),
                                  U32(chromaDryCol(), 0.25f + 0.75f * td), barW * 0.5f);
            if (tt > 0.f)
            {
                const ImVec4 c = chromaTuneCol();
                const float bw2 = barW * 0.75f;
                for (int g = 2; g >= 1; --g)
                    dl->AddRectFilled(ImVec2(x - bw2 * 0.5f - g * 1.5f * s, yBot - tt * hh - g * 1.5f * s),
                                      ImVec2(x + bw2 * 0.5f + g * 1.5f * s, yBot), U32(c, 0.08f * tt), bw2);
                dl->AddRectFilled(ImVec2(x - bw2 * 0.5f, yBot - tt * hh), ImVec2(x + bw2 * 0.5f, yBot),
                                  U32(c, 0.35f + 0.65f * tt), bw2 * 0.5f);
            }
        }

        // baseline + C labels (every other octave when the view is narrow)
        dl->AddLine(ImVec2(x0, yBot), ImVec2(x1, yBot), U32(P.textFaint, 0.6f), 1.f);
        const int step = semiPx * 12.f < 34.f * s ? 2 : 1;
        for (int oct = 1; oct <= 9; ++oct)
        {
            const float f = noteHz(12 * (oct + 1));
            if (f < fMin || f > fMax) continue;
            const float x = X(f);
            dl->AddLine(ImVec2(x, yBot), ImVec2(x, yBot + 3.f * s), U32(P.textFaint), 1.f);
            if (oct % step != 0) continue;
            char lb[8];
            std::snprintf(lb, sizeof(lb), "C%d", oct);
            textCentered(dl, fSmall, ImVec2(x, yBot + 3.f * s + fs(fSmall) * 0.5f + 1.f * s), U32(P.textFaint), lb);
        }

        // region edges with grab tabs
        for (int e = 0; e < 2; ++e)
        {
            const float x = e == 0 ? xLo : xHi;
            const bool hot = hotEdge == e || chRegionDrag == e;
            dl->AddLine(ImVec2(x, yTop - 6.f * s), ImVec2(x, yBot), U32(hot ? P.c : P.b, hot ? 1.f : 0.55f), hot ? 2.f * s : 1.f * s);
            dl->AddRectFilled(ImVec2(x - 4.f * s, yTop - 11.f * s), ImVec2(x + 4.f * s, yTop - 1.f * s),
                              U32(hot ? P.c : P.b, hot ? 1.f : 0.75f), 2.f * s);
        }

        // legend + region readout
        {
            const float ly = ia.y + 7.f * s;
            float lx = ia.x + 10.f * s;
            dl->AddCircleFilled(ImVec2(lx + 3.f * s, ly + fs(fSmall) * 0.5f + 1.f * s), 3.f * s, U32(chromaDryCol()), 12);
            text(dl, fSmall, ImVec2(lx + 10.f * s, ly), U32(P.textDim), "DRY");
            lx += 10.f * s + textSize(fSmall, "DRY").x + 12.f * s;
            dl->AddCircleFilled(ImVec2(lx + 3.f * s, ly + fs(fSmall) * 0.5f + 1.f * s), 3.f * s, U32(chromaTuneCol()), 12);
            text(dl, fSmall, ImVec2(lx + 10.f * s, ly), U32(P.textDim), "TUNED");
            char flo[24], fhi[24], buf[64];
            formatValue(LVC_CHLOW, std::min(lo, hi), flo, sizeof(flo));
            formatValue(LVC_CHHIGH, std::max(lo, hi), fhi, sizeof(fhi));
            std::snprintf(buf, sizeof(buf), "%s \xe2\x80\x93 %s", flo, fhi);
            textRight(dl, fSmall, ImVec2(ib.x - 10.f * s, ly), U32(hotEdge >= 0 || chRegionDrag >= 0 ? P.c : P.textDim), buf);
        }

        if (! on)
        {
            const char* msg = "PITCHMAP IS OFF";
            const ImVec2 ts = textSize(fBold, msg);
            const ImVec2 c((ia.x + ib.x) * 0.5f, (yTop + yBot) * 0.5f);
            dl->AddRectFilled(ImVec2(c.x - ts.x * 0.5f - 12.f * s, c.y - ts.y * 0.5f - 6.f * s),
                              ImVec2(c.x + ts.x * 0.5f + 12.f * s, c.y + ts.y * 0.5f + 6.f * s), U32(P.inset, 0.9f), 8.f * s);
            textCentered(dl, fBold, c, U32(P.textDim), msg);
        }
        dl->PopClipRect();

        // hover readout: the note under the cursor, or what an edge does
        const bool onHandle = std::fabs(io.MousePos.x - (nearEdge == 0 ? xLo : xHi)) < 7.f * s;
        if (hov && ! onHandle && ! act && io.MousePos.y > yTop && io.MousePos.y < yBot)
        {
            const int n = static_cast<int>(std::lround(69.f + 12.f * std::log2(F(io.MousePos.x) / refHz)));
            if (n >= 0 && n < LiveCutVis::kChromaNotes)
            {
                char nn[8], tip[96];
                noteName(n, nn, sizeof(nn));
                std::snprintf(tip, sizeof(tip), "%s  %.1f Hz%s", nn, noteHz(n), tr(((mask >> (n % 12)) & 1u) ? "  (in key)" : ""));
                tooltip("%s", tip);
            }
        }
        else if (hov && onHandle && ! act)
        {
            tooltip("Drag to set the %s edge of the region (click anywhere moves the nearest edge, double-click resets)",
                              tr(nearEdge == 0 ? "low" : "high"));
        }
    }

    void drawPitchmapPanel(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        const float top = panel(dl, a, b, "SPECTRAL PITCHMAP");
        if (panelDice(dl, "pitchmapdice", a, b))
            randomize(kDiceChroma);
        const float x0 = a.x + 16.f * s, x1 = b.x - 16.f * s;
        const float w = x1 - x0;
        const bool on = pv[LVC_CHROMA] > 0.5f;
        // the key / scale is also the delay's TUNE grid: it stays lit while the delay uses it
        const bool delayScale = pv[LVC_COMB] > 0.5f && pv[LVC_DLYTUNE] > 0.5f
                             && (pv[LVC_DLYMODE] < 0.5f || pv[LVC_DLYSYNC] < 0.5f);
        const bool keyLive = on || delayScale;

        // on / off, with the latency it adds
        float y = top - 4.f * s;
        {
            const int lat = vis != nullptr ? vis->chromaLatency.load(std::memory_order_relaxed) : 0;
            const float sr = vis != nullptr ? vis->sampleRate.load(std::memory_order_relaxed) : 44100.f;
            char buf[32];
            if (on && lat > 0)
                std::snprintf(buf, sizeof(buf), "+%.0f ms", 1000.f * lat / std::max(1.f, sr));
            else
                std::snprintf(buf, sizeof(buf), "%s", delayScale && ! on ? "DELAY SCALE" : "KEY");
            fxHeader(dl, x0, x1, y, buf, LVC_CHROMA);
            if (delayScale && ! on)
                dl->AddText(fBold, fs(fBold), ImVec2(x0, y + 2.f * s), U32(P.c), tr("DELAY SCALE"));
            const ImRect hr(ImVec2(x0, y), ImVec2(x1 - 44.f * s, y + 20.f * s));
            if (hr.Contains(ImGui::GetIO().MousePos))
            {
                if (on && lat > 0)
                    tooltip("Latency added by the pitchmap (reported to the host, which compensates it)");
                else if (delayScale)
                    tooltip("The delay's TUNE follows this key, scale and TUNE knob, also while the pitchmap is off");
            }
        }
        y += 26.f * s;

        // root on the keyboard, scale below
        const float kh = 40.f * s;
        drawKeyboard(dl, ImVec2(x0, y), ImVec2(x1, y + kh), keyLive);
        y += kh + 7.f * s;
        drawScaleSelector(dl, ImVec2(x0, y), ImVec2(x1, y + 22.f * s), keyLive);
        y += 22.f * s + 9.f * s;

        rangeSlider(dl, LVC_CHLOW, LVC_CHHIGH, "REGION", ImVec2(x0, y), w, false, on);
        y += 42.f * s;

        // tone
        const float cw4 = w / 4.f, r = 14.f * s;
        knob(dl, LVC_CHCOLOR, "COLOR", ImVec2(x0 + cw4 * 0.5f, y + r), r, on, false, cw4);
        knob(dl, LVC_CHPITCH, "PITCH", ImVec2(x0 + cw4 * 1.5f, y + r), r, on, true, cw4);
        knob(dl, LVC_CHTUNE, "TUNE", ImVec2(x0 + cw4 * 2.5f, y + r), r, keyLive, true, cw4);
        knob(dl, LVC_CHGAIN, "GAIN", ImVec2(x0 + cw4 * 3.5f, y + r), r, on, false, cw4);
        y += r * 2.f + 36.f * s;
        dl->AddLine(ImVec2(x0, y - 4.f * s), ImVec2(x1, y - 4.f * s), U32(P.panelBorder), 1.f);

        // spectral stage: morph and gate (each a switch over its knob), FFT size
        const float cw3 = w / 3.f;
        auto switched = [&](uint32_t sw, uint32_t kn, const char* label, float cx, const char* tip) {
            const float sScale = s;
            s = sScale * 0.8f; // compact switch: the regular toggle at 80 %
            const float tw = 38.f * s;
            toggle(dl, sw, ImVec2(cx - tw * 0.5f, y));
            s = sScale;
            if (ImGui::IsItemHovered())
                tooltip("%s", tip);
            knob(dl, kn, label, ImVec2(cx, y + 24.f * s + r), r, on && pv[sw] > 0.5f, false, cw3);
        };
        switched(LVC_CHMORPH, LVC_CHMORPHAMT, "MORPH", x0 + cw3 * 0.5f,
                 "Spectral morph: blends the processed magnitudes toward the dry ones (keeps the processed phase),\n"
                 "so the pitchmap's colour sits on the dry signal's transients and tails. Adds the FFT latency.");
        switched(LVC_CHGATE, LVC_CHGATETHR, "GATE", x0 + cw3 * 1.5f,
                 "Spectral gate: silences each processed bin while the same dry bin is under the threshold,\n"
                 "which shortens ringing and tails. Adds the FFT latency.");
        {
            const bool spec = pv[LVC_CHMORPH] > 0.5f || pv[LVC_CHGATE] > 0.5f;
            const float cx = x0 + cw3 * 2.5f, pw = std::min(cw3 - 6.f * s, 60.f * s), ph = 20.f * s;
            textCentered(dl, fSmall, ImVec2(cx, y + 8.f * s), U32(P.textDim, on ? 1.f : 0.6f), "FFT");
            static const char* const sizes[2] = { "512", "1024" };
            for (int k = 0; k < 2; ++k)
            {
                const ImVec2 pa(cx - pw * 0.5f, y + 20.f * s + k * (ph + 5.f * s));
                const bool cur = static_cast<int>(pv[LVC_CHFFT] + 0.5f) == k;
                ImGui::PushID(k);
                if (pillButton(dl, "##chfft", sizes[k], pa, ImVec2(pa.x + pw, pa.y + ph), cur, on && spec))
                    setParamGesture(LVC_CHFFT, static_cast<float>(k));
                ImGui::PopID();
                if (ImGui::IsItemHovered())
                    tooltip("FFT size of morph / gate: larger is cleaner, with more latency");
            }
        }
    }

    // Bottom signal-flow ribbon. The slicer and the five effects form one row that can be put in any order by dragging
    // (LVC_ROUTE); effects in front of the slicer process what it records. A click on SLICE, without dragging, switches
    // the slicer on / off. IN and the stages after the row are shown for context only.
    void drawSignalFlow(ImDrawList* dl, ImVec2 a, ImVec2 b)
    {
        static constexpr int kSlice = kRouteSlice;    // id of the slicer block in the movable row
        static constexpr int kRow = kFlowItems;       // slicer + four effects + pitchmap + transient shaper
        inset(dl, a, b, 8.f * s);
        text(dl, fSmall, ImVec2(a.x + 10.f * s, a.y + 3.f * s), U32(P.textFaint),
             "SIGNAL FLOW   \xc2\xb7   DRAG ANY BLOCK TO REORDER (EFFECTS BEFORE SLICE PROCESS WHAT IT RECORDS)   \xc2\xb7   CLICK SLICE TO SWITCH THE SLICER");

        int order[kRow];
        flowFromParams(pv, order);
        static const char* const names[kRow] = { "CRUSH", "DELAY", "FILTER", "REVERB", "PITCHMAP", "SLICE", "TRANSIENT" };
        const bool on[kRow] = {
            pv[LVC_CRUSHER] > 0.5f, pv[LVC_COMB] > 0.5f, pv[LVC_FILTER] > 0.5f, pv[LVC_REVERB] > 0.5f,
            pv[LVC_CHROMA] > 0.5f, true, pv[LVC_TRANSIENT] > 0.5f
        };

        const int n = kRow + 5; // IN, the movable row, DRY/WET, HP/LP, LIMIT, OUT
        const float px = 12.f * s, gap = 12.f * s;
        const float y0 = a.y + 19.f * s, y1 = b.y - 6.f * s;
        const float bw = (b.x - a.x - px * 2.f - gap * (n - 1)) / n;
        auto slotX = [&](int i) { return a.x + px + i * (bw + gap) + bw * 0.5f; };
        static const char* const tailNames[4] = { "DRY/WET", "HP/LP", "LIMIT", "OUT" };

        // ---- persistent drag / animation state: members, so every plugin window has its own ----
        static_assert(kRow == kFlowRow, "flow state size");
        int& dragFx = flowDrag;
        int* preview = flowPreview;
        float* fxX = flowX;
        ImVec2& pressPos = flowPressPos;
        bool& movedSincePress = flowMoved;
        const ImGuiIO& io = ImGui::GetIO();

        auto posOf = [&](const int* arr, int id) {
            for (int i = 0; i < kRow; ++i) if (arr[i] == id) return i;
            return 0;
        };

        // start at the committed slots so the blocks never fly in from nowhere
        if (! flowReady)
        {
            for (int e = 0; e < kRow; ++e) fxX[e] = slotX(posOf(order, e) + 1);
            flowReady = true;
        }

        // ---- hit-test the displayed (animated) blocks: every block of the row can be dragged ----
        int hoverFx = -1;
        for (int e = 0; e < kRow; ++e)
            if (ImRect(fxX[e] - bw * 0.5f, y0, fxX[e] + bw * 0.5f, y1).Contains(io.MousePos))
                hoverFx = e;
        const bool hoverSlice = dragFx < 0 && hoverFx == kSlice;

        if (dragFx < 0 && hoverFx >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            dragFx = hoverFx;
            std::memcpy(preview, order, sizeof(flowPreview));
            pressPos = io.MousePos;
            movedSincePress = false;
        }

        const int* shown = order;
        if (dragFx >= 0)
        {
            shown = preview;
            if (std::fabs(io.MousePos.x - pressPos.x) > 4.f * s || std::fabs(io.MousePos.y - pressPos.y) > 4.f * s)
                movedSincePress = true;
            // move the held block to the slot under the cursor; the others slide to make room.
            // Only Chroma may go in front of the slicer.
            int hoverPos = -1;
            for (int p = 0; p < kRow; ++p)
            {
                const float cx = slotX(p + 1);
                if (io.MousePos.x >= cx - bw * 0.5f && io.MousePos.x <= cx + bw * 0.5f)
                    hoverPos = p;
            }
            if (hoverPos >= 0 && movedSincePress)
            {
                const int from = posOf(preview, dragFx);
                if (from != hoverPos)
                {
                    const int held = preview[from];
                    if (from < hoverPos)
                        for (int j = from; j < hoverPos; ++j) preview[j] = preview[j + 1];
                    else
                        for (int j = from; j > hoverPos; --j) preview[j] = preview[j - 1];
                    preview[hoverPos] = held;
                }
            }
            // a release the host never delivered still ends the drag
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ! ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                if (movedSincePress)
                {
                    // the shaper's position is its own parameter; the other six form the route
                    int six[kRouteItems], j = 0, tp = 0;
                    for (int k = 0; k < kRow; ++k)
                    {
                        if (preview[k] == kFxTransient) tp = k;
                        else if (j < kRouteItems) six[j++] = preview[k];
                    }
                    setParamGesture(LVC_ROUTE, static_cast<float>(encodePerm(six, kRouteItems)));
                    setParamGesture(LVC_TRPOS, static_cast<float>(tp));
                }
                else if (dragFx == kSlice)
                    setParamGesture(LVC_SLICER, pv[LVC_SLICER] > 0.5f ? 0.f : 1.f); // a click on SLICE
                dragFx = -1;
            }
        }

        // ---- animate each block toward its target centre ----
        const float t = 1.f - std::exp(-io.DeltaTime * 14.f);
        for (int e = 0; e < kRow; ++e)
        {
            const bool held = e == dragFx && movedSincePress;
            const float target = held
                ? std::max(slotX(1), std::min(slotX(kRow), io.MousePos.x)) // held block follows the cursor
                : slotX(posOf(shown, e) + 1);
            fxX[e] = held ? target : fxX[e] + (target - fxX[e]) * t;
        }

        // ---- connectors, drawn between the displayed centres of consecutive stages ----
        const float my = (y0 + y1) * 0.5f;
        auto centerX = [&](int i) {
            return (i >= 1 && i <= kRow) ? fxX[shown[i - 1]] : slotX(i);
        };
        for (int i = 0; i + 1 < n; ++i)
        {
            const float x0 = centerX(i) + bw * 0.5f + 2.f * s;
            const float x1 = centerX(i + 1) - bw * 0.5f - 2.f * s;
            if (x1 <= x0) continue;
            dl->AddLine(ImVec2(x0, my), ImVec2(x1, my), U32(P.textFaint, 0.7f), 1.5f * s);
            dl->AddTriangleFilled(ImVec2(x1, my), ImVec2(x1 - 5.f * s, my - 3.5f * s),
                                  ImVec2(x1 - 5.f * s, my + 3.5f * s), U32(P.textFaint, 0.7f));
        }

        // ---- blocks ----
        for (int i = 0; i < n; ++i)
        {
            const bool inRow = i >= 1 && i <= kRow;
            const int id = inRow ? shown[i - 1] : -1;
            const bool isFx = inRow && id != kSlice;
            const bool enabled = isFx ? on[id] : (id == kSlice ? pv[LVC_SLICER] > 0.5f : true);
            const float alpha = enabled ? 1.f : 0.42f;
            const float cx = centerX(i);
            const ImRect r(cx - bw * 0.5f, y0, cx + bw * 0.5f, y1);
            const bool isHover = isFx && hoverFx == id && dragFx < 0;
            const bool isHeld = isFx && dragFx == id;

            ImU32 fill = U32(P.track, 0.75f);
            if (isFx) fill = U32(mix(P.a, P.b, 0.5f), 0.12f * alpha);
            dl->AddRectFilled(r.Min, r.Max, fill, 6.f * s);

            // only active (enabled) effects get an outline; fixed and disabled stages do not
            if (isFx && enabled)
            {
                const ImVec4 edge = isHeld ? P.c : (isHover ? P.b : mix(P.a, P.b, 0.5f));
                dl->AddRect(r.Min, r.Max, U32(edge, (isHeld || isHover) ? 1.f : 0.75f),
                            6.f * s, 0, (isHeld || isHover) ? 2.f * s : 1.f);
            }

            const char* label = inRow ? names[id] : (i == 0 ? "IN" : tailNames[i - kRow - 1]);
            textCentered(dl, fBold, r.GetCenter(), U32(enabled ? P.text : P.textDim, alpha), label);

            if (id == kSlice && ! enabled)
                textCentered(dl, fSmall, ImVec2(cx, r.Max.y - 7.f * s), U32(P.textDim, 0.85f), "OFF");
            if (id == kSlice && (hoverSlice || dragFx == kSlice))
                dl->AddRect(r.Min, r.Max, U32(dragFx == kSlice ? P.c : P.b), 6.f * s, 0, 2.f * s);
        }

        if (hoverSlice)
            tooltip(pv[LVC_SLICER] > 0.5f ? "Drag to move the slicer in the chain, click to switch it off"
                                                    : "Drag to move the slicer in the chain, click to switch it on");
        else if (hoverFx >= 0 && dragFx < 0)
            tooltip("Drag to move it anywhere in the chain (before SLICE it processes what the slicer records)");
        if (dragFx >= 0 || hoverFx >= 0)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    void drawResizeGrip(ImDrawList* dl, float W, float H)
    {
        const float sz = 22.f * s;
        const ImVec2 a(W - sz, H - sz);
        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton("##resize", ImVec2(sz, sz));
        const bool hov = ImGui::IsItemHovered();
        const bool act = ImGui::IsItemActive();
        const ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsItemActivated())
        {
            resizing = true;
            resizeStartMouse = io.MousePos;
            resizeStartSize = ImVec2(W, H);
        }
        if (act)
        {
            const float minW = kMinW * static_cast<float>(hostScale), minH = kMinH * static_cast<float>(hostScale);
            const uint nw = static_cast<uint>(std::max(minW, resizeStartSize.x + io.MousePos.x - resizeStartMouse.x));
            const uint nh = static_cast<uint>(std::max(minH, resizeStartSize.y + io.MousePos.y - resizeStartMouse.y));
            if (nw != getWidth() || nh != getHeight())
                setSize(nw, nh);
        }
        if (ImGui::IsItemDeactivated())
        {
            resizing = false;
            saveSize();
        }

        const bool wantCursor = hov || act;
        if (wantCursor != cursorIsResize)
        {
            cursorIsResize = wantCursor;
            setCursor(wantCursor ? kMouseCursorUpLeftDownRight : kMouseCursorArrow);
        }

        const ImU32 col = (hov || act) ? U32(P.b) : U32(P.textFaint);
        for (int i = 1; i <= 3; ++i)
        {
            const float o = i * 5.f * s;
            dl->AddLine(ImVec2(W - 4.f * s - o, H - 4.f * s), ImVec2(W - 4.f * s, H - 4.f * s - o), col, 1.5f * s);
        }
    }

    void saveSize()
    {
        const uint w = widthWithoutSidebar(), h = getHeight();
        if (w == lastSavedW && h == lastSavedH)
            return;
        lastSavedW = w;
        lastSavedH = h;
        persistPrefs();
    }

    // ----------------------------------------------------------------------------------------------------------------
    // CRT look for the visualisers

    void crtAdd(ImVec2 a, ImVec2 b, float rounding)
    {
        if (! crtEnabled || crtCount >= kCrtMax || b.x - a.x < 8.f || b.y - a.y < 8.f)
            return;
        crtRects[crtCount] = ImRect(a, b);
        crtRound[crtCount] = rounding;
        ++crtCount;
    }

    static void crtCallback(const ImDrawList*, const ImDrawCmd* cmd)
    {
        static_cast<HyperCutUI*>(cmd->UserCallbackData)->crtApply();
    }

    float crtRandom()
    {
        crtSeed ^= crtSeed << 13; crtSeed ^= crtSeed >> 17; crtSeed ^= crtSeed << 5;
        return static_cast<float>(crtSeed & 0xFFFFFF) / 16777216.f;
    }

    // the screen as a curved mesh: edges bow outwards, corners pulled in; vertex colour carries the vignette
    void crtMesh(const ImRect& r, float ox, float oy, float alpha, float bowX, float bowY)
    {
        const int nx = 28, ny = 16;
        const float cx = (r.Min.x + r.Max.x) * 0.5f + ox, cy = (r.Min.y + r.Max.y) * 0.5f + oy;
        const float hx = r.GetWidth() * 0.5f, hy = r.GetHeight() * 0.5f;
        const float ax = bowX / hx, ay = bowY / hy;
        auto vert = [&](int i, int j) {
            const float u = -1.f + 2.f * i / nx, v = -1.f + 2.f * j / ny;
            const float r2 = 0.5f * (u * u + v * v);
            const float vig = 1.f - 0.32f * r2 * r2;
            glColor4f(vig, vig, vig, alpha);
            glTexCoord2f((u + 1.f) * 0.5f, 1.f - (v + 1.f) * 0.5f);
            glVertex2f(cx + hx * u * (1.f - ax * v * v), cy + hy * v * (1.f - ay * u * u));
        };
        glBegin(GL_QUADS);
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i)
            {
                vert(i, j); vert(i + 1, j); vert(i + 1, j + 1); vert(i, j + 1);
            }
        glEnd();
    }

    void crtRoundedRect(const ImRect& r, float rad)
    {
        rad = std::min(rad, std::min(r.GetWidth(), r.GetHeight()) * 0.5f);
        const ImVec2 c[4] = { ImVec2(r.Max.x - rad, r.Max.y - rad), ImVec2(r.Min.x + rad, r.Max.y - rad),
                              ImVec2(r.Min.x + rad, r.Min.y + rad), ImVec2(r.Max.x - rad, r.Min.y + rad) };
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f((r.Min.x + r.Max.x) * 0.5f, (r.Min.y + r.Max.y) * 0.5f);
        for (int k = 0; k <= 4; ++k)
            for (int j = 0; j <= 6; ++j)
            {
                const int q = k % 4;
                const float t = (q * 0.5f + j / 12.f) * IM_PI;
                glVertex2f(c[q].x + std::cos(t) * rad, c[q].y + std::sin(t) * rad);
                if (k == 4) break;
            }
        glEnd();
    }

    // runs inside ImGui's OpenGL 2 renderer (projection = ImGui coordinates), right after the visualisers
    void crtApply()
    {
       #if defined(DGL_USE_GLES2) || defined(DGL_USE_GLES3) || defined(DGL_USE_OPENGL3)
        crtCount = 0; // no fixed-function pipeline there: keep the plain look
       #else
        const ImGuiIO& io = ImGui::GetIO();
        const float fbx = io.DisplayFramebufferScale.x, fby = io.DisplayFramebufferScale.y;
        const int fbH = static_cast<int>(io.DisplaySize.y * fby + 0.5f);

        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        if (crtNoise == 0)
        {
            // sparse bright specks: grain that reads as noise on a dark screen
            static unsigned char px[128 * 128];
            for (int i = 0; i < 128 * 128; ++i)
            {
                const float v = crtRandom();
                px[i] = static_cast<unsigned char>(255.f * v * v * v * v * v * v);
            }
            glGenTextures(1, &crtNoise);
            glBindTexture(GL_TEXTURE_2D, crtNoise);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 128, 128, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, px);
        }

        const ImVec4 bezel = P.dark ? hex(0x030407) : hex(0xC9CED9);
        const float ghost = 0.42f;                 // share of the previous frame left on screen (phosphor decay)
        const float bowX = 5.f * s, bowY = 6.f * s; // curvature: how far the edge centres bulge past the corners

        for (int i = 0; i < crtCount; ++i)
        {
            const ImRect r = crtRects[i];
            const int x0 = static_cast<int>(std::floor(r.Min.x * fbx));
            const int yTop = static_cast<int>(std::floor(r.Min.y * fby));
            const int w = static_cast<int>(std::ceil(r.Max.x * fbx)) - x0;
            const int h = static_cast<int>(std::ceil(r.Max.y * fby)) - yTop;
            if (w <= 4 || h <= 4)
                continue;
            const int glY = fbH - (yTop + h);
            const ImRect rp(x0 / fbx, yTop / fby, (x0 + w) / fbx, (yTop + h) / fby); // the pixel-aligned rect
            CrtScreen& sc = crtScreens[i];
            if (sc.tex == 0)
                glGenTextures(1, &sc.tex);
            glBindTexture(GL_TEXTURE_2D, sc.tex);
            if (sc.w != w || sc.h != h)
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
                sc.w = w;
                sc.h = h;
                sc.history = false;
            }

            // 1. ghosting: what was on screen last frame fades out over what is drawn now
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            if (sc.history)
            {
                glColor4f(1.f, 1.f, 1.f, ghost);
                glBegin(GL_QUADS);
                glTexCoord2f(0.f, 1.f); glVertex2f(rp.Min.x, rp.Min.y);
                glTexCoord2f(1.f, 1.f); glVertex2f(rp.Max.x, rp.Min.y);
                glTexCoord2f(1.f, 0.f); glVertex2f(rp.Max.x, rp.Max.y);
                glTexCoord2f(0.f, 0.f); glVertex2f(rp.Min.x, rp.Max.y);
                glEnd();
            }

            // 2. the frame (with its ghost) becomes the picture on the tube, and next frame's ghost
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x0, glY, w, h);
            sc.history = true;

            // 3. tube: dark bezel, then the picture on a curved surface with a vignette
            glDisable(GL_TEXTURE_2D);
            glColor4f(bezel.x, bezel.y, bezel.z, 1.f);
            crtRoundedRect(rp, crtRound[i]);
            glEnable(GL_TEXTURE_2D);
            crtMesh(rp, 0.f, 0.f, 1.f, bowX, bowY);

            // 4. bloom: soft additive copies around the picture
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            static const float off[6][2] = { { 1.2f, 0.f }, { -1.2f, 0.f }, { 0.f, 1.2f }, { 0.f, -1.2f }, { 2.4f, 0.f }, { -2.4f, 0.f } };
            for (int k = 0; k < 6; ++k)
                crtMesh(rp, off[k][0] * s, off[k][1] * s, k < 4 ? 0.06f : 0.035f, bowX, bowY);

            glEnable(GL_SCISSOR_TEST);
            glScissor(x0, glY, w, h);

            // 5. scanlines
            glDisable(GL_TEXTURE_2D);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(0.f, 0.f, 0.f, P.dark ? 0.24f : 0.10f);
            const float step = std::max(2.f, 3.f * s) / fby, lh = std::max(1.f, 1.f * s) / fby;
            glBegin(GL_QUADS);
            for (float y = rp.Min.y; y < rp.Max.y; y += step)
            {
                glVertex2f(rp.Min.x, y); glVertex2f(rp.Max.x, y);
                glVertex2f(rp.Max.x, y + lh); glVertex2f(rp.Min.x, y + lh);
            }
            glEnd();

            // 6. grain, a new pattern every frame
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, crtNoise);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glColor4f(1.f, 1.f, 1.f, P.dark ? 0.06f : 0.035f);
            const float u0 = crtRandom(), v0 = crtRandom();
            const float us = rp.GetWidth() * fbx / 128.f, vs = rp.GetHeight() * fby / 128.f; // one texel per pixel
            glBegin(GL_QUADS);
            glTexCoord2f(u0, v0);           glVertex2f(rp.Min.x, rp.Min.y);
            glTexCoord2f(u0 + us, v0);      glVertex2f(rp.Max.x, rp.Min.y);
            glTexCoord2f(u0 + us, v0 + vs); glVertex2f(rp.Max.x, rp.Max.y);
            glTexCoord2f(u0, v0 + vs);      glVertex2f(rp.Min.x, rp.Max.y);
            glEnd();

            // 7. glass: a faint sheen across the top of the tube
            glDisable(GL_TEXTURE_2D);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glBegin(GL_QUADS);
            glColor4f(1.f, 1.f, 1.f, P.dark ? 0.045f : 0.08f); glVertex2f(rp.Min.x, rp.Min.y); glVertex2f(rp.Max.x, rp.Min.y);
            glColor4f(1.f, 1.f, 1.f, 0.f); glVertex2f(rp.Max.x, rp.Min.y + rp.GetHeight() * 0.35f); glVertex2f(rp.Min.x, rp.Min.y + rp.GetHeight() * 0.35f);
            glEnd();
            glEnable(GL_TEXTURE_2D);
            glDisable(GL_SCISSOR_TEST);
        }
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        crtCount = 0;
       #endif
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Main frame

    void onImGuiDisplay() override
    {
        const float WFull = static_cast<float>(getWidth());
        const float H = static_cast<float>(getHeight());
        crtCount = 0;

        // preset browser is a dropdown popup now (no window-resize/sidebar layout)
        (void)updateSidebar();
        const float W = WFull;

        // scale handling: rebuild fonts when the size settles
        s = computeScale();
        fontAdj = s / fontBuiltScale;
        if (std::fabs(fontAdj - 1.f) > 0.02f && ! resizing && ! ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            fontWantedScale = s;
            fontRebuildPending = true;
            repaint();
        }
        ImGui::GetStyle().PopupRounding = 12.f * s;

        // persist size changes made by the host once interaction ends (grip saves on release)
        ++framesSinceOpen;
        if (sizeDirty && ! resizing && ! ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            sizeDirty = false;
            saveSize();
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(WFull, H));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                                     | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings
                                     | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;

        if (ImGui::Begin("##hypercut", nullptr, flags))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            drawBackground(dl, WFull, H);

            const float pad = 16.f * s;
            const float gap = 12.f * s;

            // header
            drawHeader(dl, W, pad);
            const float headerBottom = pad + 52.f * s;

            // control row has a fixed design height; the signal-flow strip sits below the visualisers
            const float controlsH = 372.f * s;
            const float flowH = 56.f * s;
            const float vizTop = headerBottom + gap;
            const float vizBottom = std::max(vizTop + 120.f * s, H - pad - controlsH - flowH - gap * 2.f);
            const float innerW = W - pad * 2.f;

            // capture buffer | live cuts | pitchmap spectrum
            const float vizW = innerW - gap * 2.f;
            const float leftW = vizW * 0.31f, midW = vizW * 0.43f;
            drawBufferView(dl, ImVec2(pad, vizTop), ImVec2(pad + leftW, vizBottom));
            drawCutsView(dl, ImVec2(pad + leftW + gap, vizTop), ImVec2(pad + leftW + gap + midW, vizBottom));
            drawPitchmapView(dl, ImVec2(pad + leftW + midW + gap * 2.f, vizTop), ImVec2(W - pad, vizBottom));

            // CRT pass over the screens drawn so far (tooltips and popups come later, so they stay crisp)
            if (crtCount > 0)
            {
                dl->AddCallback(&HyperCutUI::crtCallback, this);
                dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
            }

            const float flowTop = vizBottom + gap;
            const float flowBot = flowTop + flowH;
            drawSignalFlow(dl, ImVec2(pad, flowTop), ImVec2(W - pad, flowBot));

            const float cTop = flowBot + gap;
            const float cBot = H - pad;
            // panels follow the default signal flow: shaper, pitchmap, filter, crush | slicer | delay, reverb, master
            const float avail = innerW - gap * 9.f;
            // design widths 124 / 220 / 140 / 132 / 176 / 184 / 182 / 148 / 158 / 136 on the 1740 canvas
            static const float dw[10] = { 124.f, 220.f, 140.f, 132.f, 176.f, 184.f, 182.f, 148.f, 158.f, 136.f };
            float wp[9];
            for (int k = 0; k < 9; ++k) wp[k] = avail * dw[k] / 1600.f;
            float x = pad;
            drawTransientPanel(dl, ImVec2(x, cTop), ImVec2(x + wp[0], cBot));  x += wp[0] + gap;
            drawPitchmapPanel(dl, ImVec2(x, cTop), ImVec2(x + wp[1], cBot));   x += wp[1] + gap;
            drawFilterPanel(dl, ImVec2(x, cTop), ImVec2(x + wp[2], cBot));     x += wp[2] + gap;
            drawCrusherPanel(dl, ImVec2(x, cTop), ImVec2(x + wp[3], cBot));    x += wp[3] + gap;
            drawSequencePanel(dl, ImVec2(x, cTop), ImVec2(x + wp[4], cBot));   x += wp[4] + gap;
            drawAlgorithmPanel(dl, ImVec2(x, cTop), ImVec2(x + wp[5], cBot));  x += wp[5] + gap;
            drawShapePanel(dl, ImVec2(x, cTop), ImVec2(x + wp[6], cBot));      x += wp[6] + gap;
            drawDelayPanel(dl, ImVec2(x, cTop), ImVec2(x + wp[7], cBot));      x += wp[7] + gap;
            drawReverbPanel(dl, ImVec2(x, cTop), ImVec2(x + wp[8], cBot));     x += wp[8] + gap;
            drawMasterPanel(dl, ImVec2(x, cTop), ImVec2(W - pad, cBot));

            drawResizeGrip(dl, WFull, H);

            // keyboard shortcuts (when the host gives the plugin window keyboard focus)
            const ImGuiIO& kio = ImGui::GetIO();
            if (! kio.WantTextInput && (kio.KeyCtrl || kio.KeySuper))
            {
                if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
                    kio.KeyShift ? redo() : undo();
                else if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
                    redo();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);

        // everything changed from the UI this frame becomes an undo step once the mouse is released
        commitUndo();
    }

    void onResize(const ResizeEvent& ev) override
    {
        UI::onResize(ev);
        if (framesSinceOpen > 30)
            sizeDirty = true;
        repaint();
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HyperCutUI)
};

// --------------------------------------------------------------------------------------------------------------------

UI* createUI()
{
    return new HyperCutUI();
}

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
