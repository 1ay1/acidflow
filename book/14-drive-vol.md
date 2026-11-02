# 14 · Drive, volume, and gain staging

Two small-seeming knobs. DRIVE and VOL. Both deal with amplitude. But one of them fundamentally shapes your tone, and the other is just the output fader, and they're not interchangeable.

This chapter explains the difference, and why gain staging (how loud each stage is relative to the next) matters more than beginners realize.

## VOL: the output fader

VOL is the simple one. It scales the 303 voice. 0% = silence. 100% = full. 50% = half amplitude. It's applied with the amp envelope, after the filter and just *before* DRIVE, so a higher VOL also pushes DRIVE a little harder, the way turning up a real 303 makes its output stage crunch.

VOL does nothing to the *tone* of the sound — it just scales the amplitude. If the signal is saturated (at 1.0 or clipping), VOL can turn it down to an unsaturated level. If the signal is quiet, VOL can turn it up. But the filter character, the envelope shape, the accent dynamics are all set at this point and VOL just controls "how much of all that to send out."

### When to adjust VOL

- **Balancing against drums**: if the 303 is too loud relative to the drum bus, lower VOL. If it's too quiet, raise it.
- **Avoiding clipping at export**: WAV export can clip if VOL is at 100% and the 303 is hot. Pull it back to 80-85% for safety.
- **Intentionally dimming**: if the 303 is just a background element in a specific section, lower VOL.

That's it. VOL is not a creative tool. It's a level control.

## DRIVE: the creative tool

DRIVE is completely different. DRIVE is **output-stage saturation**: after the filter and the amp envelope, the voice is pushed into a soft clipper, the way a real 303's output stage compresses before it hits the rail.

### What saturation does

Saturation (also called "soft clipping" or "waveshaping") is a non-linear amplifier. Small signals pass through linearly. Large signals get *compressed* — the peaks are rounded off instead of just being louder.

Plotted (output vs input):

```
out
 1.0 |         ____________
     |     ___/
     |   _/
     |  /
     | /
 0.0 |/
     |
     0              input →              1.0
```

Without saturation, output = input. With saturation, small inputs still equal their outputs, but large inputs get limited — doubling the input at large levels doesn't double the output.

The musical effect of saturation is **harmonic enrichment**. A pure sine wave fed through saturation comes out with added 2nd, 3rd, 4th harmonics (the saturation creates these). A saw wave fed through saturation comes out *buzzier*, *grittier*, and with a fuller low-mid spectrum because the saturation adds and enhances harmonics that the filter can then shape.

### DRIVE's placement in the signal chain

In acidflow, DRIVE comes *after* the filter and the amp envelope:

```
Oscillator → (OD) → Filter → Envelope VCA → DRIVE → Output
```

This matters. Post-VCA drive squashes the *finished* note: accents and resonance peaks get rounded off and gain bite, without changing how the filter itself behaves. If you want to distort the signal *going into* the filter, so the resonance rings against the extra harmonics, that's the OD DRIVE in the FX rack (Chapter 27).

On the real 303, the built-in drive character is a quirk of the output stage (it saturates at high levels), plus any external effects the producer added. acidflow models that output stage as the DRIVE knob.

### DRIVE settings by character

- **0% DRIVE**: clean. No saturation. Sound is the pure oscillator through filter. Useful if you want to apply external distortion and don't want internal saturation piling on.
- **25% DRIVE**: subtle warmth. Low-level saturation adds a small amount of harmonics. You can hear it as a "thicker" character but it's not aggressive.
- **50% DRIVE**: clear saturation. The sound starts to have a bit of grit. Still musical, not yet aggressive.
- **75% DRIVE**: heavy saturation. The sound has obvious drive character. Good for techno. Some producers leave it here constantly.
- **100% DRIVE**: extreme. The saturation is dominating. The pure saw/square character is gone; what you hear is a gritty, harmonically-complex tone.

Most classic acid records use 25-50% DRIVE. Acid techno pushes higher. Ambient acid uses lower.

## DRIVE + filter: the interaction

Here's why DRIVE is non-trivial: it changes how the filter *sounds*, even though it comes after it.

**Without drive**, the filter's output passes through clean. The resonance peak is clearly defined. The filter sweep sounds like a filter sweep.

**With drive**, the resonance peak is the loudest part of the signal, so it's the first thing the saturator squashes. The peak gets rounder and adds its own harmonics. The filter sweep sounds *fatter* and *more aggressive*, and accents bite harder.

Specifically: adding DRIVE makes the same CUTOFF + RES settings feel more extreme. A moderate RES (60%) with 50% DRIVE sounds grittier than the same RES clean. To feed *distortion into* the filter instead, so the resonance rings against it, use OD DRIVE in the FX rack.

Practical implication: **if you add DRIVE, you may need to lower RES** to maintain the same apparent character. Otherwise the sound will get harsher than you intended.

## Gain staging

"Gain staging" is a mixing term. It means setting the right level at each stage of your signal chain so that:

1. Each stage has enough signal to overcome noise.
2. No stage clips.
3. The final output is at a sensible level.

In acidflow, the stages are roughly:

```
Oscillator [-1, +1] → OD DRIVE → Filter → VCA × VOL → DRIVE → Delay → Reverb → Limiter → Master
```

Each stage has its own implicit gain based on the knobs you set. Good gain staging means being aware of what each stage does to the level.

### Common gain-staging mistakes

**Everything at maximum**: Oscillator is hot (hits +1, -1). Filter doesn't reduce much. DRIVE saturates. Delay feeds back heavily. Reverb is 100% wet. The limiter is working flat out. Sound is loud and distorted, but not in a good way — it's brick-walled, lacks dynamics, and listeners will turn it down.

**Everything at minimum**: Oscillator is weak. Signal barely makes it through the filter. Envelope is applying to a quiet signal. Output is inaudible at sensible listening volumes. You're burying the instrument.

**Good gain staging**: Oscillator is hot but not clipping. DRIVE is moderate. Filter shapes without crushing. Delay and reverb are returned at reasonable mix levels. VOL is set so the output is loud but has headroom. The limiter only catches the odd peak. Sound is punchy, with dynamic range.

### A practical gain-staging checklist

1. Set VOL to 75% as a baseline. Don't go above 85% unless you know what you're doing.
2. Set DRIVE on the voice to 25-50%. This gives you harmonic richness without dominating.
3. Keep RES below 95% unless you *want* self-oscillation (not just tolerate it).
4. In the FX rack, keep delay MIX and reverb MIX below 40%. Wet FX should support, not dominate.
5. Master output should land around -6 dB RMS with peaks around -3 dB. If you're clipping the master, pull VOL down first, then individual voices.

## DRIVE on the voice vs DRIVE in the FX rack

Wait — there's a DRIVE in both places?

Yes. Two different knobs:

- **Voice DRIVE** (Chapter 14, this one): output saturation on the 303 voice, after the filter.
- **OD DRIVE** (Chapter 27): saturation on the 303 voice *before* the filter.

They do similar things but at different places in the chain. OD DRIVE changes what the filter is fed, so the resonance rings against the distortion. Voice DRIVE squashes the finished note after the filter.

Can you use both? Yes — and this is often *the* technique for getting a dense acid sound. Voice DRIVE makes the filter scream. OD DRIVE makes the whole mix grind. Together they produce a sound that's fat, harmonically rich, and unmistakably acid-techno-ish.

Use voice DRIVE for character. Use OD DRIVE for cohesion. If you're not sure which, start with voice DRIVE and only add OD DRIVE if the mix feels weak.

## The volume/clipping warning

One technical gotcha worth mentioning. acidflow's audio path is floating-point internally, so it doesn't "clip" in the traditional sense until the final output stage. But when the signal hits the output device (your sound card, speakers), it must be converted to a bounded range ([-1, +1]). Anything above or below gets hard-clipped by the DAC.

Hard clipping sounds *bad*. It's not musical saturation — it's digital distortion. Frequencies above Nyquist alias down. Peaks are squashed. The sound loses definition.

If you hear harsh crunchy distortion that doesn't sound like intentional drive, you're probably hard-clipping. Pull VOL down 20% and see if it goes away.

A safer approach: keep master peaks around -3 dB or lower. This gives headroom for the DAC conversion and any FX spillover.

## The relationship: VOL vs DRIVE

Summary:

| Control | Where in chain  | What it does                 | When to use               |
|---------|----------------|------------------------------|---------------------------|
| DRIVE   | Pre-filter     | Adds harmonic saturation     | Tone shaping              |
| VOL     | Post-everything | Scales output amplitude     | Level balancing           |

They are *not* substitutes. If you want more volume, turn up VOL. If you want more *character*, turn up DRIVE. These are different tools.

## Try this

1. Set DRIVE to 0%, VOL to 50%. Play. Listen to the clean character.
2. Set DRIVE to 50%. Play. Notice the added grit, warmth.
3. Set DRIVE to 100%. Play. Aggressive.
4. Now set DRIVE back to 50% and lower VOL to 30%. Same character, just quieter.
5. Set VOL to 100%. If your speakers allow, this is probably too loud — turn down your main volume, not VOL.
6. Set DRIVE to 75%, RES to 80%. Very aggressive. Now lower RES to 60%. Still feels aggressive because DRIVE is fattening everything.
7. Try a long filter sweep (mouse drag on CUTOFF while playing) with DRIVE at 0% vs DRIVE at 75%. Notice how the sweep with drive sounds more substantial.

Part II is done. You've now mapped every knob on the 303 voice panel. Nine controls, each with its own chapter, each with its own purpose. If you remember nothing else:

- **CUTOFF and RES** define the filter's state.
- **ENVMOD and DECAY** define how the envelope moves that state.
- **ACCENT** is the dynamic element that makes patterns feel alive.
- **Slides** are the melodic connective tissue.
- **TUNE and WAVE** set the oscillator's basic territory.
- **DRIVE** shapes the tone via saturation.
- **VOL** is just a level control.

With these nine controls and the rhythm theory from Part I, you can already make a complete 303 track. The next part — the sequencer — is about how to use the steps and modifiers inside a pattern to write musically, rather than just randomly.
