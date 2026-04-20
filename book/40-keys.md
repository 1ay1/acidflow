# 40 · Complete keyboard and mouse reference

This chapter is the full keybind reference. It's duplicated from the MANUAL for convenience — if there's a conflict, the MANUAL is authoritative.

Most of acidflow's controls are keyboard-based. This reference is organized by *focus* — which section is currently active. The same key can do different things depending on what's focused.

## Global keys (work from any section)

These work regardless of focus:

| Key | Action |
|---|---|
| `Tab` | Cycle focus forward (Knobs → FX → Sequencer → Drums → Transport → Knobs …) |
| `Shift+Tab` | Cycle focus backward |
| `Space` | Play / pause |
| `r` | Randomize current section |
| `R` | Randomize everything (knobs + pattern + drums + FX) |
| `M` (Shift+M) | Evolve pattern — one small mutation per press |
| `T` (Shift+T) | Cycle color theme |
| `n` | Toggle song mode |
| `k` | Toggle jam mode |
| `O` (Shift+O) | Toggle MIDI out |
| `I` (Shift+I) | Toggle MIDI sync in |
| `e` | Export pattern as WAV (4 loops) |
| `E` (Shift+E) | Export pattern as MIDI (.mid) |
| `p` | Export pattern as shareable text |
| `P` (Shift+P) | Import pattern from text |
| `Shift+1` … `Shift+9` | Save pattern to slot 1-9 |
| `1` … `9` | Load pattern from slot 1-9 (from non-Sequencer/Drums focus) |
| `?` | Toggle keyboard reference overlay |
| `q` / `Q` / `Esc` | Quit (closes help overlay first if open) |

## When Knobs is focused

| Key | Action |
|---|---|
| `←` / `→` | Select previous / next knob |
| `↑` / `↓` | Adjust selected knob ±5% |
| `[` / `]` | Coarse adjust ±10% |
| `0` | Reset selected knob to default |
| `w` | Toggle WAVE (saw ↔ square) |
| `m` | Mute the 303 voice (toggle) |

Knob order (left to right):
1. TUNE
2. WAVE (saw/square toggle)
3. CUTOFF
4. RES
5. ENVMOD
6. DECAY
7. ACCENT
8. DRIVE
9. VOL

## When FX is focused

| Key | Action |
|---|---|
| `←` / `→` | Select previous / next FX knob |
| `↑` / `↓` | Adjust (DLY DIV steps through the four divisions) |
| `[` / `]` | Coarse adjust |
| `0` | Reset FX knob |
| `m` | Mute the FX bus |

FX knobs (left to right):
1. FX DRIVE
2. DLY MIX
3. DLY FB
4. DLY DIV (1/16, 1/16 dotted, 1/8, 1/8 dotted)
5. REV MIX
6. REV SIZE
7. REV DAMP

## When Sequencer is focused

| Key | Action |
|---|---|
| `←` / `→` | Select previous / next step (wraps) |
| `↑` / `↓` | Transpose step ±1 semitone |
| `<` or `,` | Octave down |
| `>` or `.` | Octave up |
| `c` `d` `e` `f` `g` `a` `b` | Set step pitch (root) at the current octave |
| `a` | Toggle accent (clears rest if set) |
| `s` | Toggle slide (clears rest if set) |
| `m` | Toggle mute / rest |
| `v` | Cycle probability (100% / 75% / 50% / 25%) |
| `j` | Cycle ratchet (×1 / ×2 / ×3 / ×4) |
| `F` (Shift+F) | P-lock cutoff |
| `G` (Shift+G) | P-lock resonance |
| `H` (Shift+H) | P-lock env-mod |
| `J` (Shift+J) | P-lock accent amount |
| `L` (Shift+L) | Clear all p-locks on step |
| `x` | Clear step (rest, no flags, no locks) |

Note: `1` through `9` normally load pattern slots, but in Sequencer focus these are consumed for their own uses. Switch focus to load slots by number.

## When Drums is focused

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

Voice order:
1. BD (kick)
2. SD (snare)
3. CH (closed hat)
4. OH (open hat)
5. CL (clap)

## When Transport is focused

| Key | Action |
|---|---|
| `↑` / `↓` | Previous / next preset |
| `,` / `.` | Previous / next preset (alt) |
| `[` / `]` | BPM ±2 (clamped 40-220) |
| `{` / `}` | Pattern length ±1 (clamped 4-16) |
| `-` / `=` | Swing ±2% (clamped 50-75) |
| `m` | Mute playback bus |

## Jam mode keys

When jam mode is on (`k` to toggle), the regular keybinds are suspended; the keyboard becomes a two-octave piano.

Lower octave (current octave):
| C | C♯ | D | D♯ | E | F | F♯ | G | G♯ | A | A♯ | B |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `z` | `s` | `x` | `d` | `c` | `v` | `g` | `b` | `h` | `n` | `j` | `m` |

Upper octave (current + 1):
| C | C♯ | D | D♯ | E | F | F♯ | G | G♯ | A | A♯ | B |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `q` | `2` | `w` | `3` | `e` | `r` | `5` | `t` | `6` | `y` | `7` | `u` |

Plus:
| Key | Action |
|---|---|
| `←` / `→` | Octave down / up |
| `'` | Toggle accent for subsequent presses |
| `;` | Toggle slide for subsequent presses |
| `k` or `Esc` | Leave jam mode |

## Mouse reference

### Title bar

| Action | Effect |
|---|---|
| Left click | Toggle play / pause |
| Scroll wheel | Nudge BPM ±1 |

### Knobs

| Action | Effect |
|---|---|
| Left click | Focus the knob |
| Left drag (vertical) | Adjust the value (up = increase, down = decrease) |
| Scroll wheel | Adjust ±5% |
| Right click | Reset to default |

WAVE knob is special: any click or scroll toggles saw/square (no drag).

### Sequencer steps

| Action | Effect |
|---|---|
| Left click | Select the step |
| Scroll wheel | Transpose ±1 semitone |
| Right click | Toggle rest |

No drag-to-paint — click selects, keyboard edits.

### Transport area

Right portion is a scroll-to-browse zone:

| Action | Effect |
|---|---|
| Left click | Focus Transport |
| Scroll wheel | Browse presets |

### Help overlay

When the help overlay (`?`) is open, mouse is ignored. Close first.

## The focus cycle

Pressing `Tab` cycles through sections in this order:

```
Knobs → FX → Sequencer → Drums → Transport → Knobs → ...
```

`Shift+Tab` reverses.

Which section has focus:
- Visual: the focused section has a highlighted border.
- Keyboard: context-specific keys apply to the focused section.
- Non-conflicting keys work globally.

## Key conflicts: when the same key does different things

Several keys have different meanings per focus:

### `m`
- Knobs: mute 303 voice.
- FX: mute FX bus.
- Sequencer: toggle rest on current step.
- Drums: mute drum bus.
- Transport: mute playback bus.
- Jam mode: play note B.

### `c` `d` `e` `f` `g` `a` `b`
- Sequencer: set step pitch.
- Drums (`c` only): clear current voice row.
- Jam mode: play corresponding note.
- Elsewhere: no action.

### `a`, `s`
- Sequencer: toggle accent / slide.
- Jam mode: play notes (`a` is C♯ upper, `s` is C♯ lower).
- Elsewhere: no action.

### `1` `2` `3` `4` `5` `6` `7` `8` `9` `0`
- Anywhere except Sequencer/Drums: load pattern slot.
- Drums: quick-toggle step 1-10 on current voice.
- Sequencer: consumed internally.
- Jam mode: play upper-octave notes.

### `[` `]`
- Knobs: coarse adjust selected knob.
- FX: coarse adjust selected knob.
- Drums: drum bus gain.
- Transport: BPM ±2.

### `,` `.`
- Transport: previous / next preset.
- Sequencer: octave down / up.

### `<` `>`
- Sequencer: octave down / up.
- Elsewhere: no action.

## Keyboard shortcuts I use most

Personal favorites — you'll develop your own:

- `Space` — play/pause. Constant.
- `Tab` — change focus. Constant.
- `r` — randomize current section.
- `R` — randomize everything.
- `M` — one mutation.
- `?` — show full keybind list on-screen.
- `Shift+1` — save to slot 1 (muscle memory: always save to slot 1 first).
- `1` — load slot 1 (when not in Sequencer/Drums).
- `e` — export WAV.

## A note on terminal keybinds

Terminals don't universally support every key combination. Some caveats:

- Some shifted numeric keys (`Shift+1` to `Shift+9`) may not work on non-US keyboard layouts. The fallback is to unshift and remap.
- `Esc` followed by a character can be interpreted as Alt+character — some terminals deliver this delayed.
- Function keys (F1-F12) aren't used in acidflow because of terminal inconsistency.

If a key doesn't work for you, check the `?` help overlay for alternate bindings.

## Printing this reference

You can print the MANUAL.md (easier to keep on a desk). Or memorize a few key combinations and look up the rest via `?` in-app.

Most users learn the 20 most-used keys in a week and never need the reference. Beyond those, refer occasionally.

## Try this

1. Start acidflow.
2. Press `?`. The help overlay appears.
3. Use the mouse wheel to scroll through the reference.
4. Close with `?` or `Esc`.
5. Open again. Try each key listed for Knobs section.
6. Open again. Try each key listed for Sequencer section.

After half an hour of practice, you'll know the core keybinds by heart.

Next: glossary.
