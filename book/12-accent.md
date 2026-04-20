# 12 · Accent — the circuit's secret weapon

This chapter is about one knob. ACCENT. If you only deeply understand one control on the 303 after reading this book, make it this one. Accent is the single feature that separates competent 303 emulation from real acid character. It's also the feature that producers who know what they're doing spend the most time thinking about.

## What accent is

On most synthesizers, "accent" is a velocity parameter — you play a note harder, and it comes out louder or brighter. That's not what the 303 does.

On the 303, **accent** is a per-step boolean flag — either this step is accented or it isn't, no in-between. When an accented step plays, three things happen:

1. The amp envelope hits harder (slightly louder note).
2. The filter envelope hits harder (filter opens more).
3. The **accent capacitor** gets a pulse of charge. This capacitor *persists between notes*, draining slowly, and adds to the envelope of subsequent notes as long as it has charge left.

That third thing is the magic. The accent doesn't just affect the accented note — it affects the notes around it, building up and draining based on the pattern of accents.

## The accent capacitor, explained

Imagine a bucket. When you accent a note, you pour water into the bucket. The water drains out slowly — not instantly, but over a few hundred milliseconds. Every note, accented or not, reads the bucket's current water level and adds it to the envelope.

If you have one accented note followed by silence, the bucket fills on the accent and then slowly drains.

If you have *two* accented notes in a row, the second pour happens before the first drain has fully emptied. Net water level is higher than after a single pour. The second accented note hits harder than the first.

If you have three accented notes in a row, even more so.

If you have four or more accented notes in a row, you reach saturation — the bucket doesn't fill indefinitely because the drain rate accelerates at high levels. The peak accent intensity stabilizes after about 3-4 consecutive accents.

This is all implemented in acidflow as a persistent `accent_level` floating-point variable. When an accented note triggers, `accent_level` gets pumped up. On every audio sample, `accent_level` decays exponentially toward zero. When any note triggers (accented or not), the current `accent_level` is added to the envelope depth.

## What accent sounds like

Set up a pattern of all identical notes (press `c` 16 times to get all C notes). Play it with no accents — a flat drone.

Now accent step 5 only. Press Space. You'll hear:

- Steps 1-4: normal, quiet, filter moving by whatever ENVMOD is set to.
- Step 5: louder, filter opens wider.
- Steps 6-9: gradually settling back to normal as the accent capacitor drains.

The accented note *and its tail* are all perceptibly different from the ambient level.

Now add another accent on step 9. Replay:

- Steps 1-4: normal.
- Step 5: accented (loud).
- Steps 6-8: settling.
- Step 9: accented again. Slightly louder than step 5, because the bucket hasn't fully drained.
- Steps 10-12: settling.

Now make a pattern of accents on steps 1, 2, 3, 4, rest-rest-rest-rest, 9, 10, 11, 12, rest-rest-rest-rest:

- Step 1: first accent, loud.
- Step 2: louder (bucket still full from step 1).
- Step 3: louder still.
- Step 4: even louder, approaching saturation.
- Steps 5-8: rest, but the bucket is draining.
- Step 9: by now the bucket is mostly empty, so step 9 is like a fresh first accent again.
- Etc.

You have a *dynamic contour* across the pattern, produced entirely by the accent placement.

## Why this matters musically

Accent is the 303's equivalent of a drummer's dynamics. A drummer doesn't hit every beat equally hard — they phrase. Louder here, softer there, a build, a release. The accent's capacitor-drain behavior gives the 303 the same kind of phrasing. You can write patterns that build in intensity across several steps by stacking accents, then release when the bucket drains.

This is also why acid patterns feel *organic* despite being rigidly quantized. The dynamics are dynamic. Even with identical pitches, identical CUTOFF, identical everything, an accented pattern feels like music; an un-accented pattern feels like a drum machine click track.

## The four canonical accent patterns

Over the decades, acid producers have evolved a handful of standard accent rhythms. These are worth internalizing because they're the vocabulary the genre speaks.

### 1. The offbeat accent

Accents on steps 3, 7, 11, 15 — the "and" of each beat.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Accent:  .  .  A  .  .  .  A  .  .  .  A  .  .  .  A  .
```

Syncopated, pushes against the 4-on-the-floor kick. The kick lands on beat 1, the accent lands on the and of beat 1. Feels propulsive.

### 2. The backbeat accent

Accents on steps 5 and 13 (beats 2 and 4) — aligned with the clap.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Accent:  .  .  .  .  A  .  .  .  .  .  .  .  A  .  .  .
```

Locks with the drum's backbeat. Feels solid, anchored. Good for verse sections.

### 3. The Acid Tracks pattern

Accents on 1, 4, 7, 10, 13, 16 — every third or fourth step, creating a rolling feel.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Accent:  A  .  .  A  .  .  A  .  .  A  .  .  A  .  .  A
```

Classic Phuture. The accents form their own rhythmic layer, almost a dotted-eighth pattern inside a sixteenth-note grid.

### 4. The crescendo

Accents on 1, 5, 9, 13, 14, 15, 16 — four regular accents, then an intensifying flurry at the end of the bar.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Accent:  A  .  .  .  A  .  .  .  A  .  .  .  A  A  A  A
```

Uses the capacitor-stacking to build tension at bar-end. Great for setting up transitions.

## Accent + pitch choices

Which *pitches* you accent matters enormously. A common rule: **accent the melodically-important notes**. If your pattern is mostly on the root with occasional visits to the 5th, accent the 5th visits. The accent tells the listener "this is where the melody is going."

Counter-intuitive corollary: **accent the surprising notes**. If your pattern jumps up an octave on step 9, accent step 9. The accent legitimizes the jump — without it, the jump sounds accidental; with it, it sounds intentional.

This is part of why the randomizer on acidflow biases toward accenting off-root notes. It's implementing this heuristic.

## Accent + slide

Slides and accents interact in specific ways:

- An **accented non-slid note**: full accent hit. Loudest, brightest.
- An **accented slid note**: accent is added, but because the note doesn't retrigger the envelope, the accent is *mid-flight* of the previous envelope. Effect is subtle — the slid note inherits some extra filter opening.
- A **non-accented note after an accent**: inherits the accent capacitor's remaining charge. Feels "warm from the previous accent."

The common technique is to accent the target of a slide. Slide from root up to 5th, accent the 5th. The slide delivers you to the accent. The accent announces the arrival. Then the pattern can return to the root without accent, as a release.

## ACCENT knob: intensity of each accent

The ACCENT knob on acidflow (in the Knobs panel, next to ENVMOD and DECAY) controls *how intense* an accent is — both the amp boost and the filter-envelope boost.

- **0% ACCENT**: accents are invisible. Functionally equivalent to no accents.
- **30% ACCENT**: accents are subtle. You can hear them but they're not dramatic.
- **50% ACCENT**: the 303 default. Accents add noticeable weight and filter opening.
- **75% ACCENT**: dramatic. Accents produce big filter sweeps and audibly louder notes.
- **100% ACCENT**: extreme. Accents and their capacitor-stacking dominate the pattern dynamics.

Most producers set ACCENT somewhere between 50% and 75%, then design patterns around that level.

## How accents work in acidflow's implementation

For reference, acidflow models accent as:

```
accent_level : float (persistent state)

on each audio sample:
  accent_level *= exp(-sample_dt / ACCENT_TAU)   // drain

on accented note trigger:
  accent_level += ACCENT_PUMP * ACCENT_knob      // pump

on note trigger (any):
  effective_envmod = ENVMOD_knob + accent_level
  effective_amp    = AMP_base    + accent_level * amp_accent_ratio
```

ACCENT_TAU is approximately 150 ms. ACCENT_PUMP is approximately 0.8. These values are tuned to match real-303 behavior. The net effect: an accent's "tail" is audible for about 300-500 ms after the accented note, and consecutive accents stack noticeably but saturate after 3-4.

## Practical accent advice

1. **Start with 2-4 accents per bar**, not 10. Fewer accents make each one feel special.
2. **Put accents on off-beats** by default (steps 3, 7, 11, 15). This is the default "acid" feel.
3. **Don't accent every note** — it's the same as accenting none, just louder.
4. **If you want a busier pattern to feel active, add accents; if you want it to feel driving, remove some**. Accents add variation; uniform accenting adds aggression.
5. **Build accent clusters** for tension. Three accents in a row creates a crescendo.
6. **Put accents where you want the listener's ear** — they're attention markers.

## Accent and the mix

Accents do more than shape the 303 voice — they shape its relationship to the drums. A well-accented 303 line sits in the mix differently than a flat one because the accented notes have more presence. In practice:

- A 303 with good accents can sit behind the drums and still be heard, because the accents punch through.
- A 303 without accents needs to be louder to be audible, which makes the whole mix feel cluttered.

Good accent design is thus partly a *mixing* decision — you're using accents to let the 303 occupy a specific slot in the frequency and dynamic space without needing to push its fader up.

## Try this

1. Load preset 07 (Plastikman). Play. Notice how few accents it has. Count them. (Probably 2-3 per bar.)
2. Add accents on steps 3, 7, 11, 15 (four offbeats). Listen to how the character shifts. Suddenly much more rolling.
3. Remove those accents. Add accents on steps 1, 2, 3, 4 (four in a row). Listen to the capacitor-stack build.
4. Turn ACCENT knob from 50% to 100%. Dramatic difference.
5. Turn ACCENT knob to 0%. The pattern feels lifeless.
6. Restore accents and ACCENT = 60%. Now try changing DECAY while the pattern plays. Notice how long DECAY makes accents "bloom" more, short DECAY makes them snap.

Accent is the most *musical* control on the synth. Spend time here. It's the difference between making a 303 line and making an acid line.

Next chapter: slides.
