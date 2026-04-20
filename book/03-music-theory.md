# 3 · Music theory for acid producers

Acid gets by on less theory than almost any other genre of Western music. You can build a career on one scale, three intervals, and a couple of rhythmic ideas. *But* — and this is the whole point of this chapter — knowing which scale, which intervals, and which rhythms is what separates a repeating 303 line that sounds like acid from one that sounds like someone fiddling with a synth.

This chapter is dense. If you already know theory, skim. If you don't, read slowly. Nothing here is optional; every other chapter assumes you can follow the terminology.

## Notes, pitch, and the equal-tempered scale

A musical pitch is a frequency. When you hit the `c` key in acidflow's sequencer to set a step's root pitch, you're telling the oscillator to run at 261.63 Hz. The next key up — C♯ — runs at 277.18 Hz. The ratio between those two frequencies is the twelfth root of two, about 1.0595. That's the defining equation of the **equal-tempered chromatic scale**: twelve equal ratios per octave.

The twelve notes, starting from C:

```
C  C♯  D  D♯  E  F  F♯  G  G♯  A  A♯  B  C
1  2   3  4   5  6  7   8  9   10 11  12 (octave)
```

Each step up is called a **semitone**. Two semitones make a **whole tone**. Twelve semitones make an **octave**, which doubles the frequency.

In acidflow, pressing `↑`/`↓` in the sequencer section nudges a step's pitch by one semitone. `<`/`>` nudges by twelve (one octave). The letter keys `c d e f g a b` jump to that pitch class at the currently-displayed octave. You don't need to remember frequencies — you need to remember the *intervals* between notes, because those intervals are what the listener actually hears.

## Intervals

An interval is the distance between two notes. It's measured in semitones. The commonly-named ones:

| Semitones | Name            | Sound character                                   |
|-----------|-----------------|---------------------------------------------------|
| 0         | Unison          | The same note                                     |
| 1         | Minor 2nd       | Dissonant, leaning, pulling                       |
| 2         | Major 2nd       | Step-wise, stable-ish                             |
| 3         | Minor 3rd       | Dark, bluesy, pensive                             |
| 4         | Major 3rd       | Bright, open, consonant                           |
| 5         | Perfect 4th     | Solid, open, suspension-y                         |
| 6         | Tritone         | Maximum dissonance — the "devil's interval"       |
| 7         | Perfect 5th     | Strong, stable, the skeleton of rock and metal    |
| 8         | Minor 6th       | Somber, pre-resolve                               |
| 9         | Major 6th       | Sweet, open, country                              |
| 10        | Minor 7th       | Bluesy, jazzy, unresolved                         |
| 11        | Major 7th       | Sophisticated, floating                           |
| 12        | Octave          | The same note at double/half frequency            |

Acid uses a *very* narrow subset of these. Essentially: unison, octave, minor 3rd, perfect 5th, minor 7th, and the occasional tritone for menace. Major-mode intervals (major 3rd, major 6th, major 7th) show up rarely and usually signal a deliberate departure from genre norms.

## Scales and modes

A **scale** is a subset of the twelve chromatic pitches, repeating every octave. Acid lives in about four of them, which we'll cover. Theory textbooks will give you fifty. You don't need fifty.

### The minor pentatonic

The single most important scale in acid, dance music, and blues-descended pop generally. In C:

```
C  D♯  F  G  A♯  C
1  4   6  8  11   12
```

Five notes per octave (pent-atonic). No minor 2nd (which would be dissonant), no major 3rd (which would be bright), no 6th, no major 7th. Dark, open, and impossible to sound bad in.

In acidflow, set all your pattern steps to C, then walk them up through D♯, F, G, A♯, C. You've written a scale. Now scramble the order, add some rests, add some octave jumps, and you're writing an acid bassline. Truly — that's the method.

### The natural minor (Aeolian mode)

Adds two more notes to the pentatonic:

```
C  D  D♯  F  G  G♯  A♯  C
1  3  4   6  8  9    11  12
```

The added notes are the major 2nd (D, 3rd position) and minor 6th (G♯, 9th position). Natural minor is the go-to scale for "dark but not aggressive" — think *Voodoo Ray*, think a lot of Detroit techno leads, think Hardfloor at their most melodic.

### The Phrygian mode

Same notes as natural minor, but shifted so the lowest note is the 5th of the parent. In C:

```
C  C♯  D♯  F  G  G♯  A♯  C
1  2   4   6  8  9    11  12
```

The distinguishing feature is the **minor 2nd** between C and C♯. That's the flamenco-metal-Middle-Eastern sound. It's menacing and exotic. Phrygian shows up in faster acid techno, especially the Stay Up Forever end of the scene. It's the reason a lot of Hardfloor-era acid sounds threatening rather than groovy.

### The minor blues scale

Pentatonic plus the flat-5 (tritone):

```
C  D♯  F  F♯  G  A♯  C
1  4   6  7   8  11   12
```

The added F♯ (tritone) is a passing note — you land on it briefly on the way from F to G, and it provides the characteristic "blue note" grind. Useful for acid lines that want a slight bluesy or funky edge.

## The root, and why you should stay on it

In acid, the **root note** — the pitch the track is "in" — is usually the most-played note in the pattern. On *Acid Tracks* it's A. On *Voodoo Ray* it's C. On *Energy Flash* it's C.

Staying on the root, with occasional departures, does three things:

1. **Grounds the listener.** The ear treats returns to the root as resolutions. Every deviation becomes tension, every return becomes release.
2. **Makes the filter do the work.** If the melody moves constantly, the filter's effect is spread thin. If the melody is mostly one note, the filter sweep becomes *the* melodic event.
3. **Amplifies accents.** An accented note on the root feels like a punctuation mark. An accented note on a weird scale tone feels like a question.

A 303 pattern that's 60% root, 20% octave, 15% 5th, and 5% "anything else" is already most of a classic acid line.

## Chord theory, briefly, and why you mostly don't need it

Since the 303 is monophonic, chords are not directly usable. But understanding chord structure helps you write bass lines that sound *implied-harmonic* — they outline a chord progression even though they're monophonic.

A **triad** is a chord of three notes: root + 3rd + 5th. A **minor triad** is root + minor 3rd + perfect 5th. When you play C–D♯–G (C minor triad notes in sequence) as a bassline, the listener's ear assembles a minor chord retrospectively. This is called **arpeggiation**.

In acid, arpeggiation shows up most often as the pattern that walks **root → 5th → octave → 5th → root** (in steps like C–G–C'–G–C). That's the classic *Acid Tracks*-style line. You can substitute a minor 7th for the octave (C–G–A♯–G) for a bluesier feel. You can add the minor 3rd (C–D♯–G–C') for a stronger minor-chord implication.

Chord progressions — moving between chords — are rare in acid. Most tracks stay on one chord ("one-chord vamps") for their entire length, with the variation coming from filter, accent, and pattern mutation instead of harmonic motion. This is a genuine constraint of the genre, not a failure. Dance music as a whole is built around long static harmonic sections because long static harmonic sections are what let the groove work without distraction.

## Rhythm vs. pitch: which matters more in acid?

Rhythm. By a factor of maybe 5:1.

You can play the worst possible pitch choices — random notes, atonal clusters — and if the rhythm is right, the line will still feel like acid. You can play perfect modal pitch choices and if the rhythm is wrong, the line will feel like a synth demo. The pitch theory above is *supporting structure*; the rhythm (next chapter) is the skeleton.

This is why the randomizer on acidflow (Chapter 33) can produce usable patterns without any musical intelligence — the rhythmic palette (accent/slide/rest/ratchet) is well-constrained, and within that palette, almost any pitch choice sounds acceptable if the rhythm holds up. Whereas randomizing a piano line tends to produce gibberish.

## Quick reference: the most-used acid vocabulary

For ear training:

- **Root → octave → root**: the foundational dance-bass gesture.
- **Root → 5th → root**: slightly more melodic, still skeletal.
- **Root → minor 7th → root**: bluesy, classic.
- **Root → minor 3rd → 5th → octave**: arpeggiated minor chord, suggests harmony.
- **Root → minor 2nd → root**: menace, tension, Phrygian color.
- **Octave jump + accent**: punctuation. The 303 loves this.
- **Root → slide up to 5th → release → root**: the single most common acid gesture. The slide is the point.

Try writing a 16-step pattern using only these gestures, in a few different orderings. That's 70% of acid.

## Key and key changes

A **key** is a scale plus a root. "C minor" means "the natural minor scale starting on C." Most acid tracks are in a single minor key for their entire duration. When they do change keys, it's usually a simple **transposition** — pressing `Shift`-something to shift the whole pattern up or down a few semitones.

acidflow doesn't have a global transpose key (it would conflict with the letter-key root shortcuts). But moving the entire pattern by a fixed interval is easy: select each step and press `↑` five times to transpose up a fourth. The psychological effect is enormous — a track can gain a second wind by jumping up a minor third (three semitones) for the breakdown section.

## What about scales in acidflow's sequencer specifically?

acidflow does not force you to stay in key. Every step can be any of the 128 MIDI pitches. This is deliberate — the original 303 didn't force a key either, and some of acid's most interesting melodic behavior comes from the moments a pattern *leaves* the scale and comes back.

But you should start, every time you're writing, with a scale in mind. Pick one of the four above. Know the root. When you reach for an octave-jump or a slide target, pick something *in the scale* unless you have a specific reason not to. The randomizer does this implicitly (its note set is biased toward minor-pentatonic / minor-blues tones). You should too.

## Try this

1. Set the pattern to all the same note (press `c` at every step). Play it. That's the zero point.
2. Go step by step and set each one to a random pitch in the minor pentatonic on C: `c`, `[Shift-]arrow to D♯`, `f`, `g`, `[Shift-]arrow to A♯`. Play. Hear how even totally random orderings inside a scale sound cohesive.
3. Now swap a couple of notes out of the scale — change one to `D` (major 2nd) or `E` (major 3rd). Play. Hear how foreign those notes feel.
4. Put the out-of-scale note on an **accented** step. Hear how the accent makes it feel intentional rather than wrong.

That last step is the deep lesson of acid melodic theory: **accent legitimizes note choice**. A weird note on a ghost step sounds like a mistake. The same note on an accent sounds like a decision.

Next chapter: rhythm, which is 5x as important, and therefore 5x as long.
