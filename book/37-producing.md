# 37 · Producing an acid track from scratch

This chapter walks through making a complete acid track in acidflow. Not a theoretical description — an actual sequence of decisions and keystrokes that produces a 4-minute track ready to export.

Follow along. At the end, you'll have a WAV file of something you made. More importantly, you'll have the workflow — the habits and decisions that go into making acid tracks reliably.

## The session plan

We're making a medium-tempo acid house track. ~125 BPM. ~4 minutes.

Structure: intro (30s), build (30s), drop+main (2 min), breakdown (30s), final main (30s).

Mood: driving but not aggressive. Classic UK-acid-house vibe.

## Step 1: Open acidflow

Launch acidflow. You should see the classic interface — knobs, FX, sequencer, drums, transport.

Default state is usually a saved pattern. Press `Shift+1` to save whatever is there to slot 1 (we'll overwrite it later). Or just hit `r` on the Sequencer to get a fresh pattern.

## Step 2: Set the tempo

Focus Transport (`Tab` until Transport is highlighted).

Current BPM shows in the top bar. Adjust to 125:
- Up/Down arrows or `[`/`]` to adjust.
- Set to 125 BPM.

Leave swing at 50% (no swing, straight). Length at 16 (standard bar).

## Step 3: Program the 303 line

Focus Sequencer. Press `r` to randomize. You get a toast: "Pedal" or "Driving" or whatever archetype. Listen.

Keep rolling `r` until you find a line you like. For this session, aim for a Pedal or Melodic archetype.

If you prefer manual programming:

```
Step:  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
Pitch: A  .  .  E  A  .  A' .  .  .  E  A  .  A  .  .
                   (A' = A one octave up)
Flags: a                 s                          
       (a = accent, s = slide)
```

This is a medium-density pattern with root emphasis on step 1, 5, 13. Two octave-ups at steps 3 and 11 for variation. One slide, two accents.

To enter: click step 1 (or use arrow keys). Type `a` for A. Move to step 4, type `e`. Step 5, `a`. And so on.

For accent: press `'` on the step. For slide: press `;`.

## Step 4: Set the 303 knobs

Focus Knobs.

- **TUNE**: 0 (default).
- **WAVE**: saw (default).
- **CUTOFF**: ~40%. Start darker; we'll open later.
- **RES**: ~70%. Enough resonance to hear squelch on accents.
- **ENVMOD**: ~55%. Moderate filter motion per note.
- **DECAY**: ~45%. Medium — not too snappy, not too sustained.
- **ACCENT**: ~60%. Clear contrast between accented and non-accented.
- **DRIVE**: 0%. We'll add later if needed.
- **VOL**: ~75%. Good level.

Press Space. Listen. Adjust knobs to taste.

## Step 5: Program the drums

Focus Drums.

Basic UK acid kit:

```
Voice  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16
BD     X  .  .  .  X  .  .  .  X  .  .  .  X  .  .  .
SD     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
CH     .  .  X  .  .  .  X  .  .  .  X  .  .  .  X  .
OH     .  .  .  .  .  .  .  X  .  .  .  .  .  .  .  X
CL     .  .  .  .  X  .  .  .  .  .  .  .  X  .  .  .
```

- Kick (BD): beats 1, 2, 3, 4 (steps 1, 5, 9, 13).
- Snare (SD): backbeat (5, 13).
- Closed hat (CH): offbeats (3, 7, 11, 15).
- Open hat (OH): steps 8, 16.
- Clap (CL): backbeat (5, 13) — layered with snare.

To enter: navigate each voice's row, click each step you want on.

Play. You should hear a classic UK acid beat.

## Step 6: Listen to the 303 + drums together

With both the 303 line and drums programmed, press Space. Listen for 30-60 seconds.

Things to notice:
- Is the kick fighting with the 303 on beat 1? If yes, lower CUTOFF slightly so the 303 has less low-end.
- Are accents audible? If not, raise ACCENT slightly.
- Is the snare-clap layer too loud? If yes, consider removing the snare or clap.

Adjust until it feels good.

## Step 7: Save slot 1 — the main

Once you're happy, press `Shift+1`. Toast: "✓ saved slot 1."

This is your MAIN pattern. We'll build variations around it.

## Step 8: Create the intro variant

Duplicate the main (it's already loaded). Modify:
- Clear the closed hats (CH steps).
- Clear the clap.
- Keep only kick and snare.
- Remove some 303 notes — maybe leave only steps 1, 5, 9, 13 (pedal on root).

This is a "stripped" version. Intro-like.

Save to slot 2 (`Shift+2`).

## Step 9: Create the breakdown variant

Load slot 1 (press `1` from a non-Sequencer/Drums section).

Modify:
- Clear ALL drums except kick on beat 1 only (step 1). One kick per bar.
- Keep the 303 line but remove accents (so it feels less aggressive).

This is the breakdown — sparse, atmospheric.

Save to slot 3 (`Shift+3`).

## Step 10: Arrangement plan

You have:
- Slot 1: Main (full).
- Slot 2: Intro (stripped).
- Slot 3: Breakdown (sparse).

Arrangement:
- Bars 1-8: Slot 2 (intro).
- Bars 9-16: Slot 1 (main, filter opens).
- Bars 17-24: Slot 1 (main, filter open).
- Bars 25-32: Slot 3 (breakdown).
- Bars 33-48: Slot 1 (main, final).

At 125 BPM with 16-step bars, each bar = 1.92 seconds. 48 bars = 92 seconds. Plus intro/outro pre/post, about 2 minutes.

For a longer track, extend each section.

## Step 11: Record the arrangement

Load slot 2 (intro). Make sure knobs are set as per Step 4.

Press `e` (WAV export) to start recording... actually, acidflow's WAV export records a fixed 4 loops of the current pattern. For live arrangement, you may need to export live.

Alternative: use an external recorder (OBS, system audio capture) to capture the arrangement as you perform it.

For this walkthrough, let's use the WAV export approach:

1. Load slot 2. Press `e`. Wait for it to finish. This is your intro section.
2. Load slot 1. Close the filter (CUTOFF at 25%). Press `e`. This is the start of the main, filter still partially closed.
3. Load slot 1 again. Open CUTOFF to 70%. Press `e`. This is the main at full strength.
4. Load slot 3. CUTOFF at 35%. REV MIX up to 50%. Press `e`. This is the breakdown.
5. Load slot 1. CUTOFF at 75%. REV back to 15%. Press `e`. This is the final main.

You now have 5 WAV files. In a DAW (or ffmpeg), concatenate them in order. You have a rough track.

## Step 12: Live-perform the arrangement

For a more natural performance, you can run acidflow in real-time and record its audio output with an external tool:

1. Start your system audio recording tool. Point it at acidflow's output.
2. In acidflow, load slot 2. Press Space.
3. After 8 bars (about 15 seconds), load slot 1.
4. Over the next 8 bars, slowly raise CUTOFF from 25% to 70%. This is the build.
5. After another 8 bars, load slot 3. Raise REV MIX to 50%.
6. After 8 bars, load slot 1 again. Drop REV MIX back to 15%. Open CUTOFF to 80%.
7. After 16 more bars, press Space to stop.
8. Stop your audio recording.

You now have a 60-90-second recording of an arrangement with live knob motion.

## Step 13: Add FX

Listen to what you have. Does it need FX?

Common additions:
- **Delay**: 1/8 dotted, MIX 25%, FB 0.4. Fills space.
- **Reverb**: Size 0.4, MIX 15%. Adds depth.
- **Drive**: 10-20% on the bus. Glues everything.

Enable these during the recording or add them in a DAW afterward.

## Step 14: Mix and master

In a DAW:
- Balance levels. The 303 and drums should each be audible. Neither should overwhelm.
- EQ if needed. Cut low-end on non-bass elements. Cut high-end on elements that clash.
- Compress the whole track mildly to glue.
- Limiter at the end, brickwalling peaks at -0.3 dB.

acidflow alone can't do this — it's a synth, not a mastering tool. But you can export and process externally.

## Step 15: Title, save, share

- Title: something that fits the mood. Avoid "Acid Track 47" — give it a distinct name.
- Save your acidflow slots (they're already saved via `Shift+1` etc.).
- Export pattern as text (`p`) so you can share the pattern alongside the audio.
- Upload to SoundCloud, Bandcamp, wherever.

You have a released acid track.

## Common pitfalls in first tracks

### The "doesn't grab me" track

You're bored while making it. The track is competent but forgettable. Fix: add *one thing* that's distinctive. An unusual slide. A weird knob move. A specific drum pattern that's yours.

### The "over-engineered" track

You've spent hours tweaking and it still doesn't feel right. Fix: start over. Limit yourself to 30 minutes. Use the randomizer heavily. Accept imperfect.

### The "sounds like my references" track

You've successfully copied Phuture. It's not distinctive. Fix: what would YOU do that Phuture didn't? Break one convention deliberately.

### The "too many ideas" track

You've got 3 different 303 lines, 2 drum patterns, 5 FX moves, a breakdown every 16 bars. Chaos. Fix: cut 50% of what you have. What remains should still work.

### The "no progression" track

The track starts great but feels the same at minute 4. Fix: automate something slowly over the whole track. Filter opening, reverb rising, drums thinning. One long-duration change.

## The production checklist

Before calling a track "done," verify:

- [ ] The track has a clear intro.
- [ ] There's at least one build.
- [ ] There's at least one drop (or equivalent energy shift).
- [ ] There's at least one breakdown.
- [ ] The arrangement lasts long enough (3+ minutes for a proper track).
- [ ] The mix has level balance — nothing is clipping, nothing is inaudible.
- [ ] Something is distinctive — a specific choice that makes this YOUR track.
- [ ] You could listen to it 20 times without getting bored.

If all checkboxes pass, ship it.

## The speed vs quality tradeoff

First tracks should be fast, not perfect. Your tenth track will be way better than your first. Your hundredth will be way better than your tenth. The path to good is through making many tracks, not one perfect one.

Target: 1-2 tracks per week. Not polished masterpieces — finished pieces. Ship, learn, repeat.

Producers who polish each track forever never finish anything. Producers who ship often improve fast.

## Try this

1. Follow this chapter step by step. Make a track.
2. Don't worry if it's great. Just finish it.
3. Listen to it a day later. What would you do differently?
4. Make another track, applying those lessons.
5. Keep going. Track 5 will be meaningfully better than track 1.

Next: mixing and mastering acid.
