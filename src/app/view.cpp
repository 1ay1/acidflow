// Building the frame.
#include "app.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Render
// ─────────────────────────────────────────────────────────────────────────────
Element App::build_knob_row() const {
    // TB-303 signal-flow groups. Each knob gets a group colour so adjacent
    // knobs visually bind into their functional block — OSC/FILTER/ENV/AMP —
    // without needing a horizontal divider between them.
    //   OSC     (pitch + wave)        : violet
    //   FILTER  (cutoff, Q, env mod)  : cyan
    //   ENV     (decay)               : amber
    //   AMP     (accent, volume)      : green
    auto group_color_for = [](int i) -> maya::Color {
        using maya::Color;
        switch (i) {
            case K_TUNE:   return Color::rgb(180, 130, 230); // OSC    violet
            case K_WAVE:   return Color::rgb(180, 130, 230);
            case K_CUTOFF: return Color::rgb( 90, 200, 220); // FILTER cyan
            case K_RES:    return Color::rgb( 90, 200, 220);
            case K_ENV:    return Color::rgb( 90, 200, 220);
            case K_DECAY:  return Color::rgb(230, 180,  90); // ENV    amber
            case K_ACCENT: return Color::rgb(120, 210, 140); // AMP    green
            case K_DRIVE:  return Color::rgb(120, 210, 140);
            case K_VOL:    return Color::rgb(120, 210, 140);
        }
        return tb303::clr_accent();
    };

    auto build_one = [&](int i) -> Element {
        bool focused = (g_focus == Section::Knobs) && (g_knob_sel == i);
        maya::Color gc = group_color_for(i);
        const auto id = hit(hit_id(tb303::kHitKnob, static_cast<std::uint32_t>(i)));
        if (i == K_WAVE) {
            tb303::WaveformToggle w;
            w.index       = g_wave;
            w.focused     = focused;
            w.group_col   = gc;
            w.tech_symbol = g_knob_table[i].tech;
            return (w.build() | id).build();
        }
        tb303::Knob k;
        k.label         = g_knob_table[i].label;
        k.value         = *g_knob_table[i].value;
        k.default_value = g_knob_table[i].default_v;
        // 0.005 threshold keeps UI rounding from tripping "modified" — a
        // single ↑ tap moves by 0.05, well above the noise floor.
        k.modified      = std::abs(*g_knob_table[i].value - g_knob_table[i].default_v) > 0.005f;
        k.value_text    = format_knob(i);
        k.tech_symbol   = g_knob_table[i].tech;
        k.focused       = focused;
        k.group_col     = gc;
        return (k.build() | id).build();
    };

    // Build the 8-column strip in signal-flow order: OSC (tune, wave) →
    // VCF (cutoff, res, env) → EG (decay) → VCA (accent, vol). Knob::WIDTH
    // is 8 with a 1-col gap between columns → the strip is 71 cells wide.
    // justify(Center) lets the hstack expand to the panel width and centers
    // the 8 knobs in it, so the strip stays centered on wide terminals and
    // collapses neatly (even overflow both sides) on narrow ones.
    std::vector<Element> cols;
    cols.reserve(K_COUNT);
    for (int i : {K_TUNE, K_WAVE, K_CUTOFF, K_RES,
                  K_ENV,  K_DECAY, K_ACCENT, K_DRIVE, K_VOL}) {
        cols.push_back(build_one(i));
    }
    auto strip = (dsl::h(std::move(cols)) | dsl::gap(1)
                  | dsl::justify(Justify::Center)).build();

    // ── Schematic group header ─────────────────────────────────────────────
    // One flat rail per functional block (OSC / VCF / EG / VCA). Each rail
    // is its own Element with a fixed width that exactly spans the knob
    // columns beneath it, and rails are composed into an hstack with gap(1)
    // — mirroring the knob strip's structure cell-for-cell, so both rows
    // centre to identical positions under justify(Center).
    //   OSC = TUNE(8)   + gap + WAVE(8)                       = 17
    //   VCF = CUTOFF(8) + gap + RES(8)    + gap + ENV(8)      = 26
    //   EG  = DECAY(8)                                        =  8
    //   VCA = ACCENT(8) + gap + DRIVE(8)  + gap + VOL(8)      = 26
    auto build_rail = [](const char* label, int width, maya::Color c) -> Element {
        int label_cols   = static_cast<int>(std::string_view(label).size());
        int dashes_total = std::max(0, width - 2 /*spaces*/ - label_cols);
        int left         = dashes_total / 2;
        int right        = dashes_total - left;

        std::string            txt;
        std::vector<StyledRun> runs;

        size_t l_off = txt.size();
        for (int i = 0; i < left; ++i) txt += "\xe2\x94\x80"; // ─
        txt += " ";
        runs.push_back(StyledRun{l_off, txt.size() - l_off,
            maya::Style{}.with_fg(c)});

        size_t lbl_off = txt.size();
        txt += label;
        runs.push_back(StyledRun{lbl_off, static_cast<size_t>(label_cols),
            maya::Style{}.with_fg(c).with_bold()});

        size_t r_off = txt.size();
        txt += " ";
        for (int i = 0; i < right; ++i) txt += "\xe2\x94\x80"; // ─
        runs.push_back(StyledRun{r_off, txt.size() - r_off,
            maya::Style{}.with_fg(c)});

        return Element{maya::TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = maya::TextWrap::NoWrap,
            .runs    = std::move(runs),
        }};
    };

    auto group_header = (dsl::h(
        build_rail("OSC", 17, group_color_for(K_TUNE)),
        build_rail("VCF", 26, group_color_for(K_CUTOFF)),
        build_rail("EG",   8, group_color_for(K_DECAY)),
        build_rail("VCA", 26, group_color_for(K_VOL))
    ) | dsl::gap(1) | dsl::justify(Justify::Center)).build();

    // Caption row: shows the focused knob's description inline with the
    // actual keybinds ("↑↓ ±5% · [ ] ±10% · 0 reset"). Appears whenever the
    // synth-voice section is focused; otherwise a quiet hint keeps the
    // panel height stable.
    auto build_caption = [&]() -> Element {
        std::string txt;
        std::vector<StyledRun> runs;
        bool kfoc = (g_focus == Section::Knobs);

        if (kfoc) {
            size_t off = txt.size();
            txt += "\xe2\x96\xb6 ";  // ▶
            runs.push_back(StyledRun{off, 3,
                maya::Style{}.with_fg(tb303::clr_accent()).with_bold()});

            size_t lbl_off = txt.size();
            txt += g_knob_table[g_knob_sel].label;
            runs.push_back(StyledRun{lbl_off, std::string_view(g_knob_table[g_knob_sel].label).size(),
                maya::Style{}.with_fg(tb303::clr_accent()).with_bold()});

            txt += "  ";
            size_t cap_off = txt.size();
            txt += g_knob_table[g_knob_sel].caption;
            runs.push_back(StyledRun{cap_off, std::string_view(g_knob_table[g_knob_sel].caption).size(),
                maya::Style{}.with_fg(tb303::clr_text())});

            txt += "   \xc2\xb7   ";
            size_t h_off = txt.size();
            txt += "\xe2\x86\x91\xe2\x86\x93 turn  \xc2\xb7  \xe2\x86\x90\xe2\x86\x92 select"
                   "  \xc2\xb7  [ ] \xc2\xb1" "10%  \xc2\xb7  0 reset";
            runs.push_back(StyledRun{h_off, txt.size() - h_off,
                maya::Style{}.with_fg(tb303::clr_muted())});
        } else {
            size_t off = txt.size();
            txt += "\xe2\x97\xa6 ";  // ◦
            runs.push_back(StyledRun{off, 3,
                maya::Style{}.with_fg(tb303::clr_muted())});
            size_t msg_off = txt.size();
            txt += "Tab here to shape the voice  \xc2\xb7  \xe2\x86\x90\xe2\x86\x92 select knob, \xe2\x86\x91\xe2\x86\x93 turn it";
            runs.push_back(StyledRun{msg_off, txt.size() - msg_off,
                maya::Style{}.with_fg(tb303::clr_muted())});
        }

        // Wrap the NoWrap caption in a min-width:0 / overflow:hidden box so
        // its intrinsic text length can't inflate the panel's min-content
        // width. Without this, switching to a long caption ("output stage
        // saturation [clean → pedal-grit]") would grow the whole SYNTH VOICE
        // panel and squeeze the FX RACK on the same row.
        auto text_el = Element{maya::TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = maya::TextWrap::NoWrap,
            .runs    = std::move(runs),
        }};
        return dsl::hstack()
            .min_width(Dimension::fixed(0))
            .overflow(Overflow::Hidden)(std::move(text_el));
    };

    // basis(0) + min_width(0) pins the panel's width to its flex-grow share
    // alone. Without these, yoga would use the children's intrinsic widths
    // (caption text length, strip width, etc.) as the panel's minimum content
    // size — so a long focused-knob caption would inflate the SYNTH panel and
    // squeeze the FX panel on the same row. Overflow::Hidden clips any content
    // wider than the allotted slot cleanly.
    const char* synth_title = g_synth_mute
        ? " \xe2\x97\x87 SYNTH VOICE \xc2\xb7 MUTED (m) \xe2\x97\x87 "
        : " \xe2\x97\x87 SYNTH VOICE \xc2\xb7 VCO\xe2\x86\x92VCF\xe2\x86\x92VCA"
          " \xc2\xb7 24 dB/oct LADDER \xe2\x97\x87 ";
    return (dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(g_synth_mute ? tb303::clr_dim()
                      : g_focus == Section::Knobs ? tb303::clr_accent() : tb303::clr_panel_hi())
        .border_text(synth_title, BorderTextPos::Top)
        .align_self(Align::Stretch)
        .basis(Dimension::fixed(0))
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)
        .padding(0, 2, 0, 2)(
            std::move(group_header),
            std::move(strip),
            build_caption()
        ) | hit(hit_id(tb303::kHitPanel, static_cast<std::uint32_t>(Section::Knobs)))).build();
}

// Compact FX rack panel — one bordered row below the SYNTH VOICE panel.
// Keeps the same group-colour discipline as the voice strip so related FX
// read together visually. Three groups in signal-flow order:
//   OD (pre-filter, amber) → DELAY (warm orange) → REVERB (cool teal)
Element App::build_fx_row() const {
    auto group_color_for = [](int i) -> maya::Color {
        using maya::Color;
        switch (i) {
            case FX_OD:    return Color::rgb(230, 180,  90);  // OD       amber
            case FX_DMIX:
            case FX_DFB:
            case FX_DTIME: return Color::rgb(220, 150,  80);  // DELAY    orange
            case FX_RMIX:
            case FX_RSIZE:
            case FX_RDAMP: return Color::rgb( 90, 190, 200);  // REVERB   teal
        }
        return tb303::clr_accent();
    };

    auto build_one = [&](int i) -> Element {
        bool focused = (g_focus == Section::FX) && (g_fx_sel == i);
        maya::Color gc = group_color_for(i);
        tb303::Knob k;
        k.label         = g_fx_table[i].label;
        k.tech_symbol   = g_fx_table[i].tech;
        k.focused       = focused;
        k.group_col     = gc;
        k.value_text    = format_fx(i);
        if (i == FX_DTIME) {
            // Show selected division as a quantised meter height so the
            // knob visually steps through the four positions.
            k.value         = static_cast<float>(g_delay_div) / 3.0f;
            k.default_value = 2.0f / 3.0f;
            k.modified      = (g_delay_div != 2);
        } else {
            k.value         = *g_fx_table[i].value;
            k.default_value = g_fx_table[i].default_v;
            k.modified      = std::abs(*g_fx_table[i].value - g_fx_table[i].default_v) > 0.005f;
        }
        return (k.build() | hit(hit_id(tb303::kHitFx, static_cast<std::uint32_t>(i)))).build();
    };

    std::vector<Element> cols;
    cols.reserve(FX_COUNT);
    for (int i = 0; i < FX_COUNT; ++i) cols.push_back(build_one(i));

    auto strip = (dsl::h(std::move(cols)) | dsl::gap(1)
                  | dsl::justify(Justify::Center)).build();

    // Caption row matching the SYNTH panel's style so both read together.
    auto build_caption = [&]() -> Element {
        std::string txt;
        std::vector<StyledRun> runs;
        bool ffoc = (g_focus == Section::FX);

        if (ffoc) {
            size_t off = txt.size();
            txt += "\xe2\x96\xb6 ";
            runs.push_back(StyledRun{off, 3,
                maya::Style{}.with_fg(tb303::clr_accent()).with_bold()});

            size_t lbl_off = txt.size();
            txt += g_fx_table[g_fx_sel].label;
            runs.push_back(StyledRun{lbl_off, std::string_view(g_fx_table[g_fx_sel].label).size(),
                maya::Style{}.with_fg(tb303::clr_accent()).with_bold()});

            txt += "  ";
            size_t cap_off = txt.size();
            txt += g_fx_table[g_fx_sel].caption;
            runs.push_back(StyledRun{cap_off, std::string_view(g_fx_table[g_fx_sel].caption).size(),
                maya::Style{}.with_fg(tb303::clr_text())});
        } else {
            size_t off = txt.size();
            txt += "\xe2\x97\xa6 ";
            runs.push_back(StyledRun{off, 3,
                maya::Style{}.with_fg(tb303::clr_muted())});
            size_t msg_off = txt.size();
            txt += "Tab here for the FX rack  \xc2\xb7  \xe2\x86\x90\xe2\x86\x92 select, \xe2\x86\x91\xe2\x86\x93 turn";
            runs.push_back(StyledRun{msg_off, txt.size() - msg_off,
                maya::Style{}.with_fg(tb303::clr_muted())});
        }

        auto text_el = Element{maya::TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = maya::TextWrap::NoWrap,
            .runs    = std::move(runs),
        }};
        // Same width-pin trick as the SYNTH caption — keeps the FX panel's
        // width independent of which caption happens to be showing.
        return dsl::hstack()
            .min_width(Dimension::fixed(0))
            .overflow(Overflow::Hidden)(std::move(text_el));
    };

    // Group-header rail, mirroring the SYNTH panel. Three groups in signal-
    // flow order: OD (1 col) → DELAY (3 cols) → REVERB (3 cols). Widths
    // match the knob cells beneath them.
    auto build_rail = [](const char* label, int width, maya::Color c) -> Element {
        int label_cols   = static_cast<int>(std::string_view(label).size());
        int dashes_total = std::max(0, width - 2 - label_cols);
        int left         = dashes_total / 2;
        int right        = dashes_total - left;
        std::string txt;
        std::vector<StyledRun> runs;
        size_t l_off = txt.size();
        for (int i = 0; i < left; ++i) txt += "\xe2\x94\x80";
        txt += " ";
        runs.push_back(StyledRun{l_off, txt.size() - l_off,
            maya::Style{}.with_fg(c)});
        size_t lbl_off = txt.size();
        txt += label;
        runs.push_back(StyledRun{lbl_off, static_cast<size_t>(label_cols),
            maya::Style{}.with_fg(c).with_bold()});
        size_t r_off = txt.size();
        txt += " ";
        for (int i = 0; i < right; ++i) txt += "\xe2\x94\x80";
        runs.push_back(StyledRun{r_off, txt.size() - r_off,
            maya::Style{}.with_fg(c)});
        return Element{maya::TextElement{
            .content = std::move(txt),
            .style   = {},
            .wrap    = maya::TextWrap::NoWrap,
            .runs    = std::move(runs),
        }};
    };

    auto group_header = (dsl::h(
        build_rail("OD",    8, group_color_for(FX_OD)),
        build_rail("DELAY", 26, group_color_for(FX_DMIX)),
        build_rail("REVERB",26, group_color_for(FX_RMIX))
    ) | dsl::gap(1) | dsl::justify(Justify::Center)).build();

    // Same basis/min_width pin as the SYNTH panel — keeps this panel's width
    // strictly at its flex-grow share (6/14) regardless of caption length.
    const char* fx_title = g_synth_mute
        ? " \xe2\x97\x87 FX RACK \xc2\xb7 MUTED (m) \xe2\x97\x87 "
        : " \xe2\x97\x87 FX RACK \xc2\xb7 OD\xe2\x86\x92" "DELAY\xe2\x86\x92" "REVERB \xe2\x97\x87 ";
    return (dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(g_synth_mute ? tb303::clr_dim()
                      : g_focus == Section::FX ? tb303::clr_accent() : tb303::clr_panel_hi())
        .border_text(fx_title, BorderTextPos::Top)
        .align_self(Align::Stretch)
        .basis(Dimension::fixed(0))
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)
        .padding(0, 2, 0, 2)(std::move(group_header), std::move(strip), build_caption())
        | hit(hit_id(tb303::kHitPanel, static_cast<std::uint32_t>(Section::FX)))).build();
}


Element App::render_frame() const {
    // Transport state bundle
    std::vector<tb303::PresetSummary> preset_summaries;
    preset_summaries.reserve(g_presets.size());
    for (const auto& p : g_presets) {
        preset_summaries.push_back(tb303::PresetSummary{
            .name  = p.name,
            .steps = std::vector<StepData>(p.steps.begin(), p.steps.end()),
        });
    }

    tb303::TransportState ts{
        .playing        = g_playing,
        .bpm            = g_bpm,
        .swing          = g_swing,
        .pattern_length = g_pattern_length,
        .current_step   = g_current_step,
        .pattern_index  = g_preset_index,
        .presets        = std::move(preset_summaries),
        .steps          = std::span<const StepData>{g_steps.data(), g_steps.size()},
        .focused        = (g_focus == Section::Transport),
        .song_mode      = g_song_mode,
        .song_slot      = g_song_slot,
        .jam_mode       = g_jam_mode,
        .jam_octave     = g_jam_octave,
        .jam_last_midi  = g_jam_last_midi,
        .jam_accent     = g_jam_accent,
        .jam_slide      = g_jam_slide,
        .midi_out       = g_midi_out,
        .midi_sync      = g_midi_sync,
    };

    auto knob_row   = build_knob_row();
    auto fx_row     = build_fx_row();
    auto filter_box = tb303::build_filter_response(g_cutoff, g_resonance, g_spectrum);
    auto scope_box  = tb303::build_scope(g_scope);
    auto transport  = tb303::build_transport(ts);

    // Left column stacks FILTER RESPONSE on top of SCOPE so the two visual
    // analyses share the same width (both read horizontally as log-freq /
    // time). basis(0) + min_width(0) lets middle_row's flex algorithm
    // distribute horizontal space by grow factors alone; without this the
    // filter chart's natural width would claim most of the row.
    auto left_col = dsl::vstack()
        .gap(0)
        .grow(1.0f)
        .basis(Dimension::fixed(0))
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)
        (
            std::move(filter_box) | dsl::grow(3),
            std::move(scope_box)  | dsl::grow(2)
        );

    // Drums sit in a fill() so the panel can shrink on short terminals and
    // clip the bottom voices instead of pushing the layout taller.
    auto drums_component = dsl::fill([this](int, int) -> Element {
        return tb303::build_drums(g_drums, g_drum_voice_mute,
                                  g_drum_voice_sel, g_drum_step_sel,
                                  g_playing ? g_current_step : -1,
                                  g_drum_master,
                                  g_focus == Section::Drums);
    }, /*min_w=*/0, /*min_h=*/kDrumVoices + 2);

    auto drum_panel = dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(g_drums_mute ? tb303::clr_dim()
                      : g_focus == Section::Drums ? tb303::clr_accent() : tb303::clr_panel_hi())
        .border_text(g_drums_mute ? " DRUMS \xc2\xb7 MUTED (m) " : " DRUMS ", BorderTextPos::Top)
        .basis(Dimension::fixed(0))
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)
        .padding(0, 1, 0, 1)(std::move(drums_component))
        | hit(hit_id(tb303::kHitPanel, static_cast<std::uint32_t>(Section::Drums)));

    // Middle row hosts three stacked panels side by side: scope/filter (left),
    // transport (centre), drums (right). voice_row (below) is given shrink(1)
    // so on short terminals the knob row compresses first and middle_row keeps
    // enough height to render the 16-voice drum grid unclipped.
    auto middle_row = (dsl::h(
        std::move(left_col)   | dsl::grow(3) | hit(hit_id(tb303::kHitScope)),
        std::move(transport)  | dsl::grow(2) | hit(hit_id(tb303::kHitTransport)),
        std::move(drum_panel) | dsl::grow(2)
    ) | dsl::gap(1) | dsl::grow(1)).build();

    auto seq = tb303::build_sequencer(g_steps, g_pattern_length, g_step_sel,
                                      g_playing ? g_current_step : -1);
    auto seq_panel = dsl::vstack()
        .border(BorderStyle::Round)
        .border_color(g_synth_mute ? tb303::clr_dim()
                      : g_focus == Section::Sequencer ? tb303::clr_accent() : tb303::clr_panel_hi())
        .border_text(g_synth_mute ? " SEQUENCER \xc2\xb7 MUTED (m) " : " SEQUENCER ", BorderTextPos::Top)
        .basis(Dimension::fixed(0))
        .min_width(Dimension::fixed(0))
        .overflow(Overflow::Hidden)
        .padding(0, 1, 0, 1)(std::move(seq))
        | hit(hit_id(tb303::kHitPanel, static_cast<std::uint32_t>(Section::Sequencer)));

    int help_auto = static_cast<int>(g_help_bar_auto_t / kHelpBarChipDur);
    auto help_bar = tb303::build_help_bar(g_focus, g_help_open,
                                          g_help_bar_scroll, g_term_w, help_auto)
                  | hit(hit_id(tb303::kHitHelpBar));

    // Title bar: a compact brand strip with a pulse-colored dot that flips
    // from muted → red while playing, so the play state is legible from a
    // glance even when the transport panel is off-screen. The ◆ icon brightens
    // on the downbeat (step%4==0) and fades back over the step — an ambient
    // cue that's easy to sync a shoulder nod to without looking at the cells.
    bool on_downbeat = g_playing && g_current_step >= 0 && (g_current_step % 4 == 0);
    float pulse_amt = on_downbeat ? (1.0f - g_step_phase) : 0.0f;
    maya::Color brand_col = on_downbeat
        ? maya::Color::rgb(
            static_cast<uint8_t>(255),
            static_cast<uint8_t>(138 + (255 - 138) * pulse_amt),
            static_cast<uint8_t>(40  + (140 - 40)  * pulse_amt))
        : tb303::clr_accent();

    auto brand = Element{TextElement{
        .content = "\xe2\x97\x86 TB-303",                    // ◆
        .style   = Style{}.with_fg(brand_col).with_bold(),
        .wrap    = TextWrap::NoWrap,
    }};
    auto brand_sub = Element{TextElement{
        .content = std::string("  acid bass simulator  \xc2\xb7  ") + acid_audio_driver(),
        .style   = Style{}.with_fg(tb303::clr_muted()),
        .wrap    = TextWrap::NoWrap,
    }};

    // Right-side status. When a toast is active it REPLACES the play indicator
    // (single element at the right edge) so there's never both a toast AND a
    // "live" glyph on the header at the same time. When the toast timer hits
    // zero we also clear the string so nothing stale can leak into a later
    // render path.
    Element status_el;
    if (g_toast_t > 0.0f) {
        float t = g_toast_t / kToastDur;
        uint8_t r = static_cast<uint8_t>(120 + (255 - 120) * t);
        uint8_t g = static_cast<uint8_t>(120 + ( 70 - 120) * t);
        uint8_t b = static_cast<uint8_t>(120 + ( 70 - 120) * t);
        status_el = Element{TextElement{
            .content = g_toast,
            .style   = Style{}.with_fg(maya::Color::rgb(r, g, b)).with_bold(),
            .wrap    = TextWrap::NoWrap,
        }};
    } else {
        // When the user is tracking to live.wav, replace the play glyph with
        // a bright blinking REC + elapsed seconds so the indicator is
        // unmissable during a take. Blink rate is ~1 Hz off the same timer
        // the toast uses — coarse enough to read, not enough to distract.
        if (acid_is_recording()) {
            float sec = acid_record_seconds();
            bool blink_on = (static_cast<int>(sec * 2.0f) & 1) == 0;
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%s REC  %5.1fs",
                          blink_on ? "\xe2\x97\x8f" : " ", sec);
            status_el = Element{TextElement{
                .content = buf,
                .style   = Style{}.with_fg(tb303::clr_hot()).with_bold(),
                .wrap    = TextWrap::NoWrap,
            }};
        } else {
            status_el = Element{TextElement{
                .content = g_playing ? "\xe2\x97\x8f  live" : "\xe2\x97\x8b  idle",
                .style   = Style{}.with_fg(g_playing ? tb303::clr_hot() : tb303::clr_muted())
                                 .with_bold(),
                .wrap    = TextWrap::NoWrap,
            }};
        }
    }

    // Group brand on the left, status on the right, with `justify(SpaceBetween)`
    // doing the separation. Previously this used a manual `dsl::space` spacer
    // between brand_sub and status_el. That worked but left a subtle artefact:
    // as the status text shrank (long toast → short "● live"), the spacer had
    // to grow to compensate, and on some terminals the last columns of the old
    // toast text lingered visually until the next full repaint. Using
    // SpaceBetween puts each group at a fixed edge, so the status swap happens
    // in-place at the right margin and old glyphs are always overwritten.
    auto brand_group = dsl::hstack().gap(0)(
        std::move(brand),
        std::move(brand_sub)
    );
    auto title_bar = dsl::hstack()
        .padding(0, 1, 0, 1)
        .justify(Justify::SpaceBetween)
        .align_items(Align::Center)
        (std::move(brand_group), std::move(status_el))
        | hit(hit_id(tb303::kHitTitle));

    // Help replaces the main workspace (keeps title + status bar visible) so
    // the user always has context and knows how to close it.
    if (g_help_open) {
        return (dsl::v(
            std::move(title_bar),
            tb303::build_help_overlay(g_help_ov_scroll) | dsl::grow(1)
                | hit(hit_id(tb303::kHitHelpOverlay)),
            std::move(help_bar)
        ) | dsl::gap(0)).build();
    }

    // Take picker modal — same frame skeleton as help, just a different body.
    if (g_takes_open) {
        std::vector<std::string> names;
        names.reserve(g_takes_entries.size());
        for (auto& p : g_takes_entries) names.push_back(p.stem().string());
        return (dsl::v(
            std::move(title_bar),
            tb303::build_take_picker(names, g_takes_sel) | dsl::grow(1),
            std::move(help_bar)
        ) | dsl::gap(0)).build();
    }

    // Sequencer spans the full width at the bottom — drums moved up next to
    // transport so step_row is a single-panel row. shrink(0) pins its natural
    // height on terminal resize so only middle_row flexes.
    auto step_row = dsl::hstack()
        .gap(0)
        .align_items(Align::End)
        .shrink(0)(
            std::move(seq_panel) | dsl::grow(1)
        );

    // Synth voice + FX rack on one row — the voice panel is ~8 knobs wide, FX
    // is ~6, so together they fill a wide terminal in one glance (signal flow
    // reads left → right: voice → FX chain). Stretch keeps the shorter panel's
    // border flush with the taller one's height. shrink(0) pins this row to
    // its natural height when the terminal is resized — only middle_row flexes.
    auto voice_row = dsl::hstack()
        .gap(1)
        .align_items(Align::Stretch)
        .shrink(1)(
            std::move(knob_row) | dsl::grow(8),
            std::move(fx_row)   | dsl::grow(6)
        );

    return (dsl::v(
        std::move(title_bar),
        std::move(voice_row),
        std::move(middle_row),
        std::move(step_row),
        std::move(help_bar)
    ) | dsl::gap(0)).build();
}
