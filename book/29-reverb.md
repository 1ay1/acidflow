# 29 · Reverb: room, plate, space

Reverb is the third effect in the FX rack. Where delay gives you discrete echoes, reverb gives you a diffuse wash — the sound of a space. Reverb is how you take a signal recorded in a tiny dry environment (a sequencer's output) and make it feel like it's in a room, a hall, a plate, a cathedral.

Reverb is the most subtle of the three FX and, used carelessly, the most destructive. A little gives depth; too much turns everything to mud.

## What reverb is

When a sound is made in a room, the direct sound reaches your ears first. Then, microseconds later, the sound bounces off walls, floor, ceiling, objects. Those reflections arrive at your ears spread out over time. Each reflection then bounces again, producing second-order reflections, third-order, and so on — thousands of reflections per second after the initial impulse.

The pattern of reflections is what your ear interprets as "the room." A long, dense, slowly-decaying pattern sounds like a big cathedral. A short, sparse, quickly-decaying pattern sounds like a small tiled bathroom.

## How reverb is synthesized

Real acoustic reverb is incredibly complex — thousands of reflection paths, each with its own delay, attenuation, and filtering. Synthesizing it perfectly requires convolution with an impulse response of an actual space.

But we can approximate it cheaply with **Schroeder reverb**: a network of comb filters and allpass filters that produces enough diffuse echoes to *feel* like a room, even though it doesn't perfectly match any specific space.

acidflow uses this approach. The implementation:

- **4 parallel comb filters**. Each comb delays the input and adds feedback, producing a series of echoes at a specific rate. Four combs at different delays produce a denser texture.
- **2 serial allpass filters**. Allpass filters don't change the frequency response but "smear" the phase, turning the discrete comb echoes into a continuous wash.
- **Lowpass filter inside each comb's feedback** (damp control) — long tails progressively lose high frequencies, mimicking air absorption and plate damping.

This is the classic Freeverb / Schroeder topology used in most low-cost reverbs since the 1960s. It's not pristine but it sounds musical.

## Controls

acidflow has three reverb parameters:

- **REV MIX** — wet send amount (0 to 1). 0 = bypass. Higher = more audible reverb.
- **REV SIZE** — effectively the reverb's decay time. Higher = longer tail.
- **REV DAMP** — how quickly high frequencies die in the decay. Higher = duller, warmer tail.

### Size: the room scale

Under the hood, size controls the comb feedback coefficient:

```
comb_g = 0.70 + 0.28 × size_parameter
```

So size = 0 → comb_g = 0.70 (short feedback, ~0.6 second RT60).
Size = 1 → comb_g = 0.98 (long feedback, ~4 seconds RT60).

RT60 is the time for the reverb tail to drop by 60 dB (practically inaudible).

- **Small room** (0-0.25 size): 0.6-1.2 second RT60. Feels like a bathroom or bedroom.
- **Medium room** (0.25-0.5): 1.2-2 seconds. A club's main room.
- **Hall** (0.5-0.75): 2-3 seconds. A concert hall.
- **Cathedral** (0.75-1.0): 3-4+ seconds. An actual cathedral or abandoned warehouse.

Acid tracks typically use small to medium (0-0.5 size). Larger tails are for ambient/dub.

### Damp: high-frequency rolloff

Damp controls a lowpass filter inside each comb's feedback loop. The filter coefficient is:

```
damp_coef = 0.05 + 0.45 × damp_parameter
```

A higher coefficient means a lower cutoff — more high-frequency attenuation per pass.

- **Damp = 0**: bright, shimmery tail. Cymbals and hats echo indefinitely. Glass-like.
- **Damp = 0.5** (default 0.35 area): natural room behavior. Highs fade moderately.
- **Damp = 1.0**: dark, boomy tail. All highs die quickly; only low-mid survives.

Most realistic rooms have damp around 0.3-0.5. A pure plate reverb (metal plate with contact mics) has less damp — brighter. A real room has more damp because air absorbs highs.

### Mix: how loud is the reverb

- **0-10%**: inaudible or barely audible. Subtle depth. No "reverb effect."
- **10-25%**: you notice it. Adds space without being wet.
- **25-50%**: wet. Reverb is a significant part of the sound.
- **50-75%**: extreme wet. The dry signal is nearly swamped.
- **75-100%**: reverb-dominated. Like a soundscape/ambient track.

Acid tracks usually sit at 5-20% reverb mix. Dub acid might push to 30-40%. Anything over 50% is genre-different (ambient, shoegaze, not acid).

## Reverb types acidflow approximates

acidflow's Schroeder topology naturally produces a sound close to a **plate reverb** or a **small-to-medium room**. It doesn't cleanly simulate:

- **Hall reverbs**: would require longer pre-delay and more complex late-reflection networks.
- **Convolution / impulse response reverbs**: require large FFTs.
- **Spring reverbs**: their distinctive "boing" requires a specialized model.

What you get is a versatile, plate-ish room that sounds right for electronic music. Not the best choice for orchestral/classical context but perfect for acid.

## When to use reverb

### For depth (the most common use)

A dry 303 sits "at the front of the speaker" — flat, in-your-face. A touch of reverb (5-10% mix) pushes it back slightly, giving the impression of being in a space. The mix feels more three-dimensional.

This use should be *imperceptible as reverb*. You shouldn't hear "wet"; you should just feel the track is less claustrophobic.

### For tails and sustain

A short 303 note (high DECAY) has almost no sustain. Reverb gives it a tail. Even if your DECAY is at 0 (super-snappy), a bit of reverb stretches each note into a lingering presence.

Good for adding warmth to staccato 303 programs.

### For space on specific elements

Shared-FX architecture means you can't reverb-only one voice. But you can use the FX DRIVE + delay settings to make certain elements dominate the reverb wash.

Example: 303 loud, drums low. Reverb hits both, but the 303 dominates. The track feels "303 in a room, drums up close." Subtle but effective.

### For breakdowns

Push reverb MIX to 50-70% during a breakdown. The track goes from defined to misty. Bring it back to 10% at the drop — sudden clarity.

### For dub aesthetic

Dub music (especially dub techno / ambient dub) uses reverb heavily. Size 0.5-0.75, mix 30-50%, damp 0.3. Wash, space, echo, depth. The mix is mostly reverb with some 303 pokes.

## When NOT to use reverb

### When the kick is already loud

Reverb on the kick = muddy. Kicks are transient events; reverb smears the transient and the kick loses punch.

Shared-bus architecture means you have no choice — if you reverb the 303, you reverb the kick. Solution: keep reverb minimal (MIX < 20%) when drums are prominent.

### On already-busy 303 lines

A 303 with hits on every step, resonance high, accents heavy = already cluttered. Adding reverb piles a wash on top. The pattern loses definition.

Save reverb for sparse, moderate-density programs.

### For "making it sound better"

Beginners apply reverb as a crutch. "This sounds thin, let me add reverb." Reverb doesn't fix thin — it just covers it up.

Fix thin signals at the source: tune, shape, arrange. Don't reach for reverb first.

### To simulate rooms you don't understand

Reverb presets often have names ("Large Hall", "Cathedral", "Tiled Bathroom"). acidflow doesn't have presets — you set size and damp manually. Think about the *acoustic* you want, not the label.

A "cathedral" sound isn't just long decay. It's long decay with specific early reflection patterns that acidflow doesn't model. So don't try to make acidflow sound like a cathedral; make it sound like itself — a plate-ish versatile algorithm that works for electronic music.

## The reverb in signal chain

From chapter 27:

```
Master → FX DRIVE → Delay → Reverb → Output
```

Reverb is last. This means:

- **Delay echoes are reverberated**. Each echo has a bit of reverb on it. The track feels like delayed hits in a space.
- **Drive harmonics are reverberated**. Driven high-frequencies get reverb tails. Can make the track feel "bright and spacious" or "noisy and washy" depending on how much drive and reverb.

If you want drier delay, reduce reverb mix. If you want wetter delay, raise it.

## Reverb and mono vs stereo

acidflow's reverb output is stereo — the four parallel combs produce slightly different delay times that decorrelate between left and right, giving a sense of width.

A dry signal with no reverb sounds centered / mono. Adding reverb widens it. This is partly why reverb makes tracks feel "bigger" — not just depth but width too.

If you want maximum stereo width, reverb is your primary tool. Delay doesn't stereo-widen (it's mono tape-style in acidflow).

## Reverb and loudness

Reverb adds RMS level. A signal with reverb has higher average energy than without, because the tail continues to produce energy after the original note decays.

Practical implication: enabling reverb can make a track clip if it was already close to the ceiling. Lower VOL by 2-3 dB when you add significant reverb.

## Reverb and frequency

Reverb's wash tends to emphasize mid-range frequencies (300 Hz - 2 kHz). Low frequencies are harder to reverberate (they require large spaces); high frequencies die quickly (damp).

This mid-range emphasis is why reverb can muddy a mix: if your 303 is in 300 Hz - 1 kHz and your kick is in 50-80 Hz, the reverb tail lives in 303 territory, pushing the 303 forward and adding mid-range build-up.

To counter: use damp to roll off reverb highs (tail stays below 3 kHz). Or use smaller size to shorten tails. Or accept the mid-range emphasis if it suits your aesthetic.

## Reverb and dub techniques

Dub techno (Basic Channel, Maurizio, Deepchord) uses reverb as a primary instrument, not a secondary effect. The reverb is half the sound.

Technique for dub-ish acid:

1. Sparse 303 pattern (4-6 hits per bar).
2. High reverb SIZE (0.6-0.7).
3. Medium MIX (30-40%).
4. Moderate DAMP (0.4-0.5).
5. Small amount of delay (1/8, mix 20%, feedback 0.5).
6. Drums minimal — kick, clap, maybe hats.

Result: a spacious, atmospheric acid track. Not aggressive but haunting.

## Reverb and the breakdown

The go-to breakdown reverb trick:

1. Main section: reverb mix 15%, size 0.3.
2. Approaching breakdown: automate (manually, via section focus + knob tweak) reverb mix up to 50-60%.
3. At the breakdown: kill the drums (mute drum bus with `m`). Only the 303 plays, swimming in reverb.
4. Resolve: bring drums back, pull reverb mix down to 15%.

This produces a cinematic "drop into space then snap back" that works every time.

## Reverb vs delay

Two different spatial tools:

- **Delay** = discrete echoes, preserves timing, creates polyrhythm.
- **Reverb** = diffuse wash, blurs timing, creates space.

Use delay for rhythmic interest. Use reverb for depth. They do different things.

They also combine well: some delay + some reverb = "echoes in a space." A classic combination. acidflow's FX chain puts delay before reverb, so delayed echoes get reverberated naturally.

## Reverb and tempo

Faster tempos → shorter reverb tails work better. At 160 BPM, a 4-second tail is still going when the next bar starts, creating pile-up.

Rough guide:

- 120 BPM → size up to 0.5 is safe (tails up to ~2 seconds).
- 130 BPM → size up to 0.4 (tails ~1.5 seconds).
- 140+ BPM → size 0.3 or less (tails under 1 second).

Faster tracks want tighter reverb. Or accept wash as aesthetic (gabber-acid).

## Reverb and scale choice

Whatever scale you're in, reverb will reinforce those pitches. This is good: your consonant notes sustain in the wash, adding harmonic density.

But: self-oscillating filters (RES at max) produce a specific pitch. Reverb reinforces that pitch. Multiple self-oscillating notes in a line produce multiple sustained pitches, all clashing in the reverb tail.

To counter: either don't self-oscillate in reverb-heavy tracks, or ensure self-oscillation pitches are in your scale (so the clash is musical, not random).

## Reverb and the "heatmap"

acidflow's filter heatmap (Chapter 38) shows the frequency response of the filter. It doesn't show reverb. Reverb's contribution to the spectrum isn't visualized.

If you're using the heatmap to guide filter settings, remember: the actual output you're hearing has reverb on top of the filtered signal. What the heatmap shows is *before* FX.

## Try this

1. Set up a simple 303 pattern with 6 hits. REV MIX at 0. Play. Dry.
2. Raise REV MIX to 15%. SIZE 0.3. DAMP 0.4. Play. Subtle depth added.
3. Raise REV MIX to 30%. Play. Clearly audible reverb.
4. Raise SIZE to 0.7. Play. Long trail; pattern feels spacious.
5. Raise DAMP to 0.8. Play. Same trail but darker.
6. Lower DAMP to 0.1. Play. Bright shimmery trail.
7. Now raise MIX to 60%. Play. Very wet.
8. Drop to 10% MIX, 0.3 SIZE, 0.4 DAMP. Play. Return to subtle production reverb.
9. Add delay: DLY MIX 30%, 1/8 dotted, FB 0.5. Play. Both delay and reverb on the same line. Spacious, echoing.
10. Mute drums (drums section, `m`). Only 303 in FX. Play. This is close to ambient acid.

You've explored the full reverb parameter space. The Schroeder algorithm doesn't simulate every imaginable space, but within its range, it's versatile enough for most acid production.

Part V is complete. Five chapters on FX:

- Chapter 27: FX DRIVE (overdrive on the bus).
- Chapter 28: Delay (tempo-synced tape echo).
- Chapter 29: Reverb (plate/room).

The FX rack is deceptively simple — three effects, nine parameters — but can transform the same 303 line into radically different-sounding tracks. Master it, and you have the full sonic palette of decades of acid.

Next: Part VI — Performance and live control.
