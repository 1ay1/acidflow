# 28 · Delay: repeats, echoes, space

Delay is the second effect in the FX rack. If drive colors tone, delay colors *time*. It takes the signal, holds it for a defined duration, and plays it back — producing an echo. Feed a little of the echo back into the input, and you get many echoes, each quieter than the last.

Delay is the single most important effect on the 303. More than drive, more than reverb. A plain 303 line with tasteful delay sounds hypnotic. The same line dry sounds like an exercise.

## What delay is

A delay line is conceptually simple:

1. Write the input sample into a circular buffer.
2. Read the sample that was written N samples ago.
3. Output the current input + (delayed output × wet amount).

Repeat at every sample. The output trails the input by N/sample_rate seconds.

To get multiple echoes, you feed a portion of the delayed signal back into the input. Each pass through the buffer attenuates the signal. After a few passes, it's inaudible.

## acidflow's delay

acidflow's delay is:

- **Mono tape-style**. Sums stereo input to mono before the buffer. Tape machines were mono or dual-mono; acidflow respects that convention.
- **Tempo-synced**. The delay time is locked to the sequencer BPM. You pick a beat division (1/16, 1/16 dotted, 1/8, 1/8 dotted), not a time in milliseconds.
- **~2 seconds maximum buffer** (96,000 samples at 48 kHz). More than enough for any musical division at any reasonable tempo.
- **Feedback with a tape-darkening lowpass** at ~3.5 kHz inside the feedback path. Each repeat is duller than the last, mimicking analog tape's high-frequency rolloff.
- **Feedback clamped below 1.0** (maximum 0.92) to prevent runaway self-oscillation. You can't make it scream.

### The signal flow

```
Input ──┬──────────────────────────────┬──→ Output (dry)
        │                              │
        └──→ [write buffer] ──→ [read buffer delayed by N samples]
                                          │
                                          ├──→ × wet → output
                                          │
                                          └──→ × feedback → [lowpass 3.5 kHz] → back to input
```

## Controls

acidflow exposes three delay parameters:

- **DLY MIX** — wet send amount (0 to 1). 0 = bypass (no audible delay). 1 = full wet (but the dry still passes through; delay is additive, not a replacement).
- **DLY FB** — feedback amount (0 to 0.92 internal cap). 0 = one echo only. 0.92 = many echoes that take a long time to decay.
- **DLY DIV** — division, one of: 1/16, 1/16 dotted, 1/8, 1/8 dotted.

### Division explained

At 120 BPM, one beat = 500 ms. Divisions:

- **1/16**: 125 ms (one 16th note). Fast, tight slapback.
- **1/16 dotted**: 187.5 ms (one 16th + half again). Creates polyrhythmic tension against a straight 4-on-the-floor.
- **1/8**: 250 ms (one 8th note). Classic "dub" delay, one echo per beat pair.
- **1/8 dotted**: 375 ms (one 8th + half again). The trance / big-room delay. Echoes land on syncopated positions relative to the beat, giving a "pulling forward" feel.

Dotted divisions create polyrhythmic interest because they don't line up with the 4/4 grid. A straight 1/16 delay just doubles hits; a dotted 1/16 delay lands between the hits, filling gaps.

## Why tempo-sync and not free time

You could set a delay to "192 ms" or similar, and pros often do. acidflow doesn't let you — delay is always tempo-synced.

The reason: at dance tempos, echoes that are NOT tempo-aligned sound like mistakes. The listener hears "wrong" timing. An echo that lands on a sixteenth-note boundary reinforces the meter; an echo at 212 ms at 120 BPM lands in no-man's-land and clashes.

If you really want a free-time delay, you can tune your track's BPM to produce the delay time you want. But in practice, one of the four divisions will always sound musical.

## Feedback: how many echoes

The feedback amount determines how many echoes you hear before they decay below audibility.

At feedback = 0.3, each echo is 30% the volume of the previous. After 3 echoes, amplitude is 0.027 (about -31 dB). Four echoes total.

At feedback = 0.6, each echo is 60%. After 5 echoes, amplitude is 0.078 (about -22 dB). Six or seven echoes total.

At feedback = 0.85, each echo is 85%. After 10 echoes, amplitude is 0.197 (about -14 dB). Fifteen+ echoes.

At feedback = 0.92 (maximum), echoes take forever to fade. Practically a sustained wash.

For clean, defined echoes: feedback 0.2-0.4. You hear maybe 2-4 echoes, then silence.

For "dub" echoes that trail off gradually: 0.5-0.7. Five-to-ten echoes.

For long tails approaching infinity: 0.8+. Use sparingly — every repeat builds up in the buffer.

## The tape lowpass

Inside the feedback loop is a one-pole lowpass at ~3.5 kHz. This means each pass through the feedback loop attenuates highs slightly.

After 10 passes, the high end is significantly rolled off. The echo becomes a dark, boomy whisper of its original self.

This mimics analog tape delays (like the Space Echo, Echoplex). Tape's high-frequency response degrades with each pass because of head alignment, bias, wear. Digital delays without this filter sound "sterile" — every echo is an identical-brightness copy.

acidflow's delay sounds analog because of this tape filter. It's a deliberate choice to match the aesthetic of 1980s/90s acid records that used Echoplexes and Space Echoes.

## When to use delay

### To fill space on a sparse 303 line

A 303 line with many rests can feel empty. Add a 1/8-dotted delay, and the rests get filled with echoes of the previous hits. The pattern feels denser without you adding any actual notes.

This is how Hardfloor, Oxia, and countless acid producers sound fuller than they program.

### To create polyrhythm

A straight 303 line against a dotted delay creates cross-rhythms. The original line is on-grid; the echoes are off-grid. Together they form a polymetric texture more complex than either alone.

Especially effective when the delay mix is substantial (40-60%) — the echoes are near-equal partners with the original notes.

### To add space without reverb

Reverb adds diffuse space (as if in a room). Delay adds discrete space (as if in a canyon). Delay's echoes are distinct events, not a blur. If you want the 303 to feel "far" but without the room-wash of reverb, short delay with moderate feedback does this.

### To create breakdowns

In a breakdown, crank the feedback to 0.85 and let the delay pile up. Then cut the 303's VOL — the delay continues to echo what was previously there, slowly fading. Classic transition technique.

## When NOT to use delay

### If the 303 is dense and busy

A 303 with hits on every step, accented, drive, resonance — delay on top = chaos. The echoes pile on top of the rapid original notes, creating a smear.

Reserve delay for sparse-to-medium 303 lines (4-10 hits per bar). Above that, delay overloads.

### If drums are loud in the mix

Remember: delay is on the FX bus, shared between 303 and drums. If drums are loud, they'll get delayed too. Delayed kicks = muddy low end. Delayed claps = stuttering backbeat.

If you want prominent delay AND prominent drums, lower the drum bus gain so drums don't contribute much to the delay. Or accept that delay is mostly for the 303 and keep drum hits below the point where the delay makes them messy.

### At very high feedback for extended time

0.85+ feedback builds up energy in the buffer. If you leave it there, the buffer keeps summing echoes on top of echoes. Eventually this can produce a wash that drowns the dry signal.

Use high feedback for short bursts (breakdowns, transitions) and lower back to 0.4-0.5 for the main track.

## Delay and rhythm programming

The delay interacts with the step pattern. If your 303 has a hit on step 1 and delay is 1/8, you'll hear an echo on step 3 (one eighth later = 2 sixteenths later = step 3).

If you ALSO have a programmed hit on step 3, the delay and the original hit sum — producing a double-amplitude hit.

If you have a programmed hit on step 2, the delay (step 3) will sound between programmed hits, which can be musically nice or cluttered depending on the density.

**Tip**: program 303 hits with awareness of where the delay will place echoes. Leave space in the pattern for the delays to land. You can use delay as a "second layer" of hits that you don't have to program.

## Dotted eighth and the "trance" delay

1/8 dotted (.375 ms at 120 BPM) is the delay time of every trance/progressive producer since the 1990s. It's distinctive because:

- 1/8 dotted = 3/16 of a beat.
- The echo lands 3/16 after the hit.
- The original + echo pattern goes: hit (step 1), echo (step 4 — 3 sixteenths later), which in step terms: 1, 4, 7, 10, 13, 16 (the pattern of "every 3 sixteenths").
- This is a hemiola (3 against 4) — a polyrhythm that feels like the music is "falling forward" into the next bar.

This is the "Sandstorm" / "Children" delay. It's cliché but effective. If you want that driving, pulsing, pulling-forward feel, set DLY DIV to 1/8 dotted.

## Straight eighth for dub acid

1/8 straight delay doubles hits on the offbeat. A 303 hit on step 1 echoes on step 3, which already has a hat, so they reinforce the hat pattern.

Good for laid-back, dub-influenced acid. The delay creates a "rolling" feel without the polyrhythmic tension of dotted.

## Sixteenth delay for tight slapback

1/16 delay is too fast for most acid. The echo lands 1 sixteenth later, which if you have any hit density at all, creates near-immediate doubling.

Works well for: breakdowns where you want the 303 to sound slightly "doubled" or "enhanced" without obvious echoes. The repeats are so fast they blur into a texture, not distinct echoes.

## 1/16 dotted for experimental

1/16 dotted = 3/32 of a beat. Very fast, very polyrhythmic. Creates stuttering, chattering echoes that don't line up with anything.

Use experimentally. Not a default choice.

## Delay and automation

You can p-lock delay mix per step... except acidflow doesn't p-lock FX parameters. Delay settings are global — they apply to the whole pattern identically.

This means you can't turn on delay only on one note. If you want that, you'd save two pattern slots (one with delay off, one with delay on) and chain them.

### Live delay sends

You CAN adjust delay parameters live during playback. Changes take effect immediately (with a 60 ms smoothing ramp to avoid clicks).

Common live technique: start with DLY MIX at 0. Build up the track. At the breakdown, sweep DLY MIX from 0 to 60%, add feedback from 0.3 to 0.85, let the echoes fill the space. At the drop, sweep back to 0.

This kind of live manipulation is where delay earns its keep.

## Delay and self-oscillating filters

If you have a self-oscillating 303 (RES at maximum) AND delay feedback at 0.85, you're in trouble. The self-oscillation note sustains at a single pitch, which feeds into the delay, which feeds back, which compounds.

Result: extremely loud sustained tone. Possibly clipping.

To avoid: when resonance is high, keep delay feedback moderate (0.4-0.6 max).

## Delay as melodic tool

A subtle use: program a simple 303 line and let the delay fill in the melody. With 1/8 dotted delay, your programmed notes become "points" and the delays become "connective tissue." The listener can't easily distinguish programmed from echoed — they all feel like notes.

This means you can program fewer notes than you think. Delay supplies the rest.

Many "busy sounding" acid tracks have startlingly sparse 303 programming. The delay does the work.

## Delay and melody choice

If your 303 line is in A minor pentatonic, the delay will repeat those notes, which are also in A minor pentatonic. All echoes are consonant.

If your line uses chromatic approach tones (outside the scale), the delay will repeat those too — including the "wrong" notes. Delay amplifies whatever you've programmed. Good programs sound better with delay; bad programs sound worse.

## Delay and feel

Delay slightly "softens" the feel of a track. The crisp in-the-pocket hits get smeared by echoes landing slightly off-grid. This can be:

- **Desirable**: for dub, trance, ambient acid where softness is the aesthetic.
- **Undesirable**: for driving acid techno where you want every hit to be tight and forceful.

For hard techno, use delay sparingly (mix < 20%, feedback < 0.3). For melodic acid, delay can be aggressive (mix 40-60%, feedback 0.5-0.7).

## Try this

1. Set up a 303 line with hits on steps 1, 4, 8, 11 (sparse). Set DLY MIX to 0. Play. Sparse.
2. Raise DLY MIX to 40%. Set DLY DIV to 1/8. Set DLY FB to 0.4. Play. Now you hear echoes filling the rests. The line feels denser.
3. Change DLY DIV to 1/8 dotted. Play. The echoes land in syncopated positions, creating polyrhythm.
4. Change to 1/16. Play. Tight slapback, harder to distinguish echoes from original hits.
5. Change to 1/16 dotted. Play. Chattering, complex polyrhythm.
6. Set DLY DIV back to 1/8 dotted. Raise DLY FB to 0.8. Play. Long echo trail.
7. Raise DLY FB to 0.92 (maximum). Play. Endless echoes — the sound sustains far beyond the original hits. Breakdown material.
8. Lower DLY FB to 0.3, DLY MIX to 20%. Play. Subtle, supportive delay. This is how delay often sits in final mixes — adding space without demanding attention.

Next: reverb.
