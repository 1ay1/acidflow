# 11 · The envelope — ENVMOD and DECAY

The envelope is the engine. CUTOFF sets the base position of the filter, but the envelope is what makes the filter *move*. Without the envelope, acid doesn't exist — you'd have a static filter on a bass line, which is every other kind of bass synth in history.

Two knobs: ENVMOD and DECAY. Between them, they produce the *ptoonnng* shape that defines every 303 note.

## What an envelope is, in detail

An envelope is a time-varying control signal. When a note is triggered, the envelope jumps to some peak value, then decays back to zero (or to some sustain level — but the 303 has no sustain, so always to zero).

The 303's envelope is **exponential**. This means it decays on a curve — fast at first, slower as it gets lower, asymptotically approaching zero. Plotted:

```
value
 1.0 |\
     | \
     |  \
     |   \___
     |       \______
     |              \____________
 0.0 |______________________________ time
     0     50%       100%
         (of total decay)
```

Over the decay time, the envelope drops from 1.0 to about 0.37, then to 0.14, then to 0.05, and so on. It never quite reaches zero, but becomes inaudible quickly.

This curve is the natural discharge of a capacitor through a resistor — physics, basically. Every analog envelope uses this shape because it's what analog circuits *do* when you ask them to do "fading." It's also, conveniently, the shape that human hearing finds most musical for note decays.

## There are two envelopes on the 303

acidflow, like the real 303, has two separate envelopes:

1. **The filter envelope**: controls the filter's cutoff. Modulates the cutoff up from its base position by ENVMOD octaves, then decays.
2. **The amp envelope**: controls the VCA. Opens the amp when a note triggers, then decays to silence.

Both envelopes share the same DECAY time. They're not independently controllable. This is a design constraint, not an oversight — having them lock-stepped makes the 303's sound coherent (the filter doesn't ring on after the amp has closed, and vice versa).

Some parts of the envelope behavior are a little nuanced:

- The **amp envelope** also has a short release (~30 ms) to prevent digital clicks when a note's gate closes. This release is not user-adjustable.
- The **filter envelope** has no release. It just keeps decaying from wherever it was. If a new note triggers during the decay, the envelope retriggers.
- On **slid notes**, neither envelope retriggers. The note's gate continues from the previous note's state.

## ENVMOD: how much the envelope opens the filter

ENVMOD is the depth of the filter envelope. It scales the envelope's peak value. In acidflow:

- **0% ENVMOD**: filter envelope is zero. Cutoff stays at its base position forever.
- **25% ENVMOD**: filter opens by 1 octave on each note, then closes.
- **50% ENVMOD**: filter opens by 2 octaves.
- **75% ENVMOD**: filter opens by 3 octaves.
- **100% ENVMOD**: filter opens by 4 octaves (the 303's maximum envelope range).

The sweep amount is in **octaves**, not Hz, because of the 303's V/octave modulation circuit. This means the *musical interval* of the envelope movement is constant regardless of where CUTOFF is set. If ENVMOD is 50%, the filter always opens exactly 2 octaves, whether CUTOFF starts at 100 Hz or 1 kHz.

### What different ENVMOD values feel like

At ENVMOD = 0%, every note is the same timbre. The filter is static. The pattern sounds like a monosynth, not like acid. You basically never do this.

At ENVMOD = 20%, there's a gentle filter breathing. Notes feel slightly plucked. This is a dub-acid setting — reserved, moody.

At ENVMOD = 40-60%, classic acid. Notes have a clear attack transient (the filter opening) and a decay into the base tone. The *ptoonnng* is audible.

At ENVMOD = 70-90%, aggressive acid. Every note has a big upward filter sweep. Good for lead moments.

At ENVMOD = 100%, extreme. The filter opens all the way to the top of its range on every note. Brutal — but in combination with high resonance, this is the Hardfloor scream.

### ENVMOD + CUTOFF interaction

The total filter position at note trigger is: `CUTOFF + (ENVMOD × envelope value)`. At the peak of the envelope, when the envelope is 1.0:

```
Total cutoff = CUTOFF_base + ENVMOD octaves
```

If CUTOFF is at 40% (let's say that's ~400 Hz) and ENVMOD is at 50% (2 octaves), then at the envelope peak the filter is at 400 Hz × 4 = 1600 Hz. As the envelope decays, it slides back down to 400 Hz.

This means the CUTOFF knob sets the **closed** filter position, and ENVMOD sets **how far above that** the filter opens on each note.

Practical implication: if you want really wide filter sweeps, set CUTOFF *low* (so the closed position is a dull mute) and ENVMOD *high* (so the opened position is bright). The contrast is what gives the filter sweep dramatic movement.

## DECAY: how long the envelopes last

DECAY sets the time constant for both envelopes. In acidflow, the range is **200 ms to 2000 ms** (0.2 to 2 seconds).

What does "time constant" mean? For an exponential decay, the time constant τ is the time it takes for the envelope to drop to 1/e (about 37%) of its initial value. So if DECAY is 500 ms, the envelope goes from 1.0 to 0.37 in 500 ms, from 0.37 to 0.14 in another 500 ms, etc.

In practice:

- **200 ms**: very short. Each note is a quick *pfft*. The filter opens and closes before the next note arrives (at 120 BPM, a 16th note is 125 ms, so 200 ms means the envelope still has significant life at the next step).
- **500 ms**: classic 303 snap. The envelope closes mostly between notes but still has an audible tail on each.
- **1000 ms**: long and bloomy. The filter takes its time returning to base. Notes overlap in filter-tail.
- **2000 ms**: very long. The filter barely closes between notes — it's basically sustained.

### DECAY and tempo

The 303's DECAY is in absolute seconds, not beats. A 500 ms DECAY at 120 BPM has very different character from a 500 ms DECAY at 140 BPM:

- At 120 BPM, a 16th note is ~125 ms. A 500 ms DECAY takes four 16ths to fall to 37%, seven to fall to 14%. The filter is moving across most of the pattern.
- At 140 BPM, a 16th note is ~107 ms. A 500 ms DECAY takes ~4.7 16ths to fall to 37%. Similar but the filter has less time per step to breathe before the next trigger.

This tempo-dependence is part of why the 303 sounds different at different BPMs. It's also why acid tracks tend to pick a tempo and stay there — the envelope character is locked to that tempo.

You can imitate a "slower" envelope by slowing down BPM, or imitate a "faster" envelope by speeding it up. This is why 132 BPM acid techno feels more urgent than 122 BPM acid house — same DECAY setting produces much tighter envelope behavior at 132.

### Different DECAY settings for different moods

Short DECAY (200-400 ms): punchy, staccato, rhythmic. Good for busy patterns with lots of sixteenth-notes. The filter slaps each note and moves on. Classic Detroit-techno 303 lines use this.

Medium DECAY (400-700 ms): "normal" acid. The filter has time to resonate before the next note. Sound is full-bodied. Chicago acid, UK acid.

Long DECAY (700-1500 ms): bloomy, hypnotic, dub-acid. Notes overlap in filter-tail, creating a sustained resonant drone. Plastikman, ambient acid.

Very long DECAY (1500-2000 ms): the filter is essentially always open. Pattern character is defined by envelope retriggering rather than envelope closing. Unusual, but useful for ambient and drone-style tracks.

## The envelope + accent interaction (preview)

When an accented note plays, the accent circuit adds an *additional* filter opening on top of ENVMOD. If ENVMOD is at 50% (2 octaves) and ACCENT is at 50%, an accented note opens the filter by 3 octaves instead of 2. And because the accent capacitor stacks across consecutive accents, two accents in a row might open 3.5 octaves, three in a row 4.

This is covered in detail in the next chapter. For now, just know that the envelope amount you hear on a non-accented note is *less* than the envelope amount on an accented one.

## Envelope retriggering

Every non-slid note retriggers both envelopes. So if the sequencer is playing sixteenth-notes with no slides, you get sixteen envelope-retriggers per bar. Each retrigger snaps the envelopes back to their peaks.

This is unlike most synths, where a held gate doesn't retrigger. It's also unlike "legato" modes. The 303 is *always* retriggering on non-slid notes, regardless of what the gate is doing. This is a design choice, and it produces the rhythmic, pulsing character of 303 lines.

Slides *suppress* retriggering. A slid note continues the previous envelope rather than restarting it. This makes slid notes sound *smooth* and *legato* — the filter doesn't re-snap open for them. The filter's position at the moment of the slide is the starting position for the slid note, and it continues decaying from there.

Musically, this means: **accented notes feel more attacked. Slid notes feel smoother. Rests create breathing space that lets the envelope drop fully before the next trigger.**

## ENVMOD + DECAY: finding your character

Two knobs, four broad character zones:

| ENVMOD | DECAY | Character                                                |
|--------|-------|----------------------------------------------------------|
| Low    | Short | Dry, clean, unflashy. Not very acid.                     |
| Low    | Long  | Dub, muted, hypnotic. Filter barely does anything.       |
| High   | Short | Aggressive, punchy. Classic fast acid techno.            |
| High   | Long  | Bloomy, screaming. Hardfloor at their most maximal.      |

Most classic acid sits in "medium-high ENVMOD + medium DECAY" — big enough envelope sweeps to be distinctive, long enough decay to let the filter sing.

## The envelope is the voice of the 303

One more framing. Singers shape notes with their breath — attack, body, release. The 303 shapes notes with the envelope. The envelope's rise-and-fall *is* the note's expressive shape. A 303 note without an envelope is a flat drone; a 303 note with an envelope is an utterance.

This is why acid feels *vocal* despite being monosynth. The envelope is doing the work of a voice.

## Try this

1. Load preset 01 (Acid Tracks). Set ENVMOD to 0. DECAY to 500. Play. Notice how dead this sounds.
2. Raise ENVMOD to 50%. Play. The filter is now breathing. Much more alive.
3. Now vary DECAY from 200 to 1500 while the pattern is playing. Hear how short DECAY makes the pattern feel percussive, long DECAY makes it feel sustained.
4. Set ENVMOD to 100% and DECAY to 300. Play. Extreme short sweeps on every note. Very aggressive.
5. Set ENVMOD to 100% and DECAY to 1500. Play. Extreme long sweeps. Each note's filter opens all the way and closes slowly. Drone territory.
6. Go back to a moderate combination: ENVMOD 60%, DECAY 600. This is classic acid. Memorize this feel — you'll come back to it.

Next chapter: accent, which is where this envelope gets its real character.
