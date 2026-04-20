# 16 · Writing melodies in semitones

You have 16 steps. On each step, you can set a pitch anywhere in a five-octave range. That's 60+ possible pitches per step, 16 steps per pattern — a combinatorial space so large you could never exhaust it. Nor should you. This chapter is about restricting the space in musically useful ways so your patterns have *form*, not just variety.

## The spectrum of melodic density

Acid patterns can be characterized by how *much* pitch they use:

**Drone (1 pitch)**. Every step is the same note. The entire melodic content is carried by filter/accent/rhythm. Very minimal. *Acid Tracks* is close to this — it's essentially a C drone with occasional octave jumps. Plastikman does this a lot.

**Two-note (2 pitches)**. Usually root + octave, or root + 5th. Patterns alternate. *Voodoo Ray* is a famous two-note pattern in its main hook.

**Few-note (3-5 pitches)**. A small pitch set that can be rearranged. Still firmly acid territory.

**Melodic (5+ pitches)**. Feels more like a bass line or lead part. Venturing toward melodic techno or IDM.

Most classic acid is in the 1-5 pitches range. More than 5 is unusual and often a warning sign that the producer is overcomposing.

## The root as gravitational center

In any acid pattern, there is a **root note** — the pitch that the pattern "returns to" or "sits on" most. Making this explicit is the first compositional decision.

Common roots in acid:

- **C** (most common, easiest to program).
- **A** (*Acid Tracks* is in A minor).
- **F** (slightly darker than C, common in deeper acid).
- **D** (brighter; less common in acid but shows up in Detroit techno).
- **E, G, etc.** (used occasionally).

The root doesn't have to be the lowest note in your pattern — it's the tonal center, the note the ear considers home. Usually, at least 40-60% of your steps should be on the root or its octaves.

## Starting on the root

Step 1 of your pattern should almost always be the root. This tells the listener where the pattern is grounded. Acid patterns that start on a non-root pitch feel disorienting, like the track is already halfway through something when you drop in.

Exception: deliberately-unbalanced patterns that build to the root. E.g., step 1 is a minor 7th and step 5 is the root, so every beat 2 feels like an arrival. This is a more advanced move and should be used sparingly.

## Octave jumps

The single most common melodic gesture in acid is an **octave jump** — the same note as the root, just up one octave (12 semitones).

```
Pattern: C2  C2  C2  C3  C2  C2  C2  C2
                     ^^ octave up here
```

Press `>` in acidflow to jump the selected step up an octave. Press `<` to jump down.

Octave jumps work because they preserve pitch class (same "note"), just higher. The ear doesn't perceive them as "different notes" — it perceives them as "more intense versions of the same note." They add energy without disturbing the tonal center.

Common octave-jump placements:

- **Step 5 (beat 2)**: syncopated punch between the kick hits.
- **Step 9 (beat 3)**: the midpoint of the bar.
- **Step 13 (beat 4)**: leading into the next bar.
- **Steps 14, 15, 16**: a pre-bar-end flurry of high notes.

A pattern with the root on all 16 steps plus octave jumps on steps 5, 9, 13 is already a recognizable acid shape.

## The fifth

The second most common non-root pitch is the **perfect fifth** — 7 semitones up from the root. In C, that's G.

The fifth is a consonant interval. It doesn't clash with the root. It sits in harmonic support. Patterns that alternate root and fifth sound strong and structural.

```
Pattern: C2  C2  G2  C2  C2  C2  G2  C2
                 ^       ^       ^
                 fifth   fifth   fifth
```

The fifth can also be slid into for melodic emphasis:

```
Pattern: C2  C2  G2  C2
Slide:   .   .   S   .
         The G2 is slid into from the previous C2.
```

This is the single most-used melodic gesture in acid: the slide-up-to-the-fifth-and-back.

## The minor third

7 semitones above root is the fifth. 3 semitones above root is the **minor third** — in C, that's D♯ (or E♭). This is the darkening interval; it's what makes minor keys sound minor.

```
Pattern: C2  D#2  F2  G2   ←  this is a C minor "scale walk"
              ^        ^^
              minor 3rd  fifth
```

Minor thirds appear in Phrygian-influenced patterns (the menacing ones), in minor-key arpeggios, and whenever you want a dark, bluesy color.

## The minor seventh

10 semitones above root is the **minor seventh** — in C, that's A♯ (or B♭). This is a bluesy, jazzy interval that's much used in acid basslines because it feels both dark and unresolved.

Patterns that walk root → 5th → minor 7th → root back down have a strong "blues-bass" feel:

```
Pattern: C2  G2  A#2  G2  C2
                 ^^^
                 minor 7th
```

Classic acid lines use this progression often.

## The octave above the fifth

When you do an octave jump from the fifth, you get the **octave + fifth** (12 + 7 = 19 semitones). In C, that's G3 above C2. This is the classic "scream" note — a high, piercing pitch that sits above the pattern.

Patterns that go root → octave → octave+fifth → octave make a climbing melodic shape:

```
Pattern: C2  C3  G3  C3
             ^^  ^^^  ^^
             octave octave+fifth
```

This is common in acid techno where you want to cut through a dense mix.

## Melodic patterns using the minor pentatonic

The minor pentatonic in C is: C, D♯, F, G, A♯. Five notes. Almost everything in acid fits inside this set.

A template pattern using all five notes:

```
Pattern: C2  D#2  F2  G2  A#2  C3  A#2  G2  F2  D#2  C2  C2  C2  G2  C2  C2
```

That's a scale walk — ascending through the pentatonic, peaking on C3, descending back, then a short coda. It's slightly too busy for classic acid, but it's a starting point for editing.

Trim it down:

```
Pattern: C2  .  F2  .  G2  .  C3  .  G2  .  F2  .  .  C2  .  .
```

Half the notes, same melodic skeleton. Now we're in acid territory.

## The question-and-answer form

A 16-step pattern naturally divides into two halves: steps 1-8 (first half) and steps 9-16 (second half). Many acid patterns use a **question-and-answer** structure where the first half poses a melodic idea and the second half responds to it.

Example:

```
Steps 1-8:   C2  C2  C2  G2  C2  C2  G2  C2
Steps 9-16:  C2  C2  G2  C3  C2  C2  G2  C2
             ^same    same ^
             ^  root  ^   octave response
             question     answer
```

The first half is "root-heavy with some fifth." The second half is "same, but with an octave in the middle." The octave is the "answer" — it changes the dynamic just enough to differentiate the halves.

Patterns with question-and-answer structure feel musical even when the pitches are sparse. Patterns without it feel flat — 16 steps of the same stuff.

## The crescendo shape

Another common structure: pitches start low and rise across the bar.

```
Steps: 1   2   3   4   5   6   7   8   9   10  11  12  13  14  15  16
Pitch: C2  C2  C2  C2  F2  F2  F2  F2  G2  G2  G2  G2  C3  C3  C3  C3
```

The pattern climbs through C → F → G → C (octave). The listener feels *ascending motion*. This is arrangement-scale melody — instead of the bar being a static unit, it's a rising gesture.

Combine with filter opening over the same bar and you have a "mini-buildup" that resolves at bar start.

## Avoiding bad pitches

Some pitches just sound wrong in most acid contexts:

- **Major 3rd** (4 semitones above root): too bright, breaks the minor mood. In C, E.
- **Major 7th** (11 semitones above root): sophisticated-jazzy, breaks the acid feel. In C, B.
- **Major 2nd** (2 semitones above root): passing tone only, rarely lands well. In C, D.

These aren't *banned* — some great acid records use them deliberately for color. But if you're a beginner, avoid them. Stick to pentatonic tones (root, minor 3rd, 4th, 5th, minor 7th).

## Pitches that work across scales

A handful of pitches sound good in virtually any minor-key context:

- **Root**
- **Octave of root**
- **Fifth**
- **Octave + fifth**
- **Minor third**
- **Minor seventh**

If you're stuck, fill your pattern with only these six pitches and vary via octave. You can't sound bad.

## Writing melodies in practice

A technique that works:

1. Clear the pattern (`x` on every step).
2. Set every step to root (press `c` 16 times).
3. Play. This is the drone baseline.
4. Edit 2-4 steps to be octaves (press `>` on them). Play. You've added basic variety.
5. Edit 1-2 steps to be the fifth (press `g`). Play. You have melodic motion.
6. Consider adding a minor 7th or minor 3rd on one step. Play.
7. If the pattern feels flat, *remove* notes (rest them) rather than adding more pitches. Less is more.

The point is to start dense and subtract, or start sparse and add. Either is fine; what's fatal is randomly jabbing at keys and hoping for a melody.

## Note lengths

acidflow doesn't have explicit per-step note length — each step's note is one 16th long, with the envelope decay extending beyond. The DECAY knob controls how long notes ring.

If you want a long-sustain section, turn DECAY up. If you want a staccato section, turn DECAY down. That's your length control.

Slides extend notes too — a slid note continues the previous envelope rather than re-triggering, so the effective "note length" is longer than one step.

## Transposition

To transpose the whole pattern, you have three options:

1. **TUNE knob**: global pitch shift of the entire voice. Affects all steps equally. Cheap and dirty.
2. **Step-by-step**: select each step, press `↑` or `↓` to shift individually. Tedious but precise.
3. **Save + reload**: if you've saved a pattern to a slot, load it, adjust TUNE, resave.

Transposition is underused in acid. Most tracks stay in one key. But a breakdown section transposed up a minor third (3 semitones) and then back down for the drop can add a lot of energy.

## Key changes

A full *key change* — changing scale, not just root — is rare in acid but possible. E.g., a track might start in C minor and shift to F minor for the second half. Since acidflow stays in the same semitone grid, this just means transposing every step up 5 semitones (C → F).

Key changes break the hypnotic unity of acid tracks; use them only for deliberate dramatic effect.

## Try this

1. Write a pattern using only C root notes. Play. Drone.
2. Change step 9 to G2. Play. A single melodic event in the middle of the pattern.
3. Slide step 9 into G2 (`s` on step 9). Play. Now it's a "slide up to the fifth" gesture.
4. Add rests on steps 4 and 12. Play. Rhythmic structure.
5. Change step 13 to C3 (octave up). Play. Question-and-answer form.
6. Change step 15 to A♯2 (minor 7th). Play. Bluesy touch.

This small pattern has six distinct pitches, three rests, one slide, one octave jump, and plenty of repetition. It's miles away from an acid record in production quality — but melodically, it's doing the same compositional work a real acid line does. Melody in acid is small gestures in a restricted space.

Next: the art of silence.
