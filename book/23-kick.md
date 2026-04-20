# 23 · The kick (BD) — programming and theory

The kick drum is the most important single element in dance music. Not the 303, not the chord stabs — the kick. If the kick is wrong, the track is wrong. If the kick is right, you can get away with almost anything else being mediocre.

This chapter is about what makes a good dance kick, how acidflow synthesizes it, and how to program kick patterns that work.

## What a kick drum is, acoustically

A real kick drum (acoustic, beaten with a pedal) produces:

1. A brief, high-frequency click at the moment the beater strikes the head.
2. A descending pitch "pwoom" as the head resonates, pitching down as it relaxes.
3. A low-frequency body resonance that can extend 200-500 ms.

The click is above 3 kHz. The pitch descent is typically 80 Hz down to 40 Hz over 50-100 ms. The body resonance is 50-80 Hz sustained.

Synthesized kicks (TR-808, TR-909, acidflow) replicate this with:

- A **pitch envelope** rapidly dropping the oscillator frequency.
- A **sine oscillator** as the body.
- An **amp envelope** shaping the decay.
- Optional **noise burst** for the click.

## How acidflow's BD works

acidflow's kick is:

1. A **sine oscillator** that starts at ~120 Hz and drops exponentially to ~50 Hz over ~80 ms. This is the "pwoom."
2. The pitch envelope is followed by a fixed-frequency body sustain at ~50 Hz.
3. The amp envelope is a fast exponential decay (~350-500 ms).
4. A small **transient click** at the start of the envelope (a brief noise burst).

This produces a classic "synth kick" that sits in the 40-120 Hz range, with a pronounced "pwoom" character, a short click transient, and a clean body.

It's close to TR-909 territory — the 909 kick is the dance-music kick since 1984. 808 kicks are similar but with longer decay and more low-end. Acidflow's kick is tuned more toward 909 sound.

## The kick's role in the mix

A kick does three things:

1. **Anchors the beat**. The listener's ear locks to the kick as the meter reference.
2. **Occupies the low end**. All the sub-bass frequency information is in the kick.
3. **Provides physical impact**. The kick transient is what dancers feel in their chest.

If your kick is too quiet, the beat feels weak. Too loud, everything else gets ducked. Too long in decay, it muddies the low end. Too short, it feels punchy but insubstantial.

## Programming kick patterns

### The four-on-the-floor

Kick on steps 1, 5, 9, 13. One kick per beat. This is dance music's signature.

```
Voice   1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD      X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
```

This is the default. Works for 90% of acid, house, techno.

### Offbeat kicks (not dance)

Kicks on 3, 7, 11, 15 (the offbeats). This is NOT dance music — it's swing, jazz, or experimental. The ear expects kicks on the downbeat; offbeat kicks disorient.

Don't do this unless you're making something deliberately non-dance.

### Syncopated kicks (UK acid, electro acid)

Kicks on 1, 5, 8, 9, 13. Adds a syncopated kick between beats 2 and 3, which gives a more rolling feel.

```
Voice   1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD      X  .  .  .  X  .  .  X  X  .  .  .  X  .  .  .
                                ^
                                syncopated kick on step 8
```

Used in breakbeat-influenced acid, UK rave acid, and some electro acid.

### Double kicks

Kicks on 1, 4, 5, 9, 13. Drops an extra kick on step 4 (the "a of 1"), giving a slight "re-kick" feel.

```
Voice   1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD      X  .  .  X  X  .  .  .  X  .  .  .  X  .  .  .
              ^  ^
              quick double kick on 4-5
```

Used in faster acid techno for extra drive.

### Half-time

Kicks on 1 and 9 only. Every other beat. This is the laid-back, broken-down feel.

```
Voice   1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD      X  .  .  .  .  .  .  .  X  .  .  .  .  .  .  .
```

Used in drum 'n' bass, downtempo, breakdown sections.

### No kick

A bar with NO kicks. Sometimes used for one bar as a "drop" — the kick disappears for 16th notes, then returns with impact on bar 2, step 1.

## Kick and 303 interaction

The kick and 303 share the low-frequency range. If both have lots of bass content, they can clash — mud in the low end.

Some approaches:

1. **Duck the 303 under the kick**: turn the 303's CUTOFF down slightly on kick steps (via p-lock). The 303 gets thinner exactly when the kick needs low-end space.

2. **Pitch the 303 up an octave**: move the 303's range to 200+ Hz so it doesn't compete with the 40-80 Hz kick.

3. **Accept the clash**: in aggressive acid techno, the kick-and-303 muddying IS the sound. It's a wall of low-mid-bass. Don't try to clean it up.

Modern acid tends to separate kick and bass via approach 1 or 2. Classic acid just accepts the clash.

## Kick and swing

Kicks on beats 1, 5, 9, 13 — these are all odd-numbered steps. Swing delays even steps. So the kick is *not* affected by swing.

This is fortunate — a "swung kick" sounds like a timing mistake, not a groove choice. The kick stays on-beat regardless of swing, giving dancers a reliable reference.

## Kick volume and impact

Kicks need to feel physical. This means:

- **Gain**: kick should be the loudest single voice in the mix. At least 3-6 dB above the 303.
- **Compression**: acidflow doesn't have built-in sidechain compression (a DAW technique). If your kick doesn't punch through, try adjusting the 303 down on kick steps (p-lock CUTOFF lower on 1, 5, 9, 13).
- **Headroom**: leave peak headroom so the kick transient doesn't clip. Master peak at -3 dB or lower.

## Kick timbre variations

acidflow's kick is one fixed synthesis. You can't change its pitch, decay, or tone via knobs. But you can change its perceived character by:

- **Changing the FX drive** applied to the master bus. Drive on the kick adds harmonics, making it feel "dirtier."
- **Changing delay send**. A bit of delay on the kick gives it a doubled feel.
- **Changing reverb send**. A touch of reverb on the kick adds space and "size."

For more kick timbre options, you'd need to layer external kicks or use a DAW. acidflow's built-in kick is a "standard" dance kick; variation comes from FX, not synthesis.

## The kick as compositional anchor

One reason the 4-on-the-floor kick is universal in dance: it anchors the listener to the meter. Every half-second (at 120 BPM), the kick resets the count. Even if the 303 is doing weird polymeter or the hi-hats are syncopated, the kick says "this is beat 1."

Remove the kick for a bar or two and the listener feels adrift. This is a useful breakdown effect — pull the kick out, let the tension build, drop it back in for impact.

### The kick roll

A classic transitional technique: in the last bar before a major section change, add extra kicks.

```
Voice   1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD (reg) X .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
BD (roll)X .  .  .  X  .  .  .  X  .  .  .  X  X  X  X
                                              ^^^^^^^^
                                              kick roll
```

Steps 13, 14, 15, 16 all kick — a rapid series of kicks leading into the next bar. This is the "kick roll" or "drum roll" transition. Classic acid tracks use this before the drop.

## Don't overthink kicks

For most tracks, a simple 4-on-the-floor kick is correct. Don't obsess over subtle variations unless you have a specific musical reason.

A common mistake: making the kick too interesting. Every drum on acidflow can be programmed with complexity, but the kick should usually be the simplest element. It's supposed to be the foundation; foundations aren't exciting, they're solid.

## Try this

1. Clear drums (`C`). Add only a kick on steps 1, 5, 9, 13. Play. Pure pulse.
2. Remove step 1's kick. Play. The pattern feels wrong, missing its downbeat.
3. Restore step 1. Add a kick on step 4 (right before beat 2). Play. Slight syncopation.
4. Remove the step 4 kick. Add kicks on 14, 15, 16 (a pre-bar-end roll). Play. Dramatic build.
5. Remove the roll. Program a half-time pattern (kick on 1, 9 only). Play. Much more laid back.
6. Add a kick on step 11 to the half-time. Play. Adds the classic half-time fill.

Next: snare, clap, and hats.
