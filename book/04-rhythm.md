# 4 · Rhythm, meter, and groove

Rhythm is where acid music does its real work. The filter sweeps and the pattern mutations are *ornamentation*; the rhythmic skeleton is what makes people dance. This chapter is long because rhythm is the thing beginners most consistently underestimate.

## Meter

**Meter** is the repeating count that organizes musical time. In acid and almost all dance music, the meter is **4/4** — four beats per bar, with each beat getting equal weight. The bar is the unit of musical thinking; everything loops, accents, and resolves in bars.

```
|  1  |  2  |  3  |  4  |  1  |  2  |  3  |  4  |
```

Every acid track is built on this counting structure, whether the producer thinks about it consciously or not. The kick drum hits on every "1, 2, 3, 4" — that's the **four-on-the-floor** pattern. The snare or clap hits on "2" and "4" — that's the **backbeat**. The hi-hat pattern fills in everything else.

Nothing in this is up for debate. Four-on-the-floor and backbeat are not genre conventions — they are the *definition* of dance music's rhythmic frame. Acid departs from them only in extreme cases (see broken-beat acid, late Plastikman, Aphex Twin's more deranged 303 tracks).

## Subdivisions

A **beat** can be divided into smaller equal parts. The usual divisions:

- **Quarter note** = one beat (a full count).
- **Eighth note** = half a beat (two per beat).
- **Sixteenth note** = quarter of a beat (four per beat).
- **Thirty-second note** = eighth of a beat (eight per beat).
- **Triplets** divide the beat into three instead of two or four.

acidflow's 16-step sequencer is **sixteen sixteenth-notes long** — one bar of 4/4 at 16 sixteenths = 16 steps. Each step is one sixteenth note. The entire pattern is one bar.

```
Beat:     1       2       3       4
Steps:    1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
```

Step 1 is the downbeat. Step 5 is beat 2. Step 9 is beat 3 (the midpoint of the bar). Step 13 is beat 4. These positions are **strong**; the others are **weak**. Strong and weak is the whole of acid rhythm.

## Strong and weak beats — why this is everything

A listener's ear expects something to happen on the strong beats. When something does happen there, the ear nods along. When something happens on a *weak* beat instead — especially a weak beat that's offbeat — the ear perks up. That mismatch between expectation and event is called **syncopation**, and it's the single most important rhythmic concept in dance music.

Consider two patterns, both four notes:

```
Pattern A (on-beat):     x . . . x . . . x . . . x . . .
                         1       2       3       4
                         (notes on 1, 2, 3, 4)

Pattern B (off-beat):    . . x . . . x . . . x . . . x .
                             1+      2+      3+      4+
                         (notes between beats)
```

Pattern A is a quarter-note pulse. It sounds like a metronome. Pattern B has the same density but lands on the "and" of each beat — the weak position. Pattern B sounds like it's *pushing* or *pulling* the beat. It has energy.

Acid lives in Pattern-B territory. The bass line avoids the downbeat surprisingly often. Accents land on off-beats. Slides carry the motion across the bar line. The kick drum provides the steady pulse so the 303 can afford to be rhythmically wayward.

## The 4/4 skeleton of dance music

Here's what a full dance-music bar usually looks like across drums:

```
Step:      1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Kick:      X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
Clap/snare:.  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
Closed hat:.  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
Open hat:  .  .  .  .  .  .  .  X  .  .  .  .  .  .  .  X
```

- Kick on every beat (steps 1, 5, 9, 13).
- Clap on beats 2 and 4 (steps 5 and 13). The backbeat.
- Closed hat on the off-eighths (steps 3, 7, 11, 15). The "and" of each beat.
- Open hat on the last sixteenth of beats 2 and 4 (steps 8 and 16). This is the "chick" that propels the beat into the next bar.

This is a **standard 4/4 dance pattern**. If you program exactly this on acidflow's drum grid (Chapter 25 walks through it), you have the rhythmic skeleton of 90% of acid and house records.

## Where the 303 goes on this skeleton

With the drums laid out, where should the bassline live? A few options:

1. **On every step** (all 16). Drone-like. This is *Acid Tracks*, more or less. Works when the pitch is mostly static and the filter is the main event.
2. **Every other step** (1, 3, 5, 7...). Rolling, energetic. Most acid techno.
3. **On the offbeats** (3, 7, 11, 15). Countermelody to the kick. Great for tension.
4. **Sparse** (4-8 steps with rests between). Melodic rather than groove-based. *Voodoo Ray* territory.
5. **Mixed** — some groups of steps dense, some with rests. This is what p-locks and ratchets are made for, and what most interesting acid lines actually do.

The specific choice defines the *feel* of the track before you've chosen any pitches. A constant-sixteenth 303 pattern *is* dense and aggressive regardless of the notes. A sparse 303 pattern *is* melodic regardless of the notes.

## The role of rests

A **rest** is a step with no note. Rests in acid are not silence — they're *implied notes*. The listener's brain fills them in. If a pattern plays on steps 1, 2, 3, 4, (rest), 6, 7, 8..., the rest on step 5 is heard as "a missing note we expected." That expectation-violation is energy.

Rests also let the filter's envelope *close* fully before the next note retriggers. If you play every step, the envelope is constantly being re-triggered and the accent circuit is constantly being re-charged. Adding even one rest per four steps changes the envelope dynamics dramatically.

In acidflow, `m` on a sequencer step sets it to rest. We dedicate an entire chapter to rests and ghost notes (Chapter 17).

## Swing

**Swing** is a delay applied to every second sixteenth note. In a straight 16-step pattern with 50% swing, every step falls exactly on time. With 60% swing, the even-numbered steps are delayed by some small fraction. With 75% swing, they're delayed much more — the pattern starts to feel like a triplet.

The formula acidflow uses (and that essentially all DAWs use): at `S%` swing, the even step's position is pushed forward by `(S - 50%) × 2` of its original duration. 50% = no swing. 66% = a classic "shuffle" groove. 75% = heavily triplet-feel. 100% = the even step is delayed maximally, sitting on the next step's downbeat (unusable).

Swing's effect on acid is subtle and load-bearing. Straight 50% is mechanical — good for aggressive techno, bad for groove. 55–60% feels "just slightly human" and is the most-used setting for house. 65–75% is for deep/soulful grooves, rarely appropriate for acid, but can transform a generic pattern into something with a distinct swing character.

Acid techno tends to live at 50%. Chicago acid tends to sit around 55%. Detroit acid and its descendants play with 60% more than you'd expect. Try each.

## Tempo

**Tempo** is measured in BPM — beats per minute. A beat is a quarter note. At 120 BPM, a beat lasts 0.5 seconds and a 16-step bar lasts 2 seconds.

The 303 is highly tempo-sensitive because its envelope and accent circuits have fixed time constants. A 303 at 100 BPM sounds different from a 303 at 140 BPM — not just "faster," but with different envelope shapes (because the decay has more or less time relative to the beat) and different accent dynamics (because the accent capacitor charges differently at different note rates).

Tempo conventions by subgenre:

| Subgenre                | Typical BPM |
|-------------------------|-------------|
| Chicago acid house      | 118–125     |
| UK acid (*Voodoo Ray*)  | 120–128     |
| Detroit acid techno     | 128–135     |
| Hardfloor / Acperience  | 130–138     |
| Stay Up Forever acid    | 135–150     |
| Rotterdam hardcore acid | 150–180+    |
| Electro acid (slower)   | 105–120     |

acidflow defaults to 125 BPM, right in the sweet spot for classic acid house. Push to 135 for acid techno, drop to 110 for electro acid. You'll hear the envelope character change with each shift — this is not a bug, it's the instrument's nature.

## Hemiola, polyrhythm, and pattern length

The 16-step pattern is one bar. What if you want longer patterns, or *asymmetric* patterns?

acidflow supports variable pattern length, from 4 to 16 steps. If you set the length to 12, the pattern is three beats long — but the drum machine is still 16 steps long! That means the 303 pattern and the drum pattern **re-align every bar**, producing a shifting relationship between them. This is called a **polymeter** (two meters running simultaneously) or a **hemiola** (the 3-against-4 case).

Polymeters are unusual in acid and should be used sparingly. When they work, they're hypnotic — the 303 seems to *walk* across the drum pattern. When they don't work, they sound like something is broken. Good uses:

- Length-12 303 over length-16 drums: the bass returns to the "1" of the drum pattern every three bars. Feels like a long spiral.
- Length-15 303 over length-16 drums: the bass "lags" by one step each bar, effectively walking backwards through the pattern. Plastikman territory.
- Length-8 303 over length-16 drums: the 303 pattern plays twice per drum bar. Doesn't feel polymetric — feels like a shorter loop.

The gotcha: lengths that *divide* 16 (4, 8, 16) don't produce polymeter. They just loop faster. Lengths that don't divide 16 (3, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15) do.

## Groove, swing, and micro-timing

Beyond swing, there's **micro-timing** — tiny delays or advances of specific notes that give a pattern "feel." Acidflow doesn't expose per-step timing nudge (that would be a complexity too far), but the swing parameter and the ratchet feature (Chapter 19) give you most of the musically-useful ground.

A "grooved" pattern has three ingredients:

1. **Dynamics** — accented steps feel stronger, non-accented feel weaker.
2. **Timing** — swing and/or micro-timing variations.
3. **Rests** — strategic absence, so the ear has space to *feel* what's present.

The single biggest upgrade to your patterns when you're starting is to realize that an acid line without accents, rests, and/or swing — a "flat" 16-note pattern — is not a groove. It's a drone. Drones have uses, but they are not what most acid records do.

## The pocket

Producers talk about a pattern being "in the pocket" — meaning the pattern locks into the drum groove in a way that feels inevitable. It's half art, half listening. When a 303 line is in the pocket, the listener can't imagine it being any other way.

Patterns get in the pocket by:

- Landing accents on beats the drums *also* emphasize (or deliberately avoiding them).
- Putting slides *across* a strong beat so the pitch motion crosses the downbeat.
- Matching the 303 pattern's "density profile" to the drums' density profile.

Patterns fall out of the pocket by:

- Having accents on random positions that conflict with drum accents.
- Having slides that start on weak beats and end on weak beats (the slide has nowhere to *go*).
- Being rhythmically identical to the drum pattern (no counterpoint = no interest).

You learn pocket by listening. We'll come back to it in Chapter 36 (listening studies).

## Try this

1. Program the standard 4/4 drum pattern given above. Play it on loop.
2. On the 303 sequencer, set every step to C (`c`). Every step. Play. Listen — this is a drone. Boring.
3. Now delete every other note (press `m` on steps 2, 4, 6, 8, 10, 12, 14, 16). Play. Still not very acid, but less boring.
4. Now put the notes back but accent *only* steps 1, 4, 7, 10, 13, 16. (Press `a` on each.) Play. Hear how the accents create their own rhythm inside the sixteenth-note grid.
5. Now mute steps 3 and 11. Two rests, in asymmetric positions. Play. You now have a pattern with internal rhythmic structure.
6. Bump the swing to 58%. Play. Everything loosens.

You've just built a real acid bar. It won't win any Grammys, but the rhythmic skeleton is genuine. Next chapter: what's actually happening inside the synthesizer while that pattern plays.
