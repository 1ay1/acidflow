# 22 · The 5-voice drum machine

acidflow ships a built-in 5-voice drum machine. Not a sample player — a synthesis-based drum machine, closer in architecture to the TR-808 / TR-909 than to a modern sampler. Each of the five voices is generated from scratch: waveforms, noise, envelopes, and filters, with zero samples on disk.

This chapter introduces the five voices. Subsequent chapters dig into each one.

## Why synthesized drums

The choice to synthesize rather than sample was deliberate:

1. **Coherence with the 303 voice**. If the bass is analog-modeled synthesis, the drums should be too. Sampled drums would sit in a different sonic world.
2. **Size**. acidflow is a few megabytes. A drum sample library would multiply that by 100x.
3. **Control**. Synthesized drums can be pitched, detuned, adjusted parametrically. Samples are locked to their recording.
4. **Character**. The specific sound of 808/909 synthesized drums is the "dance music" sound that acid producers have used since 1987. Samples of 808s exist, but the real-time-synthesized versions feel more dynamic.

The trade-off: acidflow's drums are less realistic than samples. A real kick drum, recorded, has subtle microtransients and body resonance that a synth can't fully replicate. But for dance music — where drums serve rhythmic function more than realistic body — synthesis wins.

## The five voices

acidflow has exactly five drum voices. This is a common count — the 909 has 11, the 808 has 16, the LinnDrum has 18 — but for acid, five is plenty. The voices are:

1. **BD** (Bass Drum / Kick)
2. **SD** (Snare Drum)
3. **CH** (Closed Hi-Hat)
4. **OH** (Open Hi-Hat)
5. **CL** (Clap)

Each occupies a specific frequency range and rhythmic function:

- **BD**: 30-100 Hz. The downbeat. Physical impact. The anchor of the pattern.
- **SD**: 150-2000 Hz (body + noise). The backbeat. Where the human ear locks tempo.
- **CH**: 4-8 kHz (high filtered noise). The subdivisions. The "tick-tick-tick."
- **OH**: 4-8 kHz (longer noise tail). The "chick" of offbeats.
- **CL**: 800-3000 Hz (noise bursts). Additional backbeat emphasis.

These five together cover the standard dance drum skeleton. Other percussion (toms, cowbells, rides) are not present — acid doesn't use them much.

## The 16-step drum grid

The drum sequencer is identical to the 303 sequencer in grid structure, but much simpler per step. Each step is either a hit (`X`) or empty (`.`). No pitch, no accent, no slide — just on/off.

Per-voice gain is separate (each voice has its own volume). The whole drum kit goes through a drum bus with its own master gain, then into the same FX rack as the 303.

### Drum grid navigation

`←`/`→` moves between steps. `↑`/`↓` moves between voices. `Space` or `x` toggles a hit on the current step and voice.

`1`..`9`, `0` are shortcuts for steps 1-10 on the currently-selected voice. Super fast for programming: press `1` to drop a hit on step 1 for the current voice, `3` for step 3, `5` for step 5, etc.

`[`/`]` adjust the drum bus master gain.

`c` clears the current voice's row. `C` clears all voices (the whole kit).

### Why a simple grid

Drums don't need per-step modifiers because:

- Accent on drums is less useful than on synth — a drum is already percussive. You can make a drum hit "heavier" by stacking multiple voices rather than accenting one.
- Slide doesn't apply — drums don't glide (well, toms can, but acidflow doesn't have toms).
- Probability could apply, but the UI complexity wasn't worth it for this program's scope. Use p-lock style patterns instead.
- Ratchet could apply (machine-gun snares), but again, complexity trade-off.

Simplicity is the feature. You can focus on *where* to place drum hits without fussing over per-hit dynamics.

## The standard patterns

Every acid producer has a small set of go-to drum patterns they reach for when starting a track. A handful of canonical ones:

### Four-on-the-floor house pattern

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
SD     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
CH     .  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
OH     .  .  .  .  .  .  .  X  .  .  .  .  .  .  .  X
CL     .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
```

- Kick on every beat.
- Snare on beats 2 and 4 (backbeat).
- Closed hat on the "and" of each beat (the offbeats).
- Open hat on the last 16th of beats 2 and 4.
- Clap empty.

This is the foundation of acid house, Chicago style, and most dance music.

### Straight techno pattern

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
SD     .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
CH     X  .  X  .  X  .  X  .  X  .  X  .  X  .  X  .
OH     .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
CL     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
```

- Kick on every beat.
- Closed hat on every offbeat AND every on-beat (8th notes).
- No snare; clap instead on beats 2 and 4.
- No open hat.

This is the relentless techno feel. Very minimal, very driving.

### Broken kick pattern

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  X  X  .  .  .  X  .  .  .
SD     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
CH     .  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
OH     .  .  .  .  .  .  .  X  .  .  .  .  .  .  .  X
CL     .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
```

- Kick on beats 1, 2, 2+, 3, 4. The syncopated kick on 2+ (step 8) and the double-kick at 2-2+ is a "broken" feel.
- Snare on 2 and 4.
- Hats as standard.

This is the UK acid / electro acid pattern — more syncopated kicks, more energy.

### Half-time pattern

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  .  .  .  .  X  .  .  .  .  .  .  .
SD     .  .  .  .  .  .  .  .  .  .  .  .  X  .  .  .
CH     .  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
OH     .  .  .  .  .  .  .  X  .  .  .  .  .  .  .  X
CL     .  .  .  .  X  .  .  .  .  .  .  .  .  .  .  .
```

Kick only on beats 1 and 3 (half the normal rate). Snare on beat 4 only. Clap on beat 2.

This is the half-time feel — common in drum 'n' bass crossover acid, electro, and breakdown sections. Slower kick rate feels more laid-back.

## The drum bus

All five drum voices sum into a single **drum bus**. The bus has:

- A **drum bus master gain** (adjustable via `[`/`]` when drums are focused).
- A **drum bus mute** (context-dependent; via `m` in drums section).
- A send to the FX rack (after the FX mix setting).

The drum bus goes into the FX chain at the same point as the 303 voice. This means the delay, reverb, and FX drive apply to both. You can't have drums dry and 303 wet — they're on the same bus.

(If you really need separate FX, you'd need to export the drums and bass separately and process in a DAW. acidflow doesn't support send-per-voice.)

## Drum volumes and mix balance

Each voice has its own gain. The default gains are tuned so a standard pattern sounds balanced. But you'll often want to adjust:

- **Kick too quiet**: raise BD gain.
- **Hats too loud**: lower CH and OH gains.
- **Snare buried**: raise SD gain.
- **Clap too dominant**: lower CL gain.

Balance is subjective but there's a common starting point:

```
BD:  0.0 dB (full)
SD: -3.0 dB (slightly back)
CH: -6.0 dB (considerably back)
OH: -6.0 dB (same as CH)
CL: -4.0 dB
```

This puts the kick upfront, the backbeat clear but not overpowering, and the hats/clap as supporting elements.

## Drum patterns and BPM

Different BPMs call for different drum patterns. At 120 BPM, a busy 16th-note hi-hat pattern feels active. At 140 BPM, the same pattern feels frantic. At 100 BPM, it feels relaxed.

You don't need to change your drum pattern for every BPM, but be aware that the same pattern feels different at different tempos. If a drum pattern feels "wrong" at a given BPM, try simplifying it (removing some hi-hat hits) or complicating it (adding some) until it fits.

## The empty kick method

A technique worth knowing: don't put a kick on step 1. Put it on steps 5, 9, 13 only.

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     .  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
```

Beat 1 is missing the kick. The clap or snare on beat 2 becomes the pattern's strongest beat. Creates a "floating" feel where the pattern doesn't land on its downbeat.

Used in trip-hop, some minimal acid, and breakdowns of otherwise-straight tracks.

## Why NOT use external drums

If you have access to a DAW or external sampler, you could theoretically route acidflow's 303 voice to an external track and layer samples in a DAW. This is fine for finished productions but has costs:

- You're out of the terminal, which defeats the "terminal instrument" ergonomic.
- The DAW adds latency to the 303 relative to your external drums.
- You lose the integrated song/jam modes.

For pure composition and prototyping, acidflow's built-in drums are sufficient. Many producers use them for full tracks.

## Try this

1. Clear the kit (`C`). Now program the standard 4/4 pattern given above: kick on 1, 5, 9, 13; snare on 5, 13; closed hat on 3, 7, 11, 15; open hat on 8, 16.
2. Play. This should sound like "a dance track."
3. Add a clap on steps 5 and 13. Play. Even thicker backbeat.
4. Remove the snare on steps 5 and 13 (leaving just the clap). Play. Different feel — clap is more electronic than snare.
5. Change kick pattern to the "broken" style (1, 5, 8, 9, 13). Play. Syncopated kick.
6. Try half-time: kick only on 1 and 9. Play. Drastically different mood.
7. Now add the 303 pattern on top of each of these. Notice how the drum pattern changes the 303's apparent feel.

Next chapters: each drum voice in depth.
