// The action table. Order within a scope is the order help shows them.
#include "actions.hpp"
#include "app.hpp"

#include <algorithm>
#include <cctype>

namespace {
using A = App;
#define RUN(body) [](App& a) { body; }

const std::vector<Action> kActions = {
    // ── Everywhere ───────────────────────────────────────────────────────────
    {"play",        Scope::Global, "Space", "Play / stop",
        "Start or stop the pattern.", 1,
        RUN(a.g_playing ? a.stop_playback() : a.start_playback())},
    {"focus.next",  Scope::Global, "Tab / S-Tab", "Next / previous section",
        "Move the orange focus box between the five panels. Each panel has its own keys.", 1,
        RUN(a.cycle_focus(+1))},
    {"palette",     Scope::Global, ": / Ctrl-K", "Command palette",
        "Search every action by name and run it. The quickest way to find anything.", 1},
    {"help",        Scope::Global, "?", "Help",
        "Show help for the panel you're in, plus everything else. Type to search.", 1},
    {"undo",        Scope::Global, "u / U", "Undo / redo",
        "Take back the last change to the pattern, knobs or drums (32 steps). U redoes.", 2},
    {"undo.run",    Scope::Global, "u", "Undo",
        "Take back the last change.", 0, RUN(a.do_undo())},
    {"redo",        Scope::Global, "U", "Redo",
        "Put back what undo took away.", 0, RUN(a.do_redo())},
    {"rand.here",   Scope::Global, "r", "Randomize this panel",
        "Roll new musical values for the focused panel only: knobs, FX, pattern, drums or tempo.", 2,
        RUN(a.randomize_focused())},
    {"rand.all",    Scope::Global, "R", "Randomize everything",
        "A whole new patch and groove in one go. Undo brings the old one back.", 0,
        RUN(a.push_undo("randomize everything"); a.randomize_everything())},
    {"mutate",      Scope::Global, "M", "Evolve the pattern",
        "One small musical change to the pattern. Press repeatedly to drift it.", 0,
        RUN(a.push_undo("evolve"); a.mutate_pattern())},
    {"jam",         Scope::Global, "k", "Jam mode",
        "Play the bass from your keyboard like a piano (z–m and q–u rows). k or Esc to leave.", 0,
        RUN(a.toggle_jam())},
    {"slot.load",   Scope::Global, "1 … 9", "Load a saved slot",
        "Load pattern slot 1–9. Save with Shift+1–9 (the !@#… keys).", 0},
    {"song",        Scope::Global, "n", "Song mode",
        "Play your saved slots in order, each for its bar count. Set bars in Transport with ( ).", 0,
        RUN(a.toggle_song_mode())},
    {"arrange",     Scope::Global, "A", "Auto-arrange a song",
        "Write a ~10 minute intro→build→drop→outro song into slots 1–9 and play it.", 0,
        RUN(a.auto_arrange())},
    {"bounce",      Scope::Global, "e", "Bounce WAV + stems",
        "Render 4 loops to bounce.wav plus separate synth and drum stems. Playback keeps going.", 0,
        RUN(a.export_wav())},
    {"record",      Scope::Global, "W", "Record a live take",
        "Record everything you hear, tweaks included, to live_<time>.wav. W again stops.", 0,
        RUN(a.toggle_live_record())},
    {"takes",       Scope::Global, "Y", "Browse takes",
        "Pick a recorded take and load its pattern back.", 0, RUN(a.open_take_picker())},
    {"midi.file",   Scope::Global, "E", "Export MIDI file",
        "Write the pattern as pattern.mid for your DAW.", 0, RUN(a.export_mid())},
    {"code.copy",   Scope::Global, "p", "Export pattern code",
        "Write a short text code for this pattern to pattern.txt, to share.", 0,
        RUN(a.export_pattern_text())},
    {"code.paste",  Scope::Global, "P", "Import pattern code",
        "Load the pattern code from pattern.txt.", 0,
        RUN(a.push_undo("import pattern"); a.import_pattern_text())},
    {"midi.out",    Scope::Global, "O", "MIDI out",
        "Send notes, drum hits and clock to other gear or a DAW.", 0, RUN(a.toggle_midi_out())},
    {"midi.sync",   Scope::Global, "I", "Follow MIDI clock",
        "Take tempo and start/stop from an external MIDI clock.", 0, RUN(a.toggle_midi_sync())},
    {"link",        Scope::Global, "K", "Ableton Link",
        "Share tempo and bar position with other apps on the network.", 0, RUN(a.toggle_link())},
    {"theme",       Scope::Global, "T", "Change colours",
        "Cycle through the colour themes.", 0, RUN(a.next_theme())},
    {"quit",        Scope::Global, "q / Esc", "Quit",
        "Leave acidflow. Your session is saved and comes back next time.", 0,
        RUN(a.g_quit_requested = true)},

    // ── Synth voice ──────────────────────────────────────────────────────────
    {"knob.pick",   Scope::Knobs, "← →", "Pick a knob",
        "Move between TUNE, WAVE, CUTOFF, RES, ENVMOD, DECAY, ACCENT, DRIVE and VOL.", 1},
    {"knob.turn",   Scope::Knobs, "↑ ↓", "Turn it",
        "Turn the knob in 5% steps. [ ] for 10%. Or drag it with the mouse.", 1},
    {"knob.reset",  Scope::Knobs, "0", "Reset knob",
        "Put the knob back to its default.", 2, nullptr, false},
    {"wave",        Scope::Knobs, "w", "Saw / square",
        "Switch the oscillator between saw (classic) and square (hollow).", 2,
        RUN(a.g_wave = 1 - a.g_wave)},
    {"knob.auto",   Scope::Knobs, ".", "Record knob moves",
        "While playing, write your cutoff/res/env/accent moves into the steps. . again stops.", 0,
        nullptr, false},
    {"learn",       Scope::Knobs, "l", "MIDI learn",
        "Bind this knob to a hardware controller: press l, then move the controller.", 2,
        nullptr, false},
    {"mute.synth",  Scope::Knobs, "m", "Mute the bass",
        "Silence the 303 voice (drums keep going).", 0, RUN(a.toggle_synth_mute())},

    // ── FX rack ─────────────────────────────────────────────────────────────
    {"fx.pick",     Scope::FX, "← →", "Pick an effect",
        "Overdrive, delay mix / feedback / time, reverb mix / size / damping.", 1},
    {"fx.turn",     Scope::FX, "↑ ↓", "Turn it",
        "Adjust in 5% steps, [ ] for 10%. Delay time steps through 1/16 … 1/8 dotted.", 1},
    {"fx.reset",    Scope::FX, "0", "Reset",
        "Put the effect back to its default.", 2, nullptr, false},
    {"fx.learn",    Scope::FX, "l", "MIDI learn",
        "Bind this effect to a hardware controller.", 2, nullptr, false},
    {"fx.mute",     Scope::FX, "m", "Mute the bass",
        "Silence the 303 voice (drums keep going).", 0, RUN(a.toggle_synth_mute())},

    // ── Sequencer ───────────────────────────────────────────────────────────
    {"step.pick",   Scope::Sequencer, "← →", "Pick a step",
        "Move along the 16 steps. Or click a step.", 1},
    {"step.pitch",  Scope::Sequencer, "↑ ↓", "Change pitch",
        "Raise or lower the note a semitone. < > jump an octave. c d e f g a b set the note.", 1},
    {"step.accent", Scope::Sequencer, "a", "Accent",
        "Make the step louder and brighter: the 303's punch.", 1},
    {"step.slide",  Scope::Sequencer, "s", "Slide",
        "Glide from this note into the next one. Two slides in a row make the 303 squelch.", 1},
    {"step.clear",  Scope::Sequencer, "x", "Make it a rest",
        "Turn the step into silence.", 2},
    {"step.prob",   Scope::Sequencer, "v", "Chance",
        "How often the step plays: 100 → 75 → 50 → 25%. Keeps loops from feeling stuck.", 0},
    {"step.ratchet",Scope::Sequencer, "j", "Ratchet",
        "Retrigger the step 1–4 times for rolls and stutters.", 0},
    {"step.lock",   Scope::Sequencer, "F G H J", "Lock a knob on this step",
        "Freeze the current cutoff / res / env / accent on this step only. L clears.", 0},
    {"pattern.evolve", Scope::Sequencer, "m", "Evolve",
        "One small musical change to the pattern.", 0, RUN(a.push_undo("evolve"); a.mutate_pattern())},
    {"pattern.clear", Scope::Sequencer, ":", "Clear the whole pattern",
        "Turn every step into a rest, to start from scratch.", 0,
        RUN(a.push_undo("clear pattern"); for (auto& s : a.g_steps) { s = StepData{}; s.rest = true; }
            a.toast_undoable("pattern cleared"))},
    {"slot.save",   Scope::Global, "S-1 … S-9", "Save to a slot",
        "Save pattern, drums and knobs to slot 1–9: hold Shift and press the number.", 0},

    // ── Drums ───────────────────────────────────────────────────────────────
    {"drum.move",   Scope::Drums, "← → ↑ ↓", "Move",
        "Left/right picks the step, up/down picks the drum.", 1},
    {"drum.hit",    Scope::Drums, "Space / x", "Add / remove a hit",
        "Toggle the drum on this step. 1–0 toggle steps 1–10 directly. Or click a cell.", 1},
    {"drum.solo",   Scope::Drums, "s", "Mute this drum",
        "Silence one drum voice.", 2},
    {"drum.clear",  Scope::Drums, "c / C", "Clear row / all",
        "Wipe this drum's row, or the whole kit with C.", 2},
    {"drum.level",  Scope::Drums, "[ ]", "Drum volume",
        "Turn the whole drum bus down or up.", 0},
    {"mute.drums",  Scope::Drums, "m", "Mute drums",
        "Silence the drums (the bass keeps going).", 0, RUN(a.toggle_drums_mute())},

    // ── Transport ───────────────────────────────────────────────────────────────────────
    {"preset",      Scope::Transport, "↑ ↓", "Choose a preset",
        "Step through the built-in patterns (Acid Tracks, Higher State, …).", 1},
    {"preset.next", Scope::Transport, "↓", "Next preset",
        "Load the next built-in pattern.", 0,
        RUN(a.push_undo("preset change"); a.load_preset(a.g_preset_index + 1))},
    {"preset.prev", Scope::Transport, "↑", "Previous preset",
        "Load the previous built-in pattern.", 0,
        RUN(a.push_undo("preset change"); a.load_preset(a.g_preset_index - 1))},
    {"tempo",       Scope::Transport, "[ ]", "Tempo",
        "BPM down / up by 2.", 1},
    {"tempo.up",    Scope::Transport, "]", "Tempo up",
        "Faster by 2 BPM.", 0, RUN(a.nudge_bpm(+2.0f))},
    {"tempo.down",  Scope::Transport, "[", "Tempo down",
        "Slower by 2 BPM.", 0, RUN(a.nudge_bpm(-2.0f))},
    {"swing",       Scope::Transport, "- =", "Swing",
        "Push every other 16th late: 50% straight, 60% MPC, 75% hard shuffle.", 1},
    {"swing.more",  Scope::Transport, "=", "More swing",
        "Shuffle the groove a bit more.", 0, RUN(a.nudge_swing(+0.02f))},
    {"swing.less",  Scope::Transport, "-", "Less swing",
        "Straighten the groove a bit.", 0, RUN(a.nudge_swing(-0.02f))},
    {"length",      Scope::Transport, "{ }", "Pattern length",
        "Make the loop shorter or longer (4–16 steps).", 2},
    {"bars",        Scope::Transport, "( )", "Bars per song slot",
        "In song mode, how long this slot plays before the next.", 0},
    {"mute.all",    Scope::Transport, "m", "Mute everything",
        "Silence both bass and drums, or bring them back.", 0, RUN(a.toggle_all_mute())},
};
#undef RUN
}  // namespace

const std::vector<Action>& actions() { return kActions; }

std::vector<const Action*> actions_for(Scope focused) {
    std::vector<const Action*> out;
    for (const auto& a : kActions) if (a.scope == focused) out.push_back(&a);
    for (const auto& a : kActions) if (a.scope == Scope::Global) out.push_back(&a);
    return out;
}

int fuzzy_score(std::string_view q, const Action& a) {
    if (q.empty()) return 0;
    auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
    // Prefer matches in the name, then in the description and keys.
    auto score_in = [&](std::string_view hay, int weight) {
        size_t hi = 0;
        int run = 0, score = 0;
        bool word_start = true;
        for (char qc : q) {
            if (qc == ' ') continue;
            bool found = false;
            for (; hi < hay.size(); ++hi) {
                const char hc = hay[hi];
                const bool ws = hi == 0 || !std::isalnum(static_cast<unsigned char>(hay[hi - 1]));
                if (lower(hc) == lower(qc)) {
                    score += 1 + (run > 0 ? 3 : 0) + (ws ? 4 : 0);
                    run = run + 1;
                    ++hi;
                    found = true;
                    break;
                }
                run = 0;
            }
            (void)word_start;
            if (!found) return -1;
        }
        return score * weight;
    };
    const int n = score_in(a.name, 3);
    // A name match always beats a description match, and a name that starts
    // with the query beats one that merely contains it.
    if (n >= 0) {
        std::string nm(a.name), ql(q);
        for (auto& c : nm) c = lower(c);
        for (auto& c : ql) c = lower(c);
        return 10000 + n + (nm.rfind(ql, 0) == 0 ? 5000 : 0);
    }
    const int h = score_in(a.help, 1);
    if (h >= 0) return 500 + h;
    const int k = score_in(a.keys, 2);
    return k >= 0 ? k : -1;
}
