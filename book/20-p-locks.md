# 20 · Parameter locks — making patterns breathe

Parameter locks (p-locks) are the deepest feature in acidflow's sequencer. They let you override specific synth parameters on a per-step basis — so while the CUTOFF knob might say 40%, a specific step can have its own CUTOFF of 75% just for that step, then the next step goes back to 40%.

This means a pattern can have 16 *different* filter settings over its course without you moving any knobs. Each step can have its own cutoff, resonance, envelope modulation, or accent level.

p-locks are borrowed from Elektron's Machinedrum (2002) and have since become the single most powerful feature in step-sequencer design. They transform a synth from "a sound with a pattern" into "a sound *per step*."

## What you can lock

In acidflow, each step can lock four parameters:

- **CUTOFF** (`F` key) — filter cutoff frequency.
- **RES** (`G` key) — filter resonance.
- **ENVMOD** (`H` key) — filter envelope amount.
- **ACCENT** (`J` key) — accent depth.

These are the four most musically-critical synthesis parameters. The others (DECAY, DRIVE, VOL, TUNE, WAVE) are not p-lockable — they stay as the global knob setting.

### Why these four and not others?

- DECAY not locked: it's shared between filter and amp envelopes; per-step changes would cause discontinuities.
- DRIVE not locked: the saturation changes the filter interaction; locking drive would be unpredictable across ratcheted steps.
- VOL not locked: it's the output level, not tonal.
- TUNE not locked: affects pitch globally.
- WAVE not locked: changing waveform mid-pattern causes an audio discontinuity.

The four that ARE locked are the ones that most directly shape how each note sounds, and they change smoothly step to step without audio artifacts.

## How p-locks work

When you press `F` on the selected step, the current CUTOFF knob value is captured as that step's lock. The step will now always play with that cutoff, regardless of where the CUTOFF knob is currently.

If you then turn the CUTOFF knob, all non-locked steps respond; the locked step keeps its locked value.

To clear a specific lock, press `F` again (it toggles off). To clear all locks on the current step, press `L`. To clear a specific parameter's lock on all steps, there's no direct shortcut — you'd need to go step-by-step.

### Visual indicator

Locked steps show a modifier letter in the display indicating what's locked. The exact glyphs are subtle but visible once you know what to look for.

## Why p-locks transform a pattern

Without p-locks, you can vary the sound in two ways:

1. Turn a knob while the pattern plays (live).
2. Use different saved slots with different knob positions and chain them in song mode.

Both are global — the whole pattern changes together.

With p-locks, you can vary the sound *within a single pattern* at the per-step level. You can have a pattern where:

- Step 1 has closed filter.
- Step 5 has open filter + high resonance.
- Step 9 has moderate cutoff with increased ENVMOD.
- Step 13 has maximum accent.

All in a single 16-step pattern, all coexisting. This is impossible on the real TB-303 without external automation.

## Canonical p-lock uses

### Dynamic filter movements per step

Lock CUTOFF to create a filter sweep that's baked into the pattern:

```
Step:    1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
CUTOFF: 20% 25% 30% 35% 40% 45% 50% 55% 60% 65% 70% 75% 80% 75% 70% 60%
```

This is a slow rise over 12 steps, then a fall. Every time the pattern loops, the filter rises and falls — without you touching the knob.

You can stack this with a live CUTOFF sweep over many bars. The per-step p-lock shape runs inside each bar; the global CUTOFF knob moves the base position up and down over 16-64 bars.

### Resonance accents

Lock RES on just a few steps to create moments of extreme resonance:

```
Step:    1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
RES:    50  50  80  50  50  50  50  50  50  50  85  50  50  50  50  50
             (lock)                  (lock)
```

Most of the pattern plays at RES 50%; steps 3 and 11 play at much higher RES. These steps get a dramatic resonance peak while the rest stays musical.

### ENVMOD variations

Lock ENVMOD differently per step to create varying filter sweep depths:

```
Step:    1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
ENVMOD: 40  40  80  40  40  40  80  40  40  40  80  40  40  40  80  40
```

Most steps have moderate envelope depth; every fourth step has extreme depth. The pattern has "big moments" every four steps.

### Accent depth per step

Lock ACCENT differently to make some accented steps *very* accented:

```
Step:    1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
ACCENT: 50  50  50  50  90  50  50  50  50  50  50  50  90  50  50  50
             (accent flags also on 5, 13)
```

Steps 5 and 13 (the backbeat) are accented AND have locked ACCENT depth at 90%. These steps feel dramatically loud compared to other accents.

## P-locks to simulate a moving CUTOFF

Here's a deep trick. Suppose you want the filter to open over 16 steps. You could do this with a live knob move, but that requires you to perform it. You could do it with p-locks: lock CUTOFF at 20% on step 1, 25% on step 2, 30% on step 3, ... 95% on step 16.

Now the pattern plays with an *automatic* 16-step filter sweep. Every bar, the same sweep repeats.

To make it sweep over 32 steps (two bars), you need two patterns chained in song mode, or you live with the one-bar sweep.

This is how "automation" gets baked into patterns.

## P-locks to simulate a specific record

You can reverse-engineer a real acid record's filter behavior by listening and setting p-locks to match. Pick a 2-bar section of *Acid Tracks*, listen to where the filter opens and closes, transcribe those positions as p-locks on cut-down steps. You'll produce a pattern that *sounds* like that section without needing to automate live.

This is a great learning exercise. Pick a record, replicate its dynamics via p-locks, and you'll understand how that record's filter was performed originally.

## P-locks and mutation

acidflow's `M` key (evolving mutation, Chapter 33) mutates the pattern step by step. By default, mutation doesn't touch p-locks — it changes pitch, accent, slide, rest, etc. But if you have p-locks set, they stay in place while the underlying pattern mutates around them.

This means you can set up a "character shell" (the p-locks define the dynamics) and let mutation handle the melodic evolution. The track feels controlled (dynamics locked) but evolving (melody shifting).

## Over-locking

A pitfall: if you lock all 16 steps on all 4 parameters, your global knobs do nothing. The global knobs only affect non-locked steps. If everything is locked, the knobs are dead.

This is usually not what you want. The point of a global knob is to let you make arrangement-level changes (slow CUTOFF sweeps over many bars). If every step is locked, you lose that ability.

Practical rule: lock only the parameters you want to *emphasize* as pattern-specific. Leave the rest global.

## Typical p-lock counts

Most effective patterns have:

- 0-3 CUTOFF locks (only a few steps with distinctive cutoff).
- 0-2 RES locks (resonance accents).
- 0-3 ENVMOD locks (filter depth variations).
- 0-2 ACCENT locks (dynamic peaks).

Total: maybe 5-10 locks across 16 steps. Not 64.

Going heavier than this tends to produce patterns that feel "over-programmed" — every step doing its own thing, with no room for the global performance knobs to contribute.

## P-locks and live performance

A common workflow:

1. Program the pattern's rhythmic skeleton (pitches, accents, slides, rests).
2. Add a handful of p-locks that give specific steps a distinct character.
3. Perform the global knobs live over the locked-pattern foundation.

The p-locks define "the pattern's identity." The global knobs perform "the track's arrangement." Together, they're rich — locked pattern for micro-level detail, live performance for macro-level motion.

## How acidflow implements p-locks

Each step stores `plock_cutoff`, `plock_res`, `plock_envmod`, `plock_accent` as optional values. If the value is set, it's used; otherwise the global knob is used.

When the step plays, the audio thread reads:

```
cutoff   = step.plock_cutoff.value_or(global.cutoff)
res      = step.plock_res.value_or(global.res)
envmod   = step.plock_envmod.value_or(global.envmod)
accent   = step.plock_accent.value_or(global.accent)
```

This happens per-step, so the values are sample-accurate. No audio glitches.

## Clearing locks

`L` clears all locks on the currently-selected step. Useful if you've locked something and want to revert.

`x` clears the entire step (pitch, modifiers, locks, ratchet, probability). Nuclear option.

There's no "clear all locks on all steps" shortcut — you'd need to step through each one. But you can just clear the pattern entirely with repeated `x`.

## P-locks vs. multi-pattern chains

An alternative to p-locks is to save two versions of a pattern to slots (one with a high cutoff, one with a low cutoff) and chain them in song mode. This gives you pattern-level variation rather than step-level.

Which is better? Depends on the effect:

- **Step-level variations**: use p-locks.
- **Phrase-level variations (bar-by-bar)**: use song mode with multiple slots.
- **Section-level variations (8 bars at a time)**: use song mode, or live knob performance.

Both tools have their place. They don't compete.

## Try this

1. Load preset 01 (Acid Tracks). Play. Notice it has no p-locks — every step uses global knobs.
2. Select step 5. Press `F` to lock CUTOFF. Now step 5 is locked at whatever CUTOFF is right now.
3. Turn the global CUTOFF knob to 90%. Play. Notice step 5 stays at its lower cutoff while everything else opens up. Step 5 is a "dark spot" in the pattern.
4. Select step 13. Press `F` to lock, then `G` to lock RES. Now step 13 has its own filter state too.
5. Play for a few bars. Now move the global CUTOFF up and down. Steps 5 and 13 don't budge; everything else responds.
6. Clear one of the locks (`L` on selected step). The step returns to tracking the global knob.

P-locks are the deepest compositional tool in the sequencer. Use them to bake character into patterns rather than relying entirely on live performance.

Next chapter: swing, length, and polymeter — the remaining sequencer-level controls.
