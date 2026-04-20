# 27 · Overdrive: harmonic character

The FX rack has three units: overdrive, delay, reverb. This chapter is about the first — overdrive, also called drive or saturation. It's the simplest of the three and also the most misunderstood.

## The two drives

acidflow has TWO drive controls:

1. **Voice DRIVE** (in the Knobs panel, Chapter 14): pre-filter saturation of the 303 voice. Shapes the voice's tone before filtering.
2. **FX DRIVE** (in the FX rack): post-filter saturation of the master bus. Shapes the sum of 303 + drums after filtering.

They do similar things (both saturate) but at different positions in the signal chain. Their effects compound — you can use both together for extreme saturation.

This chapter focuses on FX DRIVE. For Voice DRIVE details, see Chapter 14.

## What FX DRIVE does

In the FX rack, the FX DRIVE applies to the sum of 303 + drums + whatever has come through the voice so far. It's a waveshaper — a non-linear function that passes small signals clean and saturates large signals.

The specific shape is a **tanh curve**:

```
output = tanh(input × drive_amount) / tanh(drive_amount)
```

Where `drive_amount` goes from 1.0 (no drive) to ~10.0 (heavy drive). The normalization by `tanh(drive_amount)` keeps the overall level consistent — you don't get louder when you drive harder; you just get more saturated.

Tanh is a soft-clipper. Small inputs pass linearly (low distortion). Large inputs get squashed (heavy distortion). The "knee" between these two regions is smooth, not sharp.

## The musical effect

Drive does three audible things:

1. **Adds harmonics**. A pure tone fed through saturation comes out with added 2nd, 3rd, 4th harmonics. These harmonics are musical — they add richness without sounding "digital."
2. **Compresses peaks**. Loud signals get clipped smoothly. This reduces dynamic range and makes everything feel more "average" in level.
3. **Warms the sound**. The added harmonics are at musically-consonant intervals (octaves and fifths), which ear perceives as "warmth" or "fatness."

On the 303 voice specifically, drive makes the filter's squelch even more pronounced, because the pre-filter saturation produces more harmonic content for the filter to work with. Combined with high RES, heavy drive = acid-techno-scream.

On the drums, drive makes them grind together. The transient peaks of kicks get softened. The noise of hats fuses with the 303 squelch.

## FX DRIVE levels

- **0% (minimum)**: no drive. Audio passes through without saturation.
- **25%**: subtle warmth. You can hear it but it's not dramatic.
- **50%**: noticeable saturation. Sound starts to have grit.
- **75%**: heavy saturation. Clear distortion character.
- **100%**: extreme. The sound is dominated by saturation.

Most productions use 15-40% FX DRIVE. Acid techno pushes to 50-70%. Above 75% is experimental or aesthetic choice.

## When to use FX DRIVE

### As a "glue"

The FX DRIVE can make drums and 303 sound like they were recorded together, rather than separate synthesized voices. Apply a moderate drive (20-30%) and the mix feels more cohesive.

### As an aggressive signature

Heavy FX DRIVE (60-80%) is the Hardfloor / Stay Up Forever sound. Everything is hot, saturated, grinding. Used on purpose for that aggressive acid techno feel.

### To simulate tape or analog

Tape recordings and analog gear have their own saturation characteristics. FX DRIVE (at low settings, 10-20%) approximates the effect of running the mix through tape. Subtle but adds "produced" character.

### For single-frequency emphasis

Drive enhances the resonance peak. If you have a filter sweep and the resonance isn't popping through, adding FX DRIVE can make the resonance much more audible.

## When NOT to use FX DRIVE

### If the 303 already has heavy Voice DRIVE

Two drive stages compound. If Voice DRIVE is at 75% and FX DRIVE is at 75%, you're double-driving. The sound will be over-processed — harmonic clutter, loss of definition.

Pick one: Voice DRIVE for pre-filter saturation (affects how the filter behaves), or FX DRIVE for post-mix saturation (affects the whole track). Usually not both heavy.

### If you have clear drums and clear 303 that should stay separate

FX DRIVE glues them together. If your production aesthetic requires them to sound distinct (e.g., drums with their own character, 303 with its own), avoid heavy FX DRIVE.

### At mastering

FX DRIVE is a creative tool, not a mastering tool. Don't use it to "make the master louder" — that's what a limiter does, and acidflow doesn't have one. If your track isn't loud enough at export, the right fix is to raise the VOL knob until it's appropriately loud without clipping, not to drive harder.

## FX DRIVE and the 303

The 303 voice is usually the dominant element in the mix. When you drive the FX bus, the 303 gets driven most.

Drive + high RES = screaming acid. This is the acid techno signature. If you want Hardfloor, this is it.

Drive + low RES = warm acid. Adds body without screaming. Good for dub-acid.

Drive + low CUTOFF = muddy. The drive adds harmonics in frequencies the filter is already attenuating, creating a buzzing low-mid mess. Usually avoid.

## FX DRIVE and the drums

The kick is the loudest drum voice. Drive hits it hardest:

- **Kick + drive**: the kick's transient click gets shaped. Can sound punchier or can sound like it's cracking. Test carefully.
- **Snare + drive**: snare noise gets enhanced. Sounds fatter.
- **Hats + drive**: closed hats sound almost unchanged (they're mostly noise already). Open hats get more "crunch."
- **Clap + drive**: clap's multiple bursts blur together into a more unified slap.

If you don't like what drive does to the drums, the drum bus gain is lower, and the 303 has enough signal to saturate by itself, so only the 303 gets driven meaningfully.

## The drive is before delay and reverb

In the FX rack signal chain:

```
Master → FX DRIVE → Delay → Reverb → Output
```

So the delay and reverb process the already-driven signal. This means:

- Delay repeats are already saturated. Each repeat is drive-flavored.
- Reverb tails are driven. The reverb "wash" has harmonic content from the drive.

If you want clean delay/reverb on driven signal, that's what you get. If you want driven delay/reverb (drive *after* the effects), acidflow doesn't support that order.

## Drive as loudness perception

A subtle fact: a driven signal is perceived as louder than a clean signal at the same RMS level. This is because saturation increases the RMS energy in the upper-mid frequencies (where the ear is most sensitive), even if the peak level stays the same.

Practical implication: adding FX DRIVE will make your track feel louder at the same VOL setting. This is free perceived loudness — but it's also why heavily driven tracks sound exhausting after a few minutes. Your ear gets fatigued by the constant upper-mid energy.

## Two drive strategies

**Strategy 1: Drive at the voice**. Turn Voice DRIVE up (50-75%). Leave FX DRIVE at 0 or low. The 303 is heavily saturated; drums are clean. Works well when you want a screamy 303 over clean drums.

**Strategy 2: Drive at the master**. Leave Voice DRIVE low. Turn FX DRIVE up (30-50%). The whole mix is moderately saturated. Glues everything together.

Combining both is fine in moderation. Avoid both at maximum.

## The drive visualization

You can watch the scope while adjusting drive. At low drive, the waveform looks like a standard saw (ramp up, snap down). At high drive, the waveform's peaks get rounded — it starts to look more like a square wave.

At extreme drive, the saw becomes essentially a square. This is why drive "fattens" a saw — the square has fewer high-frequency harmonics than a saw, so the distortion is concentrated in the mid range where the ear is most sensitive, producing a "fat" feel.

## FX DRIVE and loudness wars

A philosophical note. Modern electronic music is often mastered extremely hot — near-maximum volume, heavily compressed, heavily limited. This "loudness war" approach can produce impressive-sounding mixes at low volumes but fatigue listeners at high volumes.

acidflow doesn't encourage this. FX DRIVE is capped at a reasonable level (about 10× linear gain before tanh saturates), and there's no master limiter. If you want loud, push your exported WAV through a DAW's mastering chain.

For *listening* experience, softer drive is usually better than heavier drive. Don't drive everything to 80% because it "sounds bigger." Drive it to 30% because that sounds *right*.

## Try this

1. Play any preset. Set all FX to 0 (drive, delay mix, reverb mix). Clean signal.
2. Raise FX DRIVE to 25%. Play. Notice subtle warmth.
3. Raise to 50%. Noticeable character change.
4. Raise to 80%. Aggressive. Possibly too much.
5. Back to 0. Now set Voice DRIVE (in knobs) to 50%. Play. Different kind of drive — pre-filter.
6. Add 25% FX DRIVE on top. Both stages driving. Compound effect.
7. Compare: Voice DRIVE 0%, FX DRIVE 75% vs Voice DRIVE 75%, FX DRIVE 0%. Both give aggressive sound, but different flavors. The former drives everything equally; the latter drives the 303 filter harder.

Next: delay.
