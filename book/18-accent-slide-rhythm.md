# 18 · Accents and slides as rhythmic DNA

We've covered accent (Chapter 12) and slide (Chapter 13) as individual controls. This chapter is about them as a *system*. Together, accent and slide form the rhythmic DNA of an acid pattern — they define its character more than the pitches do. Two patterns with identical pitches but different accent/slide placements will feel like completely different pieces of music.

## The four-state per-step grammar

Every step in your pattern is in one of four states (ignoring rest and probability for now):

| State          | Sound                                                   |
|----------------|---------------------------------------------------------|
| Plain          | Normal attack, normal filter sweep.                     |
| Accent         | Louder, brighter attack. Stacks with accent capacitor.  |
| Slide          | Smooth pitch glide from previous note. No retrigger.    |
| Accent + Slide | Smooth glide, but with accent added to envelope/amp.    |

These four states form the rhythmic alphabet. You write patterns by choosing which state each step is in.

## The four-state patterns, visually

Using `N` for plain note, `A` for accent, `S` for slide, `*` for both:

```
Pattern A:   N  N  N  N  N  N  N  N  N  N  N  N  N  N  N  N
(no dynamics, no connection, a drone)

Pattern B:   N  N  A  N  N  N  A  N  N  N  A  N  N  N  A  N
(accents on the offbeats, standard acid feel)

Pattern C:   N  S  N  S  N  S  N  S  N  S  N  S  N  S  N  S
(every other step is slid, making the pattern half-glued)

Pattern D:   N  N  *  N  N  N  *  N  N  N  *  N  N  N  *  N
(accents+slides on offbeats — dramatic, every accent is slid into)
```

Each pattern has identical pitches. Only the modifiers differ. Play these through acidflow and you'll hear four genuinely different pieces.

## The common placements

Over forty years, acid producers have evolved preferences for where accents and slides go. These are not laws, but they are conventions that work.

### Accents on off-beats

The most common accent placement: steps 3, 7, 11, 15 (the "and" of each beat). This syncopates against the kick (which lands on 1, 5, 9, 13) and provides rhythmic lift.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Accent:  .  .  A  .  .  .  A  .  .  .  A  .  .  .  A  .
```

### Slides on beats 2 and 4

Slides on the backbeat — steps 5 and 13. These align with the clap, creating a "pulling" feel into the backbeat.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Slide:   .  .  .  .  S  .  .  .  .  .  .  .  S  .  .  .
```

### Slide-to-accent

The canonical acid gesture: slide into a step that's also accented. The slide delivers the pitch, the accent emphasizes the arrival.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Accent:  .  .  .  .  A  .  .  .  .  .  .  .  A  .  .  .
Slide:   .  .  .  .  S  .  .  .  .  .  .  .  S  .  .  .
```

Steps 5 and 13 are both accented and slid. Each one is a dramatic melodic arrival.

### Accent cluster + release

Three or more accents in a row, then nothing. The accent capacitor builds, then drains.

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Accent:  A  A  A  .  .  .  .  .  .  A  A  A  .  .  .  .
```

Creates crescendos at steps 1-3 and 10-12.

### Every-other-step slide

A pattern where every other step is slid. The pattern alternates "hit, glide, hit, glide..."

```
Step:    1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Slide:   .  S  .  S  .  S  .  S  .  S  .  S  .  S  .  S
```

This is unusual but effective — feels like a snake pattern, almost a trill.

## The density spectrum

How *many* accents and slides you use defines the pattern's density character:

- **Zero accents, zero slides**: pure drone. No dynamics. Unacceptable as standalone music, but sometimes useful as a layered element.
- **2-3 accents, 1-2 slides**: minimal acid. Space, restraint.
- **4-6 accents, 2-4 slides**: standard acid. Expected density.
- **7-10 accents, 5-8 slides**: dense, active, energetic.
- **11+ accents, 9+ slides**: saturation. Everything is emphasized, so nothing is.

The sweet spot for most tracks is 4-7 accents and 2-5 slides per 16-step pattern.

## The accent-slide-rest triangle

These three modifiers work against each other. More of one means less of the others:

```
                  ACCENT
                  (emphasis)
                    / \
                   /   \
                  /     \
                 /       \
                /         \
          SLIDE───────────REST
         (connection)  (absence)
```

A pattern heavy on accents (emphasis-heavy) will feel percussive and rhythmic.
A pattern heavy on slides (connection-heavy) will feel smooth and melodic.
A pattern heavy on rests (absence-heavy) will feel sparse and spacious.

Most good patterns sit somewhere in the middle of the triangle, using all three in moderation.

## Accent + slide combinations and their feel

Let me walk through specific pairings with their musical character:

### Accent only (no slide)

Feel: punchy, emphatic, rhythmic. The pattern has dynamic events but no connective tissue.

Good for: classic acid house, Chicago-style tracks, percussive-feeling bass.

### Slide only (no accent)

Feel: smooth, legato, continuous. The pattern is all connection, no emphasis.

Good for: ambient acid, drone-like passages, moments where the filter/resonance is doing all the work.

### Accent + slide on same step

Feel: dramatic emphasized arrival. The pattern glides up to a loud point.

Good for: melodic acid where key pitches are both smooth-approached and emphasized.

### Accent on one step, slide on next

Feel: the emphasized note is followed by a "relaxing" slide. Breath in, breath out.

```
Step:    1  2  3  4  5  6  7  8
Accent:  .  A  .  .  .  A  .  .
Slide:   .  .  S  .  .  .  S  .
```

The accent on step 2, then slide into step 3. The accent is punctuation; the slide is the drift back to pattern.

### Slide leading to accent

Feel: build-up. The slide rises or bends, and the accent lands with impact.

```
Step:    1  2  3  4  5  6  7  8
Slide:   .  S  .  .  .  S  .  .
Accent:  .  .  A  .  .  .  A  .
```

This is rarer but very expressive. The slide (step 2) brings us partway somewhere, and the accent (step 3) fully arrives.

## The two-bar relief effect

If your pattern has heavy accents and slides in bar 1, bar 2 can be a *relief* — fewer accents, fewer slides — by either using song mode (Chapter 30) to alternate between two versions, or by using probability (Chapter 19) to make some accents/slides probabilistic (e.g., 50% chance they fire).

This is how you make a one-bar loop feel like a two-bar phrase.

## Accents and the drum pattern

Your 303 accent placements interact with the drum pattern. A few principles:

1. **Accent where the drums are also strong**: lock-step with kick/clap for maximum punch. The 303 and the drums emphasize the same moments, making the groove more solid.

2. **Accent against the drums**: place accents between kick hits. The 303 fills the gaps. More complex groove.

3. **Mirror the drum pattern**: if the clap is on 5 and 13, put 303 accents on 5 and 13. Very solid, slightly predictable.

4. **Complement the drum pattern**: if the clap is on 5 and 13, put 303 accents on 3, 7, 11, 15 (the alternate offbeats). The 303 and clap cover different moments, giving the whole pattern a busier feel without being cluttered.

Experiment with all four. Each produces a different track "feel."

## Slides and the drum pattern

Slides don't have the same direct interaction with drums (since slides are about pitch, not rhythm). But the *timing* of slides still matters:

- A slide that crosses a strong beat (e.g., slide from step 4 into step 5) has the pitch motion crossing the backbeat. Expressive.
- A slide that's internal to a beat (e.g., slide from step 6 into step 7) is less noticeable — the motion happens in a "weak" area.

## Accent and slide as melodic pointers

A more advanced idea: use accent and slide to *point* at specific pitches.

If a particular pitch is unusual (say, you use a minor 2nd in an otherwise pentatonic pattern), accenting or sliding into that pitch makes its unusualness feel like a deliberate choice. Without the modifier, the note sounds like a mistake.

The combination of "unusual pitch + accent + slide into it" turns a weird note into a signature. This is how acid patterns achieve dissonance without sounding broken.

## The hidden variable: WHEN you add modifiers

A tip from experienced producers: write your pattern's pitches first, play it, listen. Then add modifiers. Listen to what it does.

Do NOT write pitches and modifiers simultaneously. The modifiers change what each pitch *means*. If you're choosing them together, you're making twice as many decisions per step, and they'll be worse decisions.

Write pitches. Play. Then modifier-up one control at a time: first add accents, listen. Then add slides, listen. Then rests, listen. This sequential process produces much better patterns than trying to do everything at once.

## Accents as focus dials

Think of accents as the audio equivalent of typographic bolding. A paragraph where every word is bold is illegible. A paragraph with strategic bolding guides the reader's eye to what matters.

Your accent placements are telling the listener "these are the notes that matter." If you accent everything, nothing matters. If you accent strategically, the listener's ear follows your guidance.

## Try this

1. Load preset 01. Count the accents. (Should be around 6 per bar.) Count the slides. (Around 2.)
2. Remove all accents. Play. Notice how lifeless it feels.
3. Restore the accents. Play. You recognize the pattern again.
4. Move all accents to the off-beats (3, 7, 11, 15). Play. Still acid, but different feel.
5. Move all accents to the backbeats (5, 13). Play. Very structural.
6. Add a slide on every accented step. Play. Every emphasis is also a pitch event.
7. Remove all slides, keep accents. Play. Purely percussive feel.

The pattern's character is very responsive to accent/slide changes. Small edits produce large shifts. This sensitivity is what makes acid programming a fine art — it rewards careful attention and punishes carelessness.

Next: probability and ratchet, the features that modernize step sequencing.
