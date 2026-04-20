# 10 · The filter — CUTOFF and RES

The filter is why acid is acid. If you took the 303's filter out and replaced it with a generic state-variable lowpass, acid would not have happened. This chapter is about the two knobs that control it — CUTOFF and RES — and about why, across the entire synthesizer, these two are the ones you're going to be thinking about the most.

## What CUTOFF actually controls

CUTOFF sets the cutoff frequency of the lowpass filter. In acidflow, this ranges from **13 Hz** (essentially silence — only the deepest sub-bass passes) to **5 kHz** (most of the saw's harmonics pass, the sound is bright).

The scaling is **logarithmic**, which matches human pitch perception. A knob position of 25% might be around 100 Hz; 50% around 700 Hz; 75% around 2500 Hz. The knob feels *musical* — equal knob movements produce equal *musical* changes, not equal numerical changes.

### What the filter does to a sound

Imagine the saw's harmonic content as a ladder:

- 100 Hz fundamental
- 200 Hz (2nd harmonic)
- 300 Hz (3rd harmonic)
- 400 Hz (4th harmonic)
- 500 Hz (5th harmonic)
- ... up to ~10,000 Hz and beyond.

A 4-pole lowpass filter with CUTOFF at 500 Hz passes harmonics 1-5 at full, then cuts everything above 500 Hz at 24 dB per octave. So:

- 500 Hz: full amplitude
- 1000 Hz: -24 dB (1/16 amplitude)
- 2000 Hz: -48 dB (1/256 amplitude)
- 4000 Hz: -72 dB (1/4096 amplitude)

In human terms: at CUTOFF = 500 Hz, you hear the first five harmonics clearly and everything else is essentially gone. The sound is *dull*. Moving CUTOFF up to 2000 Hz lets harmonics 1-20 through, which sounds much brighter.

### The static filter test

Set ENVMOD to 0 (filter is static — the envelope doesn't move it). Set RES to 20% (low, so you don't confuse yourself with resonance). Play a pattern.

Now sweep CUTOFF slowly from 0% to 100%. What you hear:

- **0-20%**: very dull, low-pass almost closed. Sub-bass and fundamentals only. Almost dubstep-like.
- **20-40%**: some mid-range coming in. Sound is starting to have shape.
- **40-60%**: good body. Classic "warm" tone. Most of the harmonics needed for recognizable pitch.
- **60-80%**: bright. The upper harmonics are audible, buzz is returning.
- **80-100%**: nearly unfiltered. Most of the saw's full character.

This is the *territory map* of CUTOFF. Every acid record lives somewhere on this spectrum.

## What RES does

RES is resonance, or Q — positive feedback around the filter. At high resonance, the frequencies right at the cutoff point are *boosted*, creating a ringing peak.

Visually, the filter's frequency response curve goes from this (low RES):

```
amp |___
    |   \__
    |      \__
    |         \____
    |_____________________ freq
          ^cutoff
```

To this (high RES):

```
amp |        _
    |___    / \
    |   \__/   \__
    |            \____
    |_____________________ freq
          ^cutoff
```

That peak right at the cutoff point is the resonance. Higher Q = taller peak = more pronounced ringing.

### The three zones of resonance

RES has three distinct behavioral zones in the 303:

**Zone 1 — Low (0% to ~40%)**: The filter is essentially just a plain lowpass. Resonance exists but isn't the dominant character. Sound is "filtered," not "resonated." Use this range for:
- Dub-acid where you want a muted, non-aggressive character.
- Background elements that shouldn't pull attention.
- Extreme drives where you want the saturation to be the star and the filter to just tidy up.

**Zone 2 — Middle (~40% to ~80%)**: The resonance peak is clearly audible — you hear a whistle at the cutoff frequency. But the filter hasn't started self-oscillating. This is the *expressive* range. Use this for:
- Most acid tracks. The main acid sound lives here.
- Filter sweeps where the resonance is audible but not overwhelming.
- Anywhere you want the filter to "sing" without "screaming."

**Zone 3 — High (~80% to 100%)**: The filter is on the verge of self-oscillation, and small envelope moves push it over. At 100%, self-oscillation is constant — the filter produces a sine wave at the cutoff frequency even when the oscillator is silent. Use this for:
- Extreme acid techno.
- Solo/lead moments where you want maximum intensity.
- Specific effect sounds — sustained whistles, pitch-tracking synth tones.

Most records spend most of their time in Zone 2 and visit Zone 3 during drops.

### The self-oscillation trick

When RES is near 100% and the filter is self-oscillating, the self-oscillation frequency is the cutoff frequency. If you modulate the cutoff, you modulate the pitch of the self-oscillation. Because the envelope modulates cutoff (via ENVMOD), each note effectively triggers a *pitched sine sweep* inside the filter.

This is the "acid whistle." When you hear a 303 squealing across a pattern, you're hearing:

1. Low saw oscillator pitch (controlled by the step's note).
2. Self-oscillating filter (controlled by RES).
3. The envelope modulating the cutoff (controlled by ENVMOD + DECAY).
4. The accent adding extra envelope kick (controlled by ACCENT).
5. The result: a note that has a *melodic low part* and a *screaming high part* that moves.

This is the key interaction of the acid sound. Once you hear it clearly, you cannot unhear it.

## The CUTOFF + RES interaction

The two knobs don't operate independently. Their combination determines the character.

### Low CUTOFF + Low RES

Muted, dubby, bass-heavy. Good for background bass duties or for a verse before the drop.

### Low CUTOFF + High RES

This is magic territory. The cutoff is low enough that the fundamental and a few harmonics pass, but the resonance peak is right in the low-mid range, producing a *woofing* effect. Every note has a deep, organic growl. Plastikman territory. Excellent for slow, hypnotic tracks.

### Middle CUTOFF + Middle RES

The "default acid" zone. Bright enough to be interesting, resonant enough to sing. Most Chicago acid records live here. If you're new and don't know where to start, this is the safe spot.

### Middle CUTOFF + High RES

Screaming territory. The resonance peak is in the upper-mid, which is where human hearing is most sensitive. This is sharp and loud. Acid techno.

### High CUTOFF + Low RES

Almost unfiltered. Bright, buzzy, upfront. Not very "acid" — the filter isn't doing enough interesting work. Use for contrast, rarely as a main sound.

### High CUTOFF + High RES

The filter is open wide and self-oscillating in the treble. Painful. Rarely useful. If you want this sound, it's usually for a short drop moment, not a sustained texture.

## Filter envelope (preview of ENVMOD + DECAY)

CUTOFF by itself is the filter's *base* position. ENVMOD adds an *envelope* to the filter, which moves the cutoff upward each time a note plays, then decays back to the base.

So: if CUTOFF is at 30% (base) and ENVMOD is at 50%, then each note starts with the filter open to about 60% (base + envelope), then decays back to 30% over the decay time. The envelope action is what creates the *ptoonnng* shape of every acid note. Without it, the filter is static.

Set ENVMOD to 0 to hear a "flat" filter. Set ENVMOD to 50% and the filter breathes with every note. Set ENVMOD to 100% (full 4 octaves) and every note opens the filter maximally before it closes.

The envelope is covered in its own chapter (Chapter 11). For this chapter, just know that CUTOFF is only half the filter story — the other half is the envelope's movement relative to that base.

## Live filter moves: the fundamental acid technique

The most common gesture in acid music is a slow rotation of the CUTOFF knob while a pattern is playing. Start with CUTOFF low (~25%), let the pattern build, then slowly raise CUTOFF over 16 or 32 bars up to ~80%, then let it drop back. This is the filter arc, and it's the main form of "arrangement" most acid records have.

Listen to *Acid Tracks* — it's twelve minutes of this move, repeated. Listen to *Higher State of Consciousness* — the filter movement *is* the melody. Listen to any of the Hardfloor tracks — the filter arc is three-dimensional because RES is moving too.

acidflow lets you do this with:

- Mouse drag on the CUTOFF knob while the pattern plays.
- Up/down arrows repeatedly (slower but more precise).
- Song mode chaining different save slots with different CUTOFF values.
- MIDI automation via an external controller.

Practice this. Every acid producer, whatever their specific style, has this move built into their reflexes.

## The filter heatmap visualization

At the bottom of the acidflow screen is a live filter response heatmap. It shows the current frequency response of the filter as a row of pixels, with the x-axis being frequency and the intensity being amplitude.

When the envelope is moving the filter, the heatmap moves too. Watch it during a pattern — you'll see the cutoff peak dance back and forth with each note. This is an excellent teaching tool. You can *see* what you're hearing.

At high resonance, the peak is much taller than at low resonance. At low cutoff, the peak is to the left; at high cutoff, to the right.

## Key takeaways

1. **CUTOFF is where the filter is.** Log-scaled from sub-bass to brilliance.
2. **RES is how resonant it is.** Low = neutral, middle = singing, high = screaming.
3. **The two together define the texture** more than any other pair of controls.
4. **Moving CUTOFF live is the primary acid technique**. You will do this a lot.
5. **RES above 85% is self-oscillation territory** and fundamentally changes the character.

## Try this

1. Load a preset. Pick one that isn't too busy — preset 07 (Plastikman) is good.
2. Set ENVMOD to 0 (filter doesn't move). Set RES to 40%. Slowly sweep CUTOFF from 0 to 100% and back. Memorize how each region sounds.
3. Repeat with RES at 70%. Same sweep. Hear how the sweep becomes much more expressive because the resonance peak is now moving across the spectrum.
4. Repeat with RES at 95%. Now the filter is screaming and you're *playing pitches* with the CUTOFF knob. Feel the difference.
5. Restore ENVMOD to 50%. Now you have a moving filter plus a moving base. Sweep CUTOFF again. This is the acid sound.
6. Try `R` (randomize pattern) a few times while moving CUTOFF. Different patterns respond to the same filter move differently. That's the interaction between pitch content and filter character that you'll spend your career learning.

Next up: the envelope. The engine that drives all this.
