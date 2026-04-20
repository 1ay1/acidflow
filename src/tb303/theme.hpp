#pragma once
// Shared palette + small text-measurement helper for the tb303 widgets.
// Every widget header pulls in this file, so anything broadly reused by more
// than one widget (like display_cols) belongs here.
//
// Themes are runtime-selectable — the UI thread flips the current index and
// every subsequent render reads the new palette. All widgets access colours
// via `clr_*()` functions (NOT raw constants) so a theme switch is one
// pointer-bump away. Adding a theme = append to kThemes below; everything
// else follows from the function calls that already exist throughout the UI.

#include <cstddef>
#include <string_view>

#include <maya/maya.hpp>

namespace tb303 {

using namespace maya;

struct Palette {
    const char* name;
    Color accent;
    Color accent_d;
    Color hot;
    Color slide;
    Color text;
    Color muted;
    Color dim;
    Color panel;
    Color panel_hi;
    Color grid;
    Color ok;
};

// Theme library. Each entry is a self-contained palette; add new ones here.
// Ordering matters for the cycle key — index 0 is the default, then rotate.
inline constexpr Palette kThemes[] = {
    { "classic",                                                     // 303 silver + orange
      Color::rgb(255, 138,  40),  // accent   (Roland orange)
      Color::rgb(160,  80,  20),  // accent_d
      Color::rgb(255,  70,  70),  // hot      (red, for accents/playing)
      Color::rgb( 90, 200, 255),  // slide    (cyan)
      Color::rgb(225, 225, 230),  // text
      Color::rgb(120, 120, 130),  // muted
      Color::rgb( 70,  70,  80),  // dim
      Color::rgb( 55,  58,  70),  // panel
      Color::rgb(110, 115, 135),  // panel_hi
      Color::rgb( 40,  45,  55),  // grid
      Color::rgb( 70, 220, 130),  // ok
    },
    { "cyber",                                                       // magenta / cyan / ink
      Color::rgb(255,  60, 200),  // hot pink
      Color::rgb(140,  20, 110),
      Color::rgb(255,  90, 255),  // fuchsia
      Color::rgb( 80, 255, 220),  // mint
      Color::rgb(235, 235, 255),
      Color::rgb(130, 120, 180),
      Color::rgb( 70,  60, 100),
      Color::rgb( 35,  25,  60),
      Color::rgb(130, 100, 200),
      Color::rgb( 25,  20,  40),
      Color::rgb(120, 255, 200),
    },
    { "moss",                                                        // earth / forest
      Color::rgb(190, 210,  90),  // chartreuse
      Color::rgb(110, 130,  40),
      Color::rgb(255, 200, 120),  // amber
      Color::rgb(200, 160, 100),  // tan
      Color::rgb(230, 225, 210),
      Color::rgb(140, 150, 120),
      Color::rgb( 75,  85,  65),
      Color::rgb( 45,  55,  45),
      Color::rgb(110, 130, 100),
      Color::rgb( 30,  40,  30),
      Color::rgb(150, 220, 140),
    },
    { "ice",                                                         // steel / blue
      Color::rgb(120, 200, 255),  // ice blue
      Color::rgb( 40, 100, 160),
      Color::rgb(255, 120, 180),  // coral
      Color::rgb(180, 255, 240),  // frost
      Color::rgb(235, 240, 250),
      Color::rgb(130, 145, 170),
      Color::rgb( 60,  70,  85),
      Color::rgb( 30,  38,  52),
      Color::rgb(100, 125, 160),
      Color::rgb( 22,  28,  40),
      Color::rgb(120, 230, 200),
    },
    { "mono",                                                        // monochrome
      Color::rgb(230, 230, 230),
      Color::rgb(140, 140, 140),
      Color::rgb(255, 255, 255),
      Color::rgb(200, 200, 200),
      Color::rgb(235, 235, 235),
      Color::rgb(140, 140, 140),
      Color::rgb( 70,  70,  70),
      Color::rgb( 45,  45,  45),
      Color::rgb(120, 120, 120),
      Color::rgb( 30,  30,  30),
      Color::rgb(200, 200, 200),
    },
};
inline constexpr int kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);

// Currently-selected theme. Single int, no atomics needed — UI thread is the
// only writer and only reader. Stored in a header-inline variable so there's
// no standalone .cpp file to add to the build.
inline int g_theme_idx = 0;

inline const Palette& active_theme() {
    int i = g_theme_idx;
    if (i < 0 || i >= kThemeCount) i = 0;
    return kThemes[i];
}
inline void set_theme(int i) {
    if (i < 0) i = 0;
    if (i >= kThemeCount) i = kThemeCount - 1;
    g_theme_idx = i;
}
inline void cycle_theme() {
    g_theme_idx = (g_theme_idx + 1) % kThemeCount;
}

inline Color clr_accent()   { return active_theme().accent;   }
inline Color clr_accent_d() { return active_theme().accent_d; }
inline Color clr_hot()      { return active_theme().hot;      }
inline Color clr_slide()    { return active_theme().slide;    }
inline Color clr_text()     { return active_theme().text;     }
inline Color clr_muted()    { return active_theme().muted;    }
inline Color clr_dim()      { return active_theme().dim;      }
inline Color clr_panel()    { return active_theme().panel;    }
inline Color clr_panel_hi() { return active_theme().panel_hi; }
inline Color clr_grid()     { return active_theme().grid;     }
inline Color clr_ok()       { return active_theme().ok;       }

// Rough display-width counter that treats every UTF-8 multi-byte sequence as
// a single East-Asian Narrow glyph. Not perfect in general, but for our
// restricted glyph set (ASCII + box-drawing + block elements, all 1-wide) it
// matches the real column count exactly. Shared by Knob + WaveformToggle for
// rendering the tech-labeled rule row.
inline int display_cols(std::string_view s) {
    int cols = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if ((c & 0x80) == 0)         { ++cols; i += 1; }
        else if ((c & 0xE0) == 0xC0) { ++cols; i += 2; }
        else if ((c & 0xF0) == 0xE0) { ++cols; i += 3; }
        else if ((c & 0xF8) == 0xF0) { ++cols; i += 4; }
        else                         { ++cols; i += 1; }
    }
    return cols;
}

} // namespace tb303
