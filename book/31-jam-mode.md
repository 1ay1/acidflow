# 31 · Jam mode: live performance

The sequencer is acidflow's main way of producing notes. It's a step-sequencer — you program a pattern, and it plays back mechanically. Same 16 steps every bar.

But sometimes you want to *play* the 303, not *program* it. You want to finger a melody, hear it through the 303's filter, decide if it works. You want to audition notes before committing them to a step. You want to improvise.

Jam mode is for this. It turns the computer keyboard into a two-octave piano routed to the 303 voice.

## The concept

Jam mode exists in the lineage of:

- **303 keyboard mode**: the original TB-303 had a small keyboard for playing in a pattern note-by-note.
- **Tracker editors**: Amiga/DOS music programs (SoundTracker, FastTracker, Impulse Tracker, later Renoise) used computer keyboard layouts as piano, which this mode preserves.
- **DAW virtual keyboards**: most DAWs let you play MIDI from the computer keyboard.

Jam mode is acidflow's version. Press a key, hear a note. Release, note ends. Do melody.

## The keyboard layout

Jam mode uses the standard tracker / Renoise layout, which is based on a piano keyboard with the bottom row as "white keys":

```
Upper octave: q 2 w 3 e r 5 t 6 y 7 u
Lower octave: z s x d c v g b h n j m
                (C D E F G A B rows)
```

The `z` row is C, D, E, F, G, A, B (white keys). The `s`/`d`/`g`/`h`/`j` row above is the black keys (sharps). Same for `q` row (C+1, D+1, ...) and the numbers row.

So `z` plays C in the current octave. `q` plays C in the next octave up. `s` plays C♯ in the current octave. And so on.

This covers two octaves continuously. Combined with the octave shift keys (`←`/`→`), you can play across the full 303 range.

## Activating jam mode

Press `k` from anywhere. Jam mode on. The status line shows "JAM Oct3 ♩A C4" or similar — the current octave and the last played note.

Press `k` (or `Esc`) again to exit. Status line stops showing the jam info; sequencer resumes normal operation.

## What happens to the sequencer

When jam mode is ON:

- The sequencer **stops playing the 303 notes**. No step-sequenced bass line.
- Drums still play if the drum pattern is running. You get drums-plus-live-303.
- FX (drive, delay, reverb) still apply to your jammed notes.
- Knobs (CUTOFF, RES, ENVMOD, DRIVE, VOL) still affect the sound.

So jam mode is essentially "play the 303 with a live keyboard instead of the step sequencer, over the drums."

## The envelope behavior

Each key press retriggers the 303 voice. The envelope fires fresh. The filter sweeps. You hear one note of acid.

But there's a subtlety: terminals don't deliver reliable key-*release* events. So acidflow uses a fixed note gate of ~250 ms per press. A key press starts a note; 250 ms later (or when you press another key), the note releases.

This means:

- Short staccato notes: press keys briefly. Each one rings for 250 ms max.
- Legato-ish: press a new key before the previous 250 ms elapses. The new press retriggers, but if the note is long enough, you perceive continuity.
- Can't hold a long sustained note indefinitely. 250 ms gate then release.

For a better "keyboard feel," use MIDI input (Chapter 32) instead. Jam mode is a quick-sketch tool.

## Accent and slide in jam mode

Press `'` (apostrophe) to toggle ACCENT for subsequent presses. Until you toggle it off, every note you play will be accented.

Press `;` (semicolon) to toggle SLIDE. Until you toggle it off, every note slides from the previous note.

This lets you jam with the full 303 articulation vocabulary. Play a plain note, then toggle accent on, play an accented note, toggle slide on, slide into the next note, etc.

Combined state (accent + slide) is possible — toggle both.

## Octave navigation

The lower row (`z`-`m`) plays the "current octave." Default is octave 3 (C3 = MIDI 48).

Press `←` to shift down one octave. Status line updates: `Oct2`.
Press `→` to shift up one octave. Status line: `Oct4`.

Range: low enough for deep bass, high enough for shrill squeals. At high octaves with high RES, you get self-oscillating screams. At low octaves with low CUTOFF, you get sub-bass pulse.

## Jam mode workflows

### Sketching melody

You have a drum pattern but no 303 line yet. Turn on jam mode. Play over the drums. Find notes that work. When you find a phrase you like, memorize it.

Exit jam mode. Program those notes into the step sequencer.

This is the "audition then commit" workflow. More efficient than randomly trying step-by-step.

### Finding a root note

Before you can program, you need to pick a root. Jam over the drums. Try C. Try A. Try D. Which one feels "right" for your mood? Use jam mode to find it.

### Testing note changes

You have a programmed pattern. One note feels wrong but you don't know what's better. In jam mode, play over the running drums, trying notes at the position where the "wrong" note is. When you find the note that works, exit jam mode and update the step.

### Performing live

In a live context, you can toggle jam mode on and off mid-set. Start with a programmed pattern. When the breakdown hits, turn on jam mode and play a live solo over the drums. Toggle back at the drop.

This is unusual for acid (acid is mostly pre-programmed), but can be a dramatic live moment.

### Developing an ear

If you're new to music, jam mode is a way to learn note relationships experientially. Pick a drum pattern. Play over it with one hand, starting from C. Note which keys sound consonant, which don't. Build intuition for minor pentatonic vs chromatic.

## The two-octave constraint

Only 24 notes are directly playable (two octaves). Combined with octave shifting, you can play any note, but not without a `←`/`→` press to shift.

In practice:

- Play a phrase in one octave.
- Shift octave.
- Play a phrase in the new octave.
- Shift again.

For fast, multi-octave phrases, this is limiting. The 303 is a bass instrument so two octaves is often enough.

## Why the 250 ms gate

Terminal applications only reliably receive key-*down* events. Key-up is less reliable — some terminals deliver it, some don't, some deliver it only after a delay.

To work around this, acidflow auto-releases notes after 250 ms. Even if the key-up event is missed, the note won't hang forever.

Downside: you can't hold a note longer than 250 ms. For music that needs long sustained notes (drones, pads), this is a limitation.

The 303 rarely needs long sustained notes — it's a plucky, staccato instrument — so 250 ms gates fit the instrument. If you need longer, use MIDI input.

## Polyphony: you don't have any

Press two keys simultaneously in jam mode. You get... one note. The second key press retriggers, replacing the first.

This is because the 303 voice is monophonic. It has one oscillator; it can only play one pitch at a time.

You can play *melodic sequences* in jam mode, just not chords. Acid music is almost entirely melodic anyway — this is acid, not jazz.

## Jam mode and song mode

Song mode and jam mode can be combined, but with caveats:

- If song mode is on and you enter jam mode, the song mode chain continues in the background (for drum changes), but the 303 is played live.
- The chain still advances through slots — each pattern wrap triggers the next slot. But since you're controlling the 303 live, the only thing that actually changes is the drum pattern.

So: drum variation via song mode + live 303 via jam mode = a simple live performance setup.

## Jam mode and FX

The FX rack (drive, delay, reverb) applies to jam-mode-played notes normally. So your jammed notes get delayed and reverberated just like sequenced notes.

If delay is at high feedback, your jammed notes stack echoes. Tap a key, hear it echo N times. The musical effect is very different from sequenced playback — you're creating a cloud of delayed notes by tapping short hits.

Try: turn on delay 1/8 dotted with 0.7 feedback. Jam a few notes. The space fills with echoes. Play around the echoes. This is "live dub acid."

## The MIDI alternative

If jam mode's keyboard feels limited, MIDI input (Chapter 32) is the upgrade. A real MIDI keyboard gives you:

- Velocity sensitivity (via MIDI velocity → ACCENT mapping).
- Proper note-off events (longer notes work).
- More octaves without shifting.
- Two-handed playing.

Jam mode is for quick sketches from the computer keyboard. MIDI is for real performance.

## Limitations summary

- Monophonic (like the 303, by design).
- Two octaves at a time.
- 250 ms max note length.
- No velocity (ACCENT is on/off, toggled via `'`).
- No sustain pedal.

These are acceptable for the instrument. A 303 doesn't have polyphony or velocity either.

## Try this

1. Press `k` to enter jam mode. Status line shows "JAM Oct3".
2. Press `z`. You hear C3. Press `c`. You hear E3. You just played a C-E interval.
3. Toggle slide (`;`). Press `z` then `c` again. The E3 slides from C3. 
4. Toggle slide off (`;`). Toggle accent on (`'`). Press `n` (A3). You hear a loud accented A.
5. Shift octave up (`→`). Press `z`. You hear C4, an octave higher.
6. Play a pentatonic phrase: `z x v n z` (C, D, F, A, C — A minor pentatonic fragment, but starting on C, which is C major pentatonic... try `n y z r n` for A3, D4, C4, F4, A3 which is A minor pentatonic).
7. Turn up delay (DLY MIX 40%, 1/8 dotted, FB 0.6). Jam a sparse sequence. Hear the echoes fill.
8. Exit jam mode (`k`). The step sequencer returns.
9. Program the phrase you just jammed into step 1-5 of the sequencer. Play. The programmed version should sound like what you jammed.

You've learned to use jam mode for auditioning and playing. It's not a full keyboard, but for quick melodic exploration over drums, it's fast.

Next: MIDI I/O.
