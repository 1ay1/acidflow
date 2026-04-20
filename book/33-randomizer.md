# 33 · The randomizer and evolving mutation

There's a persistent mythology around the early acid records: the producers just fiddled with the TB-303 randomly and stumbled into genius. Phuture's *Acid Tracks* was "discovered by accident." DJ Pierre didn't know what he was doing; the machine made music for him.

This is mostly false. Phuture worked with the 303 for weeks before *Acid Tracks*. They knew what they were doing. But there's a grain of truth — the 303's combination of accents, slides, filter motion, and fast step programming is so strange that nobody fully predicts what they'll get. Happy accidents happen often. The randomness *informs* the composition.

acidflow embraces this with a built-in randomizer. This chapter is about how to use it productively — not to replace composition, but to catalyze it.

## The two random keys and one mutation key

- **`r`**: randomize the *focused* section only.
- **`R`**: randomize *everything* (knobs + pattern + drums + FX).
- **`M`**: evolving mutation — one small change to the current pattern.

`r` and `R` *replace* what you have. `M` *evolves* it.

## Why biased, not uniform

Naive uniform randomization — pick each parameter uniformly across its range — produces musically awful results ~80% of the time. CUTOFF at 5%, RES at 95%, ENVMOD at 80% = screeching chaos. Random pattern with 100% probability of slides on rests = broken.

acidflow's randomizer is *biased*: it knows the parameter space has sweet regions and boring regions, and it aims for the sweet ones. The results are statistically more likely to be musical.

This is opinionated. A "real" random knob might be fun for exploration, but a biased random is more useful for actually making tracks.

## `r` — randomize the focused section

Focus determines what gets randomized:

- **Knobs focused**: randomize the 303 patch.
- **FX focused**: randomize the FX rack (drive, delay, reverb).
- **Sequencer focused**: randomize the 303 pattern.
- **Drums focused**: randomize the drum grid.
- **Transport focused**: randomize BPM, swing, and length.

So "what do I randomize" = "what section am I on." Use Tab to change focus.

## Knob archetypes

When you randomize knobs, acidflow picks one of four pre-baked archetypes, then jitters within that archetype's range:

### Classic
- Mid CUTOFF (around 50-60%).
- Strong RES (around 70%).
- Moderate ENVMOD (around 50-65%).
- Medium DECAY.
- This is the universal "acid starting point." Sounds like most acid records from any era.

### Squelch
- Very high RES (around 85-95%).
- Low CUTOFF (around 10-25%).
- Tight DECAY (fast).
- Heavy ACCENT pattern — or at least, the filter is tuned so accents stand out dramatically.
- This is self-oscillation territory. Sounds like Hardfloor at its peak, or any "screaming acid" record.

### Driving
- Open CUTOFF (around 60-75%).
- Less envelope motion (moderate ENVMOD).
- Medium RES.
- The *Energy Flash* sound — the 303 is present and harmonic-rich but not screaming.

### Dubby
- Low CUTOFF (around 20-35%).
- Long DECAY.
- Moderate RES.
- Plastikman / dub techno. Sustained filter motion over many bars.

Each archetype is a multidimensional region of the knob space. The randomizer picks a region, then picks a specific point in it.

Waveform: ~30% square, ~70% saw. Saw is canonical 303; square is a rare-but-used variant.

A toast (corner notification) tells you which archetype was rolled. "↻ knobs: Squelch" or similar.

## Pattern archetypes

When you randomize the pattern, similar archetype logic:

### Pedal
- Most steps play the root note (pedal point).
- Occasional jumps (octave, fifth, minor third).
- Sparse rests.
- Heavy on the root. Classic acid. Pattern example: *Acid Tracks*, *Energy Flash*.

### Driving
- Dense — every step plays.
- More accents.
- Mostly root + octave.
- Beltram-style. Pattern example: *Energy Flash*, *Mentasm*.

### Melodic
- Higher jump rate (notes other than root).
- More colour tones (fifth, minor third, minor seventh).
- More slides.
- FSOL / Stakker Humanoid territory. Pattern example: *Stakker Humanoid*.

### Dub
- Very sparse — most steps rest.
- Long decay does the work.
- Occasional accented notes.
- Plastikman. Pattern example: *Spastik*, *Helikopter*.

## Pitch draw

Notes aren't drawn uniformly from MIDI. They're drawn from a weighted **minor pentatonic + ♭7 + octave + octave-5** above C2 — the canonical 303 vocabulary.

Specifically: root, minor 3rd, perfect 4th, perfect 5th, minor 7th, octave root, octave fifth. Seven pitches over a ~1.5 octave range.

Some of these get more weight (root and octave root get the most). Others less (minor 7th gets little). The result is statistically acid-sounding.

## Pattern constraints

Three constraints are hard-coded:

1. **Downbeats favor the tonic**. Steps 1, 5, 9, 13 are weighted toward the root note. This keeps the groove anchored.
2. **At least one accent per bar**. Flat patterns without accents sound dead. Randomizer ensures one accented note per 16-step cycle.
3. **Slides only survive if the previous step plays**. You can't slide from a rest. Randomizer removes slide flags that violate this.

These constraints bake in hard-won acid wisdom. You CAN'T randomize a pattern that sounds "wrong" in these specific ways; the randomizer won't produce them.

## `R` — randomize everything

Capital R re-rolls knobs AND pattern AND drums AND FX in one shot. Single toast: "↻ all randomized."

Good for: "I want something entirely new to react to."

## `M` — evolving mutation

Lowercase `r`/`R` wipe and replace. Uppercase `M` *evolves* — one small change to what you have.

The mutation picks one of these, weighted:

- **28%**: toggle accent on a random non-rest step.
- **24%**: toggle slide on a random non-rest step (where the next step plays).
- **20%**: nudge one step's pitch by ±1 or ±2 semitones.
- **12%**: jump one step ±12 semitones (octave shift).
- **10%**: toggle rest on a random step (never step 1 — that's the anchor).
- **6%**: rotate the whole pattern left or right by one step.

Bias: toward *vibe* changes (accent + slide = 52% combined), less toward *pitch* changes (20% nudge + 12% octave = 32%), rare on structural changes (rest toggle + rotate = 16%).

This bias is deliberate. Repeated M presses produce variations that stay melodically related but add rhythmic interest. Accent and slide toggles reshape groove without changing notes. Pitch nudges add small melodic variance. Octave jumps add big variance occasionally. Rest toggles and rotations completely change the pattern rarely.

Press M 5-10 times in a row and you'll hear the pattern "walk." Each step is related to the previous; after 10 steps, the pattern is noticeably different but still feels like a descendant of the original.

## Workflow with randomization

### Discovery

Randomize to find something you wouldn't have programmed. Press `R` a dozen times, listening to each result. When something sparks interest, stop and work with it.

This is like flipping through radio stations — you're not composing, you're auditioning.

### Variation generation

You have a pattern. Press `M` to get a variation. If it's better, keep it. If not, undo (Ctrl+Z isn't a thing — just M again for a different direction).

This is the "I like this, but I want more" workflow. Ten mutations produce ten related patterns. Save the best ones to slots (Chapter 30) and chain them.

### Inspiration mining

Stuck on what to program? Randomize. The randomizer will propose structures you didn't think of. Even if you don't keep the exact output, the idea behind it (a specific slide placement, a specific accent pattern) might inspire.

### Reacting to the machine

Acid has always been a dialogue between producer and machine. The machine produces something; the producer reacts. Randomize = give the machine a turn. Your turn next.

## Workflow without randomization

Some producers never use randomizers. They compose deliberately, every note chosen. Both approaches are valid.

Using the randomizer doesn't make you less of a composer. It's a tool for exploration. The tasteful decisions — which output to keep, how to refine, what to discard — are still yours.

## When the randomizer is wrong

The randomizer can produce bad outputs. Common failures:

- **Patch too quiet**: randomized knobs might have VOL low, CUTOFF low, ENVMOD low. Sound barely audible. Just randomize again.
- **Pattern too dense**: Driving archetype plus heavy accent = overwhelming. Either pick a different archetype or use `M` to rest some steps.
- **Random tonal clash**: Pattern is in implied A minor, drums are... well, drums aren't pitched, so this doesn't apply. But if you're layering acidflow with other instruments at a specific key, randomizer picks might clash with those.

Don't trust every random output. Judge each one. Keep what works; discard what doesn't.

## The randomizer as a teacher

Over many randomizations, you build intuition for:

- Which CUTOFF + RES combinations produce squelch vs drone.
- Which accent patterns feel like acid vs mechanical.
- Which pitch combinations sound interesting vs random.

This intuition comes from *judging outputs*, not from reading. The randomizer is a stream of examples; you develop taste by responding to each.

If you're new to acid, spend an hour just randomizing and listening. You'll end up with a vocabulary — "oh, that's what self-oscillation sounds like; that's what dense vs sparse means in practice."

## The paradox of design

Here's a subtle point: the randomizer's bias is *opinionated*. It avoids bad outputs, which means it also reduces surprise. A perfectly uniform randomizer might produce terrible 90% of the time but occasionally produce something truly alien that your brain would never have thought of.

acidflow's biased randomizer reduces the 90% bad at the cost of never producing the truly alien. For most users, the tradeoff is correct — they want usable outputs, not surprises.

If you want surprise, turn knobs manually. Break conventions deliberately. The randomizer is not your friend if you want weirdness.

## Randomization and genre

Each archetype biases toward a genre:

- **Classic + Pedal**: 1988-90 Chicago / UK acid.
- **Squelch + Driving**: Hardfloor, Stay Up Forever era.
- **Driving + Driving**: Detroit techno, Belgian new-beat.
- **Dubby + Dub**: Plastikman, dub techno.
- **Classic + Melodic**: FSOL, Stakker, early Warp.

You can't explicitly pick these; the randomizer picks for you. But you can keep rolling until you get the flavor you want, then refine with `M`.

## Try this

1. Focus the Knobs section. Press `r`. Watch the toast. What archetype was rolled?
2. Press `r` again. Different archetype? Listen to the difference.
3. Focus the Sequencer. Press `r`. Pattern archetype toast.
4. Press Space to play. Listen.
5. Press `M`. One step changed. Listen to the difference.
6. Press `M` five more times. Pattern walked a bit. Compare to where you started.
7. Press `R`. Wipe everything. Listen to the fresh combination.
8. Now try: press `R` 10 times in a row. Each time, listen for 5-10 seconds. Rate each 1-10 for how acid-like it sounds.

You've seen the randomizer as a tool, not just a button. Use it, but judge its outputs.

Part VI is complete. Four chapters on performance and workflow:

- Chapter 30: Song mode (chaining slots).
- Chapter 31: Jam mode (live keyboard).
- Chapter 32: MIDI I/O.
- Chapter 33: Randomizer and mutation.

You now have the mechanics of performing and iterating on acid tracks. Next: Part VII — Making Records.
