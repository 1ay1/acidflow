# 25 · Programming acid drums

You have five voices and 16 steps. That's 80 possible hit-positions per pattern. Most will be empty. The art of drum programming is in which ones aren't — and why.

This chapter is about compositional principles for acid drum patterns specifically. Not generic "dance music drums" — acid drums, with their particular conventions.

## Start simple

Every beginner's instinct is to add more drums. More hats. More claps. More kicks. Resist.

A great acid drum pattern is *transparent* — the listener can hear every hit as a distinct event. A busy pattern is *muddy* — the hits blur into a wall.

Start with the minimum:

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
CL     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
```

Kick on every beat. Clap on backbeat. That's it. You can make a full track with just this. Many acid records are functionally just kick + clap drums.

Add hats only if the pattern feels missing something. Add open hats only if you want a "tail" on certain beats. Add snare only for techno-leaning tracks.

## The less-is-more principle

A drum pattern with fewer total hits often feels more energetic than one with more hits. Counterintuitive but true.

Why? Because each hit in a sparse pattern has room to *be heard*. The ear tracks each hit as a distinct rhythmic event. In a busy pattern, the hits merge into a texture and lose individual impact.

Count the hits in *Acid Tracks*'s drum pattern. You'll find maybe 10-12 per bar out of 80 possible positions. 12-15% density. That sparseness is why each hit feels like a punch.

Compare to a typical techno pattern: 20-30 hits per bar, 25-40% density. Still sparse compared to "every position," but twice as dense as acid house.

Modern acid techno: 25-35 hits per bar. Yet more dense.

When you're starting a track, aim for the low end (10-15 hits). You can always add more later if it feels thin.

## The kick is the skeleton, everything else is ornament

Program the kick first, alone. Make sure it grooves (usually 4-on-the-floor). Then add voices one at a time, listening after each.

If you add a voice and the pattern loses groove, that voice is wrong. Remove it and try somewhere else.

Common mistake: adding all five voices at once and then trying to "fix" them. You end up with a pattern that's kind of OK but not great. Starting from the kick and building out produces stronger patterns.

## Filling the subdivisions

A standard principle: the kick covers beats (quarter notes). The hats cover subdivisions (eighths and sixteenths). The snare/clap cover the backbeat.

This hierarchy gives each voice a clear rhythmic role:

- Kick: rare events (4 per bar).
- Snare/clap: less-rare events (2 per bar).
- Closed hat: frequent events (4-8 per bar).
- Open hat: occasional accents (0-2 per bar).

When each voice plays its characteristic rate, the pattern has hierarchical structure — you can hear the different time-scales.

When all voices play at the same rate (e.g., everything on 16th notes), the pattern loses hierarchy and feels like a wall.

## The offbeat closed-hat convention

Classic acid house puts closed hats on the *and* of each beat (steps 3, 7, 11, 15). This is the "unz-tik-unz-tik" convention.

Why this position? Because it's exactly between the kicks. The kick hits on beat 1, the closed hat fills the space before beat 2, kick hits on beat 2, closed hat fills the space before beat 3, etc.

This creates a steady *alternation* — kick, hat, kick, hat — that feels like walking.

Deviation: some producers put closed hats on every 8th (both on-beat and off-beat). This makes the pattern feel more driving, less laid-back.

Deviation: some producers put closed hats on every 16th. Too busy; pattern feels frantic.

Classic acid: offbeat closed hats only.

## The open hat convention

Open hats usually land on step 8 or step 16 — the last 16th-note of beats 2 or 4.

Why? Because the open hat's long tail bleeds across the bar line (or the beat boundary), creating a "chick" that announces the next beat.

Deviation: open hat on step 4 (last 16th of beat 1) — rare, feels like the open hat is *leading* to beat 2 rather than *following* beat 2.

Deviation: open hat only on step 16 (not step 8) — creates a once-per-bar "chick" that's more dramatic.

Classic acid: open hat on steps 8 AND 16 for standard feel, only on 16 for more subtle.

## The backbeat

Beats 2 and 4 — steps 5 and 13 — are the backbeat. Something distinctive should happen there.

Options:

- Snare only.
- Clap only.
- Both layered.
- Neither (pattern without backbeat — rare, usually avoid).

A pattern without a backbeat loses the characteristic "dance music" feel. Dancers anchor to the backbeat.

## Accent with density

A trick for making a beat feel stronger: stack multiple drums on the same step. E.g., step 5 already has snare and clap both — that backbeat feels huge compared to a single-hit backbeat.

For the downbeat (beat 1): kick alone is usually enough. Layering too many drums on beat 1 makes it overwhelming.

For the backbeat (beats 2 and 4): double or triple layers can work. Snare + clap + sometimes a closed-hat layer gives a thick, dance-floor backbeat.

## The ghost-note trick

You can add "ghost notes" — extra drum hits at low volumes that fill in subdivisions subtly. In acidflow, you don't have per-hit velocity, so you can't do this directly. But you can approximate by:

- Using a voice other than the expected one. E.g., put a snare hit where a closed hat would go. Creates a syncopation.
- Adding a clap on a non-backbeat position for syncopated accents.

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
CL     .  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
                                ^
                                syncopated clap on step 9 (beat 3)
```

Now the pattern has a clap on beats 2, 3, and 4 — not just 2 and 4. Creates rhythmic tension.

## Variations for different subgenres

### Chicago acid house drums

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
CL     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
CH     .  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
```

Minimal. No snare. No open hat. Just kick, clap, offbeat closed hat. This is Phuture territory.

### UK acid house drums

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
SD     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
CH     .  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
OH     .  .  .  .  .  .  .  X  .  .  .  .  .  .  .  X
CL     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
```

Snare AND clap on backbeat. Open hats for bounce. This is *Voodoo Ray* territory.

### Detroit acid techno drums

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
CH     X  .  X  .  X  .  X  .  X  .  X  .  X  .  X  .
CL     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
```

Kick on every beat. Closed hat on every 8th (busier). Clap on backbeat. No snare. This is relentless.

### Hardfloor / acid techno drums

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  X  X  .  .  .  X  .  .  .
SD     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
CH     .  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
OH     .  .  .  X  .  .  .  X  .  .  .  X  .  .  .  X
CL     .  .  .  .  X  .  .  .  .  .  .  .  X  .  X  X
```

Syncopated kick on step 8. Snare + clap on backbeat. Extra open hats. Clap rolls at bar end. Aggressive.

### Plastikman minimal acid drums

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  .  .  .  .  X  .  .  .  .  .  .  .
CH     .  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
CL     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
```

Half-time kick. Closed hats only. Clap on backbeat. Extremely sparse.

### Modern acid techno drums

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
SD     .  .  .  .  X  .  .  .  .  .  .  .  X  .  X  X
CH     X  X  X  X  X  X  X  X  X  X  X  X  X  X  X  X
OH     .  .  .  .  .  .  .  X  .  .  .  .  .  .  .  X
CL     .  .  .  .  X  X  .  .  .  .  .  .  X  X  .  .
```

Sixteenth-note hats. Snare/clap combinations on backbeat. Snare roll at end of bar. Dense.

## The bar-end fill

Across all subgenres, the last 1-2 steps of a bar often have a "fill" — extra drum hits to add interest to the bar end.

Common fills:

- **Snare roll** (3-4 snares on steps 13-16).
- **Clap roll** (similar, with clap).
- **Double kick** (extra kicks on 15, 16).
- **Open hat on 14 AND 16** (two open hats).

Fills happen in every other bar, or every 4 bars, or on specific "phrase endings" (beats 15, 16 of the 4th bar of a phrase).

## The variation principle

Listeners get bored with identical bars. Even when you program a pattern, consider how to vary it across time:

1. **Chain two saved slots** in song mode — one with a slightly denser pattern, one less dense. Alternate every 4 bars.
2. **Use probability** on some drum voices... wait, acidflow doesn't have drum-voice probability. This is a limitation — you'd simulate via song mode.
3. **Mute/unmute voices live**. E.g., remove open hats for 8 bars, bring them back for 8 bars.

Variation over time is covered more in the arrangement chapter (Chapter 34).

## When to break the rules

All the conventions in this chapter are just that — conventions. Acid has rules; the best records break them deliberately.

- *Phuture - We Are Phuture*: No clap, no snare. Just kick and hats with 303.
- *Hardfloor - Trancescript*: Kicks in unusual patterns to create a galloping feel.
- *Plastikman - Spastik*: Almost no drums at all; the 303 carries everything.

Break rules *after* you know them. Breaking them before is just ignorance.

## Try this

1. Program the Chicago acid house pattern above. Play. Memorize its feel.
2. Add a snare on backbeat to transform it into UK acid. Play.
3. Switch to 8th-note hats (CH on every 8th) for Detroit techno. Play.
4. Add a syncopated kick (step 8) for Hardfloor. Play.
5. Strip down to half-time + offbeat hats for Plastikman. Play.
6. Push to 16th-note hats + bar-end rolls for modern acid techno. Play.

You've cycled through the major subgenre drum conventions. Each one changes the track's identity. Pick the one that fits the acid line you're writing.

Next: the drum bus and FX sends.
