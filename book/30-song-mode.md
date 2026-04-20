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

- **`Shift+1` through `Shift+9`**: save the current pattern to slot 1-9. Includes 303 pattern, drum grid, BPM, length, swing, and p-locks — everything that makes this pattern distinct.
- **`1` through `9`**: load the pattern from slot 1-9.
- **`n`**: toggle song mode on/off.

When song mode is ON:
- The sequencer auto-advances through saved slots on every pattern wrap.
- The transport shows `♪ SONG N` where N is the current slot.
- You can still load slots manually (`1`-`9`); that re-anchors the chain.

When song mode is OFF:
- The current pattern loops forever (or until you load another).

## How the chain works

The chain rule is simple: on every pattern wrap (step 16 → step 1), jump to the next saved slot.

Examples:

- Saved slots: 1, 2, 3. Chain: 1 → 2 → 3 → 1 → 2 → 3 → ...
- Saved slots: 1, 3, 5. Chain: 1 → 3 → 5 → 1 → 3 → 5 → ... (skips empty slots)
- Saved slots: 1, 2. Chain: 1 → 2 → 1 → 2 → ...
- Saved slots: 1. Chain: 1 → 1 → 1 → ... (one slot, plays forever; harmless)

You don't explicitly write a chain. The chain IS the set of saved slots, in numerical order.

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

Each slot plays for one pattern cycle. At 120 BPM with a 16-step pattern in 4/4, that's 2 seconds per cycle, 10 seconds total for the 5-slot chain.

Too fast! Real arrangements need each section to last 8-32 bars. With song mode's one-cycle-per-slot rule, you need to duplicate slots to extend sections.

### The duplicate-to-extend technique

Since each slot plays for one cycle, and you want the main pattern to last 16 bars, you save it in multiple slots:

```
Slot 1: Intro (plays once = 2 seconds)
Slot 2: Main pattern copy 1 (plays once)
Slot 3: Main pattern copy 2 (identical to slot 2)
Slot 4: Main pattern copy 3 (identical to slot 2)
...
Slot 7: Breakdown
```

Slots 2-6 are the main pattern repeated. The chain naturally spends 5 cycles on the main pattern.

This is workable but wasteful. Only 9 slot slots; if you use 5 for the main pattern, you have 4 left for everything else.

### A better approach: longer patterns

Instead of a 1-bar pattern (16 steps), program a 4-bar pattern (use length = 64 steps... wait, acidflow has a max pattern length of 16).

Hmm. acidflow's pattern grid is fixed at 16 steps maximum. You can't program a 4-bar pattern in a single slot. So the duplicate-to-extend is your tool.

Think of it this way: each slot is a 1-bar pattern. An arrangement is a string of bars, each chosen from your saved slots. 9 slots, 9 different bar types.

For a typical 128-bar track:

- Pick 3-5 distinct patterns (saved to 3-5 slots).
- Arrange the chain so the total cycle length matches some phrase structure.
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

Slots don't save knob positions. This means:

- The 303's CUTOFF, RES, ENVMOD, DECAY, DRIVE, VOL, TUNE, WAVE are all current-session state.
- FX parameters (DLY MIX/FB/DIV, REV MIX/SIZE/DAMP, FX DRIVE) are current-session state.

So when song mode chains from slot 1 to slot 2, the notes change, but the knobs stay where you last put them.

This is intentional — it lets you perform knob moves across the chain. Turn the filter slowly upward while the chain cycles; the whole arrangement is bathed in that rising filter.

Downside: if slot 1 had a specific filter setting you want and you've since tweaked knobs, reloading slot 1 doesn't restore the filter. You have to manually set it back.

**Workflow**: save pattern slots early in the session. Later, focus on knob performance during playback. Don't tweak-and-save; save once, perform over it.

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
Slot N (main, full drums)
Slot N+1 (same 303 pattern, but drums cleared — 303 only)
```

Two adjacent slots. Slot N+1 has the same 303 line but all drums off. Transition is a sudden removal of beat.

For more drama: pair with a reverb sweep. In slot N+1, the reverb mix has been raised. The 303 swims in reverb during the breakdown. When the chain returns to slot N (drums back, reverb back to normal), the energy returns.

Except... reverb mix isn't per-slot. It's a knob you have to manually turn. So this is a performance move, not a chain automation.

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

1. **One cycle per slot**. No way to say "slot 1 for 4 cycles, then slot 2 for 8 cycles." You have to duplicate slots.
2. **No knob automation across slots**. Knob changes are live performance only.
3. **Linear chain only**. Can't have "slot 1, 2, 3, 2, 1" — the chain is always in increasing slot order.
4. **No FX presets per slot**. You can't save "slot 1 = dry, slot 2 = delayed." The FX knobs are session state.
5. **Max 9 slots**. Long arrangements need creativity.

For more complex arrangements, export WAVs of different slots and arrange in a DAW.

## Workarounds for the limitations

### Simulating longer slots

Duplicate: save slots 2 and 3 identically. The chain spends 2 cycles on "slot 2 material."

### Simulating non-linear chains

Save slot 1 with your "A section" material. Save slot 2 with "B section." Manually re-order by saving to different slot numbers.

E.g., you want chain A → B → A → C. Save A to slot 1. Save B to slot 2. Save A to slot 3 (same as slot 1). Save C to slot 4. Now the chain 1→2→3→4→1→... produces A → B → A → C → A → B → A → C.

### Simulating FX presets

Can't. This is truly a session-state limitation. You turn knobs during the chain playback — that's the performance.

## Try this

1. Program a basic pattern. Save to slot 1 (`Shift+1`).
2. Remove the 303 notes. Keep drums. Save to slot 2 (drums-only variant).
3. Remove drums, re-add 303. Save to slot 3 (303-only variant).
4. Press `n` to start song mode. Listen to the chain: full → drums-only → 303-only → full → ...
5. During playback, slowly raise CUTOFF. The whole chain is bathed in rising filter.
6. Press `n` to stop. Manually load slot 1. You're back to the main pattern.
7. Try a more extended arrangement: 5 slots for intro, main, build, breakdown, drop.
8. Export WAV (`e`) with the chain running. Listen to the full arrangement as a file.

Next: jam mode.
