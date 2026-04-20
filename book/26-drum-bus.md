# 26 · The drum bus and sends

The five drum voices don't each go to the output individually — they all sum into a single **drum bus** before reaching the FX rack and master output. This chapter is about the drum bus: what it does, how to use it, and why the bus architecture matters for acid.

## What a "bus" is

In audio mixing terminology, a **bus** is an aggregate output that combines multiple source signals. All five drum voices feed into the drum bus; the drum bus's output is a single summed stereo signal that then gets processed and routed further.

Think of the drum bus like a subgroup fader on a mixing console. You can adjust the whole drum section's level with one control, rather than adjusting each voice individually.

## Why a drum bus

A few reasons:

1. **Level balance**. One knob adjusts all drums relative to the 303, which is critical for mix balance.
2. **Unified FX**. When you apply delay or reverb to the drum bus, it applies to all drums at once, maintaining their internal balance.
3. **Bus saturation / compression**. Though acidflow doesn't have bus compression, it's a common technique in production — compressing the drum bus "glues" the drums together as a cohesive element.

## acidflow's drum bus architecture

```
BD ──┐
SD ──┤
CH ──┼──→ Sum ──→ Drum bus gain ──→ To FX rack ──→ Master output
OH ──┤                                    ↑
CL ──┘                             (shared with 303)
```

All drums sum. Drum bus gain applies. The bus output goes into the same FX rack as the 303 voice — they share the FX chain.

## Controls

The drum bus has one user-accessible control:

- **Drum bus master gain**. Adjustable via `[` and `]` when the drums section is focused. Affects the entire drum sum.

Default is 0 dB (unity). Raising above unity can push the drums toward clipping; lowering pulls them back in the mix relative to the 303.

There's also:

- **Drum bus mute**. Pressing `m` when drums are focused mutes the entire drum bus while leaving the 303 running. Useful for A/B testing ("does the 303 work without drums?").

## Balancing the mix

The single most common mixing task is balancing drum bus against 303 voice:

- **If drums feel too dominant**: lower drum bus gain. Or raise VOL on the 303.
- **If drums feel weak**: raise drum bus gain. Or lower VOL on the 303.
- **If the mix feels muddy**: likely the kick and 303 are competing in the low end. Lower drum bus or change 303 pitch/cutoff.

A typical starting balance:

- 303 voice VOL: 75%
- Drum bus gain: 0 dB (unity)
- Master output should peak around -3 dB

Adjust from there by ear.

## FX on the drum bus

The FX rack (drive, delay, reverb) processes the summed drum-bus-plus-303 signal. You can't apply FX to drums alone — they share the same chain as the 303.

This has implications:

- **Delay on drums**. If DLY MIX is up, the delay applies to both drums and 303. Drums can sound weird with delay — kicks especially get duplicated and muddy. Usually you want delay on the 303, not drums.
- **Reverb on drums**. Same issue. Reverb on drums adds wash; reverb on 303 adds space. Shared FX means you have to choose.
- **FX drive on drums**. Drive makes the drums crunch. This CAN be desirable for aggressive acid techno, but usually overwhelms the mix.

### The compromise

Since FX is shared, acidflow is not ideal for dry-drums-wet-synth mixes. You have three options:

1. **Keep FX minimal**: use only a touch of delay/reverb so drum splatter is acceptable.
2. **Accept the wet drums**: make it a stylistic choice. Wet acid techno can sound great.
3. **Export separately**: export drums and 303 to separate WAV tracks (via WAV export done twice, once with 303 muted, once with drums muted), then mix in a DAW.

Most producers use option 1. A little FX, mostly on the 303, drums ride along with it.

## The kick as the anchor

When you adjust drum bus gain, remember: the kick is the loudest drum voice (typically). Adjusting drum bus gain primarily shifts the kick's level relative to everything else.

If you want specifically to make the kick louder without making the hats louder, you can't directly — acidflow doesn't have per-voice gain on the user knob panel. The voice gains are internally set to produce a balanced default; to change one voice's gain, you'd modify the code.

Practical implication: acidflow is opinionated about drum mix. You get bus-level control, not voice-level control. This simplifies the UI but reduces flexibility.

## Working around the shared FX

If you really want dry drums and wet 303, the workflow is:

1. Set up the pattern with drums AND 303 in acidflow.
2. Set FX to your taste for the 303 (delay, reverb).
3. Export WAV (`e`).
4. Now mute the drums (drums section, `m`).
5. Set FX to zero.
6. Export WAV again.

Combine in a DAW. The first WAV has drums with FX. The second has only the 303 with FX. You can then mix them separately.

This is hacky but works.

## Bus compression (simulated)

acidflow doesn't have bus compression. But you can simulate its effect via drive on the drums — crank the FX DRIVE while drums are playing, and the saturation will glue everything together in a quasi-compression way.

It's not real compression — drive adds harmonics; compression tightens dynamics. But the audible effect of "everything sounds more together" is similar.

If you need real compression, that's a DAW task. acidflow is a synth + drum machine + basic FX, not a full mixer.

## Drum bus and song mode

When you chain patterns in song mode (Chapter 30), the drum bus gain persists across patterns — unless you've saved it differently in each slot.

This is a feature: you can chain a pattern with full drums and a pattern with ducked drums for dynamic sections. To do this, save two slots:

- Slot 1: verse. Drum bus at 0 dB.
- Slot 2: breakdown. Drum bus at -10 dB (drums pulled way back).

Chain them: slot 1 for 16 bars, slot 2 for 8 bars, slot 1 for 16 bars. The drums swell and reduce at the breakdown.

## The drum bus in visualization

acidflow's scope and filter heatmap show the final master output — 303 + drums + FX. The drum bus isn't separately visualized.

If you want to see only the drums, mute the 303 (Knobs section, `m`). The scope will then show the drum bus in isolation.

## Thinking about the drum bus as a "track"

In a DAW metaphor, the drum bus is a stereo audio track. The individual voices are the "clips" on that track. You've bounced them down to one stereo signal.

This mental model helps when you're balancing and applying FX: the drum bus is one element in the mix. The 303 is another element. These two elements plus the FX rack produce the final master.

## Try this

1. Set up a drums-only pattern (mute the 303 via Knobs section `m`).
2. Listen to the drum bus alone. This is the backbone.
3. Unmute 303. Now you hear both. The drum bus is still the same; you've added the 303 on top.
4. Adjust drum bus gain via `[` `]`. Listen to the balance change.
5. Add some delay (DLY MIX up to 30%). Hear it on both drums and 303.
6. Reduce delay. Raise reverb (REV MIX to 30%). Same thing.

The drum bus is a simple piece of architecture but it determines how drums sit in the mix. Master it, and your tracks will have better balance.

Part IV is done. Five chapters on drums and rhythm:

- Chapter 22: overview of the 5 voices.
- Chapter 23: the kick.
- Chapter 24: snare, hats, clap.
- Chapter 25: programming principles.
- Chapter 26: the drum bus.

You now understand everything acidflow's drums can do. Next part: the FX rack.
