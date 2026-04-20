# 15 · The 16-step grid

Part III is about the sequencer — the thing that tells the 303 voice what to play, when. The sequencer is a grid of 16 steps, each of which can be a rest or a note, with various modifiers on each note. This chapter is the orientation. The following chapters dig into each of the compositional choices.

## Why sixteen steps

Sixteen steps is one bar of 4/4 at sixteenth-note resolution. Four beats, four subdivisions per beat, sixteen total. This is the minimum granularity that lets you write dance rhythms faithfully. You can't write acid at eighth-note resolution — it's too coarse, there's no room for syncopation. You don't need thirty-second notes — they're too fast to be useful at dance tempos, and the 303 can't play them meaningfully (the envelope can't settle in 31 ms).

So 16. The original TB-303 had 16 steps. Every acid machine since has had 16 steps. Every acid plugin has 16 steps. It's the single most standardized dimension of the genre.

acidflow lets you *shorten* the pattern to as few as 4 steps (`{` in Transport) but not extend beyond 16 on the 303. The drum grid is always 16 steps. If the 303 pattern is shorter, it repeats against the drum grid — producing polymeter (Chapter 21).

## Grid coordinates

Each step has a position from 1 to 16. In acidflow, we 1-index the display (so you see steps labeled 1-16, not 0-15). Internally the code is 0-indexed like any sensible software; the display lies for user-friendliness.

Beats correspond to steps like this:

```
Beat:    1        2        3        4
Steps:   1 2 3 4  5 6 7 8  9 10 11 12  13 14 15 16
         ^  ^ ^ ^ ^  ^ ^ ^ ^  ^  ^  ^  ^  ^  ^  ^
         |  | | | |  | | | |  |  |  |  |  |  |  |
         D  e e e B  e e e B  e  e  e  B  e  e  e
         o  i i i k  i i i k  i  i  i  k  i  i  i
         w  g g g t  g g g t  g  g  g  t  g  g  g
         n  h h h   2h h h h  3  h  h  4  h  h  h
```

Where "D" = downbeat, "B" = beat, "e" = eighth, other "e"s are sixteenths. The first position of each beat is called the **beat** or **downbeat** of that beat (strong). The second position is the **ee-and** — slightly weaker. The third is the **and** — the off-eighth. The fourth is the **and-ee** — weakest.

Musicians count sixteenths as "one-ee-and-a, two-ee-and-a, three-ee-and-a, four-ee-and-a." The numbers are the beats. The "and"s are the off-eighths. The "ee"s and "a"s are the sixteenth subdivisions.

## What each step contains

In acidflow, a step is a structure with several fields:

- **pitch** (in semitones from some reference)
- **octave** (combined with pitch for the actual note)
- **accent** (bool — is this step accented?)
- **slide** (bool — is this step slid?)
- **rest** (bool — is this step silent?)
- **probability** (0, 25, 50, 75, 100 — percent chance of this step playing)
- **ratchet** (1, 2, 3, or 4 — how many sub-hits per step)
- **plock_cutoff** (optional override of the CUTOFF knob just for this step)
- **plock_res** (optional override of RES)
- **plock_envmod** (optional override of ENVMOD)
- **plock_accent** (optional override of ACCENT)

That's twelve fields per step. Across 16 steps, 192 parameters. Plenty of room to write a pattern with individual character.

Most fields default to neutral values:
- Pitch = C3 (middle C).
- All booleans false.
- Probability 100%.
- Ratchet 1.
- No p-locks.

A "fresh" step is just "play C3 at 100% probability with no accent, slide, or lock."

## Pitch in the sequencer

The pitch of a step is set in semitones. Each semitone is 1/12 of an octave. You enter pitches using:

- **Letter keys** (`c d e f g a b` and variants). These jump to that note-class at the current displayed octave.
- **Arrow keys** (`↑/↓`). These nudge by one semitone.
- **Angle brackets** (`<`/`>`). These nudge by one octave.

Sharp and flat notes are reached by pressing the plain letter key, then nudging with arrow keys. For example, to enter C♯: press `c`, then `↑`.

The display shows pitches in scientific notation: `C3` is middle C, `C4` is an octave up, `C2` is an octave down. A4 is 440 Hz by convention. The 303's useful range is roughly C1 to C5 — below C1 the pitch is indistinguishable from drone, above C5 the pitch is too high to feel like a bass.

## The other modifiers

We've covered accent and slide in their own chapters. For the others, quick reference — each one is expanded in later chapters:

- **Rest** (`m` in sequencer): the step is silent. No note plays. See Chapter 17.
- **Probability** (`v`): cycle through 100/75/50/25. See Chapter 19.
- **Ratchet** (`j`): cycle through 1/2/3/4. See Chapter 19.
- **P-locks** (`F`/`G`/`H`/`J`): override CUTOFF/RES/ENVMOD/ACCENT for this step only. See Chapter 20.

These are the "extended" features that distinguish acidflow's sequencer from the plain 303 sequencer. The original 303 had only pitch, accent, slide, and rest. acidflow adds probability, ratchet, and p-locks — all of which are borrowed from Elektron's drum machines, which modernized step sequencing in the 2000s.

## Entering a pattern: the workflow

The typical acidflow sequencer workflow, in the order most people do it:

1. **Set a pitch skeleton**. Decide on the root note and a few other pitches. Use letter keys and octave jumps.
2. **Add rests**. Make some steps silent (`m`) to create rhythmic structure.
3. **Add accents**. Decide which steps should be emphasized (`a`).
4. **Add slides**. Connect some notes (`s`).
5. **Tweak probability and ratchet**. If you want some steps to be occasional or stuttered.
6. **Add p-locks**. If you want per-step filter/resonance/envelope/accent variations.

You can also let the randomizer do most of this (`R` then `r`), or load a preset. But understanding the workflow helps you edit what the randomizer gives you.

## Navigating the grid

`←` and `→` move the step selector. The selected step has a bright border.

In acidflow, the grid also shows a live **playhead** — the currently-playing step — highlighted differently from the selected step. They can be different; you can edit step 3 while step 9 is playing. The playhead moves with the sequencer; the selector moves with your arrow keys.

If you want to edit the currently-playing step, pause first (Space), edit, then resume.

## Selecting multiple steps (sort of)

acidflow doesn't have a formal "multi-select" for the sequencer. But some operations affect ranges:

- **`x`**: clears the current step.
- **`L`**: clears all p-locks on the current step.
- **`R`** (capital): randomizes the whole pattern.
- **`r`** (lowercase): randomizes the section in focus. If the sequencer is focused, this randomizes the pattern.

For fine-grained edits, you move the selector and edit one step at a time.

## The pattern as a whole

A sequencer pattern is a *loop*. When step 16 finishes, step 1 plays next. Forever, until you stop the transport.

Patterns are saved as part of a user slot or as MIDI/WAV/text export. When you load a preset or a slot, the whole pattern is replaced.

Patterns are *not* automatically generative — they play the same thing every bar unless you explicitly use probability, mutation (Chapter 33), or song mode (Chapter 30) to vary them.

## Pattern length

The default pattern length is 16 steps. You can change it to anything from 4 to 16 using `{`/`}` in the Transport section.

Shortening the pattern:
- Makes the loop faster (more repetitions per minute).
- Creates polymeter against the always-16-step drum grid.
- Emphasizes the pattern's cyclical nature.

Use a shorter pattern when:
- You want the bass line to loop 2-4 times per drum bar.
- You want a polymetric relationship between bass and drums.
- You're making sparse minimal acid.

Long patterns (12-16 steps) are the classic format. Most presets are 16.

## The grid visually

On-screen, each step is shown as a column containing:

- Pitch at the top (e.g., "C3").
- Modifier letters below: `A` for accent, `S` for slide, `—` for rest, `p50` for 50% probability, `×2` for 2-ratchet.
- Border highlights: selected = bright, playhead = special color based on theme.

At certain densities, the display compresses. At a terminal width of 80 columns, each step might only be 4 characters wide. At 120+ columns, each step has 6-8 characters and more information fits.

## The scope and filter heatmap

While the sequencer plays, the scope and filter heatmap show what's happening. Watch these while you edit — they're a feedback channel for "did that edit do what I expected?" If you raise ENVMOD and the heatmap doesn't show more filter movement, something's wrong.

## Sequencer runs on the audio thread

A technical note (can skip if you don't care about implementation): acidflow's sequencer is driven by the audio thread, not the UI thread. Step changes are sample-accurate, meaning the transition between steps happens exactly at the right sample (not at the rough "every few milliseconds" UI tick).

Practically: there's no timing jitter. The pattern plays tightly. This matters at high tempos and for MIDI clock export accuracy.

## Try this

1. Start with a fresh pattern. Press `x` on every step to clear them. Now every step plays C3 with no modifiers.
2. Leave step 1 alone. Rest steps 2, 3, 4. Play. You hear a single note every four beats. Basic pulse.
3. Add a note on step 9 (another C3). Leave 10, 11, 12 as rests. Play. Now a two-hit pattern — beats 1 and 3. Even more basic.
4. Fill in all 16 steps with C3. Play. Solid 16th-note drone.
5. Add octave jumps on steps 5, 9, 13 (press `<` on each to lower an octave, then `>` to raise — or just use arrow keys). Play. Variety.
6. Add slides on steps 5, 9, 13 (the octave jumps). Play. Now smooth.
7. Add accents on 3, 7, 11, 15. Play. Now rhythmic dynamics.

You just assembled a pattern from nothing. Continue reading — the next chapters are about the compositional choices each layer represents.
