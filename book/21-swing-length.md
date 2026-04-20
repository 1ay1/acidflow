# 21 · Swing, length, and polymeter

Three controls at the pattern level:

- **Swing**: delays every even step by some amount.
- **Length**: changes how many steps are in the pattern (4-16).
- **BPM**: tempo (discussed less formally in other chapters).

The first two have strong musical character and deserve their own chapter.

## Swing, in detail

Swing is a rhythmic feel that delays every even-numbered step by a small amount. A straight pattern has equal spacing between all 16 steps; a swung pattern has uneven spacing, with even steps slightly "late."

In acidflow, swing ranges from 50% (no swing, straight time) to 75% (maximum shuffle). Press `-` to decrease swing, `=` to increase.

### The math

At 50%, every step is spaced 1/16 apart. Let's say the bar is 2000 ms. Each step is 125 ms.

At 60% swing, even steps are delayed. The formula: even step n is at position `n × (125 × (S/50))` ms, where S is 50 to 75. At S=60, even steps fall at 125 × 1.2 = 150 ms past the previous odd step, instead of 125 ms.

In practice: at 60% swing, even steps (2, 4, 6, ...) are delayed by 25 ms. The pattern has a "limp" to it — odd steps on time, even steps late.

At 75% swing (max), even steps are delayed by a full 16th-note duration. The pattern becomes almost a triplet feel (three sixteenths grouped together, long-long-short... long-long-short...).

### What different swing values feel like

- **50% (straight)**: mechanical, rigid. Good for aggressive techno, hard acid, maximum groove is not the goal.
- **52-55%**: barely perceptible swing. The pattern feels very slightly "off" from mechanical. Chicago-acid norm.
- **57-62%**: noticeable swing. Shufflier. The groove starts to feel like it's moving its hips. Classic house territory.
- **65-72%**: obvious shuffle. Triplet feel is dominant. Funk, deep house, certain broken-beat genres.
- **73-75%**: extreme swing. Almost triplet-quantized. Unusual for acid; common in jazz-fusion electronica.

For acid specifically, most records sit at 50-55% swing. Acid techno at 50%. Chicago acid at 52-56%. Anything higher is a stylistic decision.

### Swing on the 303 vs swing on the drums

acidflow's swing applies to BOTH the 303 sequencer and the drum grid equally. So if you turn swing up, both the bassline and the drums swing together.

This is the correct default. Swing that affects only one layer creates a conflict between instruments — the 303 swings while the drums don't, and the whole rhythm falls apart. Lock-step swinging keeps the pattern coherent.

(Some DAWs offer independent swing per track. This is a niche capability rarely useful for acid specifically.)

### Swing and slides

A slide is a pitch glide from one note to the next. When swing delays the second note, the slide also has more time to complete.

At 50% swing: slide has 125 ms to transit from one pitch to the next.
At 60% swing: slide has 150 ms. More time to glide.
At 75% swing: slide has 250 ms. The slide is much slower and more audibly melismatic.

This can be a feature — slower slides feel more vocal. It can also be a bug if your pattern depends on rapid slides hitting a target before the next note.

### Swing and accents

Accents on swung even steps hit slightly later than their straight counterparts. If your accent pattern relies on aligning with kick hits on beat 2 (step 5), the swing delays that accent slightly. Usually imperceptible unless swing is very high.

### When to use swing

- **Use swing** to give a pattern a human, loose feel. Perfect 50% is machine-like; slight swing makes it sound recorded.
- **Use straight 50%** for aggressive, relentless tracks. Acid techno. Industrial acid. Anything where mechanical precision is the vibe.
- **Use heavy swing (65%+)** for a deliberate triplet-shuffle feel. Usually a stylistic choice, rarely fits acid house but can fit funk-influenced electro acid.

## Length

The pattern length controls how many steps are in the pattern before it loops. Default: 16.

Press `{` to decrease length, `}` to increase. Range: 4 to 16.

### Shorter patterns

A pattern of length 4 loops every 500 ms at 120 BPM — extremely tight. You can only fit one beat of music. Rare use: super-minimal acid, drone experiments.

A pattern of length 8 loops every beat at 120 BPM. Halfway between "2 bars per second" and "1 bar per 2 seconds." Can work for minimal acid with very tight repetition.

A pattern of length 12 loops every 3/4 of a bar. This creates **polymeter** with the always-16-step drum grid (see below).

### Polymeter explained

The drum grid is always 16 steps (one bar). If the 303 pattern is a different length, the 303 and drums go out of sync, realigning only at their least common multiple.

For example:

- 303 = 16 steps, drums = 16 steps. They align every bar. Normal.
- 303 = 8 steps, drums = 16 steps. The 303 loops twice per drum bar. Fast-repeat 303, no polymeter — it's just "the 303 plays its short pattern twice."
- 303 = 12 steps, drums = 16 steps. The 303 finishes its loop every 3/4 of a bar. 303 and drums realign every 12 × 16 / gcd(12,16) = 48 steps = 3 drum bars. The 303 has played 4 loops in that time. This is **polymetric**.
- 303 = 11 steps, drums = 16 steps. Realigns every 11 × 16 / 1 = 176 steps = 11 drum bars. Very slow polymeter — feels like the 303 drifts across many bars.

### What polymeter sounds like

To the listener, a polymetric pattern feels like the 303 is "walking" across the drum pattern — each bar, the 303 starts at a different position relative to the drum pattern, creating a constantly shifting rhythmic relationship.

It's hypnotic. It's also *disorienting* if used carelessly — the listener may not track the meter correctly.

### When to use polymeter

- **To create hypnotic motion** over many bars. A 12-against-16 can go for 3-4 drum bars before resolving, giving the track a long arc.
- **For minimal acid** where you want the pattern to feel "alive" without constantly programming variations.
- **For experimental tracks** where breaking meter is the point.

### When NOT to use polymeter

- **For straight dance tracks** where the groove needs to lock tightly. A polymetric 303 confuses the dancers.
- **For short tracks** where there isn't time for the polymeter to develop and resolve.
- **If you're new to acid** — nail 16-step patterns first, then experiment with length.

### The Plastikman approach

Richie Hawtin, as Plastikman, uses short-and-changing pattern lengths extensively. Tracks like "Spastik" build around very short 303 patterns (sometimes 8 or fewer steps) that repeat rapidly against unchanging drums, with the 303 mutating over time. The short loop emphasizes the mutation (Chapter 33) by making small changes feel large.

### Pattern lengths that "fit" 16

Lengths that divide 16 — 2, 4, 8, 16 — don't create polymeter. They just loop at multiples of the drum bar. These are your "safe" lengths if you want to avoid rhythmic confusion.

Lengths that don't divide 16 — 3, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15 — DO create polymeter. These are your "adventure" lengths. 12 and 14 tend to be the most musically usable (the polymeter resolves in a reasonable number of bars).

## Pattern length and melodic structure

A shorter pattern means you have fewer steps to say something with. Compose accordingly:

- **4 steps**: basically a rhythmic pulse. One melodic idea per beat. Very minimal.
- **8 steps**: enough for a short phrase with call-and-response.
- **12 steps**: three beats of melody. Unusual but workable.
- **16 steps**: full musical sentence.

Most acid records stay at 16.

## The BPM-swing-length interaction

All three controls affect the feel of your pattern:

- **BPM** changes absolute tempo.
- **Swing** changes rhythmic evenness.
- **Length** changes how much gets repeated per drum bar.

Combined, you get a huge variety of feels from the same pattern content.

**Example**: pattern X at 120 BPM, 50% swing, 16 length. Now at 135 BPM, 58% swing, 12 length — same notes, completely different vibe. Faster, swingier, polymetric. You've turned the same melodic content into a new song.

This is why great producers often save multiple slots with the same melodic content but different BPM/swing/length settings — they're exploring different renditions of the same pattern.

## Changing swing/length mid-track

Both controls can be changed while the pattern plays. The change takes effect at the next step.

Changing swing mid-pattern is smooth — the pattern "re-locks" to the new timing gradually. Use this for breakdown sections (higher swing for loose feel) returning to the main (back to low swing).

Changing length mid-pattern is more abrupt — the pattern immediately starts looping at the new length. Can sound jarring. Only use this at phrase boundaries.

## Swing and the DRUMS

Because drums swing at the same rate as the 303, the whole pattern stays coherent. But this means swing is NOT a way to make the 303 feel different from the drums — they're always synced.

For 303-vs-drums differentiation, use pattern length (polymeter) instead.

## Try this

1. Load preset 01. Set swing to 50%. Play. Notice the straight, mechanical feel.
2. Slowly raise swing to 60%. Play. Groove starts to appear.
3. Raise to 70%. Play. Shuffle dominates. Almost triplet-feel.
4. Back to 55%. Set pattern length to 12. Play. The 303 and drums are now polymetric.
5. Pattern length to 11. Play. The polymeter shifts dramatically — 303 walks across drums over 11 bars.
6. Back to 16. Back to 50% swing. Back to normal acid.

You've toured the full dynamic range of these controls. Next: let's leave the sequencer and enter the drum machine.
