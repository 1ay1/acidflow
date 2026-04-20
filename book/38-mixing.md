# 38 · Mixing and mastering acid

Once you have the musical content — 303 line, drums, arrangement — the track has to be *mixed* and *mastered*. This chapter is about what those words mean, what acidflow can and can't do, and how to get a finished sound.

## What mixing is

Mixing is balancing the elements of a track so they cohere. Specifically:

- **Level balance**: each element at an appropriate volume relative to the others.
- **Frequency balance**: each element occupies its own frequency range, not competing with others.
- **Stereo placement**: each element in an appropriate position in the stereo field.
- **Dynamic balance**: each element's dynamic range (loud-to-quiet) is appropriate.
- **Effects balance**: reverb/delay/drive applied appropriately, not overwhelming.

A well-mixed track sounds clear, present, and emotionally coherent. A poorly-mixed track sounds muddy, hollow, or harsh.

## What mastering is

Mastering happens after mixing. It's the final polishing of the full stereo mix for distribution:

- **Overall level**: the track is loud enough to be competitive with other tracks.
- **Tonal balance**: the overall EQ curve matches industry norms (or your aesthetic).
- **Limiting**: peaks are clipped smoothly so the average level can be higher.
- **Consistency**: the track sounds good on all playback systems (phones, headphones, club).

Mastering is usually done by a dedicated mastering engineer with specialized tools.

## What acidflow does for mixing

acidflow gives you:

1. **Per-voice gain (implicit)**: each drum voice has internal gain. You can't adjust per-voice; drums are baked to sit right.
2. **Drum bus gain**: `[` and `]` when drums focused. Adjust all drums together relative to 303.
3. **303 voice VOL knob**: adjusts 303 level.
4. **FX rack**: drive, delay, reverb apply to the mix.

That's it. No per-element EQ, no compression, no stereo width controls, no sidechain.

This is intentional. acidflow is a synth + drum machine + basic FX, not a mixing board. For more, you use a DAW.

## What acidflow doesn't do

- **EQ**: can't boost or cut specific frequencies per element.
- **Compression**: no dynamic range control.
- **Sidechain**: can't duck 303 under kick.
- **Per-element FX**: drive/delay/reverb are on the master bus only.
- **Stereo processing**: can't adjust stereo width or position per element.
- **Limiter**: no brickwall limiting on the master.

All of these are DAW territory.

## acidflow's built-in mix balance

Despite the minimal controls, acidflow is tuned to mix reasonably well out of the box:

- Kick is loud enough to punch without overwhelming.
- 303 sits in midrange.
- Hats are high enough to not clash with 303.
- Snare/clap on backbeat carry appropriate weight.

This "default mix" is good for rough drafts. It won't win mastering awards, but it lets you produce listenable tracks in acidflow alone.

## Mixing within acidflow

Given the minimal controls, here's how to mix:

### Level balance

1. Play the pattern.
2. Listen to the kick. Is it loud enough to feel? Lower drum bus if it's overwhelming, raise if it's weak. Typical: 0 dB (unity).
3. Listen to the 303. Does it compete with the kick? Raise/lower 303 VOL relative to drums.
4. Target: kick feels physical, 303 is clearly audible, neither dominates.

### Frequency balance

acidflow doesn't have EQ, but you can affect frequencies via:

- **CUTOFF** on 303: low cutoff = dark, midrange-only. High cutoff = bright, full spectrum.
- **RES** on 303: higher RES = more emphasis on the cutoff frequency, less on others.
- **TUNE** on 303: lower TUNE = 303 lives more in the low end; higher TUNE = mid-focus.

If the track is muddy (too much low-mid), try raising CUTOFF or TUNE. If it's harsh (too much high-mid), try lowering CUTOFF.

### Dynamic balance

Without compression, your dynamics are whatever the pattern produces. Accents produce louder notes; non-accents are quieter.

The FX DRIVE acts as a mild compression — soft-clipping peaks makes the track feel more "even."

Heavy accents without drive = wide dynamic range (loud-quiet-loud). Heavy accents WITH drive = narrower range (everything clipped toward average).

If your track feels too dynamic (listener adjusts volume constantly), add some FX drive.

### Effects balance

The FX rack is your space/depth tool. Too little = flat, clinical mix. Too much = washed, muddy mix.

Guidelines:
- FX DRIVE: 15-30% for most tracks.
- DLY MIX: 10-30%.
- DLY FB: 0.3-0.6.
- REV MIX: 10-25%.
- REV SIZE: 0.3-0.5 for tight rooms; 0.5-0.7 for dubby.

These are starting points. Adjust by ear.

## The mix-then-tweak loop

Real mixing is iterative:

1. Start with a rough balance.
2. Listen on headphones.
3. Adjust one thing.
4. Listen on speakers.
5. Adjust another thing.
6. Re-listen on headphones.
7. Repeat until it sounds right on both.

Every production system sounds different. Headphones reveal detail but may mislead about bass. Speakers show the full spectrum but your room affects what you hear. A good mix works on both.

## Mastering workflow (DAW)

Assuming you export a WAV from acidflow and finish in a DAW:

1. **Load the WAV into the DAW** (Ableton, Logic, Reaper, etc.).
2. **Apply EQ**:
   - Low-cut (high-pass) at 30 Hz to remove sub-sonic mud.
   - Optional cut at 200-300 Hz if the track is muddy.
   - Optional boost at 8-10 kHz for "air."
3. **Apply compression**:
   - Low ratio (2:1 or 3:1).
   - Slow attack (30-50 ms) so transients pass through.
   - Medium release (150-200 ms).
   - Threshold so ~3 dB of reduction happens on peaks.
4. **Apply limiting**:
   - Brickwall at -0.3 dB ceiling.
   - Enough gain reduction to bring the average level up, but not pumping.
   - Target integrated loudness: ~-8 to -14 LUFS depending on aesthetic. Clubs expect louder; streaming services normalize so dynamic range is fine.
5. **Export as WAV or AIFF at the master level.**
6. **Encode to MP3/AAC** for distribution.

That's a basic mastering chain. Professional mastering is more nuanced but this gets you 80% there.

## EQ tips for acid

Frequency ranges and what lives there:

- **0-50 Hz**: sub-bass. Only the kick should live here. Cut everything else below 50 Hz.
- **50-150 Hz**: bass. Kick body and 303's fundamental. Compete carefully.
- **150-400 Hz**: low mids. 303's body. Often muddy if too boosted.
- **400-1000 Hz**: midrange. 303's character, snare body.
- **1-4 kHz**: upper mids. Clap, snare snap, 303 harmonics. Important for presence.
- **4-10 kHz**: highs. Hats, reverb shimmer. Important for air.
- **10-20 kHz**: air. Reverb tails, noise. Adds "expensive" feel.

In a DAW, you'd EQ each element:
- Kick: boost around 60-80 Hz (for thump). Cut around 300 Hz (mud). Maybe slight boost at 3 kHz (click).
- 303: depends on its character. Clean ones can cut at 200-300 Hz to avoid mud.
- Snare/clap: boost around 2-3 kHz for snap. Cut around 400 Hz.
- Hats: high-pass at 400 Hz. Let highs do their thing.

acidflow doesn't have EQ. But if you export each element separately (by muting others), you can EQ each in the DAW.

## The export-stems workflow

For advanced mixing:

1. In acidflow, mute everything except 303. Export WAV ("303_only.wav").
2. Mute 303, unmute drums. Export WAV ("drums_only.wav").
3. In a DAW, load both stems. Now you can mix them separately.

Add whatever processing you want: EQ per stem, compression per stem, stereo widening per stem. Mix the two stems together.

This loses some cohesion (the FX rack applied to both at once, baked-in; you can't remove it in the stems). But it gives you per-element control.

Better: export 303_only with no FX (turn FX off). Drums_only with no FX. Add FX in the DAW per stem. More flexible.

## Reference tracks

Always mix against a *reference* — a commercial track in the same genre that sounds the way you want yours to sound.

- In a DAW, load the reference alongside your mix.
- A/B between them. Does your track have less bass? More mid? Louder/quieter?
- Adjust your mix to match the reference's general balance.

This is how professionals mix. Every pro has a curated reference library.

For acid: pick tracks from Hardfloor, Josh Wink, Plastikman, Phuture, etc. — whichever genre you're in.

## LUFS and loudness

Modern distribution (Spotify, Apple Music, YouTube) normalizes to ~-14 LUFS. If your master is louder than that, they'll turn it down. If quieter, they'll turn it up (to a limit).

So making your track loud doesn't help on streaming — it'll be turned down to match everyone else.

For clubs: louder matters. A -8 LUFS club mix will play at club level and sound appropriately competitive. A -14 LUFS mix will sound quiet in comparison.

Know your distribution target. Master for clubs if that's your audience. Master for streaming if that's where listeners are.

## Dynamic range

Related to loudness: dynamic range is the difference between peak and average level.

- Low dynamic range (pumping, wall-of-sound): 3-6 dB difference between peak and average. Loud but fatiguing.
- Medium dynamic range: 8-12 dB. Competitive but musical.
- High dynamic range: 15+ dB. Classical, jazz. Not suitable for clubs.

Acid tracks typically sit in 8-14 dB dynamic range. Not as compressed as modern mainstream pop, but more than classical.

The FX drive in acidflow compresses dynamics slightly. In the DAW, a limiter compresses further.

## The final checklist

Before releasing a track, verify:

- [ ] Mix sounds good on headphones.
- [ ] Mix sounds good on speakers.
- [ ] Mix sounds good on a phone speaker (test on a phone — many listeners use them).
- [ ] Mix sounds good in a car (test if possible).
- [ ] Peak levels don't clip (stay under 0 dBFS).
- [ ] Average loudness is in target range (-8 to -14 LUFS).
- [ ] Low-end is controlled (no rumbling sub-bass that phones can't reproduce).
- [ ] High-end isn't harsh (no piercing frequencies).
- [ ] Stereo image is coherent (not too wide, not too narrow).
- [ ] The energy matches the emotional intent.

If all passes, release.

## Don't master in acidflow

acidflow isn't a mastering tool. Don't try to make the final, loud, polished master within acidflow.

acidflow gets you to the "rough mix" stage. The final master is a DAW + mastering plugin task.

If you don't have a DAW: use Audacity (free) with LAME and a limiter plugin. Minimum viable mastering.

Or: upload your acidflow WAV to a service that masters automatically (LANDR, etc.). AI mastering isn't great but it's often better than unmastered.

Or: release unmastered. For some purposes (SoundCloud demos, etc.), mastered vs unmastered doesn't matter.

## Try this

1. Export a WAV from acidflow (`e` key).
2. Load it into any DAW (Audacity works).
3. Apply a high-pass filter at 30 Hz.
4. Apply a limiter with ceiling at -0.3 dB.
5. Apply a gain adjustment to hit ~-9 to -12 LUFS (use a LUFS meter plugin).
6. Export the mastered WAV.
7. A/B with the original. The mastered should sound louder, more even, more "released."

You've done a basic mastering. It's not perfect but it's releasable.

Part VII is complete. Five chapters on making records:

- Chapter 34: Arrangement structures.
- Chapter 35: Subgenres.
- Chapter 36: The canon annotated.
- Chapter 37: Producing from scratch.
- Chapter 38: Mixing and mastering.

You now have the full production workflow. Next: Part VIII — Reference.
