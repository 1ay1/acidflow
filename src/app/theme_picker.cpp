// The theme picker: T opens it. A searchable list of every maya theme;
// moving the cursor previews it live, Enter keeps it, Esc puts back what was
// there before.
#include "app.hpp"

#include <algorithm>
#include <cctype>

namespace {
std::string lower(std::string_view s) {
    std::string o(s);
    for (auto& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return o;
}
}  // namespace

// Indices into all_themes() that match the search, favourites first.
std::vector<int> App::theme_matches() const {
    std::vector<int> out;
    const auto& all = tb303::all_themes();
    const std::string q = lower(g_theme_query);
    for (int i = 0; i < static_cast<int>(all.size()); ++i) {
        if (q.empty() || lower(all[static_cast<size_t>(i)].name).find(q) != std::string::npos)
            out.push_back(i);
    }
    return out;
}

void App::open_theme_picker() {
    g_theme_picker   = true;
    g_theme_query.clear();
    g_theme_before   = tb303::g_theme_idx;
    // Start the cursor on what's live.
    const auto m = theme_matches();
    g_theme_sel = 0;
    for (int i = 0; i < static_cast<int>(m.size()); ++i)
        if (m[static_cast<size_t>(i)] == tb303::g_theme_idx) g_theme_sel = i;
    g_first_run = false;
}

// Preview whatever the cursor is on.
void App::preview_theme_sel() {
    const auto m = theme_matches();
    if (m.empty()) return;
    g_theme_sel = std::clamp(g_theme_sel, 0, static_cast<int>(m.size()) - 1);
    tb303::set_theme(m[static_cast<size_t>(g_theme_sel)]);
}

bool App::handle_theme_picker_key(const Event& ev) {
    if (!g_theme_picker) return false;
    const auto m = theme_matches();
    const int n = static_cast<int>(m.size());
    if (key(ev, SpecialKey::Escape)) {
        if (!g_theme_query.empty()) {   // first Esc clears the search
            g_theme_query.clear();
            const auto all = theme_matches();
            for (int i = 0; i < static_cast<int>(all.size()); ++i)
                if (all[static_cast<size_t>(i)] == tb303::g_theme_idx) g_theme_sel = i;
            return true;
        }
        tb303::set_theme(g_theme_before);
        g_theme_picker = false;
        set_toast("theme unchanged: " + tb303::g_theme_name);
        return true;
    }
    if (key(ev, SpecialKey::Enter)) {
        if (n) preview_theme_sel();
        g_theme_picker = false;
        g_theme_chosen = true;
        set_toast("\xe2\x97\x86 theme: " + tb303::g_theme_name);
        return true;
    }
    auto move = [&](int d) {
        if (!n) return;
        g_theme_sel = std::clamp(g_theme_sel + d, 0, n - 1);
        preview_theme_sel();
    };
    if (key(ev, SpecialKey::Up) || ctrl(ev, 'p'))       { move(-1);  return true; }
    if (key(ev, SpecialKey::Down) || ctrl(ev, 'n'))     { move(+1);  return true; }
    if (key(ev, SpecialKey::PageUp))                    { move(-10); return true; }
    if (key(ev, SpecialKey::PageDown))                  { move(+10); return true; }
    if (key(ev, SpecialKey::Home))                      { move(-n);  return true; }
    if (key(ev, SpecialKey::End))                       { move(+n);  return true; }
    // T again steps forward too, so the old "press T to cycle" habit works.
    if (key(ev, 'T') && g_theme_query.empty())          { move(+1);  return true; }
    if (key(ev, SpecialKey::Backspace)) {
        if (!g_theme_query.empty()) g_theme_query.pop_back();
        g_theme_sel = 0;
        preview_theme_sel();
        return true;
    }
    if (const KeyEvent* k = as_key(ev)) {
        if (const auto* ck = std::get_if<CharKey>(&k->key);
            ck && !k->mods.ctrl && !k->mods.alt && ck->codepoint >= 0x20 && ck->codepoint < 0x7F) {
            g_theme_query.push_back(static_cast<char>(ck->codepoint));
            g_theme_sel = 0;
            preview_theme_sel();
            return true;
        }
    }
    return true;   // modal
}

// Mouse: hover previews, click keeps, wheel scrolls, click outside cancels.
bool App::handle_theme_picker_mouse(const MouseEvent& m) {
    if (!g_theme_picker) return false;
    const auto id = maya::hit_test(m.x.raw() - 1, m.y.raw() - 1);
    const int row = id && maya::hit_kind(*id) == tb303::kHitThemeRow
                  ? static_cast<int>(maya::hit_index(*id)) : -1;
    if (m.button == MouseButton::ScrollUp && m.kind == MouseEventKind::Press) {
        g_theme_sel = std::max(0, g_theme_sel - 1); preview_theme_sel(); return true;
    }
    if (m.button == MouseButton::ScrollDown && m.kind == MouseEventKind::Press) {
        g_theme_sel = g_theme_sel + 1; preview_theme_sel(); return true;
    }
    if (m.kind == MouseEventKind::Move) {
        if (row >= 0 && row != g_theme_sel) { g_theme_sel = row; preview_theme_sel(); }
        return true;
    }
    if (m.kind == MouseEventKind::Press && m.button == MouseButton::Left) {
        if (row >= 0) {
            g_theme_sel = row;
            preview_theme_sel();
            g_theme_picker = false;
            g_theme_chosen = true;
            set_toast("\xe2\x97\x86 theme: " + tb303::g_theme_name);
        } else if (!(id && maya::hit_kind(*id) == tb303::kHitThemeCard)) {
            tb303::set_theme(g_theme_before);
            g_theme_picker = false;
        }
        return true;
    }
    return true;
}
