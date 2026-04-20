# 6 · What makes the 303 sound like the 303

You can build a synth with an oscillator, a 4-pole lowpass filter, resonance, and an envelope — and it still won't sound like a 303. Many manufacturers have tried. Many plugins have failed. The 303 is a specific combination of eight or nine design decisions that *individually* might not seem important but *together* produce an unmistakable character. This chapter enumerates them.

If you only read one chapter of this book to understand what acid is as a *sound*, make it this one.

## 1. The filter is a diode ladder with tanh saturation

The 303's filter is a 4-pole lowpass. So is the Moog's. So is the ARP 2600's. But the 303's filter has a few specific quirks:

- It uses **diodes in the ladder stages** (rather than matched transistors like Moog). Diodes have a more non-linear response curve at high amplitudes, which produces a gentle, asymmetric distortion as the signal gets loud.
- Each stage has a **tanh-like saturation** (a soft-clip curve), not just linear attenuation. When the signal is small, the stage is linear. When the signal is large, the stage *clips*.
- The **global feedback** (the resonance path) also runs through a tanh stage.

These two non-linearities — per-stage and global — are why the 303 doesn't get ugly when you push the resonance. A linear high-Q filter driven hard will squeal, clip, and sound harsh. A tanh-limited filter does the *same thing* but wraps it in a soft saturation that turns the squeal into a sung note rather than a digital screech. This is why acid basses sound **expressive** at high resonance rather than **broken**.

acidflow models the tanh non-linearities precisely — each ladder stage and the global feedback all pass through soft-clipping. Without this, self-oscillation would be a buzzsaw. With this, it's a voice.

## 2. The filter resonance can self-oscillate, and the threshold is musical

On a flat bench test, most 4-pole ladders self-oscillate somewhere between 90% and 95% resonance. The 303 goes into self-oscillation around 88-92%, depending on temperature and component tolerance. This is *exactly* the place where musical behavior becomes maximally dramatic: below 85% you have resonance as a *color*; above 95% you have self-oscillation as a *note*. The narrow window between is where the filter is "about to scream" and every envelope trigger pushes it over.

acidflow's RES control spans 0.0–1.0. Self-oscillation begins around 0.85. The knob is scaled so that 0.7–0.9 is the "expressive" range and 0.9+ is the "pure screaming" range.

## 3. The envelope is exponential in V/octave space

The 303's filter cutoff is modulated by the envelope *exponentially* — it uses a V/octave exponential converter, the same circuit that converts pitch control voltage to frequency. This means a fixed envelope amplitude produces a fixed interval of filter movement, not a fixed Hz movement. If ENVMOD is 2 octaves, every note's envelope opens the filter by exactly 2 octaves, regardless of the base cutoff.

This is why a 303 envelope sweep sounds **consistent** across all notes. On a linear-modulation synth, the same envelope applied to a low cutoff sweeps the filter over a large multiplier; applied to a high cutoff, only a tiny multiplier. The 303's exponential modulation sidesteps this entirely — every note's filter sweep is the same shape, scaled to the same interval.

Practically: this is why you can walk up a pattern on the 303 and every note has the same *feel*, rather than the low notes feeling muddy and the high notes feeling thin.

## 4. The accent capacitor stacks across notes

The accent on a 303 is not velocity. It's a **capacitor** that charges when an accented note is played and drains slowly over the decay. When multiple accented notes hit in sequence, the capacitor **stacks** — the second accented note on top of an already-charged cap produces a *more intense* accent than the first.

The effect: a lone accent feels strong. Two accents in a row feel *very* strong. Three in a row feel wild. Then when the accents stop, the capacitor drains back to zero and the next accent is "lone" again. This is a dynamically non-linear accent behavior that no velocity table can reproduce.

acidflow implements this with a persistent `accent_level` variable that charges on each accent and drains exponentially between notes. The stacking behavior is accurate — you can feel the difference between `.A.A.A.A.` (four isolated accents, all similar intensity) and `AAAA....` (four consecutive accents, escalating intensity).

We have an entire chapter on accent (Chapter 12). It's *that* important.

## 5. The slide is an exponential RC curve with τ ≈ 60-100 ms

When the 303 slides between notes, the pitch doesn't interpolate linearly. It follows an **RC time constant** — the natural curve of a capacitor charging through a resistor. This produces a curve that's fast at first and asymptotically approaches the target pitch. The time constant is fixed (not knob-adjustable on the real 303), around 60-100 ms depending on the unit.

Why this matters: a linear slide sounds robotic. An RC slide sounds **vocal**. It's the same curve a singer's pitch makes when they glide from one note to another. The 303's slide is the reason acid melodies feel sung rather than typed.

acidflow uses τ ≈ 88 ms, in the middle of the real-303 range. The slide curve is computed per-sample, so there's no quantization artifact.

## 6. The oscillator drifts by ±3 cents randomly

Analog oscillators are temperature-sensitive. A 303 that's been running for five minutes has a subtly different pitch from one that's been running for five hours. Even within a single session, the oscillator microdrifts — usually within ±3 cents of the nominal pitch (a cent is 1/100 of a semitone).

This drift is **imperceptible note-by-note** but creates a living, breathing quality across a long pattern. It's the synth equivalent of a violinist's vibrato — never the same twice.

acidflow models this drift by applying a slow random-walk ±3 cents to the oscillator frequency. If you listen to a pure sustained 303 note through a tuner, you'll see the pitch wobble microscopically. The ear doesn't hear it consciously. The ear *does* hear its absence — a perfectly-tuned digital oscillator sounds "plastic" in a way that's hard to articulate until you hear the analog version next to it.

## 7. There's 2× oversampling in the filter path

High-Q filters can alias badly when driven hard at a finite sample rate. A self-oscillating filter is effectively a sine wave — and if that sine wave's frequency times the harmonics it excites exceeds Nyquist, you get aliasing (digital garbage that folds down into the audible range).

acidflow runs the filter at 2× the audio sample rate (so 96 kHz if your output is 48 kHz), then downsamples with a simple anti-aliasing lowpass. This isn't audibly different from the analog 303 (analog doesn't alias by definition), but without it, acidflow would have audible digital artifacts at high Q. With it, self-oscillation is clean.

You don't have a knob for this. It's always on. But knowing it's there explains why acidflow can scream at 95% resonance without harshness.

## 8. The output has a fixed saturation stage

After the VCA, the 303's signal passes through output circuitry that gently saturates at high amplitudes. This adds warmth — low-level signals pass through clean, high-level signals get a subtle soft-clip that adds even harmonics and rolls off extreme peaks.

acidflow models this as a tanh-shaped output stage. It's subtle. You won't hear it on a soft signal. You'll hear it when the filter is screaming at high resonance and everything is hot. The saturation is what keeps things *listenable* at that volume — without it, peaks would hit digital clipping and produce harsh distortion.

## 9. The envelope retriggers on every non-slid note

On most synths, a held-gate sequencer doesn't retrigger the envelope between consecutive notes — it only retriggers when a new gate opens. The 303 **retriggers on every non-slid note**, even if the gate is held (which it usually is — the 303 holds gate for the entire step length).

The effect: even playing a drone of the same pitch at sixteenth notes, you get *sixteen envelope re-triggers per bar*. Each note's filter opens fresh. Each note's accent capacitor gets a fresh pulse. The pattern feels *alive* rather than sustained.

Slides override this: a slid note keeps the envelope running from the previous note rather than retriggering. This is why slid notes sound more *legato* — the filter doesn't re-open for them.

acidflow implements this exactly. You can experiment by writing `.XXXXXXXX....XXX` (a bunch of non-slid notes) vs `.X-X-X-X-X...XXX` (the same notes, all slid). The first will feel rhythmic and pulsing; the second will feel smooth and flowing.

## The combined effect

All nine of these together produce the 303 sound. Remove any one and it stops being a 303:

- Without tanh non-linearity → the resonance sounds harsh.
- Without self-oscillation → no acid whistle.
- Without exponential envelope modulation → notes at different pitches feel inconsistent.
- Without stacking accent → accents feel velocity-mapped, boring.
- Without RC slides → melodies feel typed, not sung.
- Without drift → the sound feels plastic.
- Without oversampling → high Q aliases.
- Without output saturation → peaks sound digital.
- Without per-note retrigger → patterns feel like sustained drones.

acidflow models all of these. This is why it can pass a blind test against a real 303 in most contexts (though a real-303 owner with excellent ears in a quiet room will still hear differences — the circuit-level analog imperfections of a 40-year-old PCB are genuinely hard to fake).

## Where acidflow differs from a real 303

A few intentional differences:

1. **Perfect tuning reference.** A real 303 needs warm-up time to stabilize. acidflow is in tune from sample 1. This is fine — you can always decrease the tuning knob if you want to detune.
2. **No power-supply noise.** Analog instruments have a low-level hum from the mains. acidflow is silent at rest. Some producers intentionally add noise for character; acidflow doesn't.
3. **No component tolerance variation.** Each physical 303 sounds slightly different because capacitors and transistors vary between units. acidflow is deterministic — every copy sounds exactly the same.
4. **Polyphonic-ish envelope handling on re-trigger.** The actual 303 has an envelope retrigger behavior that interacts with held gates in ways the circuit diagrams don't fully explain. acidflow implements the documented behavior, not every edge case.
5. **Accents and slides are perfectly timed.** Real-303 accents have a tiny delay (a few samples) because of the capacitor's charge time. acidflow's accents are sample-accurate.

None of these differences are audible in a pattern-level test. They're the kind of thing you'd hear on a sustained test tone under careful listening.

## Why this matters for you

Two takeaways:

1. **Don't fight the character.** The 303's sound is *defined by its quirks*. If you try to tame the resonance, moderate the accents, smooth out the envelope, you're making a synth demo, not acid. Go toward the weirdness. High resonance, fast decays, lots of accents — these are the *correct* settings.

2. **Understand the knobs as character dials.** Every parameter on the panel is pointing at one of the quirks above. RES is the self-oscillation dial. ENVMOD is the V/octave sweep dial. ACCENT is the capacitor-stack dial. DECAY is the envelope retrigger-speed dial. Etc. When you turn a knob, you're not just adjusting a number — you're leaning into one of the nine weird-design-decisions that makes the instrument what it is.

The next chapter gets you into the program so we can start making noise. Everything after that is just applying what you already know.
