# 30 · Song mode: arrangement without a DAW

A 16-step pattern is a loop. Two minutes of a loop is trance-inducing. Ten minutes of an identical loop is tedious. Real tracks have *structure* — verse, breakdown, build, drop, outro. To go from loop to track, you need a way to chain variations and arrange them in time.

In a DAW, you'd have a timeline, clips, automation lanes. In acidflow, you have **song mode** and **nine save slots**. This chapter is about how to use those to build full arrangements.

## The concept

Song mode is the hardware-sequencer answer to arrangement. The TR-808 had it. The TR-909 had it. The MC-202 had it. The idea:

1. Save multiple patterns in memory (slots).
2. Tell the sequencer to chain through them in some order.
3. When one pattern ends, the next begins automatically.

acidflow follows this tradition. You save up to 9 pattern slots. Enable song mode. The sequencer cycles through your saved slots in order.

That's it. No timeline. No clips. No visual arrangement. Just "what's saved? play them in order."

## The interface

- **`Shift+1` through `Shift+9`**: save the current pattern to slot 1-9. Includes 303 pattern, drum grid, BPM, length, swing, p-locks, all 303 knobs (tune/cutoff/res/envmod/decay/accent/drive/vol/wave), and all FX knobs (overdrive, delay mix/fb/div, reverb mix/size/damp). A slot is a full snapshot of the voice, not just the notes.
- **`1` through `9`**: load the pattern from slot 1-9.
- **`n`**: toggle song mode on/off.
- **`(` and `)`**: set how many bars the *currently-focused* song slot holds before the chain advances. Cycles 1 / 2 / 4 / 8 / 16 / 32 / 64. Default is 4 bars.
- **`Shift+A`**: one-shot auto-arrange. Picks a vibe, generates all 9 slots as a coherent 10-minute song, enables song mode, starts playback. See the auto-arrange section below.
- **`Y`**: open the take picker modal. Lists every `live_<timestamp>.txt` recording in your config dir, newest first; ↑/↓ to navigate, Enter to load, Esc to cancel. Useful for recalling a take you liked without hand-copying files.

When song mode is ON:
- The sequencer holds the current slot for its configured bar count (default 4), then jumps to the next saved slot on the following wrap.
- The transport shows `♪ SONG N` where N is the current slot, plus the bars-left countdown.
- You can still load slots manually (`1`-`9`); that re-anchors the chain.

When song mode is OFF:
- The current pattern loops forever (or until you load another).

## How the chain works

The chain rule: each slot holds for its configured bar count (set per-slot with `(` / `)`), then on the next wrap the chain jumps to the following saved slot.

Examples (all with the default 4-bar hold):

- Saved slots: 1, 2, 3. Chain: 1 (4 bars) → 2 (4) → 3 (4) → 1 → 2 → 3 → ...
- Saved slots: 1, 3, 5. Chain: 1 → 3 → 5 → 1 → ... (skips empty slots)
- Saved slots: 1. Chain: 1 → 1 → ... (one slot, plays forever; harmless)

Per-slot bars let you sculpt a real arrangement instead of flash-cutting every bar. A typical build:

- Slot 1 (intro): 16 bars
- Slot 2 (main): 32 bars
- Slot 3 (breakdown): 8 bars
- Slot 4 (drop): 32 bars

To set a hold: focus the Transport panel, make the slot you want to edit the current one (load it), then press `)` until the bar count reads what you want. The count sticks for the current session.

You don't explicitly write a chain. The chain IS the set of saved slots, in numerical order, each playing for its bar count.

## Shift+A: one-key 10-minute song

Before hand-arranging anything, try `Shift+A`. It writes a complete 9-slot arrangement into slots 1-9, enables song mode, and starts playing. From cold boot to a full track in one keystroke.

What it actually does:

1. **Picks a vibe.** One of several genre archetypes (acid house, techno, ambient, etc.) — each owns a BPM range, swing feel, pattern archetype, drum archetype, knob/FX palette.
2. **Generates a spine.** Under that vibe, rolls one coherent base pattern + drum kit + knob patch + FX chain. This "spine" is what every section mutates from, so the whole 10 minutes sounds like one song rather than 9 random rolls.
3. **Lays out 9 sections with bar counts:**
   - Slot 1 — **Intro** (32 bars): kick + closed hats only, filter closed, low drive, a touch of reverb
   - Slot 2 — **Bass in** (32): bass on downbeats only, filter opening, kick/snare/hat/clap
   - Slot 3 — **Build** (32): bass mutates, resonance rising, envmod up, delay creeping in
   - Slot 4 — **Peak A** (64): full mix, bright, driven — sits in the ear long enough to feel locked
   - Slot 5 — **Variation** (32): peak energy but the bass has been mutated for motion
   - Slot 6 — **Breakdown** (16): drums drop out, reverb swells, 303 alone
   - Slot 7 — **Drop** (64): peak returns, slightly denser, biggest section
   - Slot 8 — **Peak B** (32): last full-intensity pass before the door closes
   - Slot 9 — **Outro** (16): strip back to kick + ambience
4. **Saves all 9 slots** (the pre-existing slot files are preserved as `.bak` — one deep, so a second auto-arrange does overwrite the previous backup).
5. **Loads slot 1, enables song mode, starts the transport.** You hear the intro immediately.

Total: 320 bars. At ~122 BPM, that's about 10½ minutes. The transport toast reports the exact predicted duration.

### What to do while it plays

`Shift+A` is not a render button — it's a starting point. The auto-arranger writes the slots; the *performance* is still yours. Once it's playing:

- **Record a take.** Press `W` to start live recording. The take runs alongside playback as both a WAV (audio) and a `.txt` (full slot state at the moment recording stopped). The toast right after `Shift+A` literally reminds you: "~10 min · W to record."
- **Tweak knobs across the arrangement.** CUTOFF sweeps, FX feed-ins, drum-bus level dips — all live. Because slots now save their own knob/FX values, the chain will snap back to each section's programmed sound on the next wrap; live tweaks between slot boundaries are pure performance.
- **Load the take later.** Press `Y` to open the take picker and recall any previous live_<timestamp>.txt into the live pattern state. Handy for comparing against fresh rolls.
- **Discard and re-roll.** Press `Shift+A` again. A new vibe, a new spine. Old slots get rotated to `.bak`. Keep rolling until something clicks.
- **Edit in place.** Love the arrangement but hate slot 6? Load slot 6 (`6`), re-work it, save back (`Shift+6`). The auto-arranger's plan is a scaffold, not a lock.

### When Shift+A shines

- You want to hear your vibe choices (tempo, genre) expressed as a full track without programming one.
- You need a 10-minute bed for DJ practice, jam backing, or just to have something running while working.
- You're stuck on what comes after the main section and want a reference you can learn from.
- You're recording takes and want volume — one keystroke, one take, repeat.

### When to build it by hand instead

- You have a specific pattern you want to arrange around. Auto-arrange always starts from a fresh random roll.
- You want odd section lengths, non-standard transitions, or more than 9 sections in the cycle.
- You want tight control over which elements change where — auto-arrange decides what mutates between sections.

The rest of this chapter covers hand-arrangement techniques. Both workflows use the same slots and the same song mode; `Shift+A` is just a pre-filled template.

## Designing an arrangement with song mode

Because the chain is just "all saved slots in order," you arrange by choosing:

1. **How many slots to use**. More slots = more variations per cycle.
2. **How different each slot is from the previous**. Small changes = smooth progression. Big changes = dramatic transitions.
3. **The total chain length in bars**. If each slot is one 4-bar pattern, a 4-slot chain is 16 bars. If each slot is a 16-step pattern (1 bar), a 4-slot chain is 4 bars.

### A classic arrangement

```
Slot 1: Intro — sparse 303, kick only
Slot 2: Build — more 303 notes, add hats
Slot 3: Main — full pattern, all drums, some delay
Slot 4: Breakdown — 303 only, big reverb
Slot 5: Drop back to main — same as slot 3 but maybe with extra drum fill
```

With this saved, pressing `n` starts playing: slot 1, then 2, then 3, then 4, then 5, then back to 1.

Each slot holds for its configured bar count. At 120 BPM with a 16-step pattern in 4/4, one bar is 2 seconds. The default 4-bar hold is 8 seconds per slot, so a 5-slot chain runs 40 seconds before repeating.

Real dance-music sections want 16-64 bars. Set per-slot holds with `(` / `)` to match the section role:

```
Slot 1: Intro           — 16 bars
Slot 2: Build           — 32 bars
Slot 3: Main            — 32 bars
Slot 4: Breakdown       — 16 bars
Slot 5: Main (reprise)  — 32 bars
```

Chain total: 128 bars, roughly 4 minutes at 120 BPM. Now each section sits long enough to read as a section rather than a flash-cut.

### The old duplicate-to-extend technique (when per-slot bars don't fit)

Before per-slot bars existed, the only way to extend a section was to copy the same pattern into several adjacent slots so the chain spent multiple cycles on it. Per-slot bars replace this — set the hold instead of burning slots.

There's still a use for duplicating, though: **mid-section evolution**. Save slot 2 and slot 3 with *almost* identical patterns (one-hat difference, a mutated note, a nudged knob) and give both 16-bar holds. The chain spends 32 bars on "the same thing, but evolving." Listener hears continuity with motion.

For a typical 128-bar track:

- Pick 3-5 distinct sections (saved to 3-5 slots).
- Set each slot's bar count to the section's natural length.
- Start song mode on the right slot.

### The one-change-per-section technique

A subtle arrangement technique: keep most of the pattern the same across slots, change ONE thing per transition.

```
Slot 1: Pattern P, all voices, no delay
Slot 2: Pattern P, all voices, +20% delay (slight change)
Slot 3: Pattern P2 (slightly different 303 pattern), +20% delay (bigger change)
Slot 4: Pattern P2, no kick (breakdown — loses anchor)
Slot 5: Pattern P2, kick returns (drop)
Slot 6: Pattern P, no delay (recap of slot 1)
```

Each transition changes one element. The listener's ear notices each change distinctly. The cumulative arrangement feels like it's evolving.

This is how dub techno and minimal techno producers work — tiny changes, infinite patience.

### The 4-bar technique

Classic acid arrangement is built on 4-bar phrases. Every 4 bars something happens: a drum fill, a filter sweep, a knob twist.

With song mode, you'd structure:

```
Slot 1: Bar 1 of phrase (main)
Slot 2: Bar 2 of phrase (main)
Slot 3: Bar 3 of phrase (main, maybe small change)
Slot 4: Bar 4 of phrase (bar-end fill: drum roll, accent shifts)
```

Save all four slots. Song mode chains 1→2→3→4→1→2→3→4... which is exactly a 4-bar phrase repeating.

Now slot 4 has the fill. Every 4 bars, the fill happens. Natural phrase structure.

### Long-form with just 3 slots

If you only want to save a few slots (leaving room for other patterns in the session), a simple 3-slot arrangement works:

```
Slot 1: Main (most of the track)
Slot 2: Main variant (slightly denser or with some change)
Slot 3: Breakdown (303 alone, minimal drums)
```

Chain: 1 → 2 → 3 → 1 → 2 → 3 → ...

Each slot is 1 bar. The 3-slot chain is 3 bars. After 9 cycles, you've played 27 bars, roughly 54 seconds. Good for a short arrangement or a live jam.

For a longer full track, you need to re-arrange during the set — save new variations, toggle song mode on/off, load specific slots manually.

## Song mode and knob moves

Slots save their knob positions. The full list: 303 TUNE, CUTOFF, RES, ENVMOD, DECAY, ACCENT, DRIVE, VOL, WAVE; FX OVERDRIVE, DLY MIX/FB/DIV, REV MIX/SIZE/DAMP. When the chain advances from slot 1 to slot 2, the notes change *and* every saved knob snaps to its slot-2 value at the wrap boundary.

This changes how you arrange:

- **Section dynamics are programmable.** Slot 3 can be "bright + driven" and slot 6 "dark + reverb-heavy" just by saving them with those knob positions. The chain feels dynamic without any live knob-twisting.
- **Live tweaks still work.** Knobs you turn *between* wraps apply immediately. But on the next slot change, the new slot's saved knobs take over. Think of live tweaks as "per-section decoration" — they last until the next boundary.
- **P-locks still override.** Per-step p-locks (on cutoff/res/envmod/accent) still trump the slot knob values on the steps where they're set. Slot knobs are the baseline; p-locks are the per-step paint.

**Workflow**: design each slot as a complete sonic snapshot — notes, drums, knobs, FX. Save. Let song mode do the heavy lifting; live knob moves become garnish rather than structural. If you want a slow filter sweep across multiple sections, either set each slot's CUTOFF to the right point along the arc, or override with live performance while the chain runs.

## The drop

The most important arrangement moment in dance music is *the drop*. The drum drops in, the energy spikes, the crowd reacts.

With song mode, the drop is a transition from a low-energy slot to a high-energy slot:

```
Slot 1: No kick, sparse 303, heavy reverb (tension-building)
Slot 2: Full kick, dense 303, less reverb (main drop)
```

At the transition from 1 to 2, the kick snaps in. That's the drop.

For drama: make the preceding slot have VERY different dynamics from the drop slot. If the intro is quiet, make the drop loud. If the intro is dense, make the drop sparse (dropping OUT is also a drop — a "minimal" drop).

## The breakdown

Opposite of the drop: drums disappear, leaving the 303 alone. acidflow's simplest breakdown:

```
Slot N (main, full drums, low reverb)
Slot N+1 (same 303 pattern, drums cleared, reverb raised — 303 swims)
```

Two adjacent slots. Slot N+1 has the same 303 line but all drums off, and because slot FX knobs are saved, its reverb mix can sit higher than slot N's. The transition is a drum drop-out AND a wet room opening up. When the chain returns to slot N, drums punch back and the room closes around them. No live performance required — it's all in the slots.

For extra drama, give slot N+1 a 16-bar hold so the breakdown breathes before the drop.

## The intro

Intros are slots with less than the full pattern. Common intro techniques:

- **Kick only**: slot 1 has only kick hits. Slowly builds anticipation.
- **Hats only**: slot 1 has only closed hats. A ticking clock.
- **303 only**: slot 1 has 303 but no drums. Hypnotic.
- **Fade in**: can't directly do this with slots (no volume automation) — manually fade with VOL.

An intro slot plays for ONE pattern cycle before the chain moves on. If you want a 4-bar intro, duplicate the intro across 4 slots.

## The outro

Symmetrical to the intro. A slot (or slots) with reduced elements. Kick-only, 303-only, or progressive stripping.

A subtle outro: the same pattern as the main, but with several drums removed. The listener recognizes it as "the track thinning out."

## Song mode and live performance

In a live set, song mode can be your autopilot:

1. Pre-save 5-9 slots for each track you plan to play.
2. Load slot 1, tweak knobs to taste.
3. Press `n` — song mode starts. Chain plays automatically.
4. Meanwhile, you tweak CUTOFF, RES, FX knobs, do drum mutes, etc.
5. When the track is done, press `n` again (stop song mode), load slot 1 of the next track, and continue.

This is the TB-303 + TR-909 live workflow from 1988 brought to acidflow.

## Song mode and studio composition

In the studio (writing, not performing), song mode is your way of previewing the arrangement you have so far. Save a few slots, enable song mode, listen. Does the flow work?

If not, tweak: re-save slots, re-order by saving to different numbers (slot 3 plays before slot 4, so swap what's in each).

It's not a DAW — you can't drag-and-drop regions. It's a chain: slot 1, slot 2, slot 3 in numerical order. But for simple arrangements (which most acid tracks have — acid is not compositionally complex), it's enough.

## Bouncing to WAV

When you've got your song mode arrangement working, export to WAV (`e` key). The WAV export records the current session's playback, including song mode chaining.

This is how you go from "acidflow is playing my chain live" to "here's a final WAV file." Record for however long you want, then stop.

The resulting WAV captures everything — the chain, any knob moves you made during the record, drum mutes, FX changes. It's a performance, not a render. If you made a mistake, re-record.

## Song mode limitations

Being honest:

1. **Linear chain only.** Can't have "slot 1, 2, 3, 2, 1" — the chain is always in ascending slot order, wrapping back to the first saved slot.
2. **Max 9 slots.** Long arrangements need creativity (or re-saving slots mid-performance to re-use numbers).
3. **Per-slot bars are session-only.** The bar-count setting isn't persisted to the slot file, so if you restart, every slot resets to a 4-bar hold. (Pattern content, drums, knobs, FX *are* persisted.)
4. **Pattern grid is fixed at 16 steps max.** You can't program a 4-bar-long line in a single slot — sections longer than a bar come from the bar-count hold, not from longer step counts.
5. **No step-by-step automation between slots.** Slot knob values take effect at the wrap boundary, not gradually across it. If you want a smooth sweep across a section boundary, perform it live.

For arrangements that outgrow these (non-linear chains, more than 9 sections, cross-section automation), use `Shift+A` or hand-arrange a take, record it with `W`, and finish the edit in a DAW.

## Workarounds for the limitations

### Non-linear chains

Save slot 1 with your "A section" material. Save slot 2 with "B section." Manually re-order by saving to different slot numbers.

E.g., you want chain A → B → A → C. Save A to slot 1. Save B to slot 2. Save A to slot 3 (same as slot 1). Save C to slot 4. Now the chain 1→2→3→4→1→... produces A → B → A → C → A → B → A → C.

### Mid-section evolution without burning slots

If you'd rather not clone slots just to get motion inside a section, give one slot a long bar hold (say 32) and perform live knob moves or drum mutes during its run. The slot's saved knobs set the baseline; your hands do the rest.

## Try this

### The hand-arranged path

1. Program a basic pattern. Save to slot 1 (`Shift+1`).
2. Remove the 303 notes. Keep drums. Save to slot 2 (drums-only variant).
3. Remove drums, re-add 303. Save to slot 3 (303-only variant).
4. Load slot 1 (`1`). Press `)` until the transport shows 16 bars. Save again (`Shift+1`) if you want — the bar count itself is session-only but the pattern/knobs save.
5. Do the same for slot 2 (8 bars, say) and slot 3 (16 bars).
6. Press `n` to start song mode. Listen to the chain: full for 16 bars → drums-only for 8 → 303-only for 16 → back to full.
7. During playback, slowly raise CUTOFF on the main section and watch what happens when the chain wraps: the next slot snaps to its own saved CUTOFF.
8. Press `W` to start a live recording while the chain runs; press `W` again to stop. You get a `live_<timestamp>.wav` + `.txt` in your config dir.
9. Press `Y` later to re-open that take via the picker — ↑/↓ to browse, Enter to load.

### The one-key path

1. Cold boot acidflow.
2. Press `Shift+A`. Read the toast — "arrangement: <vibe> · ~10 min · W to record."
3. Listen. The chain auto-runs; every wrap boundary changes notes, drums, knobs, FX.
4. Press `W`. Let it record for as long as you want a take — 30 seconds, 5 minutes, the full 10.
5. Press `W` again. A take is now on disk.
6. Hate the roll? `Shift+A` again. New vibe. Prior slots moved to `.bak`.
7. Love section 3 of a previous roll but want to try a new arrangement on top of it? Load the take (`Y`) to recover that state, then save it to whichever slot you want to keep (`Shift+<n>`), then `Shift+A` to re-roll the others — but note that `Shift+A` writes all 9 slots, so save your keeper to an external `pattern.txt` first with `p`.

Next: jam mode.
