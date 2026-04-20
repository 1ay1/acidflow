# 9 · The oscillator — TUNE and WAVE

The oscillator is where the sound starts. Everything after it — filter, envelope, amplifier, FX — is shaping. But if you start with the wrong *raw material*, no amount of shaping will rescue the result. This chapter is about those two small-looking controls: TUNE and WAVE.

## What the oscillator actually does

In a pure analog synthesizer, an oscillator is a voltage-controlled electronic circuit that produces a repeating waveform. The frequency of the repetition is the pitch. The shape of the repetition is the timbre.

In acidflow, the oscillator is a software function that runs at the audio sample rate (usually 48 kHz on modern systems). For each sample, it computes the next point on a saw or square waveform at the current pitch, with anti-aliasing applied (via PolyBLEP, a standard DSP technique). The output is a signal that swings between -1.0 and +1.0, which is then sent down the signal chain.

Two knobs control the oscillator:

- **TUNE** — pre-transposes the oscillator ±12 semitones (one octave).
- **WAVE** — picks between sawtooth and square.

That's it. Two knobs. But they encode the most important decision you make about your sound: *what family of acid records am I targeting?*

## TUNE

TUNE shifts the oscillator's pitch relative to the sequencer's pitch commands. If the sequencer says "play C3" and TUNE is at 0 (center), you hear C3. If TUNE is at +7 semitones, you hear G3. If TUNE is at -12, you hear C2.

### Why have a global TUNE at all?

Three reasons:

1. **Key shifting.** If you've written a pattern in C but want to try it in G♭, instead of editing all 16 steps you can just turn TUNE down six semitones.
2. **Stage retuning.** If you're playing alongside another instrument that's slightly flat (a guitarist with a vintage amp, say), TUNE lets you match.
3. **Transitional effects.** A slow TUNE move during a pattern can create a pitch-bend effect that spans the whole loop. Not common in acid, but occasionally useful for a breakdown.

On a real 303, the TUNE knob has a finer resolution than one semitone — it's a continuous pitch control. acidflow discretizes to semitones for usability (the mouse drag gives 0.5-semitone fine control; scroll wheel gives full-semitone; `0` resets to center).

### Tuning and the ±3-cent drift

Regardless of where you set TUNE, the oscillator has a small random drift of ±3 cents (0.03 semitones). This is the modeled-analog quality discussed in Chapter 6 — it keeps the sound from feeling digitally-sterile. You don't have a control for this; it's always on.

If for some reason you need perfectly stable tuning (e.g., for sync with external tuned gear), you can use acidflow's MIDI output instead, which sends exact pitch values. See Chapter 32.

### Tuning advice for beginners

Don't touch TUNE at first. Leave it at center. Write your patterns in C because that's the easiest mental model. Later, when you're comfortable, experiment with nudging TUNE by 1-2 semitones while a pattern is playing — you'll feel how the texture shifts even though the intervals between notes stay the same.

One musically-specific trick: *detuning against a fixed reference*. If you're running a drum machine with a pitched tom or a vocal sample, and your 303 line is almost but not quite in tune with it, a tiny TUNE nudge (maybe just 0.5 semitones via the mouse) can lock them together. Analog drift is chaotic; deliberate detuning is a tool.

## WAVE

WAVE picks between **sawtooth** and **square**. These are the two raw waveforms the 303 was designed to produce. They sound fundamentally different. They belong to different musical worlds.

### Sawtooth

A saw wave ramps up linearly, then drops instantly. Plotted:

```
      /|   /|   /|   /|
     / |  / |  / |  / |
    /  | /  | /  | /  |
   /   |/   |/   |/   |
```

Each cycle is "ramp up, snap down." Because the waveform is not symmetrical (all ramp, no ramp-back), it contains *all* harmonics of the fundamental frequency — 1f, 2f, 3f, 4f, etc., decreasing in amplitude as 1/n.

Translation: a saw has a rich, bright, slightly nasal character. It's the workhorse waveform for lead synths because there's plenty of high-frequency content for the filter to shape.

When you drive a saw through a resonant lowpass filter, the filter sweeps through the harmonic series, emphasizing one frequency at a time. This is the classic synth filter sweep sound. And it's the sound the 303 was designed around.

**Use saw for**: Almost every acid track you've ever heard. Classic Chicago acid, UK acid, Detroit acid, acid techno, Hardfloor, Plastikman. When in doubt, saw.

### Square

A square wave alternates high and low for equal durations. Plotted:

```
    __    __    __    __
   |  |  |  |  |  |  |  |
___|  |__|  |__|  |__|  |__
```

Each cycle is "up for half, down for half." Because the waveform is symmetrical, it contains only *odd* harmonics — 1f, 3f, 5f, 7f, etc., decreasing as 1/n.

Translation: a square has a hollow, nasal, clarinet-like character. Because it has fewer harmonics than a saw, it has less for the filter to work with — high-resonance filter sweeps on a square are less dramatic than on a saw. The character is more *woody* and less *buzzy*.

**Use square for**: Dub-acid, tech-house basslines, 808 State-style tracks, any moment where you want a *rounder* feel. The classic *Pacific State* lead is a square wave. A lot of early Chicago acid B-sides used square for variation. Some acid techno producers switch waveforms within a track for verse/chorus contrast.

### The audible difference in one sentence

A saw is *bright and buzzy*. A square is *hollow and round*. If your track feels too aggressive, switch to square. If it feels too muted, switch to saw.

### Saw + filter at full open

When you open the filter completely (CUTOFF at max) on a saw, you hear essentially the full raw oscillator. Play a note, and the sound is a bright, buzzy ramp with a sharp leading edge. This is very loud in the high frequencies — mix it at high volume and you'll hurt people's ears.

On a square at full open, you hear a hollow, trumpet-like tone. Still bright, but without the high-frequency glare of a saw.

### Saw vs square with self-oscillating resonance

Here's a subtle but important point: when RES is maxed and the filter is self-oscillating, the *oscillator waveform matters less* because the self-oscillation is contributing a pure sine tone of its own. At extreme Q, you can almost not tell saw from square — both come out screaming. As you pull Q back down toward ~70%, the difference between the two waveforms becomes audible again.

This is why classic acid records that live in high-resonance territory don't always bother with WAVE selection. At moderate resonance, it matters a lot. At self-oscillation, less.

## The waveform choice as a stylistic choice

Think of saw and square as two dialects of the same language:

| Saw                          | Square                        |
|------------------------------|-------------------------------|
| Bright, aggressive           | Hollow, rounded               |
| Classic acid house sound     | Dub-acid, tech-house sound    |
| Dominant in 1987–1995 canon  | Occasional B-side character   |
| Pushes forward in the mix    | Sits back in the mix          |
| Loves high cutoff            | Works at low cutoff too       |
| Phuture, Hardfloor, Plastikman | 808 State, some Chicago dub |

Most producers use saw 90% of the time. Some producers use square as a deliberate signature — if you've listened to a lot of 808 State, the square-wave character of their lead lines is instantly recognizable.

### When to switch WAVE

Mid-track wave-switching is rare in classic acid but possible in acidflow. A usable technique:

- **Verse in saw** — bright, aggressive, the main riff hits with full energy.
- **Breakdown in square** — same pattern, rounded, less upfront. The filter can sweep further without killing the ear.
- **Back to saw** for the drop.

This is a programming trick — you don't have a per-step WAVE lock (acidflow doesn't support this; neither does the real 303). You'd toggle `w` live during performance, or write the WAVE state into saved slots and chain slots with song mode (Chapter 30).

## The oscillator as raw material

A final framing. When you look at the rest of the acidflow panel — filter, envelope, accent, drive — you can think of all of them as *cooking* the raw oscillator output. The oscillator is the ingredient; everything else is the recipe.

This means:

- The choice between saw and square is like the choice between beef and chicken. You can season either well, but they're fundamentally different.
- TUNE is like salt — you set it and mostly forget about it.
- Everything after is the actual cooking.

## The non-oscillator acidflow doesn't have

Because the 303 was a single-oscillator design, acidflow is single-oscillator too. It does NOT have:

- **A second oscillator** for thickness or detune.
- **Oscillator sync** for harder, more aggressive tones.
- **FM (frequency modulation)** between oscillators.
- **PWM (pulse-width modulation)** — the square is a 50%-duty square, no PWM.
- **Noise generator** on the voice. (Drums have noise; the 303 voice doesn't.)
- **Sub-oscillator** below the main pitch.

If you miss any of these features, you're not trying to make acid. You're trying to make a different kind of synth lead, and you should use a different synth. Acid's character is defined by the *absence* of these features — one oscillator, one filter, one envelope. The constraint is the genre.

The honest answer to "can I get a thicker sound?" is: yes, by layering two patterns (save one, load it later, export, layer in a DAW), or by running three acidflow instances at once over MIDI. Hardfloor's *Acperience 1* is built exactly this way — three 303s layered. But within a single acidflow voice, you get one oscillator. That's the instrument.

## Try this

1. Load preset 01 (Acid Tracks). Play. Listen. Confirm it's a saw.
2. Press `w` while it's playing. You'll hear the entire character shift. Hollower. Rounder.
3. Press `w` again. Back to saw.
4. Now turn RES to 90%. Press `w` a few times. Notice that at this high resonance, the difference between saw and square is much less dramatic.
5. Back down to RES 50%. Press `w` again. The difference is obvious now.
6. Experiment with moving TUNE while the pattern is running. Try -3, -5, -7, -12. Hear how the pattern takes on different moods at different keys.

Don't rush this. Two knobs, plenty to hear.

Next: the filter.
