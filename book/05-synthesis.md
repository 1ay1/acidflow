# 5 · How synthesizers make sound

To understand what acidflow is doing — and to use it creatively rather than as a black box — you need a working model of how a subtractive analog synthesizer generates audio. This chapter is a crash course. No math beyond "things get bigger and smaller." No signal-processing jargon we don't introduce from scratch.

The TB-303 and every acidflow voice is a **subtractive synth**. That name is a huge clue: we start with a sound that has *too much* in it and *subtract* things to shape it. The chain is:

```
Oscillator  →  Filter  →  Amplifier  →  Output
```

Everything that makes synths musical happens in the filter and amplifier stages — that is, in the subtraction and the shaping. The oscillator is simple and almost boring. Let's walk through each stage.

## Oscillator: the raw material

An **oscillator** generates a periodic waveform at a given frequency. "Periodic" means it repeats. "Frequency" is how many times per second it repeats.

The waveform shape determines the **timbre** (tone color). The common subtractive shapes:

- **Sine wave**: a pure tone. One frequency, no overtones. Sounds like a test tone or a flute. Boring on its own; impossible to make aggressive.
- **Sawtooth wave**: a ramp up and a sharp drop. Contains *every* harmonic of the fundamental frequency, decreasing in amplitude. Bright, buzzy, aggressive. The classic brass/string/lead shape.
- **Square wave**: alternating high and low. Contains only the odd harmonics. Hollow, clarinet-like, nasal. Woodier than a saw.
- **Triangle wave**: a slower ramp up and slower ramp down. Contains only odd harmonics, decreasing in amplitude faster than a square. Softer than a square, closer to a sine but with a hint of edge.

The 303 (and acidflow) gives you **sawtooth and square**. No triangle. No sine. This is not a limitation — it's a specification. Saw is for *lead* tones. Square is for *dub/bass* tones. The two waveforms define the two basic territories of acid: bright-and-aggressive or muted-and-dubbier.

Saw is much more common in acid. About 90% of the records you've heard use it. Square shows up in a few genre conventions:

- Dub-acid and tech-house basslines often use square for a more *round* character.
- 808 State's *Pacific State* uses square for its iconic synth line.
- Early Chicago acid often used square for B-section contrast.

When you toggle `W` on acidflow's knobs panel, you're choosing between these two territories.

## Pitch

The **pitch** of the oscillator is set by its frequency. Frequency is determined by:

1. The sequencer's step pitch (in semitones).
2. The TUNE knob (pre-transposes the entire pattern).
3. The slide envelope (glides between notes).
4. Random ±3-cent drift (acidflow models the 303's imperfect tuning; this is an anti-feature that makes things sound analog).

In acidflow, the oscillator generates a band-limited saw or square (implemented via BLEP — a topic for Chapter 19 of your DSP textbook, not this book). "Band-limited" means it doesn't include frequencies above the Nyquist limit (half the sample rate), which would otherwise alias down as digital garbage. You don't need to care about the implementation; you do need to know that the 303's raw oscillator sounds are *full-bandwidth* — containing energy from the fundamental up to 20+ kHz. The filter will cut most of that away.

## Filter: the shape-cutter

A **filter** is a circuit (or piece of code) that lets some frequencies through and blocks others. Synth filters are usually **lowpass**: they let everything *below* a cutoff frequency through, and attenuate everything above it.

Think of a lowpass filter as an adjustable curtain over your sound. When the curtain is all the way up (cutoff = 20 kHz), you hear the full saw — bright, buzzy. When the curtain is halfway down (cutoff = 1 kHz), you hear only the low-to-mid part of the saw — muted, round. When the curtain is almost closed (cutoff = 100 Hz), you hear almost nothing except the fundamental — a dull buzz.

The **cutoff frequency** is the position of the curtain. The **resonance** (see below) is a ringing at the cutoff frequency, which makes the "edge of the curtain" audible as a sort of whistle.

### Filter slope

How sharply does the filter cut? The slope is measured in **dB per octave** — how much quieter frequencies get for every doubling of frequency above the cutoff.

- **1-pole (6 dB/oct)**: Very gentle cut. The filter is barely doing anything. Modern digital filters go this low only for subtle tone-shaping.
- **2-pole (12 dB/oct)**: Noticeable cut, still musical. Many synths default to this. Called a "state-variable" filter in some architectures.
- **4-pole (24 dB/oct)**: Aggressive cut. Everything above the cutoff disappears quickly. This is the classic Moog-style sound, and it's what the 303 uses.
- **8-pole (48 dB/oct)**: Brutal. Clinical. Surgery. Not used in musical synths often.

The 303's **4-pole ladder filter** is a specific topology (a chain of four matched transistors, each providing 6 dB/oct) with feedback around the whole thing. The feedback is where resonance comes from.

### Resonance

**Resonance** (called Q in engineering, or "emphasis" in some older literature) is positive feedback around the filter. It boosts the frequencies right *at* the cutoff point, creating a ringing peak.

At low resonance (Q < 1), the filter is just a plain lowpass. At medium resonance (Q ≈ 2-5), the frequencies near cutoff get a noticeable boost — this is the "whistle" character. At high resonance (Q > 10), the filter starts to **self-oscillate** — it rings on its own without any input, producing a pure sine wave at the cutoff frequency.

The 303's filter self-oscillates at around 90% resonance on the real unit (and on acidflow). This is not a bug — it's *the entire reason the genre exists*. When you hear an acid squeal, you are hearing a filter self-oscillating, then being excited further by the oscillator's harmonics, then being modulated by the envelope so the self-oscillation pitch *moves* with every note.

## The envelope

An **envelope** is a time-varying control signal. It's not audio itself — it's a signal that controls something else. In a classic synth there's an envelope on the amplifier (making notes fade in and out) and an envelope on the filter (making the filter move).

The 303's envelope is **extremely minimalist**: it has exactly two shapes — a decaying curve for the amp, and a separate decaying curve for the filter — both non-adjustable except for **decay time**. No attack. No sustain. No release. You press a note, the envelope snaps to full, then decays toward zero.

This is weird by synth-design standards. Most synths have full ADSR (attack/decay/sustain/release) envelopes. The 303 doesn't, and the missing controls are why it sounds like the 303. A 303 note always has:

- **Instant attack** (the snap at the beginning)
- **Exponential decay** (curved, fast at first then slow)
- **No sustain** (the envelope always returns to zero)
- **No release** (as soon as gate goes off, the envelope is wherever it was, and just keeps going)

acidflow faithfully models this, with one exception: the amp envelope has a very short release (about 30 ms) to prevent digital clicking. The filter envelope is pure 303.

### Why exponential decay matters

An exponential decay curve starts fast, then slows down, asymptotically approaching zero. If you plot it:

```
     |
1.0  |\
     | \
     |  \
     |   \___
     |       \_____
     |             \________
 0   |_______________________\_______ time
     0                         
```

The ear hears the first 30% of the decay as "the note hit" and the last 70% as "the tail." Because it's exponential, the tail is always *there* but gets quieter logarithmically — which matches how the ear perceives loudness. This produces the 303's characteristic *ptoonnng* or *boonngg* shape: a clear initial note followed by a fading, filter-modulated tail.

A *linear* decay (straight line to zero) sounds boxy and unmusical by comparison. Every subtractive synth uses exponential decay because linear decay sounds wrong.

## The amplifier

The **amplifier** (VCA — voltage-controlled amplifier) is the volume-control stage. It takes the filter's output and scales it by the amp envelope. When the amp envelope is at 1.0, you hear the filter's output at full volume. At 0.0, silence. At 0.5, half volume.

In the 303, the amp envelope is a decaying exponential, starting fresh each note. A note triggers: amp snaps to 1.0, then decays. If DECAY is short (say 300 ms), the note is a fast *puck*. If DECAY is long (say 1500 ms), the note rings on for a second and a half before fading.

The amp and filter envelopes share the same DECAY time on the 303. They are not independent. This is a design constraint that ends up being creatively helpful — you can't accidentally set the filter to decay slower than the amp and end up with a filter tail that's audible after the note is gone.

## Putting it all together: the 303 signal path

Here's what happens, step by step, when a note plays on acidflow:

1. **Gate triggers.** The sequencer tells the voice "play this note at this pitch, with these modifiers."
2. **Oscillator runs.** The saw or square runs at the step's pitch, ±tuning, ±drift.
3. **Filter envelope snaps to peak.** The filter envelope is set to its max value (ENVMOD determines how high that max is in octaves).
4. **Amp envelope snaps to peak.** The amp is opened.
5. **Audio flows.** The oscillator's output is filtered (cutoff = base cutoff + envelope amount), then scaled by the amp envelope.
6. **Envelopes decay.** Both envelopes exponentially decay toward zero. The filter closes, the amp fades.
7. **Slides, accents, and p-locks apply.** If this step is a slide, the pitch glides from the previous note. If accented, the accent envelope adds extra filter opening. If p-locked, the CUTOFF/RES/ENV/ACC values are overridden for this step only.
8. **Next step.** The sequencer advances.

That's the entire instrument, in one paragraph. Everything in Chapters 9–14 is about the specific parameters inside this loop.

## Signal paths beyond the voice

Once the voice has produced audio, it enters the rest of the signal chain:

```
303 voice  →  Drive (pre-filter OD, if enabled)  →  Delay  →  Reverb  →  Output
Drums       ─────────────────────────────────────────↗
```

The drums (Chapters 22–26) are generated in parallel, summed into the FX send, and processed through the same delay/reverb chain. The drive in the FX rack is applied only to the 303 voice (not drums), so it can be used to make the bass "scream" without muddying the kick.

We'll dissect each of these in their own chapters.

## Why this model is enough

If you grasp oscillator → filter → envelope → amp, you can already predict what every acidflow knob will do. CUTOFF moves the filter. RES controls resonance. ENVMOD scales the filter envelope. DECAY sets both envelope times. TUNE pre-transposes the oscillator. WAVE picks saw or square. DRIVE adds saturation after the filter. VOL sets output level. ACCENT is a special kind of envelope modulator (next chapter).

Nine knobs. One signal path. The rest is vocabulary.

## Try this

1. Turn CUTOFF to 50%, RES to 30%, ENVMOD to 0%, DECAY to 500 ms. Play a pattern. Flat, uninteresting — the filter isn't moving.
2. Turn ENVMOD to 50%. Play. You now hear the envelope opening and closing the filter with each note. The *ptoonnng* is back.
3. Turn RES to 70%. Play. Resonance accents the cutoff, producing the whistle.
4. Increase DECAY to 1500 ms. Play. Long tails, filter closes slowly, each note bleeds into the next.
5. Reduce DECAY to 200 ms. Play. Snappy, percussive.

You are now playing the same four synthesis parameters that defined the 303's behavior for forty years. The rest of this book is nuance.
