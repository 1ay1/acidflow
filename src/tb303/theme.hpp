#pragma once
// acidflow's colours, on maya's theme system.
//
// Every widget colour is a maya theme SLOT (Color::slot), so it follows
// whatever theme is live: acidflow's own "303" look, the terminal's palette
// (theme::native), or any of maya's built-in schemes. `T` cycles them and
// the choice is saved by name.
//
// The roles acidflow needs map onto maya's slots:
//   accent    → primary      the orange of the 303 (focus, the brand)
//   accent_d  → warning      dimmer warm tone (some drum rows, env locks)
//   hot       → error        accents, the playing step, REC
//   slide     → info         slides, the filter curve
//   ok        → success      playing, confirmations
//   text/muted/dim           text / muted / placeholder
//   panel_hi  → border       panel borders
//   grid      → surface      the faint grid and the scope bed
//
// Pixel art (the scope, the filter chart) needs real RGB, so it resolves a
// slot against the live theme with rgb_of(); on a theme without channels
// (native) it falls back to a sane literal.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <maya/maya.hpp>
#include <maya/style/schemes.hpp>
#include <maya/element/pixels.hpp>

namespace tb303 {

using namespace maya;

// acidflow's own look: the Roland orange on a dark silver-grey, the palette
// it always had, stated as a full maya Theme so maya widgets agree with it.
inline constexpr Theme kAcidTheme = Theme::derive(theme::native, [](Theme& t) {
    t.primary      = Color::rgb(255, 138,  40);   // Roland orange
    t.secondary    = Color::rgb(160,  80,  20);
    t.accent       = Color::rgb(255, 138,  40);
    t.success      = Color::rgb( 70, 220, 130);
    t.error        = Color::rgb(255,  70,  70);
    t.warning      = Color::rgb(160,  80,  20);
    t.info         = Color::rgb( 90, 200, 255);
    t.text         = Color::rgb(225, 225, 230);
    t.inverse_text = Color::rgb( 20,  20,  24);
    t.muted        = Color::rgb(120, 120, 130);
    t.surface      = Color::rgb( 40,  45,  55);
    t.background   = Color::default_color();      // keep the terminal's own bg
    t.border       = Color::rgb(110, 115, 135);
    t.highlight    = Color::rgb( 55,  58,  70);
    t.selection    = Color::rgb( 55,  58,  70);
    t.link         = Color::rgb( 90, 200, 255);
    t.placeholder  = Color::rgb( 70,  70,  80);
    t.overlay      = Color::rgb( 20,  22,  28);
});

// What `T` cycles through. "acid" first; then the terminal's own colours;
// then a handful of maya's schemes, light and dark.
struct ThemeChoice { const char* name; const Theme* theme; };
inline constexpr ThemeChoice kThemeChoices[] = {
    {"acid",             &kAcidTheme},
    {"terminal",         &theme::native},
    {"gruvbox dark",     &theme::gruvbox_dark},
    {"tokyonight",       &theme::tokyonight},
    {"dracula",          &theme::dracula},
    {"nord",             &theme::nord},
    {"catppuccin mocha", &theme::catppuccin_mocha},
    {"rose pine",        &theme::rose_pine},
    {"catppuccin latte", &theme::catppuccin_latte},
    {"github dark",      &theme::github_dark},
};
inline constexpr int kThemeCount = static_cast<int>(sizeof(kThemeChoices) / sizeof(kThemeChoices[0]));

// Everything the picker offers: the favourites above, then every one of
// maya's schemes. Index into this with all_themes()[i].
inline const std::vector<ThemeChoice>& all_themes() {
    static const std::vector<ThemeChoice> v = [] {
        std::vector<ThemeChoice> out(std::begin(kThemeChoices), std::end(kThemeChoices));
        for (const auto& s : theme::schemes) {
            bool dup = false;   // a favourite that is also a maya scheme
            for (const auto& f : kThemeChoices) dup = dup || f.theme == s.theme;
            if (!dup) out.push_back({s.name, s.theme});
        }
        return out;
    }();
    return v;
}

// Does this theme draw its own background? (native and acid don't.)
inline bool theme_is_light(const Theme& t) {
    const LitColor bg = t.resolve(t.background);
    if (!bg.has_channels()) return false;
    return bg.r() * 299 + bg.g() * 587 + bg.b() * 114 > 128 * 1000;
}

inline int g_theme_idx = 0;                 // into all_themes()
inline std::string g_theme_name = "acid";   // what's live, for prefs and toasts

inline const ThemeChoice& active_theme() {
    const auto& all = all_themes();
    const int i = (g_theme_idx >= 0 && g_theme_idx < static_cast<int>(all.size())) ? g_theme_idx : 0;
    return all[static_cast<size_t>(i)];
}
// Install the theme in maya; every slot colour repaints from it.
inline void apply_theme() { app_set_theme(*active_theme().theme); }
inline void set_theme(int i) {
    const int n = static_cast<int>(all_themes().size());
    g_theme_idx = (i >= 0 && i < n) ? i : 0;
    g_theme_name = active_theme().name;
    apply_theme();
}
// T without the picker (and the palette action): the next favourite.
inline void cycle_theme() {
    const int next = g_theme_idx >= 0 && g_theme_idx < kThemeCount - 1 ? g_theme_idx + 1 : 0;
    set_theme(next);
}
// By name, for the saved prefs: a favourite or any maya scheme.
inline bool set_theme_by_name(std::string_view name) {
    const auto& all = all_themes();
    for (size_t i = 0; i < all.size(); ++i)
        if (name == all[i].name) { set_theme(static_cast<int>(i)); return true; }
    return false;
}

// Role colours, as slots.
inline Color clr_accent()   { return Color::slot(ThemeSlot::Primary);     }
inline Color clr_accent_d() { return Color::slot(ThemeSlot::Warning);     }
inline Color clr_hot()      { return Color::slot(ThemeSlot::Error);       }
inline Color clr_slide()    { return Color::slot(ThemeSlot::Info);        }
inline Color clr_ok()       { return Color::slot(ThemeSlot::Success);     }
inline Color clr_text()     { return Color::slot(ThemeSlot::Text);        }
inline Color clr_muted()    { return Color::slot(ThemeSlot::Muted);       }
inline Color clr_dim()      { return Color::slot(ThemeSlot::Placeholder); }
inline Color clr_panel()    { return Color::slot(ThemeSlot::Highlight);   }
inline Color clr_panel_hi() { return Color::slot(ThemeSlot::Border);      }
inline Color clr_grid()     { return Color::slot(ThemeSlot::Surface);     }
// Knob/FX group colours (OSC, VCF, EG, VCA / OD, DELAY, REVERB), from the
// theme's own series ramp so they stay distinct on any scheme.
inline Color clr_group(int i) { return theme::series(theme::live(), static_cast<std::size_t>(i)); }

// The live theme's RGB for a slot, for pixel art. Named/default colours
// (theme::native) have no channels, so they get the given fallback.
inline Rgb rgb_of(Color c, Rgb fallback) {
    const LitColor l = theme::live().resolve(c);
    if (!l.has_channels()) return fallback;
    return Rgb{l.r(), l.g(), l.b()};
}

// Blend two theme colours, t = 0 → a, 1 → b. For fades (toasts, the brand
// pulse, gradients). Without channels (native) there's nothing to blend,
// so it snaps to whichever end is nearer.
inline Color mix(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const LitColor la = theme::live().resolve(a), lb = theme::live().resolve(b);
    if (!la.has_channels() || !lb.has_channels()) return t < 0.5f ? a : b;
    auto m = [t](uint8_t x, uint8_t y) {
        return static_cast<uint8_t>(std::lround(x + (static_cast<float>(y) - x) * t));
    };
    return Color::rgb(m(la.r(), lb.r()), m(la.g(), lb.g()), m(la.b(), lb.b()));
}

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
