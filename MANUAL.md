# acidflow — User Manual

> A complete reference for the acidflow TB-303 acid bass simulator. Everything that exists in the program is documented here. If something in the app is unclear, this manual either explains it or tells you why it works the way it does.

---

## Table of contents

1. [What this is](#1-what-this-is)
2. [Installing & running](#2-installing--running)
3. [The first ten minutes](#3-the-first-ten-minutes)
4. [Anatomy of the screen](#4-anatomy-of-the-screen)
5. [The synth voice — every knob explained](#5-the-synth-voice--every-knob-explained)
6. [The FX rack](#6-the-fx-rack)
7. [The sequencer](#7-the-sequencer)
8. [The drum machine](#8-the-drum-machine)
9. [Transport: BPM, swing, length, presets](#9-transport-bpm-swing-length-presets)
10. [Song mode](#10-song-mode)
11. [Jam mode](#11-jam-mode)
12. [MIDI I/O](#12-midi-io)
13. [Themes](#13-themes)
14. [Randomization & evolving mutation](#14-randomization--evolving-mutation)
15. [Saving, loading, exporting](#15-saving-loading-exporting)
16. [The 16 built-in presets](#16-the-16-built-in-presets)
17. [Mouse reference](#17-mouse-reference)
18. [Complete keyboard reference](#18-complete-keyboard-reference)
19. [How acidflow makes sound (DSP internals)](#19-how-acidflow-makes-sound-dsp-internals)
20. [Architecture & threading model](#20-architecture--threading-model)
21. [Building from source](#21-building-from-source)
22. [Files acidflow reads & writes](#22-files-acidflow-reads--writes)
23. [Performance notes](#23-performance-notes)
24. [Troubleshooting](#24-troubleshooting)
25. [Recipes — patches & patterns to try](#25-recipes--patches--patterns-to-try)
26. [Glossary](#26-glossary)

---

## 1. What this is

acidflow is a software recreation of the **Roland TB-303 Bass Line** (1981–1984), the synthesizer that — by accident — became the founding instrument of acid house, acid techno, and most electronic music styles whose names contain the word "acid". It runs entirely in your terminal, draws its own knobs, and writes audio directly to your sound card.

It's a set of pieces that fit together:

- A **DSP engine** that models the 303's signal chain from circuit analysis (oscillator → ZDF ladder filter → VCA → output saturation) — not from sample playback.
- A **16-step sequencer** with the real-303 per-step controls (pitch, accent, slide, rest) plus extras the original never had: **probability**, **ratchet**, and **per-step parameter locks** on cutoff / resonance / env-mod / accent.
- A **5-voice synthesized drum machine** (BD / SD / CH / OH / CL) with its own 16-step grid and a master send that routes drums through the same FX as the bass.
- An **FX rack** — pre-filter overdrive, tempo-synced delay, plate reverb — on the master bus.
- **MIDI I/O** on Linux (ALSA sequencer): note and drum output on channel 10, clock out, external clock/start/stop sync, and note-in routing to the bass voice. Other platforms get the UI toggles but the backend is a stub.
- **Song mode** (chain saved slots on pattern wrap) and **jam mode** (two-octave live keyboard into the 303 voice).
- A **TUI** built on the [`maya`](https://github.com/1ay1/maya) C++26 framework — keyboard-first, mouse-aware, full-screen, five switchable colour themes.

The 303 voice itself is still monophonic — the original was, too. Everything around it (drums, FX, MIDI, chaining) is additive: skip the keybinds and you have a faithful 303; use them and you have most of a sketchpad.

---

## 2. Installing & running

### Prerequisites

| Platform | Compiler | Audio dev libs |
|---|---|---|
| Linux | GCC 14+ or Clang 18+ | `libasound2-dev` (ALSA) |
| macOS | Apple Clang from recent Xcode | none — CoreAudio ships with the OS |
| Windows | MSVC 19.40+ or Clang-cl | none — WASAPI ships with the OS |

Plus **CMake ≥ 3.28** and a sibling clone of the [`maya`](https://github.com/1ay1/maya) framework. The CMake build expects to find it at `../maya` relative to the acidflow checkout.

### Build

```bash
git clone https://github.com/1ay1/maya.git
git clone https://github.com/1ay1/acidflow.git
cd acidflow
cmake -B build
cmake --build build -j
```

The build is `Release` by default (`-O3 -march=native -funroll-loops` on GCC/Clang, `/O2` on MSVC). The audio backend is selected automatically based on the platform — you don't pick one.

### Run

```bash
./build/acidflow
```

It opens in **fullscreen** terminal mode. Your terminal needs to support:

- True-color / 24-bit color (any modern emulator does)
- UTF-8 box-drawing and Braille glyphs (any modern font does)
- SGR mouse mode (every mainstream emulator since 2015 does)

There are **no command-line flags**. Everything is configured inside the app.

### Quit

Press `q`, `Q`, or `Esc`. Audio stops cleanly, the audio thread is joined, and your terminal returns to normal.

---

## 3. The first ten minutes

If you read nothing else, read this:

1. **Run it.** It opens with the *Acid Tracks* preset loaded and the cursor on the sequencer.
2. **Hit `Space`.** It starts playing at 122 BPM. The sequencer's currently-playing step lights up red.
3. **Hit `Tab` once.** Focus moves to the **Transport** panel on the right. Press `↓` to scroll through presets — each one swaps in a new pattern and the contour preview fills in.
4. **Hit `Tab` once more.** Focus moves to the **Knobs**. Use `←` and `→` to land on **CUTOFF** (the third one). Press `↑` repeatedly. You're playing the 303's signature filter sweep — the squelchy "wow" — by hand.
5. **Hit `R`.** Capital R. Everything randomizes — knobs and pattern. A toast in the top-right tells you what archetype you got. Hit `R` again until something grabs you.
6. **Press `?`** for the keyboard reference. **Press `?`** again to close it.
7. **Press `Shift+1`** to save the current pattern to slot 1. **Press `1`** later to load it back.
8. **Press `e`** to export the current pattern to a WAV (4 loops). Path will appear in a toast.
9. **Press `q`** when you're done.

Everything else in this manual is detail.

---

## 4. Anatomy of the screen

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ ◆ TB-303  acid bass simulator                                       ● live  │   ← title bar (toggleable toast on the right)
├─────────────────────────────────────────────────────────────────────────────┤
│ ┌──── ◇ SYNTH VOICE · VCO→VCF→VCA · 24 dB/oct LADDER ◇ ──────────────────┐ │   ← knob strip (12 rows, fixed)
│ │   ── OSC ──    ────── VCF ──────   EG    ────── VCA ──────              │ │
│ │   ▓▓▓ ▓▓▓ │ ▓▓▓ ▓▓▓ ▓▓▓ │ ▓▓▓ │ ▓▓▓ ▓▓▓ ▓▓▓                            │ │
│ │   TUNE WAVE │ CUT RES ENV │ DEC │ ACC DRV VOL                            │ │
│ │   ▶ CUTOFF   filter frequency [13 Hz – 5 kHz, log]   ↑↓ turn   …        │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
│ ┌── FILTER RESPONSE ──┐ ┌────── TRANSPORT ────────────────────────────────┐ │   ← middle row (fills remaining height)
│ │ ▒▒░░░░░░░▓▓▓░░░░░░░ │ │  ◯ idle      122 BPM    16 STEPS     50% SWING  │ │
│ │ ▒░░░░░░░▓███▓░░░░░░ │ │  ▌▌▌▌  ▌▌▌▌  ▌▌▌▌  ▌▌▌▌                        │ │
│ │ 10  100  1k    10k  │ │                                                  │ │
│ ├─── SCOPE ───────────┤ │  · Acid Tracks       ▁▂▁▁▆▁▁▁▁▂▆▂▁▁▂▁           │ │
│ │     ▲▲▼▼▲▼              … (scope) …          (preset browser)            │ │
│ │ peak: ▌▌▌▌░░                                                             │ │
│ └─────────────────────┘ └──────────────────────────────────────────────────┘ │
│ ┌────────────────────────────── SEQUENCER ──────────────────────────────────┐│  ← sequencer (8 rows, fixed)
│ │ STEP  │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │10 │11 │12 │13 │14 │15 │16 │  ││
│ │ PITCH │ ▆ │ ▆ │ ▆ │ █ │ ▇ │ ▆ │ ▆ │ ▆ │ ▆ │ ▆ │ █ │ ▆ │ ▆ │ ▆ │ ▇ │ ▆ │  ││
│ │ FLAG  │ A │ · │ · │AS │ · │ · │ A │ · │ · │ S │ A │ · │ · │ A │ S │ · │  ││
│ │ NOTE  │C2 │C2 │C2 │C2 │C3 │C2 │C2 │C2 │C2 │C2 │C3 │C2 │C2 │C2 │D2 │C2 │  ││
│ └───────────────────────────────────────────────────────────────────────────┘│
│ Tab section · ←→ select · ↑↓ semi · < > oct · a accent · s slide · m mute …│   ← context-sensitive help bar
└─────────────────────────────────────────────────────────────────────────────┘
```

Every panel except the title bar gets an **orange border** (or the theme's accent colour) when it's focused. The currently-active section is also implied by the help bar at the bottom — that line changes per-section so you don't need to remember out-of-context keys.

There are **five focusable sections**: **Knobs**, **FX**, **Sequencer**, **Drums**, **Transport**. Cycle them with `Tab` (forward) or `Shift+Tab` (backward), or **hover the mouse** into a panel to focus it directly. The ASCII above only sketches the core layout — the FX rack sits to the right of the Knobs strip and the Drums grid sits to the right of the Sequencer, so the final layout reads as two rows of two wide panels each, plus Transport on the right.

### Visual cues

- **`●  live`** (red, top-right) — playback is running.
- **`◯  idle`** (gray, top-right) — playback is stopped.
- **The brand `◆ TB-303`** flashes from orange → near-white on every downbeat (steps 1, 5, 9, 13) and fades back over the step. It's a glanceable beat indicator — you can sync a head nod to it without looking at the cells.
- **A toast** (e.g. "↻ all randomized", "✓ saved slot 3", "✓ wav → bounce.wav") replaces the play indicator for ~1.5 s after any action that doesn't have a permanent UI representation.
- **Knob meters fill bottom-up** (8 levels per cell × 5 cells = 40 vertical levels of resolution). A small bracket below the meter shows the **default** position; if the current value differs, the knob is "modified".
- **Sequencer cells** color-code: gray border = idle, **orange** = currently selected step, **red** = currently playing step. The PITCH row uses block-glyph height to show note pitch relative to the lowest in the pattern.

---

## 5. The synth voice — every knob explained

Nine controls, in left-to-right signal-flow order. The schematic header above the strip groups them by functional block: **OSC** (violet) → **VCF** (cyan) → **EG** (amber) → **VCA** (green).

### Selecting and adjusting knobs

When the **Knobs** section is focused (orange border):

| Key | What it does |
|---|---|
| `←` / `→` | Move selection left / right between knobs |
| `↑` / `↓` | Adjust the focused knob by **5%** of its range |
| `[` / `]` | Coarse adjust by **10%** of its range |
| `0` | Reset the focused knob to its **default** value |
| `w` | Toggle the WAVE knob (saw ↔ square) regardless of which knob is focused |

You can also drive any knob with the mouse — see [§17](#17-mouse-reference).

### 5.1 TUNE — oscillator pitch offset

- **Range:** ±12 semitones (one octave each way)
- **Default:** centered (0 semitones — what you read off the knob is what plays)
- **Display:** signed semitones, e.g. `+3.5 st`
- **Tech symbol:** `f₀` (fundamental frequency)

Detunes the entire instrument. Useful for matching a track's key without rewriting every step's pitch. Stay near the centre — large detune kills "in-key" playback against any other instrument.

### 5.2 WAVE — oscillator shape

- **Values:** `saw` (default) or `square`
- **Tech symbol:** `∿`

The TB-303 only has two waveforms. **Saw** is the canonical "acid" sound — bright, full of harmonics, what 95% of acid records use. **Square** is reedier, slightly hollow, and pitch-dependent in width: its duty cycle scales from ~71% at low pitches to ~45% at high pitches (the same circuit quirk the original has). The square output is also DC-centered so resonance feedback doesn't carry a DC bias that would shift pitch at high Q.

`w` toggles it from anywhere in the Knobs section; click or scroll on the WAVE column to toggle from anywhere with the mouse.

### 5.3 CUTOFF — filter frequency

- **Range:** 13 Hz – 5 kHz, **logarithmic**
- **Default:** 40% (≈ 120 Hz)
- **Display:** `Hz` below 1 kHz, `kHz` above
- **Tech symbol:** `fc`

The single most important knob on the instrument. Sets where the 4-pole low-pass filter starts cutting. Below cutoff, frequencies pass; above it, they roll off at 24 dB/octave. The defaults put cutoff low so the envelope has plenty of headroom to *sweep upward* — that upward sweep is the squelch.

The displayed number is the **base** cutoff — the actual cutoff at any given moment is `base × 2^(envmod·MEG + accent·acc_env)`, which can be many octaves higher when the envelope and accent fire together. The orange line on the **FILTER RESPONSE** panel shows the *live* cutoff after modulation.

### 5.4 RES — resonance / Q

- **Range:** 0–100% (mapped to feedback gain `k = 0..12`)
- **Default:** 75%
- **Display:** percentage
- **Tech symbol:** `Q`

The amount of feedback around the filter. At 0%, you have a flat 4-pole roll-off. As you raise it, a peak builds at the cutoff frequency, getting taller and narrower. Around **90% the filter self-oscillates** — the peak becomes a sustained sine wave at the cutoff frequency, even with no oscillator input. This is exactly where a real 303 self-oscillates (`k ≈ 10–12` for a diode ladder, vs `k = 4` for a Moog ladder).

acidflow doesn't gain-compensate the passband, so the audible volume drops as you raise resonance. That volume drop is a *feature* — it's what a real 303 does, and it's why high-resonance accents punch the way they do (the accent envelope adds gain back).

The **FILTER RESPONSE** panel visualizes Q as the height of the peak around the cutoff line.

### 5.5 ENVMOD — envelope-to-cutoff sweep range

- **Range:** 0–4 octaves of cutoff sweep
- **Default:** 65% (≈ 2.6 octaves)
- **Display:** octaves, e.g. `2.6 oct`
- **Tech symbol:** `→fc`

How much the **filter envelope** (MEG) lifts the cutoff when a note triggers. At 0 the cutoff sits still and you get a flat dub-style filter. At max the cutoff jumps a full 4 octaves on every note-on, then decays back over the **DECAY** time — the classic squelch motion.

ENVMOD is the *amount*; DECAY is the *speed*. They work together.

### 5.6 DECAY — filter envelope length

- **Range:** 0.2 – 2 seconds (logarithmic)
- **Default:** 30% (≈ 400 ms)
- **Display:** `ms` below 1 second, `s` above
- **Tech symbol:** `τ` (time constant)

How long the filter envelope (MEG) takes to decay back to zero after a note triggers. Short (left side) gives snappy, percussive squelches — the cutoff jumps and snaps back. Long (right side) gives slow filter sweeps that evolve over multiple steps.

**Important:** on **accented** notes the DECAY pot is **bypassed** in the real circuit, and acidflow does the same — accented notes always use a 200 ms decay no matter where DECAY is set. This is why accents punch and non-accents flow.

### 5.7 ACCENT — accent emphasis

- **Range:** 0–100% (controls the cap charge added per accented note)
- **Default:** 65%
- **Display:** percentage
- **Tech symbol:** `C₁₃` (the 303's accent capacitor)

How much "bump" each accented step gets. ACCENT controls **two** things simultaneously, both modeled after the 303's cap-and-resistor accent circuit:

1. **Filter cutoff bump** — adds up to ~2 octaves of cutoff on top of whatever the main envelope is doing. Cross-coupled with resonance: at low Q the accent is largely shunted away (also a real-circuit behaviour).
2. **VCA gain bump** — multiplies the amp by `(1 + accent_amt)` while the note is accented.

Critically, the accent envelope **doesn't fully discharge between notes** (it has a ~200 ms time constant on its own cap). Two consecutive accents stack; three stack more. This is why long chains of accented 16ths produce the *rising* squelch that defines 303 lines like *Acperience* and *Energy Flash*.

Per accent, the cap is bumped by `0.45 × accent_amt`. So at full ACCENT, one accent leaves 55% headroom and a second accent will come close to clipping the cap.

### 5.8 DRIVE — output saturation

- **Range:** 0–100% (maps to tanh pre-gain 1.3× – 6.0×)
- **Default:** 20%
- **Display:** percentage
- **Tech symbol:** `∫`

A post-VCA `tanh` saturator — the 303's BA662 VCA chip starts compressing before the rail; this captures the same character. The pre-gain is **compensated** by dividing by `tanh(pre_gain)` on the way out, so the perceived loudness stays roughly constant as you sweep DRIVE. Effectively this is a **texture** knob, not a volume knob.

- 0% — clean, what comes out of a 303 straight into a mixer.
- 30–50% — gentle bite, more aggressive on accents.
- 70%+ — "303 into a fuzz pedal". Punishing on accent stacks.

### 5.9 VOL — master output level

- **Range:** 0–100%
- **Default:** 65%
- **Display:** percentage
- **Tech symbol:** `VCA`

Linear gain on the final output, smoothed at 15 ms to avoid zipper noise on knob jumps. Don't park it at 100% — leave headroom for the saturator and accent boost.

---

## 6. The FX rack

A separate focusable panel, right of the Knobs strip. Seven controls, laid out left-to-right in **signal-flow order** so reading the panel is reading the signal chain:

| Idx | Label | What it does |
|---|---|---|
| 1 | **O-DRIVE** | Pre-filter tanh overdrive. 0 = clean, 1 = "303 into a fuzz pedal". Sits **before** the ladder, so it hits the filter pre-compressed and makes the envelope sweep more aggressive. |
| 2 | **DLY MIX** | Wet/dry send into the tape-style delay. 0 bypasses the wet path. |
| 3 | **DLY FB** | Delay feedback. Clamped < 1 so it can't self-oscillate into a blow-up, but high values will self-sustain until you pull the mix down. |
| 4 | **DLY TIME** | Tempo-synced division — cycles through `1/16` → `1/16d` → `1/8` → `1/8d`. Delay time is derived from the sequencer's BPM, so repeats stay locked to the beat. |
| 5 | **REV MIX** | Plate-reverb wet/dry send. 0 bypasses. |
| 6 | **REV SIZE** | Scales the plate's decay time (room → hall → cathedral). |
| 7 | **REV DAMP** | Rolls off HF inside the feedback loop so long tails darken — bright plate ↔ dark dub chamber. |

The whole rack sits on the **master bus**, after the VCA and drum sum. One signal chain:

```
303 voice + drum sum → [O-DRIVE is upstream, on the 303 voice only]
                      → delay → reverb → out
```

So the delay and reverb are shared between bass and drums. O-Drive only colours the 303 voice (and it's *pre*-filter — the saturation hits the ladder, not the output).

### Editing FX

When the **FX** section is focused (orange border):

| Key | What it does |
|---|---|
| `←` / `→` | Move selection between FX knobs |
| `↑` / `↓` | Adjust by 5% (DLY TIME steps through the four divisions) |
| `[` / `]` | Coarse adjust by 10% |
| `0` | Reset to default |
| `m` | Mute the FX bus (see [§9 transport](#9-transport-bpm-swing-length-presets) — the mute keys apply per-section) |

Mouse behaviour matches the main Knobs strip — click, vertical drag, scroll, right-click to reset.

---

## 7. The sequencer

The sequencer lives in the bottom-left panel and has **4 rows × 16 columns**:

- **STEP** — the step number (1–16). Highlights orange for the selected step, red for the playing step.
- **PITCH** — a sparkline-style block glyph whose height encodes pitch relative to the lowest note in the pattern. Quick visual read of the line's contour.
- **FLAG** — `·` plain, `A` accent only, `S` slide only, `AS` both, `R` rest.
- **NOTE** — the MIDI note as a name + octave (e.g. `C2`, `Eb3`).

### Editing steps

When the **Sequencer** section is focused:

| Key | What it does |
|---|---|
| `←` / `→` | Move selection left / right (wraps at the ends) |
| `↑` / `↓` | Transpose the selected step ±1 semitone (clamped to MIDI 12–96) |
| `<` or `,` | Down one octave |
| `>` or `.` | Up one octave |
| `c d e f g a b` | Set the step's pitch to that letter at the **current octave** |
| `a` | Toggle **accent** on this step (also clears rest if set) |
| `s` | Toggle **slide** into the next step (also clears rest if set) |
| `m` | Toggle **mute** / rest on this step |
| `x` | **Clear** this step entirely (rest, no flags, no locks) |
| `v` | Cycle **probability** — `100 → 75 → 50 → 25 → 100` |
| `j` | Cycle **ratchet** — `1 → 2 → 3 → 4 → 1` (sub-triggers per step) |
| `F` (Shift+F) | Snapshot current CUTOFF knob as a **p-lock** for this step |
| `G` (Shift+G) | Snapshot current RES knob as a p-lock |
| `H` (Shift+H) | Snapshot current ENVMOD knob as a p-lock |
| `J` (Shift+J) | Snapshot current ACCENT knob as a p-lock |
| `L` (Shift+L) | Clear **all** p-locks on this step |

### Per-step flags explained

**Accent.** Marks the step as "play me louder and brighter". Triggers the accent capacitor, which boosts both VCA gain and filter cutoff for the duration of the note. Stacks across consecutive accents — see [§5.7](#57-accent--accent-emphasis).

**Slide.** Marks the step as "*glide* from the previous note's pitch into this one over the step duration". Slide has two important details acidflow models faithfully:

1. **Slide doesn't retrigger envelopes.** In a real 303, the gate signal stays high during a slide — the previous note's envelopes keep decaying from where they were rather than restarting. acidflow does the same. Without this, sliding chains of notes would re-pluck the envelope on every step and lose the "continuously flowing" character.
2. **Slide is exponential, with τ ≈ 88 ms.** This is the actual RC time constant of the 303's portamento circuit (R ≈ 400 kΩ, C ≈ 0.22 µF). The pitch settles in roughly 4τ ≈ 350 ms regardless of the interval — so big jumps audibly *bend* into place; small jumps snap.

If the previous step is a rest, slide is suppressed for that note (you can't glide *from* nothing).

**Rest.** No note plays. The previous note's envelopes continue decaying naturally. A rest in the middle of a slide chain breaks the slide — the next non-rest note retriggers fresh.

### Probability, ratchet, and p-locks

Extensions that a real 303 doesn't have, but that modern groove-boxes do:

**Probability (`v`).** Cycles `100 → 75 → 50 → 25`. Rolled once per step at trigger time — a 50% step plays half the time. Two practical rules: probability is ignored during **WAV / MIDI export** (the file would play differently every open, which is surprising), and probability can't save a rest step (rest wins).

**Ratchet (`j`).** Cycles `1 → 2 → 3 → 4`. A ratchet of N divides the step duration into N equal sub-slices and retriggers the note on each. Ratchets preserve accent and slide (slide applies to the **last** sub-trigger only, so the chain still resolves into the next step). A ratchet of 1 is the default single hit.

**Per-step parameter locks (p-locks).** Four of the knobs — CUTOFF, RES, ENVMOD, ACCENT — can be **snapshot per step** with `F / G / H / J` respectively. When a p-lock is set, that step uses the locked value instead of the live knob position, regardless of where you've since moved the knob. Multiple locks compose (set cutoff and res together) and `L` clears every lock on the current step. Locks survive save/load. They're the quickest path to classic 303 tricks like "one step really opens the filter" without having to automate anything.

### What's between two steps

Each step is one **16th note** at the current BPM (so 4 steps = 1 beat, 16 steps = 1 bar). Step duration is `60 / BPM / 4` seconds — shuffled by SWING (see [§9](#9-transport-bpm-swing-length-presets)).

Steps are scheduled **on the audio thread** — the sequencer's step boundaries are sample-accurate, and live playback matches the offline WAV bounce to the sample. The old UI-driven scheduler had ~33 ms of jitter; that's gone.

Notes don't play when **playback is stopped** — editing is silent. To audition a change, hit `Space`.

---

## 8. The drum machine

Nine synthesized drum voices that run alongside the 303, each with its own 16-step lane. The voices are **generated**, not sampled — the engine cost per voice is a handful of envelopes plus a noise LFSR or a small oscillator bank. No samples on disk.

| Row | Voice | What it is |
|---|---|---|
| 1 | **BD** | Kick — sine with a 150→50 Hz pitch sweep, ~50 ms sweep / ~200 ms amp decay, plus a 2 ms transient click for the initial snap. |
| 2 | **SD** | Snare — two-tone body (180 + 330 Hz sines, fast decay) summed with cascaded-bandpass noise (~1.5k / 4k emphasis). |
| 3 | **CH** | Closed hat — 808-style six-square metallic oscillator (205/370/540/775/1050/1400 Hz) through a bandpass + HPF with a dash of white-noise hiss. Tight ~25 ms env. |
| 4 | **OH** | Open hat — same metallic oscillator as CH but with a ~320 ms envelope. |
| 5 | **CL** | 808-style clap — 3 fast noise bursts spaced ~12 ms apart, plus a ~280 ms bandpassed (~1.3 kHz) dense-noise tail. |
| 6 | **LT** | Low tom — 80 Hz sine with a narrow pitch sweep (110→80 Hz) and a 300 ms amp decay. Pitched percussion. |
| 7 | **HT** | High tom — 180 Hz sine, 230→180 Hz sweep, 220 ms amp decay. |
| 8 | **RS** | Rimshot — 1.65 kHz + 2.2 kHz sines plus a bandpassed noise burst, 18 ms decay. Sharp transient click. |
| 9 | **CB** | 808 cowbell — two detuned squares (540 + 800 Hz) through a bandpass, 180 ms decay. Classic metallic "clonk". |

All nine voices sum into a **master drum bus** that feeds the FX chain (delay + reverb) alongside the 303 voice. Per-voice gains are baked to sit right when the bass is at defaults; a global drum-bus knob `[ / ]` scales everything.

### Editing the grid

When the **Drums** section is focused:

| Key | What it does |
|---|---|
| `←` / `→` | Move step selection left / right |
| `↑` / `↓` | Move voice selection up / down (BD → SD → CH → OH → CL) |
| `Space` / `x` | Toggle a hit at the selected cell |
| `1` … `9`, `0` | Quick-toggle a hit on steps 1 … 10 of the **currently-selected voice row** (great for drawing a kick or hi-hat pattern without chasing the cursor) |
| `[` / `]` | Drum-bus master gain −/+ (reads out at the bottom of the panel) |
| `c` | Clear every hit on the **current voice** row |
| `C` (Shift+C) | Clear the entire kit |
| `m` | Mute the drum bus (independent of the 303 voice — `m` in the Knobs / FX section mutes the synth instead) |

The grid shows every row at once. The currently-playing column pulses red; the selected cell gets an accent-coloured glyph. Voice rows are colour-coded (kick in hot red, snare in accent, hats in cyan, clap in red-dim) so the pattern reads by timbre at a glance.

Drums are stored inside the pattern slot — save to slot 3 and you save the 303 pattern, the drum grid, and the master gain. Loading slot 3 restores all three.

---

## 9. Transport: BPM, swing, length, presets

The **Transport** panel on the right of the middle row shows playback state and the preset browser. When focused:

| Key | What it does |
|---|---|
| `Space` | Toggle play / pause (also works from anywhere) |
| `↑` / `↓` | Previous / next preset (wraps both ways) |
| `,` / `.` | Same as `↑` / `↓` (no shift required) |
| `[` / `]` | BPM −2 / +2, clamped to 40–220 |
| `{` / `}` | Pattern length −1 / +1, clamped to 4–16 steps |
| `-` / `=` | Swing −2% / +2%, clamped to 50% (straight) – 75% (hard shuffle) |
| `m` | Mute the entire playback bus (status line retains the transport state, just silent) |

The status line also surfaces mode badges when active: **SONG N** when song mode is chaining slots, **JAM Oct3 ♩A C4** when jam mode is live, and **MIDI OUT / MIDI SYNC** when the MIDI bridge is enabled. They're drawn right of the BPM/swing display so the panel stays quiet in the default case.

### BPM

The default is **122**, which lands in the middle of canonical acid tempos (Phuture's *Acid Tracks* at 120, Adonis's *No Way Back* at 122, Josh Wink's *Higher State of Consciousness* at 124). 128+ pushes into acid techno (Beltram, Hawtin); 132+ is hardcore territory.

### Swing

acidflow's swing is the standard MPC-style 16th-note shuffle: even-indexed steps (0, 2, 4, …) play **on the grid**, odd-indexed steps are **delayed** by `(swing − 0.5) × pair_duration`. Each pair still sums to two straight 16ths, so the pattern stays locked to the bar — only the *feel* changes:

| Swing | Feel |
|---|---|
| 50% | straight 16ths (default — the canonical 303 feel) |
| 56–58% | gentle MPC swing |
| 60–62% | classic MPC-60 / MPC-3000 swing — what most hip-hop / house records use |
| 66% | hard shuffle, almost a triplet feel |
| 75% | maximum — close to swung 8th + 16th |

Swing applies uniformly across all steps. There's no per-step swing offset.

### Pattern length

The sequencer always has **16 slots** internally. Setting pattern length to e.g. 12 plays steps 1–12 in a loop and ignores 13–16 (they still appear in the editor — they're just skipped). This is how you get odd-meter patterns like 3/4 (length=12) or 7/8 (length=14).

### Preset browser

The preset list shows all 16 built-in presets with a 16-glyph **contour preview** alongside each name — a sparkline of that preset's pitches that updates as you edit. The currently-loaded preset is marked.

Loading a preset overwrites the current pattern entirely. **Save first** if you've made edits worth keeping (see [§15](#15-saving-loading-exporting)).

If you've made edits to a preset's pattern, the preset index is marked as "custom" (`-1` internally) — the displayed name will still be the preset you started from, but acidflow knows it's been modified.

---

## 10. Song mode

Press `n` anywhere to toggle song mode. When on, the sequencer **auto-advances through saved pattern slots** on every pattern wrap — the classic hardware "chain" mode. The transport's status line shows `♪ SONG N` where N is the currently-playing slot.

How it picks the next slot:

- Start at the currently-loaded slot (if you loaded from a slot before enabling song mode) or the lowest saved slot otherwise.
- On each pattern wrap (step N loops back to step 1), jump to the next saved slot 1–9 (cyclically). If only one slot is saved, song mode just re-loads that slot on every wrap (harmless — no perceptible glitch because the loaded pattern is identical).
- Empty slots are skipped: if slots 1, 3, and 5 are saved, the chain is 1 → 3 → 5 → 1 → …

You don't pre-author a chain — the chain *is* the set of saved slots, in order. To change the chain, save / erase slots. It's fast and intentionally dumb.

Save pattern versions to slots 1..9 (`Shift+1 … Shift+9`) before enabling song mode, otherwise there's nothing to chain through.

---

## 11. Jam mode

Press `k` anywhere to toggle jam mode. When on:

- The sequencer **stops producing** the bass line (drums still play if you're running a drum pattern).
- The computer keyboard turns into a **two-octave tracker-layout piano**, each press retriggering the 303 voice at that pitch.

Keys are the standard Renoise / Fasttracker layout:

| Row | C | C♯ | D | D♯ | E | F | F♯ | G | G♯ | A | A♯ | B |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Lower (current octave) | `z` | `s` | `x` | `d` | `c` | `v` | `g` | `b` | `h` | `n` | `j` | `m` |
| Upper (current octave + 1) | `q` | `2` | `w` | `3` | `e` | `r` | `5` | `t` | `6` | `y` | `7` | `u` |

| Key | Action |
|---|---|
| `←` / `→` | Octave down / up (current octave shown in the status line as `Oct3`) |
| `'` | Toggle **accent** for subsequent presses |
| `;` | Toggle **slide** for subsequent presses |
| `k` or `Esc` | Leave jam mode |

Terminals don't reliably deliver key-*release* events, so each press arms a short note gate (~250 ms) that auto-releases unless you press another key. This works for legato-ish phrases but isn't going to feel like a real keyboard — jam mode is for sketching lines to paste into the sequencer, not for performance.

---

## 12. MIDI I/O

MIDI is currently **Linux-only** (ALSA sequencer). On macOS and Windows the UI keys still work, but the backend is a stub and nothing gets forwarded. The ALSA backend creates one sequencer client with a single input and single output port — connect them with `aconnect` or your DAW.

| Key | What it does |
|---|---|
| `O` (Shift+O) | Toggle **MIDI out** — forwards bass note-on/offs, drum hits on channel 10, and clock ticks when playing |
| `I` (Shift+I) | Toggle **MIDI sync in** — slave our BPM and transport to external `MIDI Clock` + `Start` / `Stop` messages |

Note-in (an external controller → the bass voice) is always on when the backend is up; you don't need to enable anything extra to play the 303 from an external keyboard.

### What gets sent when MIDI out is on

- **Bass voice** — note-on with velocity 112 (accent) or 80 (non-accent), note-off at step end. Slides are rendered as a small note overlap so DAWs interpret them as legato / portamento.
- **Drum hits** — channel 10, one note-on per hit, General MIDI kit mapping (BD = 36, SD = 38, CH = 42, OH = 46, CL = 39).
- **MIDI clock** — 24 PPQN clock ticks at the current BPM, plus `Start` when you hit `Space` and `Stop` when you pause.

### What gets received when MIDI sync in is on

- **Clock ticks** average into a smoothed tempo estimate. The transport BPM readout floats toward the external tempo rather than jumping to it — this keeps the display readable even when the incoming clock jitters.
- **Start** / **Stop** drive the transport. You don't have to press `Space`.

Enabling sync in does **not** force MIDI out on — you can slave to a clock without broadcasting anything. And you can have MIDI out on without sync in (internal tempo, exporting to other gear).

The backend is a zero-overhead path when both toggles are off — no allocations, no thread activity.

---

## 13. Themes

Five palettes. Press `T` (Shift+T) to cycle them — the change is immediate and applies to every panel, border, glyph, and status light.

| Theme | Feel |
|---|---|
| **classic** | 303-silver + Roland orange on near-black (default) |
| **cyber** | hot pink + mint cyan on deep ink |
| **moss** | chartreuse + amber on forest green |
| **ice** | ice-blue + coral on steel |
| **mono** | pure greyscale — for screenshots or "I'm done looking at colours" |

Theme choice is not persisted across runs — every launch starts on **classic**.

---

## 14. Randomization & evolving mutation

Two keys for randomization (`r`, `R`), one for mutation (`M`).

### `r` — randomize the focused section

If the **Knobs** section is focused, `r` rolls a new patch. If **FX** is focused, `r` rolls a new FX preset. If **Sequencer** is focused, `r` rolls a new pattern. If **Drums** is focused, `r` rolls a new drum pattern. If **Transport** is focused, `r` rolls a new BPM / swing / length. The currently-focused area is what gets randomized.

acidflow's randomizer is **biased**, not flat. Naive uniform randomization across all parameters produces muddy, inaudible, or flat-out boring patches ~80% of the time. So instead:

**Knob randomization** picks one of four **archetypes** at random, each pre-baked with sensible inter-knob relationships:

| Archetype | Character |
|---|---|
| **Classic** | Mid cutoff, strong Q, moderate sweep, medium decay — the universal acid starting point. |
| **Squelch** | Self-osc territory: very high Q, low cutoff (max headroom for the envelope), tight decay, heavy accent. |
| **Driving** | Open filter, less envelope motion, medium resonance — the *Energy Flash* engine sound. |
| **Dubby** | Low cutoff, long decay, moderate Q — Plastikman / dub techno. Sustained filter motion over bars. |

…then jitters within that archetype's ranges. Wave is `square` ~30% of the time (saw is canonical 303). The toast tells you which archetype was rolled.

**Pattern randomization** picks one of four **groove archetypes**:

| Archetype | Character |
|---|---|
| **Pedal** | Sparse, low jump rate, heavy on the root (most acid records) |
| **Driving** | Dense, every step plays, more accents (Beltram-style) |
| **Melodic** | High jump rate, more colour tones, more slides (FSOL / Stakker) |
| **Dub** | Very sparse — most steps rest, long decays do the work (Plastikman) |

Pitches are drawn from a weighted **minor pentatonic + ♭7 + octave + octave-5** above C2 (the canonical 303 vocabulary heard on *Voodoo Ray*, *Acperience*, etc.). Constraints baked in:

- **Downbeats** (steps 1, 5, 9, 13) favour the tonic so the groove lands.
- **At least one accent per bar** is guaranteed — flat patterns sound dead.
- **Slides only survive** if the previous step actually played (you can't glide into a note from a rest).

### `R` — randomize everything

Capital R. Re-rolls knobs **and** pattern in a single shot. One combined toast (`↻ all randomized`). Good for "give me something new to react to" energy.

### `M` — evolving mutation

Unlike `r` / `R`, which replace what you have, `M` (Shift+M) performs a **single small change** and leaves the rest of the pattern alone. Press it N times and the pattern "walks" — recognisably descended from where it started, but different. A weighted pick from:

| Mutation | Roughly |
|---|---|
| Toggle accent on a random non-rest step | 28% |
| Toggle slide on a random non-rest step (where the next step plays) | 24% |
| Nudge one step's pitch by ±1 or ±2 semitones | 20% |
| Jump one step ±12 semitones (octave hop) | 12% |
| Toggle rest on a random step (never step 1 — that's the anchor) | 10% |
| Rotate the whole pattern left or right by one step | 6% |

Because the menu is biased toward *vibe* changes (accents and slides) over pitch churn, repeated presses keep the melodic spine intact. Great for "I like this but I want one more variation".

### Why a randomizer at all?

Two reasons. First, the 303 famously *can't* be played by humans the way other synths can — its UI is a 16-step pitch grid, not a keyboard. The whole tradition of "acid" is *finding* riffs by mucking with parameters until something good comes out. The randomizer is just a faster way to "muck with parameters". Second, every randomization is musical *enough* to be a starting point — you'll edit knobs and steps from there.

---

## 15. Saving, loading, exporting

acidflow stores user data under `~/.config/acidflow/` (or `$XDG_CONFIG_HOME/acidflow/`). The directory is created the first time anything is written.

### Pattern slots

| Key | Action |
|---|---|
| `Shift+1` … `Shift+9` | Save current pattern + BPM + length to slot N |
| `1` … `9` | Load pattern from slot N |

A slot file looks like this (`~/.config/acidflow/pattern_3.txt`):

```
# acidflow pattern v1
16 122
36 0 1 0
36 0 0 0
36 0 0 0
36 0 1 1
…
```

The format versions up over time — current slots carry the 303 pattern, per-step probability / ratchet / p-locks, and the drum grid + master gain. Older-format files still load (missing fields default sensibly), and newer acidflows will rewrite the file in the current format on next save.

The keybinds use the **shifted top-row digits** (`! @ # $ % ^ & * (` on a US keyboard) for save. acidflow accepts both forms — whatever your terminal sends as the shifted digit will save.

**Slot 0** is reserved internally; users address slots 1–9.

Slot operations save **pattern + drums + BPM + length + swing + p-locks**. They don't save knob positions or FX — knobs are part of your live state, not the pattern.

### WAV export (`e`)

Press `e` from anywhere. acidflow:

1. Pauses the live audio engine (it'd race the offline render otherwise).
2. Renders 4 loops of the current pattern at the current BPM into `~/.config/acidflow/bounce.wav` — 44.1 kHz, 16-bit PCM, **mono**.
3. Restarts the live audio engine.
4. Resumes playback if you were playing.

You'll see a `✓ wav → bounce.wav` toast on success or `✗ wav failed` on disk error. The export uses the **same DSP** as live playback — same filter, same envelope, same saturation — but runs synchronously so it can't drop frames.

The 4-loop count is fixed. If you need more, run the export multiple times into different files (rename between exports) or concatenate after the fact.

### MIDI export (`E`)

Press `E` (Shift+E) to write a **Standard MIDI File (type 0)** to `~/.config/acidflow/bounce.mid`. One loop of the pattern is exported — a MIDI file loops natively in any DAW, unlike a WAV.

Translation rules:

- **Accent** → velocity 112, non-accent → 80.
- **Slide** → an 8-tick overlap with the next step's note-on (DAWs interpret this as legato / portamento).
- **Ratchet** → distinct sub-notes inside the step so each hit is a separate note event (matching what the audio engine does).
- **Probability** is **ignored** in export (flat 100%). Exporting a file that plays different notes every time you open it would be surprising.
- **Swing** is preserved — 480 PPQN with each pair of 16ths split `(240·ratio, 240·(1−ratio))` to match the audio-thread math.
- Drums are **not** exported yet — only the 303 voice.

### Shareable text export / import (`p` / `P`)

Press `p` to write the current pattern to `~/.config/acidflow/pattern.txt`. Press `P` (Shift+P) to read it back. Same format as slot files — human-readable, one line per step, trivial to paste into chat or diff between variations. A quick way to share a pattern: `cat pattern.txt`, copy-paste, the receiver puts it in `~/.config/acidflow/pattern.txt` and hits `P`.

The text slot is a single file, so it's overwritten on every `p`. For persistent archival, use `Shift+N` save slots or copy `pattern.txt` out to another name.

---

## 16. The 16 built-in presets

These are **plausible reconstructions** of canonical acid tracks — not lifted MIDI. The original sequencer data for most of these isn't publicly documented; the patterns here come from community consensus (Attack Magazine tutorials, KVR / Gearspace mega-threads, Robin Whittle's analysis at firstpr.com.au) and aim to evoke each track when paired with the right knob settings.

| # | Name | Inspired by | Notes for getting the sound |
|---|---|---|---|
| 1 | **Acid Tracks** | Phuture, *Acid Tracks* (1987) | Pedal point on C with two octave-ups on accented slides. The song's identity is the cutoff sweep — slowly raise CUTOFF over many bars. |
| 2 | **Higher State** | Josh Wink, *Higher State of Consciousness* | Pattern is deliberately simple — open the filter slowly across 32+ bars. Long DECAY helps. |
| 3 | **Acperience** | Hardfloor, *Acperience 1* | A-minor, heavy accents, octave-plus slides back to root for the signature Hardfloor scream. Try high RES + short DECAY. |
| 4 | **No Way Back** | Adonis, *No Way Back* | Early Chicago acid. Call-and-response between C root and upper Eb / F — first 303 line with a real *riff* shape. |
| 5 | **Voodoo Ray** | A Guy Called Gerald, *Voodoo Ray* | (Actually an MC-202, but the line translates.) A-minor pentatonic, open filter, "sung" rather than squelchy — drop RES below 50%. |
| 6 | **Energy Flash** | Joey Beltram, *Energy Flash* | Relentless 16ths on E1. A rave engine, not a melody. Heavy DRIVE. |
| 7 | **Plastikman** | Richie Hawtin / Plastikman | Sparse, high-resonance, long-decay minimalism. Space and filter motion carry the groove. |
| 8 | **Hoover Dub** | Generic dub-techno hoover bass | Extremely sparse, long decay, subsonic. The performance is filter motion over many bars. |
| 9 | **Squelch** | (Showcase) | Heavy-resonance demo. Single-octave jumps stress the accent envelope stack — turn ACCENT up. |
| 10 | **Detroit** | Underground Resistance / Detroit techno | Rolling A-minor bassline. Even, 16th-note feel. |
| 11 | **Dub 303** | Dub techno bass | Half-time feel, extremely spacey. Try BPM 60 and let it run. |
| 12 | **Liquid** | (Showcase) | Wall-to-wall slides — every step glides into the next. A demo of the slide-doesn't-retrigger envelope behaviour. |
| 13 | **Humanoid** | Stakker / Humanoid | Busy, FSOL-era octave hops every step. |
| 14 | **Melodic** | (Showcase) | Longer phrased line, pseudo-arpeggio through A-minor. |
| 15 | **Bassline** | UK bassline | Syncopated, swung — bump SWING to 60%. |
| 16 | **Empty** | (Blank slate) | All rests. Build something from scratch. |

Loading a preset overwrites the current pattern. The preset name stays in the transport panel until you edit a step, at which point the indicator flips to "custom" and the preset list deselects.

---

## 17. Mouse reference

acidflow has full mouse support. SGR mouse mode is enabled at startup; everything in the workspace is hit-tested by row.

### Title bar

| Action | Effect |
|---|---|
| Left click | Toggle play / pause |
| Scroll wheel | Nudge BPM ±1 |

### Knobs

| Action | Effect |
|---|---|
| Left click | Focus the knob (also focuses the Knobs section) |
| Left drag, vertical | Adjust the value — up = increase, down = decrease, ~2% per row |
| Scroll wheel | Adjust by ±5% |
| Right click | Reset to default |

The drag is **captured** — once you start dragging, your cursor can leave the knob's column and the drag continues until you release the button. This is how every other slider on every other UI works.

The WAVE knob is special: any click or scroll on it just toggles between saw and square (no drag).

### Sequencer steps

| Action | Effect |
|---|---|
| Left click | Select the step (also focuses the Sequencer section) |
| Scroll wheel | Transpose ±1 semitone |
| Right click | Toggle rest (mute) |

There's no drag-to-paint on the sequencer — clicking a step selects it, then keyboard takes over for editing.

### Transport area

Right ~3/5 of the middle row is a **scroll-to-browse** zone for the preset list:

| Action | Effect |
|---|---|
| Left click | Focus the Transport section |
| Scroll wheel | Browse presets (scroll-down advances the list) |

There's no per-row hit-test on individual presets — the area is treated as one big browse zone because the transport panel's exact layout reflows with terminal size.

### Help overlay

When the help overlay is open (`?`), mouse input is **ignored** — close it first (`?` or `Esc`) before clicking around.

---

## 18. Complete keyboard reference

This is the full canonical list, also accessible inside the app via `?`.

### Global (work from any section)

| Key | Action |
|---|---|
| `Tab` | Cycle focus forward (Knobs → FX → Sequencer → Drums → Transport → Knobs …) |
| `Shift+Tab` | Cycle focus backward |
| `Space` | Play / pause |
| `r` | Randomize current section |
| `R` | Randomize everything (knobs + pattern) |
| `M` (Shift+M) | Evolve pattern — one small mutation per press |
| `T` (Shift+T) | Cycle colour theme (classic / cyber / moss / ice / mono) |
| `n` | Toggle **song mode** (chain saved slots on pattern wrap) |
| `k` | Toggle **jam mode** (two-octave live keyboard into the 303 voice) |
| `O` (Shift+O) | Toggle **MIDI out** (notes + drums + clock) |
| `I` (Shift+I) | Toggle **MIDI sync in** (slave to external clock) |
| `e` | Export current pattern as WAV (4 loops) |
| `E` (Shift+E) | Export current pattern as MIDI `.mid` (one loop) |
| `p` | Export pattern as shareable text (`pattern.txt`) |
| `P` (Shift+P) | Import pattern from `pattern.txt` |
| `Shift+1` … `Shift+9` | Save pattern to slot 1–9 |
| `1` … `9` | Load pattern from slot 1–9 (Sequencer / Drums consume these for their own uses, so load from any other section) |
| `?` | Toggle keyboard reference overlay |
| `q` / `Q` / `Esc` | Quit (closes help overlay first if open) |

### When **Knobs** is focused

| Key | Action |
|---|---|
| `←` / `→` | Select previous / next knob |
| `↑` / `↓` | Adjust selected knob ±5% |
| `[` / `]` | Coarse adjust ±10% |
| `0` | Reset selected knob to default |
| `w` | Toggle WAVE (saw ↔ square) |
| `m` | Mute the 303 voice (toggle) |

### When **FX** is focused

| Key | Action |
|---|---|
| `←` / `→` | Select previous / next FX knob |
| `↑` / `↓` | Adjust (DLY TIME steps through the four divisions) |
| `[` / `]` | Coarse adjust |
| `0` | Reset FX knob |
| `m` | Mute the FX bus |

### When **Sequencer** is focused

| Key | Action |
|---|---|
| `←` / `→` | Select previous / next step (wraps) |
| `↑` / `↓` | Transpose step ±1 semitone |
| `<` or `,` | Octave down |
| `>` or `.` | Octave up |
| `c d e f g a b` | Set step pitch (root) at the current octave |
| `a` | Toggle accent (clears rest if set) |
| `s` | Toggle slide (clears rest if set) |
| `m` | Toggle mute / rest |
| `v` | Cycle probability (100/75/50/25) |
| `j` | Cycle ratchet (1/2/3/4) |
| `F` (Shift+F) | P-lock cutoff (snapshot current knob) |
| `G` (Shift+G) | P-lock resonance |
| `H` (Shift+H) | P-lock env-mod |
| `J` (Shift+J) | P-lock accent amount |
| `L` (Shift+L) | Clear all p-locks on step |
| `x` | Clear step (rest, no flags, no locks) |

### When **Drums** is focused

| Key | Action |
|---|---|
| `←` / `→` | Select step |
| `↑` / `↓` | Select voice (BD / SD / CH / OH / CL) |
| `Space` / `x` | Toggle hit on selected cell |
| `1` … `9`, `0` | Quick-toggle step 1..10 on current voice |
| `[` / `]` | Drum-bus master gain −/+ |
| `c` | Clear current voice row |
| `C` (Shift+C) | Clear entire kit |
| `m` | Mute drum bus |

### When **Transport** is focused

| Key | Action |
|---|---|
| `↑` / `↓` | Previous / next preset |
| `,` / `.` | Previous / next preset (alt) |
| `[` / `]` | BPM ±2 (clamped 40–220) |
| `{` / `}` | Pattern length ±1 (clamped 4–16) |
| `-` / `=` | Swing ±2% (clamped 50–75) |
| `m` | Mute playback bus |

### Jam mode keys

See [§11](#11-jam-mode) for the full layout. While jam mode is on, the tracker piano claims most letter keys — leave jam mode (`k` or `Esc`) to go back to the regular section keybinds.

### Help overlay

When open, **only** `?` and `Esc` close it. Everything else is a no-op until you close it. The overlay **scrolls** — use the mouse wheel over the modal to read entries that fall off the bottom on short terminals.

---

## 19. How acidflow makes sound (DSP internals)

This section describes what's actually happening inside `src/engine.cpp` when you press play. Skip it if you only care about using the instrument.

### The signal chain

```
PolyBLEP osc (saw/sq, drift ±3¢)
  └─► [2× oversample]
      └─► ZDF/TPT 4-pole ladder filter (per-stage tanh, fb tanh)
          └─► VCA (VEG × accent × master)
              └─► tanh saturator (DRIVE)
                  └─► 16 Hz HPF
                      └─► out
```

Mono float32 at the platform's mix rate (44.1 kHz on ALSA / CoreAudio, usually 48 kHz on WASAPI).

### Oscillator

A single anti-aliased oscillator. Two waveforms:

- **Saw**: naive `2·phase − 1` ramp minus PolyBLEP correction at the wrap. PolyBLEP is a 2-sample band-limited step that cancels most of the aliasing the naive saw would produce.
- **Square**: pulse with pitch-dependent duty cycle (71% at low pitches, 45% at high — the same circuit characteristic the original has). PolyBLEP is added on the rising edge and subtracted on the falling edge. The square is **DC-centered** by subtracting `2·duty − 1` from the output, so resonance feedback doesn't carry a DC bias that would shift pitch at high Q.

**VCO drift.** Slow filtered noise (1-pole LPF at 0.4 Hz) scaled to ±3 cents and applied as a V/oct offset on every sample. Without this, long sustained notes sound "computer-perfect" — adding drift makes the synth feel analog without being inaccurate.

### Filter

A **4-pole low-pass diode ladder**, modelled with the **Zero-Delay Feedback / Topology-Preserving Transform** technique (Zavalishin's *Art of VA Filter Design*). The integrators use the trapezoidal rule (TPT). Pre-warped integrator gain `g = tan(π·fc/sr)`.

Each stage has a gentle `tanh(y · 1.05) × 0.95` after the integration — this models the diode-junction compression of the real ladder and is what gives 303s their *creamy* (not brittle) high-resonance squelch.

The **global feedback** path is `tanh(input − k·output)`. Resonance maps the user's RES knob to `k = 0..12`. A diode ladder self-oscillates around `k ≈ 10–12` (vs `k = 4` for a Moog ladder), which means the top of the RES knob hits actual self-oscillation rather than just being a tall-but-stable peak.

The implicit feedback equation is solved with a **2-iteration fixed-point** seeded by the previous output. Two iterations is enough to be indistinguishable from Newton-Raphson at the mild non-linearity of `tanh` — and stable through self-oscillation.

**No passband gain compensation.** The volume drop at high resonance is intentional and is part of the sound.

### Oversampling

The whole non-linear section (filter + saturator) runs at **2× the audio rate**. Two sub-samples are produced per output sample by linear interpolation between consecutive oscillator outputs, each is run through the filter, and the two results are averaged for decimation. This halves the high-Q aliasing that would otherwise grate on accented notes.

### Envelopes

Three envelopes, all exponential:

| Envelope | What it modulates | Time constant |
|---|---|---|
| **MEG** (filter envelope) | Cutoff (via ENVMOD knob) | 0.2–2 s via DECAY knob; **forced 200 ms on accents** |
| **Accent cap** | Cutoff (via ACCENT knob) and VCA gain | Fixed ~200 ms — does **not** fully discharge between notes (this is what makes accents *stack*) |
| **VEG** (amp envelope) | VCA gain | ~3 s while gate is held; ~8 ms snap-off when gate releases. 3 ms linear attack ramp on note-on so hard gates don't click. |

The accent cap is the secret of the 303 sound. Each accented note adds `0.45 × accent_amt` to the cap's charge (and clamps at 1.0). Because the cap has its own slow decay, two consecutive accents overlap — the cap stays high. Three or four accents back-to-back rise into a sustained squelch. This is the *Acperience* / *Energy Flash* envelope motion.

### Cutoff modulation

The actual filter cutoff at any sample is:

```
fc = base_cutoff × 2^(envmod·MEG + accent_max·acc_coup·acc_env)
```

Note this is **exponential** (V/oct), not linear in Hz — V/oct is what makes the filter *scream* up an octave on every accent rather than plodding up by a fixed Hz amount. `envmod·MEG` rides the user's ENVMOD knob (0–4 octaves max). `accent_max` is fixed at 2 octaves; `acc_coup = 0.5 + 0.5·res_knob` — so at low resonance the accent contribution is shunted (matches the stock 303 schematic).

Cutoff is clamped to `[20 Hz, min(10 kHz, 0.45·sr)]` so the TPT pre-warp doesn't blow up near Nyquist of the 2× oversampled rate.

### Slide

When a slide note triggers, the **previous note's gate stays high** — envelopes don't retrigger, and only the pitch CV glides exponentially toward the new target with a single-pole LPF coefficient `1 − exp(−dt/0.088)`. τ = 88 ms is roughly the RC of the real 303's slide circuit (R ≈ 400 kΩ, C ≈ 0.22 µF).

### VCA & saturator

`amp = VEG × (1 + accent_amt if accented) × smooth(master)`. After the VCA, a `tanh(y · drive_pre) × drive_norm` saturator with `drive_pre = 1.3 + DRIVE·4.7` (range 1.3–6.0×) and `drive_norm = 1 / tanh(drive_pre)` for loudness compensation.

Final stage: a 16 Hz one-pole HPF that matches the 303's output-coupling cap and removes any DC drift from the filter at very low cutoffs.

### Parameter smoothing

CUTOFF, RES, and VOL are one-pole smoothed at a 15 ms time constant, per-sample. This kills the audible "zipper" you'd otherwise get from discrete (keyboard) knob steps. 15 ms is short enough to feel instant under the finger and long enough to smear a 5% knob jump into inaudibility.

### Render block size

The audio thread renders in 256-frame blocks (~5–6 ms at 44.1 kHz). This is the latency budget.

---

## 20. Architecture & threading model

```
┌─────────────────────────────────────────────────────────────────┐
│                       UI THREAD (main.cpp)                      │
│                                                                 │
│  Event handler ──► writes std::atomic params (relaxed)          │
│                                                                 │
│  Render @ 30 FPS ──► snapshots scope ring, peak, live fc        │
│                  ──► fires note triggers (atomic seq counter)   │
│                                                                 │
└──────────────────┬──────────────────────────────────────────────┘
                   │
                   │  (lock-free, allocation-free)
                   │
┌──────────────────▼──────────────────────────────────────────────┐
│                       AUDIO THREAD                              │
│                       (audio_alsa / coreaudio / wasapi.cpp)     │
│                                                                 │
│  acid::render(buf, 256) ──► writes 256 mono float32 samples     │
│                                                                 │
│  Block-rate: snapshot atomic params; one-pole smooth per-sample │
│  Per-sample: osc → 2× → ladder → VCA → saturator → HPF → out    │
│                                                                 │
│  Write to ALSA / CoreAudio / WASAPI (blocking)                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Lock-free SPSC

Every cross-thread channel is `std::atomic` only. There are **no mutexes** and **no allocations** anywhere on the audio path.

**Parameter updates** are relaxed-store atomics — the audio thread reads them once per render block (256 frames) and one-pole smooths per-sample. Tearing on a multi-byte parameter would be inaudible and would heal in 15 ms anyway.

**Note triggers** use a single `std::atomic<uint64_t>` sequence counter. The UI:

1. Stores `freq`, `flags`, `slide_time` into relaxed atomics.
2. Bumps the seq counter with **release** ordering.

The audio thread:

1. Loads the seq counter with **acquire** ordering at the top of each render.
2. If it changed, reads the relaxed atomics — they're guaranteed visible because of the release/acquire pair.

This pattern is wait-free SPSC, scales to one note-on per render block (overkill — you'll never trigger faster than one per 16th-note step), and never blocks either thread.

### Scope ring

Every output sample is written to a 4096-entry ring buffer with a relaxed atomic write index. The UI samples the most-recent N samples each frame for the oscilloscope. Reads can race writes at the boundary — the result is visually invisible.

### Platform abstraction

`engine.cpp` is platform-agnostic. The three audio backends are swapped at CMake-configure time based on the platform:

- **ALSA** (`audio_alsa.cpp`) — Linux. Spawns a worker `std::thread`, opens `default` PCM device, writes blocking via `snd_pcm_writei`.
- **CoreAudio** (`audio_coreaudio.cpp`) — macOS. AudioQueue-based. Pull callback fills buffers from the engine.
- **WASAPI** (`audio_wasapi.cpp`) — Windows. Shared-mode audio client; usually forced to 48 kHz by the OS mixer.

The shared interface is the C-style API in `src/audio.hpp` — `acid_start()`, `acid_stop()`, parameter setters, note triggers, scope tap, peak meter, live-fc readout, WAV render.

### TUI

[`maya`](https://github.com/1ay1/maya) handles rendering, layout (flexbox-style DSL), input events (keyboard / mouse / resize), and the 30 FPS event loop. acidflow contributes its own widget set under `src/tb303/`:

- `knob.hpp` — vertical 8-row × 5-cell meter with label, value, tech symbol, default marker.
- `sequencer.hpp` — 16-column grid with box borders, pitch sparkline, flag glyphs.
- `scope.hpp` — Braille-cell oscilloscope with zero-crossing trigger and peak meter.
- `filter_response.hpp` — log-frequency heatmap of the live filter response with a moving cutoff bar.
- `transport.hpp` — playback status, BPM / swing / length displays, preset browser.
- `waveform_toggle.hpp` — saw / square button.
- `help_bar.hpp` — context-sensitive footer.
- `help_overlay.hpp` — full keyboard reference (modal).
- `theme.hpp` — shared color palette (orange-accent on near-black).

Every widget is a function returning a `maya::Element`. Per-frame, `render_frame()` builds the entire tree from scratch and hands it to `maya` — no manual diffing or redraw logic.

---

## 21. Building from source

### Standard build (Linux / macOS)

```bash
cmake -B build
cmake --build build -j
```

The default build type is `Release` with `-O3 -march=native -funroll-loops` on GCC/Clang, `/O2` on MSVC. The optimization level matters — Debug builds will use measurably more CPU per audio block.

### Windows build

Windows uses a multi-config generator (Visual Studio), so you need to pass `--config` explicitly and launch the binary from the per-config output directory:

```bat
cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
.\build\Release\acidflow.exe
```

Two MSVC-only knobs live in `CMakeLists.txt`:

- **C++23 fallback.** Current MSVC (VS 2026 / 19.50) doesn't advertise the `cxx_std_26` language dialect to CMake, so the script drops to C++23 when `MSVC` is defined. GCC/Clang builds remain on C++26. Nothing in the codebase currently requires a C++26-only feature.
- **`_USE_MATH_DEFINES` + `/utf-8`.** The former exposes `M_PI` (MSVC hides it behind that macro by default); the latter tells `cl.exe` the source is UTF-8 so the Unicode glyphs in `maya/style/border.hpp` don't trigger C4566 warnings.

Run the executable in a terminal that supports ANSI escapes and UTF-8 — Windows Terminal, WezTerm, Alacritty, or similar. The legacy `cmd.exe` console lacks both and will render garbage.

### Debug build

```bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j
```

Useful if you want to step through the DSP. Audio will still play; you'll just spend more cycles per sample.

### Where the audio backend gets picked

`CMakeLists.txt` checks `APPLE`, `WIN32`, `UNIX` in that order:

```cmake
if(APPLE)        list(APPEND ACIDFLOW_SOURCES src/audio_coreaudio.cpp)
elseif(WIN32)    list(APPEND ACIDFLOW_SOURCES src/audio_wasapi.cpp)
elseif(UNIX)     list(APPEND ACIDFLOW_SOURCES src/audio_alsa.cpp)
endif()
```

There's no override flag. If you want to force a different backend (e.g. PulseAudio or PipeWire on Linux), patch the CMake or replace `src/audio_alsa.cpp` with your own that implements the C interface in `src/audio.hpp`.

### Linking maya

acidflow expects a sibling `../maya` checkout and pulls it in via `add_subdirectory()`. If you'd prefer `FetchContent`, swap that line in `CMakeLists.txt`.

### `compile_commands.json`

`CMAKE_EXPORT_COMPILE_COMMANDS` is on by default, so `clangd`, IDEs, and `bear` will all see the full compile flags from `build/compile_commands.json`.

---

## 22. Files acidflow reads & writes

acidflow stores everything under `~/.config/acidflow/` (or `$XDG_CONFIG_HOME/acidflow/` if set). Nothing else on your filesystem is touched. No logs, no caches, no telemetry, no network.

| Path | Purpose |
|---|---|
| `~/.config/acidflow/pattern_1.txt` … `pattern_9.txt` | Saved sequencer patterns |
| `~/.config/acidflow/bounce.wav` | Most recent WAV export — overwritten on every export |

To wipe all saved data: `rm -rf ~/.config/acidflow/`.

---

## 23. Performance notes

- **CPU** at 44.1 kHz, 256-sample blocks: well under 5% of one core on any laptop made in the last decade. The hot loop is fully scalar; SIMD is left on the table because there's only one voice.
- **Memory:** ~2 MB resident (dominated by the maya rendering buffer).
- **Latency:** 256 frames at 44.1 kHz = ~5.8 ms per render block, plus whatever the OS audio buffer adds. ALSA's `default` device typically lands at 10–20 ms total round-trip; CoreAudio is similar; WASAPI in shared mode is usually a bit worse.
- **UI frame rate:** capped at 30 FPS (configured in `RunConfig`). The renderer is diff-based — only changed cells get pushed to the terminal — so even on slow terminals the UI stays smooth.
- **Audio safety:** the audio path doesn't allocate, doesn't take locks, and doesn't call into the runtime (no `printf`, no `iostream`, nothing that could page-fault).

---

## 24. Troubleshooting

### "no sound"

- Check that audio is **playing** — the title bar should say `● live` in red, not `◯ idle`.
- Check **VOL** isn't at 0%. Check **CUTOFF** isn't at 0% — at 13 Hz with default RES you'll hear nothing.
- On Linux, check that `default` ALSA device works: `aplay /usr/share/sounds/alsa/Front_Center.wav`.
- If you're inside a `tmux`/`screen` session that's been suspended, the audio thread may have lost its device — restart acidflow.

### "the screen is broken / corrupted"

- Resize your terminal — acidflow re-renders on resize.
- Make sure your terminal supports true-color and UTF-8 box-drawing. A test: `echo -e '\033[38;2;255;138;40m◆\033[0m'` should print an orange diamond.
- If you're on Windows, use Windows Terminal — `cmd.exe` and old PowerShell consoles don't support enough escape sequences.

### "the build can't find maya"

The CMake expects `../maya` to exist relative to the acidflow checkout. Either clone `maya` next to acidflow, or edit `CMakeLists.txt` line 15 to point at wherever you have it.

### "the build can't find ALSA"

```bash
sudo apt-get install libasound2-dev   # Debian/Ubuntu
sudo dnf install alsa-lib-devel       # Fedora
sudo pacman -S alsa-lib               # Arch
```

CMake also tries pkg-config first; if pkg-config can't see ALSA but the library is installed, the fallback `find_library(asound)` should still work.

### "every keystroke is doubled"

You're probably running in a terminal with key-repeat dialed up plus `--input-method` weirdness. Try a clean terminal (foot, alacritty, kitty, iterm2, Windows Terminal).

### "the help bar shows a key but pressing it does nothing"

Some keys in the help bar are *focused-only* — they only work when the relevant section is the active one. The focused section has an orange border. `Tab` to it first.

### "my pattern sounds different on macOS vs Linux"

WASAPI and CoreAudio sometimes resample to 48 kHz; ALSA usually delivers 44.1 kHz. The DSP is rate-aware (filter pre-warp uses the actual sample rate), but there are subtle differences in how the saturator interacts with the band-limited oscillator at different rates. The character should be identical.

### "exporting a WAV produced silence / glitches"

Make sure the pattern actually has notes (not all rests). The export pauses live audio while it runs, so you won't hear it producing — wait for the toast. If the file is corrupt, check that `~/.config/acidflow/` is writable.

---

## 25. Recipes — patches & patterns to try

A non-exhaustive starting set.

### Squelch-monster

- Knobs: **Squelch** archetype (hit `r` on Knobs until you get it). Or manually: TUNE 50% · WAVE saw · CUTOFF 25% · RES 90% · ENVMOD 80% · DECAY 15% · ACCENT 80% · DRIVE 35% · VOL 65%
- Pattern: `Squelch` preset
- BPM: 128, swing 50%

### Higher State of Long Filter

- Knobs: TUNE 50% · WAVE saw · CUTOFF 20% · RES 80% · ENVMOD 50% · DECAY 80% · ACCENT 50% · DRIVE 10% · VOL 65%
- Pattern: `Higher State` preset
- BPM: 124
- *Performance:* slowly raise CUTOFF over many bars (drag the knob with the mouse).

### Acid techno engine

- Knobs: **Driving** archetype
- Pattern: `Energy Flash`
- BPM: 132–138, swing 50%
- Bump DRIVE to 50–70% for the warehouse-pedal sound.

### Dub minimalism

- Knobs: **Dubby** archetype
- Pattern: `Plastikman` or `Hoover Dub`
- BPM: 60–72
- Let it run for two minutes. Don't touch anything. Then nudge CUTOFF.

### Liquid acid

- Knobs: **Classic** archetype
- Pattern: `Liquid` preset (every step slides — no envelope retrigger anywhere)
- BPM: 122
- The whole point is to hear the slide-doesn't-retrigger envelope behaviour.

### "Build my own"

- Pattern: `Empty` (preset 16)
- Hit `c` on a few steps to lay down root notes, then `↑` to push some of them up by semitones.
- Tab to Knobs and dial in something — start with **Classic** archetype if you don't know where to begin.
- Hit `Shift+1` to save it. Now you can mess up freely and `1` back.

---

## 26. Glossary

| Term | Meaning |
|---|---|
| **303** | The Roland TB-303 Bass Line, 1981. The instrument acidflow models. |
| **Acid** | A genre / aesthetic defined by the 303's filter sweep — house and techno styles built around the squelchy filter motion. |
| **Accent** | Per-step "play this louder and brighter" flag. Stacks across consecutive accents thanks to a slow-discharging cap. |
| **BLEP / PolyBLEP** | Band-limited step. A short correction signal added at oscillator discontinuities to suppress aliasing without expensive oversampling. |
| **Cutoff** | The frequency above which the low-pass filter starts attenuating. The single most important parameter. |
| **DECAY** | How fast the filter envelope (MEG) decays. Bypassed (forced 200 ms) on accented notes. |
| **DSP** | Digital signal processing. The synthesis maths. |
| **Envelope** | A time-varying control signal — typically rises fast on note-on and decays. The 303 has three: filter (MEG), accent cap, amp (VEG). |
| **ENVMOD** | How much the filter envelope modulates the cutoff. 0 = no movement, max = 4-octave sweep on every note. |
| **HPF** | High-pass filter. The 303 has one at 16 Hz on the output to remove DC. |
| **LPF** | Low-pass filter. The 303's main filter is a 4-pole (24 dB/oct) LPF. |
| **MEG** | Modulation Envelope Generator — the 303's filter envelope. |
| **Q / Resonance** | The amount of feedback around the filter. High Q produces a peak at the cutoff frequency; very high Q makes the filter self-oscillate. |
| **Self-oscillation** | The state where the filter sustains a sine wave at the cutoff frequency on its own, with no oscillator input. Reached around 90% RES on the 303. |
| **Slide / Portamento** | A note glides into the next one rather than stepping discretely. The 303's slide doesn't retrigger envelopes. |
| **SPSC** | Single-Producer Single-Consumer. The lock-free queue pattern acidflow uses for UI → audio communication. |
| **Squelch** | Onomatopoeia for the 303 sound — high-resonance filter sweep with a fast decay. |
| **Swing** | Rhythmic shift where odd-indexed 16ths are delayed. 50% = straight, 60–62% = MPC swing, 75% = hard shuffle. |
| **TPT / ZDF** | Topology-Preserving Transform / Zero-Delay Feedback. A digital filter design technique that preserves the analog filter's structure even at high cutoff frequencies — used for the 303's ladder. |
| **TUI** | Terminal User Interface. acidflow's UI is rendered into a terminal emulator, not a window. |
| **VCA** | Voltage-Controlled Amplifier. The 303's output amp, controlled by VEG and accent. |
| **VCF** | Voltage-Controlled Filter. The 303's main filter. |
| **VCO** | Voltage-Controlled Oscillator. The 303's single oscillator. |
| **VEG** | Voltage-controlled amp Envelope Generator — the 303's amp envelope. Fixed ~3 s decay. |
| **V/oct** | Volt-per-octave. The 303's modulation is exponential in pitch — a 1 V (or 1 unit) increase doubles the frequency. |

---

*That's everything. If something in acidflow doesn't behave the way this manual says it should, the manual is wrong — file an issue.*
