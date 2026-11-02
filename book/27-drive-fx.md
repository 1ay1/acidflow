# 27 · Overdrive: harmonic character

The FX rack has three units: overdrive, delay, reverb. This chapter is about the first — overdrive, also called drive or saturation. It's the simplest of the three and also the most misunderstood.

## The two drives

acidflow has TWO drive controls:

1. **OD DRIVE** (in the FX rack): saturation of the oscillator *before* the filter. It distorts the raw saw or square, and the filter then shapes that richer signal.
2. **Voice DRIVE** (in the Knobs panel, Chapter 14): saturation of the voice's *output*, after the filter and the amp envelope.

Both apply to the 303 voice only. The drums don't go through either; they join the mix afterwards and go straight to the delay and reverb.

They do similar things (both saturate) but at different points, so they sound different, and they compound. You can use both together for extreme saturation.

This chapter focuses on OD DRIVE. For Voice DRIVE details, see Chapter 14.

## What OD DRIVE does

OD DRIVE sits between the oscillator and the filter. It's a waveshaper — a non-linear function that passes small signals clean and saturates large signals.

The specific shape is a **tanh curve**:

```
output = tanh(input × drive_amount) / tanh(drive_amount)
```

Where `drive_amount` goes from 1.0 (no drive) to 6.0 (full drive). The normalization by `tanh(drive_amount)` keeps the overall level consistent — you don't get louder when you drive harder; you just get more saturated. (Under the hood it uses an anti-aliased tanh, so even heavy settings don't turn into digital fizz.)

Tanh is a soft-clipper. Small inputs pass linearly (low distortion). Large inputs get squashed (heavy distortion). The "knee" between these two regions is smooth, not sharp.

## The musical effect

Drive does three audible things:

1. **Adds harmonics**. A pure tone fed through saturation comes out with added harmonics. These are musical — they add richness without sounding "digital."
2. **Compresses peaks**. Loud signals get clipped smoothly.
3. **Warms the sound**. The added harmonics sit at musically-consonant intervals, which the ear hears as "warmth" or "fatness."

Because OD DRIVE comes *before* the filter, the filter gets more harmonic content to work with. The resonance has more to ring against, so the squelch gets louder and nastier. Combined with high RES, heavy OD = acid-techno scream.

## OD DRIVE levels

- **0% (minimum)**: no drive. The oscillator goes into the filter clean.
- **25%**: subtle warmth. You can hear it but it's not dramatic.
- **50%**: noticeable saturation. The saw starts to have grit.
- **75%**: heavy saturation. Clear distortion character.
- **100%**: extreme. "303 into a fuzz pedal."

Most productions use 15-40% OD DRIVE. Acid techno pushes to 50-70%. Above 75% is experimental or aesthetic choice.

## When to use OD DRIVE

### As an aggressive signature

Heavy OD DRIVE (60-80%) is the Hardfloor / Stay Up Forever sound. The 303 is hot, saturated, grinding. Used on purpose for that aggressive acid techno feel.

### To make the resonance pop

Drive enhances the resonance peak. If you have a filter sweep and the resonance isn't popping through, adding OD DRIVE gives the filter more to resonate with and makes the peak much more audible.

### For a pedal-in-front-of-the-synth feel

A lot of classic acid was a 303 run through a cheap distortion pedal or a hot mixer channel, with the filter still sweeping. OD DRIVE at 20-40% gets you that.

## When NOT to use OD DRIVE

### If the 303 already has heavy Voice DRIVE

Two drive stages compound. If Voice DRIVE is at 75% and OD DRIVE is at 75%, you're double-driving. The sound will be over-processed — harmonic clutter, loss of definition.

Pick one as the main character: OD DRIVE to change how the filter behaves, or Voice DRIVE to squash and bite the finished note. Usually not both heavy.

### With a low CUTOFF

Drive + low CUTOFF = muddy. The drive adds harmonics the filter immediately removes, leaving a buzzing low-mid mess. Open the filter up a bit first.

### As a mastering tool

OD DRIVE is a creative tool for the bass, not a mastering tool. It doesn't touch the drums and won't make the track louder — acidflow's look-ahead limiter already holds the master at −1 dBFS. If your track isn't loud enough at export, push the exported WAV through a proper mastering chain.

## OD DRIVE and RES

Drive + high RES = screaming acid. This is the acid techno signature. If you want Hardfloor, this is it.

Drive + low RES = warm acid. Adds body without screaming. Good for dub-acid.

## The drive is before delay and reverb

The 303's whole chain runs before the FX sends:

```
Oscillator → OD DRIVE → Filter → VCA → Voice DRIVE ──┐
Drums ───────────────────────────────────────────────┴→ Delay → Reverb → Limiter → Output
```

So the delay and reverb process the already-driven bass. This means:

- Delay repeats are already saturated. Each repeat is drive-flavored (and the tape delay adds a little saturation of its own as repeats build up).
- Reverb tails carry the drive's harmonics.

There's no way to drive *after* the effects in acidflow.

## Drive as loudness perception

A subtle fact: a driven signal is perceived as louder than a clean signal at the same RMS level. Saturation puts more energy in the upper mids, where the ear is most sensitive.

Practical implication: adding OD DRIVE makes the bass feel louder at the same VOL setting. That's free perceived loudness — but it's also why heavily driven tracks are tiring after a few minutes.

## Two drive strategies

**Strategy 1: Drive the filter**. Turn OD DRIVE up (30-60%), leave Voice DRIVE low. The filter has more to chew on; the squelch screams.

**Strategy 2: Drive the output**. Leave OD DRIVE low, turn Voice DRIVE up (40-70%). The filter sounds normal, but every note gets squashed and bitey, and accents slam.

Combining both is fine in moderation. Avoid both at maximum.

## The drive visualization

You can watch the scope while adjusting drive. At low drive, the waveform looks like a standard saw (ramp up, snap down). At high drive, the waveform's peaks get rounded — it starts to look more like a square wave.

At extreme drive, the saw becomes essentially a square. This is why drive "fattens" a saw: the energy moves toward the mids, where the ear is most sensitive, producing a "fat" feel.

## OD DRIVE and loudness wars

A philosophical note. Modern electronic music is often mastered extremely hot — near-maximum volume, heavily compressed, heavily limited. This "loudness war" approach can sound impressive at low volumes but tires listeners at high volumes.

acidflow doesn't encourage this. OD DRIVE tops out at a 6× pre-gain, and the master limiter only catches peaks at −1 dBFS — it isn't a loudness maximiser. If you want loud, push your exported WAV through a DAW's mastering chain.

For *listening* experience, softer drive is usually better than heavier drive. Don't drive everything to 80% because it "sounds bigger." Drive it to 30% because that sounds *right*.

## Try this

1. Play any preset. Set all FX to 0 (OD, delay MIX, reverb MIX). Clean signal.
2. Raise OD DRIVE to 25%. Play. Notice subtle warmth going into the filter.
3. Raise to 50%. The squelch gets louder and grittier.
4. Raise to 80%. Aggressive. Possibly too much.
5. Back to 0. Now set Voice DRIVE (in the Knobs panel) to 50%. Play. A different kind of drive — the filter is clean, the output is squashed.
6. Add 25% OD DRIVE on top. Both stages driving. Compound effect.
7. Compare: OD DRIVE 75% with Voice DRIVE 0%, then Voice DRIVE 75% with OD DRIVE 0%. Both are aggressive, but the first changes the filter's character and the second changes the note's punch.

Next: delay.
