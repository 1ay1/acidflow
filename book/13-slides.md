# 13 · Slides — the glue of acid melody

After accent, slide is the second most important per-step modifier. Accents provide dynamics. Slides provide *connection*. Together, they form the expressive language of the 303.

## What a slide is

A slide (also called **portamento** or **glide** or **legato**) is a continuous pitch movement from one note to the next, as opposed to an instantaneous jump. On the 303, slides are per-step: each step has a slide flag, and when that flag is on, the pitch glides into this step from the previous step's pitch.

acidflow uses an **exponential RC curve** for slides, matching the real 303's analog behavior. The time constant is τ ≈ 88 ms. This means:

- A slide is fast at first, slowing as it approaches the target pitch.
- About 63% of the slide completes in the first 88 ms.
- About 95% is done by 250 ms.
- The slide never *quite* reaches the target in finite time, but is indistinguishable within ~300 ms.

At 120 BPM, a 16th note is 125 ms. So a slide that starts at step 5 is mostly complete by the time step 6 arrives. At faster tempos, the slide might still be in motion when the next step triggers — producing continuous sliding across multiple steps.

### The curve matters

A linear slide (constant rate of pitch change) sounds **robotic**. The RC curve — fast start, slow end — is what the human voice does when sliding between notes. It's what a bass player's fretting finger does. It's what a trombone does. The RC curve is the biologically-natural pitch-glide shape, which is why it feels *vocal*.

This is non-negotiable. If you ever use a synth that does linear slides and wonder why it doesn't sound like a 303, the curve shape is the reason.

## When does a slide happen?

On acidflow, toggle the slide flag on a step with the `s` key in the sequencer. That step now has a slide flag. When the sequencer plays *into* that step from the previous note, the pitch glides from the previous note's pitch to this step's pitch.

A slide requires a previous note to glide from. If the previous step was a rest, there's nothing to slide from, and the note just starts at its pitch like any other. (Edge case in acidflow: we handle slide-after-rest gracefully by treating it as a non-slid note.)

If the current step is itself a rest, the slide flag does nothing.

## Slides suppress envelope retrigger

This is a critical point, already mentioned in Chapter 11 but worth emphasizing:

**A slid note does NOT retrigger the envelope.**

The filter envelope and the amp envelope continue from wherever they were at the end of the previous note. The filter doesn't re-snap open. The amp doesn't re-punch to full.

The effect: slid notes sound **smoother** and **quieter** than non-slid notes. The attack transient of the filter opening is absent. Instead, the pitch smoothly glides under a continuing envelope.

This is why slid notes feel like *part of the previous note* rather than new notes. The 303 is deceptively suggesting a connected line of melody rather than a string of discrete notes.

## What slides do musically

Three main musical functions:

### 1. Melodic connection

If your pattern jumps from C2 to C3 (an octave up), the ear hears two distinct notes. If you slide into C3, the ear hears the *journey* from C2 to C3 — the pitch ascending — plus the arrival at C3. This adds musical movement to a pattern that would otherwise be stepwise.

Acid bass lines use this extensively. Rather than just stating pitches, they *trace* melodic contours by sliding between them. Sliding up by an octave feels like a dramatic gesture; sliding up by a fifth feels like a resolution; sliding up by a minor second feels like a menacing approach.

### 2. Legato smoothing

If you have a dense pattern and it feels too *pulsed* (too many separate attacks), adding slides between some notes smooths it out. Each slid note merges into the previous note's envelope, so the pattern feels less like 16 discrete hits and more like a flowing line.

### 3. Pitch-bend effects

If you slide into a note that's far from the previous note (say, slide from C2 up to C4, a two-octave jump), the slide becomes an audible pitch bend — a sweep up from low to high. Used aggressively, this is a classic acid gesture, especially at the start of phrases or after rests.

## Slide placement patterns

Some conventions acid producers have evolved:

### Slide to the target of a leap

When the pattern jumps to an octave or a 5th, slide into the target. This tells the listener "we're going somewhere" and makes the arrival feel intentional. Without the slide, the leap feels abrupt.

```
Pattern (no slides): C2  C2  C3  C2
                         ^^ abrupt octave jump

Pattern (with slide): C2  C2  C3  C2
                              ^ slide flag
                         ^^ now a smooth rise into C3
```

### Slide on every step (drone-like)

Some acid lines — especially Plastikman — mark *every* step as a slide. The result is that the entire pattern is a continuous pitch curve rather than a series of note attacks. The envelope retriggering stops; the amp sustains across the whole bar. This produces a drone-like, sustained character that's hypnotic.

```
Pattern (all slides):  C2   D#2  F2   G2
                         ∿   ∿    ∿    ∿
```

Try this on acidflow: build a pattern, then press `s` on every step. Play. The character is fundamentally different — more singing, less rhythmic.

### Slide to punctuate rests

If your pattern has a rest followed by a note, consider making the *next note after the rest* a slide. Since there's no previous note to slide from (the rest), the slide flag does nothing, *except* in acidflow's semantics, which handle slide-after-rest sensibly. In practice: don't count on slides after rests doing anything; structure your slides around continuous note runs.

### Slide and release

The inverse of "slide to the leap target" — sometimes slide *away* from a held note. E.g., hold a root for a few steps, then slide up to the 5th, then *slide back down* to the root. The return slide adds a "swoop back" feeling.

```
Pattern:  C2  C2  C2  C2  G2  G2  C2  C2
Slide:    .   .   .   .   S   .   S   .
```

The slides are on steps 5 (into G2) and 7 (into C2). You get a smooth rise to G2, a pause at G2, then a smooth descent back to C2.

## Slides and accents together

The combination is where 303 patterns come alive:

- **Slide + accent**: the slide glides up to a loud, bright note. Very dramatic. Classic "slide into the accented 5th" move.
- **Slide without accent**: smooth connection, doesn't draw attention to itself. Glue between melodic statements.
- **Accent without slide**: punchy standalone note. Rhythmic emphasis.
- **Neither**: the default rhythmic step.

A typical dense acid bar might have 6-8 accents, 3-5 slides, with overlaps (some steps both accented and slid). The pattern has dynamic contour (from accents), melodic contour (from pitches), and connecting gesture (from slides).

## How slides interact with probability and ratchet

acidflow adds two features the real 303 didn't have: probability and ratchet.

- **Probability**: if a step has probability < 100%, the note has a random chance of being skipped. If it's skipped, the slide flag is effectively moot (no note to slide into).
- **Ratchet**: a step set to ratchet ×2, ×3, or ×4 plays multiple sub-notes in the time of one step. Slides on ratcheted steps apply to the first sub-note only — subsequent sub-notes within the step re-trigger normally. This produces a "slide then rapid-fire" effect that's musically useful.

These are covered in Chapter 19 and 20.

## The one-slide-per-bar trick

A compositional technique: write a mostly non-slide pattern, then add exactly ONE slide somewhere unexpected. The single slide becomes the pattern's signature gesture — the one moment per bar where the line connects rather than hits.

This works because the ear normalizes to the pattern's average behavior. A pattern where every step is slid feels smooth throughout. A pattern where one step is slid feels like it has *one moment of smoothness* in a sea of hits, which the ear latches onto.

Try this: turn off all slides in your pattern. Now add one slide, ideally on an off-beat or a step that's not part of a regular pattern. Play. That one slide is now a compositional event.

## The inverse: remove a slide to create punctuation

If your pattern has many slides, *removing* one creates the opposite effect — a single moment of abrupt attack in a smooth sea. Plastikman uses this a lot.

## Slide direction: upward vs downward

Slides can go up (glide from a lower pitch to a higher one) or down (glide from higher to lower). Both work the same mechanically; they feel different musically.

- **Upward slides** feel like *rising action* — tension, approach, anticipation.
- **Downward slides** feel like *releasing* — descent, settling, resolution.

A common gesture: slide up into a note, then immediately slide back down. Trace-and-return. Very vocal.

## When not to slide

Slides are addictive. A common mistake is adding too many of them. Symptoms:

- The pattern feels squishy, no rhythmic bite.
- The accents stop being audible because their envelopes aren't retriggering crisply.
- The filter stops opening on each note, so ENVMOD stops having dramatic effect.

If your pattern feels flat despite good accents and a moving filter, try *removing* some slides. The non-slid notes will retrigger the envelope, restoring the bite.

## Slides in acidflow vs the real 303

Two differences worth noting:

1. **Slide time is fixed in acidflow** at τ ≈ 88 ms. The real 303's slide time varies slightly with component temperature and has a component-tolerance spread. Most plug-in emulations also fix the slide time; it's not musically audible as a difference.

2. **Slide curve is perfect exponential in acidflow**. The real 303 has tiny asymmetries in the RC slide curve due to analog component behavior. Inaudible in practice; immeasurable in normal listening conditions.

Neither difference matters. acidflow slides sound right.

## Try this

1. Load preset 15 (Liquid) — it's the one designed around slides. Play. Notice how every note flows into the next. Barely any attack transients.
2. Press `s` on step 1 to toggle its slide off. Play. Notice the lone punch of step 1.
3. Press `s` on steps 5 and 13 (beats 2 and 4). Play. Now you have a pattern with rests from slides on two specific beats.
4. Load preset 01 (Acid Tracks). Add a slide on step 5 (the second beat). Play. Notice how this single slide changes the pattern's character.
5. Write a pattern from scratch: 8 notes on steps 1-8, all same pitch. Add slides on steps 2, 4, 6, 8. Play. This is a "partial drone" — half the steps retrigger, half slide. Classic.
6. Now slide every step. Play. Pure drone/sustained character.

Slides are the connector between your pitches and your accents. Master these three controls — pitch, accent, slide — and you have most of acid's expressive vocabulary.

Next: the last of the 303-voice knobs, DRIVE and VOL, and the concept of gain staging.
