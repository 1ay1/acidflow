# 8 · Anatomy of the interface

acidflow's screen is a dense single-page instrument. Everything is visible at once. Nothing is hidden behind tabs. This is deliberate — a terminal synth has to fight against its own environment for attention, and hiding controls behind menus is how you get an instrument that's fast to *learn* but slow to *play*.

This chapter walks through the screen region by region. It's a reference chapter; read it once and come back when you need to.

## Layout overview

```
┌─ TITLE BAR ─────────────────────────────────────────────────────┐
│ 01 Acid Tracks   BPM 125   LEN 16   SW 50    SONG JAM MIDI-OUT │
├─ KNOBS ──────────────────────────────────────────────────────────┤
│  TUNE  WAVE  CUTOFF  RES  ENVMOD  DECAY  ACCENT  DRIVE  VOL     │
│  (( )) ( )  (( ))    ( )  (  )    (( ))  ( )     ( )    (( ))   │
├─ FX RACK ────────────────────────────────────────────────────────┤
│  DRIVE   DLY TIME   DLY FB   DLY MIX   REV SIZE  REV DAMP  MIX │
├─ SEQUENCER (303 VOICE) ──────────────────────────────────────────┤
│  C2  C2  G2  C3  C2  C2  G2  C2  D#2 C2  G2  C2  A#2 C2  G2  C2│
│  .A. .   .S. .A.  .A. .   .   .S.  .   .A. .S. .   .A. .   .S. │
├─ DRUMS ──────────────────────────────────────────────────────────┤
│  BD  X . . . X . . . X . . . X . . .                             │
│  SD  . . . . X . . . . . . . X . . .                             │
│  CH  . . X . . . X . . . X . . . X .                             │
│  OH  . . . . . . . X . . . . . . . X                             │
│  CL  . . . . . . . . . . . . . . . .                             │
├─ TRANSPORT ──────────────────────────────────────────────────────┤
│  PRESET: 01 Acid Tracks   BPM 125   LEN 16   SW 50              │
├─ SCOPE + FILTER ─────────────────────────────────────────────────┤
│  [live waveform]  |  [filter frequency response heatmap]         │
├─ HELP BAR ───────────────────────────────────────────────────────┤
│  Tab focus · Space play · ? help · r rand · q quit · ...         │
└──────────────────────────────────────────────────────────────────┘
```

The exact vertical layout depends on your terminal size. acidflow adapts — smaller terminals collapse panels, larger terminals give each panel more breathing room.

## The five focus sections

acidflow has five focusable sections. The currently-focused section shows a brighter border. Keyboard input is routed to the focused section.

1. **Knobs** (9 synth parameters for the 303 voice).
2. **FX** (7 knobs for the effects rack).
3. **Sequencer** (the 303 step grid).
4. **Drums** (the 5×16 drum grid).
5. **Transport** (preset, BPM, pattern length, swing).

Cycle focus with **Tab**. Reverse with **Shift-Tab**. Hover the mouse over any panel to focus it (no click needed). Click also focuses, and initiates whatever drag/scroll the click implies.

Each section's specific shortcuts only work when the section is focused. Global shortcuts (`Space`, `?`, `r/R`, theme cycle, MIDI toggles, etc.) work regardless of focus. This is explicit in the help overlay and consistent throughout.

## Title bar

The top strip shows:

- **Preset number and name** — `01 Acid Tracks`, `05 Voodoo Ray`, etc. Even if you've modified the pattern, the preset name shows until you load a new preset or save to a user slot.
- **BPM** — the current tempo. You can scroll on the title bar to nudge it.
- **LEN** — pattern length, 4–16.
- **SW** — swing, 50–75.
- **Status badges** — shown only when active:
  - `SONG` — song mode is on (chained playback).
  - `JAM` — jam mode is on (keyboard-as-piano).
  - `MIDI-OUT` — MIDI output is enabled.
  - `MIDI-SYNC` — external MIDI clock sync is enabled.
  - `MUTE 303` — the 303 voice is muted.
  - `MUTE DRUMS` — the drum bus is muted.

If the terminal is too narrow to show all the badges, the least-important ones drop off.

## Knobs panel

Nine knobs control the 303 voice. They are, left to right:

- **TUNE**: pre-transposes the oscillator ±12 semitones.
- **WAVE**: toggles saw (default) vs. square.
- **CUTOFF**: filter cutoff, 13 Hz to 5 kHz (log-scaled).
- **RES**: filter resonance, 0 to just-below-self-oscillation.
- **ENVMOD**: how much the envelope opens the filter, 0 to 4 octaves.
- **DECAY**: decay time for both filter and amp envelopes, 200 ms to 2000 ms.
- **ACCENT**: how much accent strengthens accented notes.
- **DRIVE**: pre-filter saturation / overdrive.
- **VOL**: output volume.

Each knob shows a circular indicator, a numeric value, and a short label. Use `←`/`→` to select, `↑`/`↓` to adjust by 5%, `[`/`]` to adjust by 10%, `0` to reset. `w` toggles WAVE. `m` (in this section) mutes the 303 voice without stopping the sequencer.

## FX rack panel

Seven knobs control the FX rack. Left to right:

- **FX DRIVE**: pre-filter overdrive of the 303 voice (distinct from the `DRIVE` knob in the voice panel, though similar in effect — the voice DRIVE is per-voice, the FX DRIVE is on the master).
- **DLY TIME**: delay time, quantized to tempo-synced divisions (1/16, 1/16 dotted, 1/8, 1/8 dotted).
- **DLY FB**: delay feedback. Higher = more repeats.
- **DLY MIX**: dry/wet balance.
- **REV SIZE**: reverb room size. Small to large.
- **REV DAMP**: high-frequency damping in the reverb. High damping = darker tail.
- **REV MIX**: reverb dry/wet.

Same keybindings as the voice knobs. `m` mutes the entire FX bus — useful A/B comparisons.

## Sequencer panel

Sixteen columns, each representing one 16th-note step. Each step shows:

- **Pitch** on top — e.g., `C2`, `D#3`, `G1`. Color-coded by octave.
- **Modifiers** on bottom — letters indicating accent (A), slide (S), rest (—), probability (p), ratchet (×2/×3/×4), and p-lock state.

The currently-selected step has a brighter border. The currently-playing step (while Space is running) has a highlight that moves through the pattern.

Navigation: `←`/`→` moves the selected step. `↑`/`↓` nudges pitch by semitone. `<`/`>` nudges by octave. Letter keys (`c d e f g a b`) set the pitch class at the current octave.

Modifiers: `a` toggles accent. `s` toggles slide. `m` toggles rest (the rest key is context-sensitive — in the sequencer it means "this step is silent"; in the knobs panel it means "mute voice"). `v` cycles probability (100 → 75 → 50 → 25 → 100). `j` cycles ratchet (×1 → ×2 → ×3 → ×4 → ×1).

Parameter locks: `F` p-locks the cutoff for this step to the current knob value. `G` p-locks resonance. `H` p-locks envmod. `J` p-locks accent level. `L` clears all locks on the current step. `x` clears the step entirely.

The sequencer section is where most of the compositional work happens. Chapters 15–21 are all about techniques for writing in it.

## Drums panel

Five rows (BD, SD, CH, OH, CL), 16 columns each. Each cell is either a hit (`X`) or empty (`.`).

Navigation: `←`/`→` moves between steps. `↑`/`↓` moves between voices. `Space` or `x` toggles a hit. `1`..`9`, `0` are shortcuts for quick-toggling steps 1–10 on the currently-selected voice (super fast for programming).

`[` and `]` adjust the drum bus master gain. `c` clears the current row; `C` clears the whole kit.

Each drum has a per-voice gain that can be set via the voice icons, but the interface is primarily grid-based for speed.

## Transport panel

Three controls:

- **Preset browser** — `↑`/`↓` to browse the 16 built-in presets.
- **BPM** — `[`/`]` to adjust by 2 BPM; shift-`[`/`]` for 10 BPM. Scroll wheel on the title bar also nudges BPM.
- **Length** — `{`/`}` adjusts pattern length (4–16).
- **Swing** — `-`/`=` adjusts swing (50–75%).

Mute: `m` in transport mutes the master output (both 303 and drums).

## Scope and filter heatmap

At the bottom of the screen, two real-time visualizations:

- **Oscilloscope**: a Braille-cell waveform display of the master output, zero-crossing triggered so it looks stable rather than scrolling. Use this to see the character of your sound — is it peaked (accented), is it clipping (drive too high), is it thin (filter closed).
- **Filter response heatmap**: a frequency-response plot of the current filter state, updated in real time. You can literally watch the filter open and close with each note. The resonance peak is visible as a bright band. Useful for understanding what the filter is doing as the envelope moves.

Neither visualization is essential, but they're instructive. If you're learning subtractive synthesis, watching the filter heatmap change as you turn knobs is faster than any textbook.

## Help bar

The bottom strip scrolls through context-appropriate hints: which shortcuts are relevant to the currently-focused section, what the global keys are, what the status flags mean. You can ignore it once you've memorized everything, but it's a good safety net for new users.

## Themes

Five color themes are built in:

1. **classic** — 303 silver + Roland orange.
2. **cyber** — magenta and cyan on ink.
3. **moss** — chartreuse and amber on forest.
4. **ice** — steel blue and coral on slate.
5. **mono** — black and white only.

Cycle with `T`. The choice affects every color in the program (accents, highlights, text, backgrounds). Themes are saved with user slots — if you save a pattern to slot 3 in ice theme, loading slot 3 later will also set the theme.

## Mouse

Everything is mouse-operable:

- **Hover** a panel to focus it.
- **Click** a knob to select it.
- **Drag** a knob vertically to adjust (up = increase).
- **Scroll** on a knob to fine-tune.
- **Right-click** a knob to reset.
- **Click** a sequencer step to select it.
- **Click** a drum cell to toggle.
- **Scroll** on the title bar to nudge BPM.

The mouse is optional — every mouse interaction has a keyboard equivalent — but it's significantly faster for certain workflows (setting long pattern sequences, live knob-riding).

## Why the UI is the way it is

A few deliberate decisions worth noting:

1. **Everything on one page.** You shouldn't have to remember where a feature lives. You should be able to see it.
2. **Focus sections, not modal dialogs.** Keyboard shortcuts are section-specific, which means each section can use simple one-letter keys without collision. The cost is that you have to remember which section you're in.
3. **Mouse and keyboard are equivalent.** Some people prefer one, some the other. The program supports both cleanly.
4. **Immediate visual feedback.** Every parameter change is visible instantly in the scope, the heatmap, or the pattern display. You should never have to guess what the instrument is doing.

With the interface mapped, we can start digging into the knobs themselves. The next chapter is about the oscillator — TUNE and WAVE — and why those two controls, small as they seem, define the territory your sound lives in.
