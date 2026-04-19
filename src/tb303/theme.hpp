#pragma once
// Shared palette + small text-measurement helper for the tb303 widgets.
// 303-inspired colours: Roland silver body + iconic orange/red accents.
// Every widget header pulls in this file, so anything broadly reused by more
// than one widget (like display_cols) belongs here.

#include <cstddef>
#include <string_view>

#include <maya/maya.hpp>

namespace tb303 {

using namespace maya;

inline constexpr Color clr_accent()   { return Color::rgb(255, 138,  40); } // orange
inline constexpr Color clr_accent_d() { return Color::rgb(160,  80,  20); }
inline constexpr Color clr_hot()      { return Color::rgb(255,  70,  70); } // red for accent/playing
inline constexpr Color clr_slide()    { return Color::rgb( 90, 200, 255); } // cyan for slide
inline constexpr Color clr_text()     { return Color::rgb(225, 225, 230); }
inline constexpr Color clr_muted()    { return Color::rgb(120, 120, 130); }
inline constexpr Color clr_dim()      { return Color::rgb( 70,  70,  80); }
inline constexpr Color clr_panel()    { return Color::rgb( 55,  58,  70); }
inline constexpr Color clr_panel_hi() { return Color::rgb(110, 115, 135); }
inline constexpr Color clr_grid()     { return Color::rgb( 40,  45,  55); }
inline constexpr Color clr_ok()       { return Color::rgb( 70, 220, 130); }

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
