# 19 · Probability and ratchet

The original TB-303 had a very simple sequencer: pitch, accent, slide, rest, end of list. Period. Every bar was identical to the last. If you wanted variation, you had to either perform it live by turning knobs, chain multiple patterns, or accept rigid repetition.

acidflow adds two features the real 303 didn't have: **probability** and **ratchet**. Both are borrowed from Elektron's drum machines (Machinedrum, Digitakt) and have become standard in modern step sequencers. They transform a 16-step pattern from "the same bar every bar" into "a bar with built-in variation."

This chapter is about both features, and about how to use them to create evolving patterns without explicitly programming long sequences.

## Probability

Probability is simply: each step has a chance of firing. The chances are discrete — 25%, 50%, 75%, or 100% (always fires).

Press `v` in the sequencer on the selected step to cycle its probability: 100 → 75 → 50 → 25 → 100.

### What it sounds like

A step at 100% fires every time the pattern plays — standard behavior.

A step at 75% fires most of the time. Over 16 bars, it'll fire 12 times and miss 4.

A step at 50% fires half the time. Pattern alternates between having that note and not, but randomly — not every other bar.

A step at 25% fires rarely. Most bars it's a rest; occasionally it fires and adds a surprise.

### Why this is useful

**Variation without programming**. Instead of writing a 32-step or 64-step pattern with slight variations, you write a 16-step pattern with a few probabilistic steps and let the randomness do the varying.

**Ghost notes**. Probability at 25-50% creates ghost-note behavior — notes that you "expect" to be there sometimes and missing other times. Adds organic feel.

**Dynamic density**. A pattern with 3-4 probabilistic steps feels denser in some bars and sparser in others, without you doing anything. The track breathes.

### Canonical probability usage

**Scattered low-probability events**: put one or two 25% steps in a pattern. These are "lucky strikes" — rare but audible when they happen.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Note:    X  X  X  p  X  X  X  X  X  p  X  X  X  X  X  p
                   25         25                    25
```

Three steps at 25%. Most bars they don't play. Occasionally one fires. Pattern has natural variety.

**50% steps on offbeats**: make every offbeat step a 50%. The pattern alternates between a clean "onbeat" groove and a denser "offbeat accents" groove, randomly.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Prob:    1  .  p  .  1  .  p  .  1  .  p  .  1  .  p  .
             50        50        50        50
```

**75% on most steps + 100% on anchors**: make steps 1, 5, 9, 13 (the downbeats) firm at 100%, make the rest at 75%. The pattern is mostly the same but has occasional "missed" notes, making it feel more human.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Prob:   100 75 75 75 100 75 75 75 100 75 75 75 100 75 75 75
```

### Probability and song structure

If your track is long (5+ minutes), a fixed pattern gets monotonous. A probabilistic pattern stays fresh for longer because no two bars are identical.

But beware: too many probabilistic steps and the pattern loses identity. Keep the "core" steps (downbeats, important melodic events) at 100%. Only make the ornamental steps probabilistic.

### Probability with modifiers

Probability applies to the *whole step*. If a step is probabilistic and it fires, all its modifiers (accent, slide, pitch, ratchet) apply. If it doesn't fire, the step is as-if-rested.

So you can have an accented-slid step at 50% probability. Half the time it fires (with accent and slide); half the time it's silent.

This is a powerful technique for creating rhythmic ornaments that come and go.

## Ratchet

Ratchet is: one step fires multiple notes in sequence, all within the step's time duration. A ratchet of 2 fires two sub-hits. A ratchet of 3 fires three. A ratchet of 4 fires four.

Press `j` in the sequencer to cycle the ratchet value: 1 → 2 → 3 → 4 → 1.

### What it sounds like

Ratchet 1 is the default — one note per step.

Ratchet 2 on a step plays two notes in the time of one step. At 120 BPM with 16th-note steps, a step is 125 ms. Ratchet 2 plays two 62.5 ms notes — functionally 32nd notes.

Ratchet 3 plays three notes per step — roughly triplet-32nds.

Ratchet 4 plays four notes per step — 64th-note buzzes.

### Why this is useful

**Rhythmic stutters**. A single ratcheted step creates a *drill* or *stutter* at that position — a rapid-fire burst. Very distinctive.

**Triplet feel**. Ratchet 3 produces triplet subdivisions in an otherwise 16th-note pattern. Great for syncopated polyrhythmic feels.

**Impact moments**. Use a ratchet on the last step of the bar (step 16, or step 14/15/16 all ratcheted) for a machine-gun fill that leads into the next bar.

### Canonical ratchet usage

**Final-step stutter**: ratchet step 16 to 3 or 4. The pattern ends each bar with a rapid-fire hit that fills the last 16th-note duration.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Ratchet: 1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  4
                                                      ^
                                              4 hits here
```

**Every-four-steps ratchet**: ratchet 2 on steps 4, 8, 12, 16. Each beat ends with a double-hit that leads into the next beat.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Ratchet: 1  1  1  2  1  1  1  2  1  1  1  2  1  1  1  2
                  ^           ^           ^           ^
```

**Occasional 3-ratchet**: one step in the bar with ratchet 3, producing a "triplet burst" that syncopates against the 4/4 feel.

### Ratchets with slides

A ratcheted step's first sub-note respects the slide flag (slides in from the previous note). Subsequent sub-notes within the step do not re-slide — they retrigger.

So: a ratcheted + slid step starts with a smooth slide-in for the first sub-hit, then machine-guns the remaining sub-hits as retriggered notes.

This is a niche but evocative effect.

### Ratchets with accents

A ratcheted step's accent applies to *all* sub-hits. If the step is accented and ratcheted 3, you get three accented sub-hits — which stacks the accent capacitor aggressively.

Use with care. Stacking 3 accents in 125 ms produces extreme accent capacitor saturation that can dominate the rest of the bar.

## Combining probability and ratchet

You can have a step that's both probabilistic AND ratcheted. E.g., a step at 50% probability with ratchet 3. When it fires, you get a triplet burst; when it doesn't, silence.

This is the "occasional stutter" effect — sometimes the pattern has a ratchet-burst somewhere, sometimes it doesn't. Interesting, unpredictable variety.

## The "mathematical" probability calculation

acidflow uses a per-bar RNG for probability. Each time the sequencer's counter hits a probabilistic step, a new random number is drawn. If the number is below the probability threshold, the step fires.

There's no memory — step 5 at 50% could fire 8 bars in a row if the dice fall that way. This is intentional — real randomness clumps, and the clumping is part of what makes probabilistic patterns feel alive.

If you want guaranteed variation, don't use probability — use song mode (Chapter 30) with explicit pattern chains.

## Ratchet implementation

Ratcheted sub-hits are evenly distributed within the step's duration. A ratchet-3 step produces sub-hits at positions 0, 33%, and 67% of the step duration. Each sub-hit retriggers the envelope.

The VCA applies to each sub-hit, so the ratchets are audibly separate notes, not a continuous tone.

## Musical use of ratchet

Ratchets are typically used sparingly — one or two per pattern. If every step is ratcheted, the pattern is a continuous roll and the individual steps blur.

A well-placed ratchet:

- Surprises the listener at a specific rhythmic moment.
- Fills a transition (bar-end, phrase-end).
- Emphasizes a particular note that would otherwise feel too static.
- Adds "finish" to a pattern that would otherwise end flatly.

A poorly-placed ratchet:

- Distracts from the rest of the pattern.
- Creates rhythmic clutter (multiple ratchets in a row).
- Conflicts with the drum pattern's subdivisions.

Use ratchets like exclamation points — sparingly, at specific moments.

## Probability and ratchet in different subgenres

**Classic acid (1987-1995)** didn't have these features; patterns were rigidly repeated. Using probability/ratchet makes your track *more modern* than classic acid.

**Acid techno (2000s+)** uses ratchets heavily for machine-gun fills. Probability less common.

**Plastikman / minimal acid** uses both, especially probability, to create evolving-but-sparse textures.

**Modern electronic music** (since Elektron's influence) uses both features extensively. If you want a contemporary-sounding acid track, use them. If you want a period-accurate 1989-style track, avoid them.

## The meta-point

Probability and ratchet together give your pattern temporal evolution — the pattern isn't identical each bar. This is *compositional depth* achieved without expanding pattern length.

Without these features, a 16-step pattern can only be what it is; it takes song mode or external automation to vary it. With them, a single 16-step pattern can play for 4-8 minutes without feeling identical throughout.

## Try this

1. Start with a plain pattern. Add one step with 50% probability. Play for 8 bars. Observe the irregularity.
2. Add a second 50% step elsewhere. Play. Now the pattern has two independent "maybe" points, creating 4 possible variants per bar.
3. Make a ratchet-3 on step 16. Play. Notice the drill at the end of each bar.
4. Make the ratchet-3 step ALSO probabilistic at 50%. Play. Sometimes the drill happens, sometimes not.
5. Design a pattern where the "first half" is all 100% steps (solid core) and the "second half" has 2-3 probabilistic steps (variable ornaments). Play for 8 bars. This is a more musical use of probability than random scattering.

Probability and ratchet are the modern acid toolkit. Use them to breathe life into otherwise-static patterns.

Next: parameter locks, the feature that lets you vary the filter and envelope settings step by step.
