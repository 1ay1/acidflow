# 24 · Snare, hats, clap

Four of acidflow's five drum voices: SD (snare), CH (closed hi-hat), OH (open hi-hat), CL (clap). Together with the kick (Chapter 23), these five voices cover the rhythmic skeleton of every dance subgenre acid intersects.

This chapter describes each voice, its synthesis, its role, and its typical programming.

## SD — Snare

### What it is

A snare drum in the acoustic world is a two-component sound:

1. **Body**: the drum head (tuned to somewhere around 150-300 Hz), struck by the stick. Produces a pitched *thwack*.
2. **Snare wires**: thin metal wires against the bottom head that rattle when the drum is struck. Produces a broad-spectrum noise burst.

A good snare is about 40% body and 60% snare wire (rough proportions). Too much body and it sounds like a tom. Too much wire and it sounds like applause.

### How acidflow synthesizes it

acidflow's SD is:

- A **sine tone** at ~180 Hz (the body), with its own short envelope (~50 ms decay).
- A **noise burst** (white noise through a bandpass filter centered ~2 kHz) providing the snare-wire character.
- An **amp envelope** with fast attack and medium decay (~200-300 ms).

The combination produces a snappy, electronic-sounding snare. It's more 909-style than 808 — the 808 snare is a longer, boomier body; the 909 is tighter and more snare-wire-dominant. acidflow leans toward the 909 sound.

### Snare vs. clap

Snare and clap fill similar rhythmic roles (both on the backbeat). They sound different:

- **Snare**: short attack, moderate decay, body + noise. Traditional.
- **Clap**: multiple short noise bursts close together, no body. Electronic.

Classic acid house uses clap instead of snare. Techno uses both. Drum 'n' bass uses snare. Pick based on the subgenre.

### Snare programming

Most common: snare on beats 2 and 4 (steps 5 and 13). This is the backbeat.

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
SD     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
```

Occasionally: snare on ghost-note positions (e.g., step 8 or 16) to add syncopated layers. This is more of a drum 'n' bass or breakbeat technique; rarely useful in acid.

Snare rolls: multiple snares in rapid succession as a transitional fill.

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
SD     .  .  .  .  X  .  .  .  .  .  .  .  X  X  X  X
                                                ^^^^^^
                                                snare roll
```

Use rolls sparingly — once per 8-16 bars — as transitions.

## CH — Closed Hi-Hat

### What it is

A hi-hat is two cymbals on a stand, played with the foot pedal. "Closed" means the cymbals are held together, producing a short, crisp *tick*. The sound is short (maybe 80-120 ms) and bright.

Acoustically, a closed hat is broadband noise filtered to emphasize 4-10 kHz, with a very fast envelope.

### How acidflow synthesizes it

acidflow's CH is:

- **White noise** through a highpass filter (~4 kHz cutoff). This gives the "tinny" character of a closed hat.
- An **amp envelope** with very fast attack and fast decay (~50 ms).

Simple, cheap, effective. Classic TR-909-style closed hat.

### Closed hat programming

The closed hat usually plays the "subdivisions" — the notes between beats.

**The standard**: closed hats on every offbeat (the "and" of each beat — steps 3, 7, 11, 15):

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
CH     .  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
```

This gives the classic "unz-tik-unz-tik" feel. Kick on the beat, hat on the and.

**The double**: closed hats on every 8th note (steps 1, 3, 5, 7, 9, 11, 13, 15):

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
CH     X  .  X  .  X  .  X  .  X  .  X  .  X  .  X  .
```

Every 8th note has a closed hat. More driving, more energetic.

**The sixteenth**: closed hats on every 16th note (all 16 steps):

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
CH     X  X  X  X  X  X  X  X  X  X  X  X  X  X  X  X
```

Constant closed hats. Very driving, can feel overwhelming. Used sparingly — maybe for a build section.

### Hi-hat and tempo

At slower BPMs, 16th-note hats feel sparse enough to work. At faster BPMs (140+), 16th-note hats become a "tsss" roll and can overwhelm the mix. Use 8th-note or offbeat hats at fast tempos.

## OH — Open Hi-Hat

### What it is

"Open" hi-hat — the cymbals are pedaled open, so when struck, they ring for 200-500 ms before decaying. Much longer than closed.

Acoustically: similar noise content to closed, but with much longer envelope, and some additional tonal resonance from the cymbal vibration.

### How acidflow synthesizes it

acidflow's OH is:

- **White noise** through a highpass filter (~4 kHz cutoff, slightly different from CH to avoid them sounding identical).
- An **amp envelope** with fast attack and much longer decay (~300-400 ms).
- A slight resonant peak in the filter to give it more tonal character.

### Open hat programming

The open hat usually lands on an upbeat — giving a "chick" that punctuates the groove.

**The classic**: open hat on step 8 and 16 (the last 16th of beats 2 and 4):

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
OH     .  .  .  .  .  .  .  X  .  .  .  .  .  .  .  X
                            ^                       ^
                            chick                   chick
```

Each open hat's long tail bleeds across into the next beat, creating a "chick-ticka-chick" groove when combined with closed hats.

**Variation**: open hat on step 4 and 12 (the last 16th of beats 1 and 3):

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
OH     .  .  .  X  .  .  .  .  .  .  .  X  .  .  .  .
```

Different feel — open hats on "off-beats" of odd beats instead of even beats.

### Open hat sustain

Because open hats ring, they create ambiance and sustain. Too many open hats = muddy, washy drums. One open hat per bar is often enough. Two per bar is common. Three+ starts to clutter.

## CL — Clap

### What it is

A clap (acoustic) is hand-clapping — a sharp broadband noise burst. The specific electronic "clap" sound comes from the TR-808 (1980), which pioneered the synth clap.

The 808 clap is actually NOT a single noise burst — it's three or four rapid bursts close together (within 30-50 ms), which gives it a distinctive "flam" character. Sounds like several people clapping simultaneously but not perfectly synchronized.

### How acidflow synthesizes it

acidflow's CL is:

- **Three short noise bursts** (each ~5 ms) separated by ~15 ms gaps. This gives the 808-clap "flam" sound.
- A **longer noise tail** after the bursts, with a highpass filter (~800 Hz) to emphasize the mid-high range.
- Overall envelope: burst phase (~30-50 ms), then sustained tail (~200-300 ms).

This is close to the classic 808/909 clap sound.

### Clap programming

Clap usually plays on beats 2 and 4 (steps 5 and 13) — backbeat. Same as snare.

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
CL     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
```

You can layer clap WITH snare for a fatter backbeat, or use clap INSTEAD of snare for a more electronic feel. Acid house tends to prefer clap; acid techno often uses both.

### Clap variation: "double clap"

Two claps close together on a single backbeat:

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
CL     .  .  .  .  X  X  .  .  .  .  .  .  X  X  .  .
                      ^                       ^
                      double                  double
```

Clap on step 5 AND step 6 (or step 13 and 14). Creates a double-hit backbeat that's more aggressive.

### Clap as build

In a build section, clap rolls work like snare rolls:

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
CL     .  .  .  .  X  .  .  .  X  .  X  .  X  X  X  X
                                     ^
                                     density increasing
```

Clap density increases over the bar, peaking at the end. Leads into the next section.

## Programming the five together

The five voices are used in characteristic combinations. The standard is:

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
SD     .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
CH     .  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
OH     .  .  .  .  .  .  .  X  .  .  .  .  .  .  .  X
CL     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
```

No snare. Clap on the backbeat. Kick on the downbeat. Hats on the offbeats. This is "acid house drums" in its simplest form.

For acid techno, swap clap for snare:

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
SD     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
CH     .  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
OH     .  .  .  .  .  .  .  X  .  .  .  .  .  .  .  X
CL     .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
```

And for maximum density, use both clap AND snare:

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
SD     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
CH     .  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
OH     .  .  .  .  .  .  .  X  .  .  .  .  .  .  .  X
CL     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
```

Snare + clap = "layered backbeat." Fatter than either alone.

## The frequency spectrum

Each drum voice occupies a specific frequency range, keeping them from overlapping:

- BD: 30-100 Hz (sub/low bass)
- SD: 150-300 Hz (body) + 1-3 kHz (noise)
- CH: 4-8 kHz (high)
- OH: 3-10 kHz (high, wider range than CH)
- CL: 800-3000 Hz (mid-high)

You can see: BD lives in the low end, SD occupies mid-low plus mid-high, CL is mid-high, CH/OH are high. Minimal overlap. That's why you can stack all five and still hear each distinctly.

## Try this

1. Clear drums (`C`). Program standard acid house: kick 1/5/9/13, closed hat 3/7/11/15, open hat 8/16, clap 5/13. Play.
2. Swap clap for snare: remove clap on 5/13, add snare. Play. Now it's more techno-sounding.
3. Add clap back but leave snare. Now layered. Play. Fatter.
4. Add sixteenth-note hats (CH on every step). Play. Much busier.
5. Remove all hats. Play. Feels empty.
6. Try a 909-style gospel fill: SD on steps 5, 13, 14, 15, 16 as a rolling snare fill at end of bar. Play.

Each drum is a small decision. Five drums times 16 steps = 80 decisions per bar. Most of them stay default; the few you change define the pattern's character.

Next: programming principles for acid drum patterns.
