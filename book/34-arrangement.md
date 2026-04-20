# 34 · Arrangement structures in acid music

A pattern is not a track. You can loop the same 16 steps forever, but at some point the listener loses interest. A track has *structure* — sections, transitions, tension, release. The arrangement is the shape of the track over time.

This chapter is about how acid tracks are typically arranged. Not the universal "verse-chorus" of pop music, but the specific forms acid has evolved over forty years.

## Why arrangement matters

Electronic dance music is long-form: 5-10 minutes typical, 12+ minutes for extended mixes. Over that duration, attention must be managed. Listeners (and dancers) expect the track to develop, change, surprise — and then return to a familiar state.

Without arrangement, a 5-minute loop is hypnosis. With arrangement, it's a *journey* — a crafted experience.

Good acid arrangement is simple. The 303 is a repetitive instrument; its power is cumulative. Arrangement shouldn't fight this — it should *guide* the accumulation.

## The canonical form

Most acid tracks from 1987 to today follow some variant of this arrangement:

```
Intro → Build → Drop → Main → Breakdown → Drop 2 → Outro
 8 bars  16 bars   4 bars 32 bars  16 bars   4 bars  8 bars
```

That's ~88 bars, or 2.5-3 minutes at 125 BPM. Extended mixes expand the Main and Breakdown sections; shorter radio edits compress everything.

Individual sections can have more or fewer bars. The sequence is what matters.

## The sections explained

### Intro (8-32 bars)

Purpose: establish tempo and feel without fully committing.

Typical content:
- Kick drum or partial drums only.
- No 303, or very sparse 303.
- Maybe a filter sweep rising.

Goal: let the DJ mix this in over the previous track's outro. Give the dancer 8-16 bars to decide "is this my track?"

### Build (8-16 bars)

Purpose: increase tension.

Typical content:
- 303 line enters, maybe with filter partially closed.
- Drums fill out (adding hats, claps).
- Filter slowly opens.
- Reverb or delay increases.

Goal: tell the dancer "something's about to happen." The ear expects a release.

### Drop (2-4 bars)

Purpose: the energy peak.

Typical content:
- Full drum pattern.
- 303 with filter fully open.
- All elements present.
- Often accompanied by a cymbal crash or other accent in DAW productions — acidflow doesn't have crashes, but the sudden full-mix is the drop itself.

Goal: maximum impact.

### Main (16-64 bars)

Purpose: the "song" — the part the listener will remember.

Typical content:
- Main pattern running at full strength.
- Occasional variations (drum fills, extra claps).
- Filter motion: maybe cycles between open and partially-closed.
- Knob work (live tweaking) during this section is common.

Goal: develop the track. Not static; not chaotic. Just enough variation to keep interest.

### Breakdown (8-32 bars)

Purpose: create contrast.

Typical content:
- Drums mute or reduce dramatically (often just kick, or NO drums).
- 303 may continue, in a reduced form.
- Reverb/delay up heavily.
- Filter may close.

Goal: remove the primary energy (drums), create a "space" that the listener feels. Prepare for the return.

### Second Drop (2-4 bars)

Purpose: second energy peak, often stronger than the first.

Typical content:
- Same as first drop, but maybe with more elements.
- Snare roll or drum fill leading in.

### Outro (8-16 bars)

Purpose: let the DJ mix out, give the listener resolution.

Typical content:
- Drums reduce gradually (or end abruptly for "wall" outro).
- 303 continues to the end or stops.
- Reverb tail trails off.

## Acid-specific arrangement patterns

### The "filter journey" pattern

Some acid tracks (especially Richie Hawtin, Green Velvet) are arranged entirely around filter motion. The drums and the 303 pattern are IDENTICAL throughout the track. The only thing that changes is the filter cutoff, slowly rising over 5 minutes.

```
Minute 1: CUTOFF at 20%. Track is muffled, sub-focused.
Minute 2: CUTOFF at 35%. Track is clearer.
Minute 3: CUTOFF at 55%. Track is at full strength.
Minute 4: CUTOFF at 75%. Track is aggressive.
Minute 5: CUTOFF at 45%. Pulled back. Starting to fade.
```

This is the *Acid Tracks* arrangement. Minimal compositional effort, maximum hypnotic effect.

To do this in acidflow: save one pattern slot. Play it for 5 minutes. Slowly tweak CUTOFF over time. Export.

### The "pattern variation" pattern

Classic acid-house arrangement (Phuture, Adonis): the 303 pattern itself changes across the track. Maybe 3-5 different variations, chained together.

```
Bars 1-16: Pattern A (sparse, establishing).
Bars 17-32: Pattern B (denser, driving).
Bars 33-48: Pattern A (back to establish).
Bars 49-64: Pattern C (climactic, with slides).
Bars 65-80: Pattern A.
Bars 81-96: Pattern B.
```

Each pattern plays for 16 bars. The arrangement is the sequence of patterns.

To do this in acidflow: save patterns A, B, C to slots 1, 2, 3. Don't use song mode; manually load slots as you go.

### The "less is more" pattern

Plastikman / dub techno arrangement: tiny changes over long durations. The track evolves glacially.

```
Bars 1-32: Pattern at full strength.
Bars 33-34: Add one closed hat.
Bars 35-64: Pattern with the new hat.
Bars 65-66: Remove a different element.
Bars 67-96: Slowly evolving pattern.
...
```

Each small change is an event. The listener attends to the micro-changes because there's nothing else. 10-minute tracks built on 3-4 total changes.

### The "energy ramp" pattern

Hardfloor / acid techno: progressive intensity.

```
Bars 1-16: One 303 line, no accents.
Bars 17-32: Same line, accents on every beat.
Bars 33-48: Accent on every step.
Bars 49-64: Add another 303 layer (in a DAW; acidflow is monosynth only).
```

Every 16 bars, intensity ramps up. By the end, it's maxed out. Then breakdown and restart.

## The role of knob motion

Across all these arrangement patterns, *knob motion* is primary. The knobs are the live performance.

Most producers record their arrangement by letting the pattern loop while they turn knobs in real time. Filter sweeps, drive pushes, delay sends — all happen by hand.

This means: acid arrangement is partly composed (pattern choices, section length) and partly performed (knob moves during playback).

To make good arrangements: plan your section structure, then perform the knobs over the section. Record the performance to WAV.

## The 32-bar phrase

Dance music is structured on multi-bar phrases. Specifically: 4-bar phrases, which group into 8-bar super-phrases, which group into 16-bar sections, which group into 32-bar half-sections.

Every 4 bars: a drum fill or minor variation.
Every 8 bars: a slight energy shift.
Every 16 bars: a section change.
Every 32 bars: a major structural change (build, drop, breakdown).

This hierarchy is extremely important. Dancers are trained to expect changes at these boundaries. Change at a 3-bar boundary feels wrong; change at a 4-bar boundary feels right.

acidflow's 16-step pattern = 1 bar. Your arrangement should respect 4-bar, 8-bar, 16-bar, 32-bar boundaries.

Tip: set a reference count in your head or on paper. "Okay, we're on bar 1. At bar 5, do a fill. At bar 9, add the hats. At bar 17, the drop."

## The "intro from silence" vs "intro mixed"

Two arrangement philosophies:

**Intro from silence**: the track starts quiet and builds up to the main. The listener hears this as "a track starting."

**Intro mixed**: the intro is engineered for a DJ to mix the PREVIOUS track's outro into this one. The intro is a reduced version of the main, matching the previous track's energy. The listener hears this as "two tracks blending."

Most club-oriented tracks are mixed intros. Most listening-oriented tracks (album tracks, not singles) are intros from silence.

## Extended vs radio edits

The same track can be arranged at different lengths:

- **Extended mix** (7-12 minutes): long intro, long main, long breakdown, long outro. For DJ use.
- **Radio edit** (3-4 minutes): short everything. Gets to the main quickly. For radio or non-DJ listening.
- **Original mix** (somewhere between): usually the producer's "definitive" version.

When you make a track, think about which version you're making. acidflow's WAV export captures whatever you play, so the duration is up to you.

## The build-up techniques

Specific techniques to build tension:

1. **Filter opening**: slowly raise CUTOFF. The track "opens up."
2. **Drum layering**: start with kick, add hats, add claps, add snare — each layer raises intensity.
3. **Accent density**: start with no accents, add one per bar, add more, peaking at all accents.
4. **Resonance rising**: RES at 30% at the start, raising to 80% at the build peak. Adds screaming.
5. **Pitch rising**: shift 303 line up an octave or a few semitones at the build end.
6. **Delay/reverb increase**: more wash as the build progresses.

Combine multiple for maximum effect. A great build uses 3-4 of these simultaneously.

## The drop techniques

The drop is the moment of release. To make it feel impactful:

1. **Everything at once**: all drums, full 303, full FX. In a DAW, also add the bassline or extra layers at this moment.
2. **Beat silence just before**: cut the kick for the last 1-2 beats before the drop. The drop beat feels more impactful after the silence.
3. **Snare roll**: 4-8 rapid snare hits leading into the drop.
4. **Filter-closed-then-full-open**: close the filter for the last bar before the drop, then open it fully AT the drop.
5. **Bass drop**: pitch the 303 down an octave at the drop. Feels like the mix "drops" in frequency.

Most acid tracks use 1-3 of these for the drop.

## The breakdown techniques

Breakdowns are negative space. Techniques:

1. **Drum removal**: mute the drum bus. Only 303 plays.
2. **303 sustain**: play a single sustained 303 note during the breakdown.
3. **Reverb wash**: crank reverb mix to 60%+.
4. **Filter close**: close CUTOFF to 10-20%. Everything gets muffled.
5. **Delay feedback up**: DLY FB to 0.85 during breakdown. Echoes pile up.
6. **Silence**: literal silence. 2 bars of nothing. Dramatic but risky — dancers may stop dancing.

## Transitions between sections

Transitions are the 2-4 bars that connect one section to the next. They're critical — a weak transition makes the whole arrangement feel disjointed.

### Transition techniques

- **Drum fill**: kick/snare/hat rolls in the last bar of the outgoing section.
- **Filter sweep down then up**: close the filter in the last bar, open it sharply at the new section start.
- **Reverse cymbal**: a whooshing sound leading into the new section. acidflow doesn't have reverse cymbals — use a snare roll or filter sweep instead.
- **Brief silence**: a 1-beat gap before the new section. Creates anticipation.

## Arrangement and song mode

Chapter 30's song mode automates part of this. The chain moves from slot to slot automatically. But:

- Song mode's one-cycle-per-slot rule means each slot is 1 bar. Arrangement needs 16+ bar sections, requiring duplicates.
- Knob moves AREN'T automated by song mode. Filter sweeps must be performed manually.

So song mode is useful for pattern changes in an arrangement but not for knob motion. The knob motion must be performed live.

## Writing down an arrangement

Some producers diagram their arrangements before performing. A template:

```
Section   | Bars | Pattern | Drums    | CUTOFF | RES  | FX
----------|------|---------|----------|--------|------|------
Intro     | 1-16 | none    | Kick     | 40%    | 30%  | Dry
Build     | 17-32| Main    | + Hats   | 40→60% | 40%  | Dly 10%
Drop      | 33   | Main    | + Clap   | 80%    | 70%  | Dly 20%
Main A    | 34-64| Main    | Full     | 70%    | 70%  | Dly 20%
Breakdown | 65-80| Main    | Kick only| 30%    | 50%  | Dly 40% Rev 40%
Drop 2    | 81   | Main    | Full     | 80%    | 70%  | 
Main B    | 82-112| Main   | Full     | 75%    | 75%  | 
Outro     | 113-128| Main  | Reducing | 60→30% | 50%  | Rev 30%
```

With this plan, you know what knob to be on at what bar.

## Don't over-arrange

A common mistake: too many sections, too many changes. Every 8 bars something happens. The listener can't settle.

Good acid arrangements are *patient*. Let a section breathe for 16, 32 bars. The repetition itself is the hypnotic effect.

"Less is more" applies to arrangement too. 4-5 total sections over 8 minutes is plenty.

## Don't under-arrange either

The opposite: 8 minutes of the same loop with no variation. Boring.

Minimum: at least one clear breakdown. At least one filter sweep. At least one drum change.

The middle ground between over and under is judgment. You develop it by listening to finished acid records and noting what happens when.

## Try this

1. Pick an arrangement target: 4 minutes.
2. Program your main pattern.
3. Decide: intro (0:00-0:30), build (0:30-1:00), drop (1:00-1:02), main (1:02-2:30), breakdown (2:30-3:00), drop 2 (3:00-3:02), main (3:02-3:30), outro (3:30-4:00).
4. Save variations to slots: slot 1 = intro (drums reduced, 303 muted); slot 2 = main; slot 3 = breakdown (drums muted except kick, 303 sparse).
5. Press record on WAV export. Press play. Manually load slots at the right times. Turn knobs per the plan.
6. After 4 minutes, stop. You have an arranged track.
7. Listen back. Does it flow? Where are the rough spots?
8. Redo. Each pass gets better.

This is how professionals make records: plan, perform, refine, repeat.

Next: subgenres and their signatures.
