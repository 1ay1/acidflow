# 32 · MIDI I/O

MIDI — the Musical Instrument Digital Interface — has been the way electronic instruments talk to each other since 1983. If you want acidflow to play nice with the rest of your studio, MIDI is how.

This chapter covers MIDI conceptually, acidflow's specific MIDI implementation, and common setups that integrate acidflow with DAWs, hardware synths, drum machines, and controllers.

## What MIDI is

MIDI is a protocol — a standardized way of sending musical events as binary messages. Events include:

- **Note on/off**: a key is pressed or released, with a pitch and velocity.
- **Control change (CC)**: a knob or slider moved, with a CC number and value.
- **Clock**: timing pulses. 24 per quarter note.
- **Start/Stop/Continue**: transport messages.
- **Program change**: switch to a different preset.

Every MIDI-capable device can send and/or receive these messages. A MIDI keyboard sends note on/offs when you press keys. A synth receives note on/offs and plays the notes. A drum machine can both send (on its outputs) and receive (to be played by other devices).

MIDI is a serial protocol — events are timestamped and sent in order. It's musical, not audio: you're not sending the sound, you're sending the instruction to make the sound.

## Why MIDI matters for acid

acidflow is a self-contained instrument. You can make a full track without MIDI. But MIDI opens up:

- **Playing from a real keyboard** instead of the computer keys.
- **Syncing** with a DAW or other hardware. Drums in one app, bass in acidflow, all locked to the same tempo.
- **Recording to a DAW** to capture the 303 part, then mixing with other instruments.
- **Driving other synths** — use acidflow's sequencer to send notes to external hardware (a Moog, an analog bass synth, anything MIDI).
- **Receiving from a DAW's sequencer** — let Ableton, Logic, or FL drive acidflow while you just use acidflow as a sound source.

## Platform caveat

MIDI in acidflow is currently **Linux-only** (ALSA). On macOS and Windows, the MIDI keys still work in the UI, but the backend is a stub — nothing actually gets sent or received. The ALSA backend creates one sequencer client with an input and output port.

To connect: use `aconnect` from the command line, or configure your DAW to use the acidflow ALSA port.

If you're on macOS or Windows, skip this chapter or treat it as reference for when the platform support arrives.

## The two toggles

- **`Shift+O`**: MIDI out. Sends notes, drums, and clock to external gear.
- **`Shift+I`**: MIDI sync in. Slaves acidflow's tempo and transport to external clock.

These are independent. You can enable one without the other. Typical combinations:

- **Neither on**: acidflow is isolated. No MIDI activity.
- **MIDI out only**: acidflow drives other gear. The bass voice plays an external synth; the drums trigger an external drum machine.
- **MIDI sync in only**: acidflow's tempo follows an external master. Note data stays internal.
- **Both on**: acidflow is fully networked. Synced to external master, broadcasting its notes.

MIDI note-in (external → acidflow bass voice) is **always on** when the backend is up. You don't need to toggle anything — just connect a MIDI keyboard and play.

## What MIDI out sends

When `Shift+O` is enabled:

### Bass voice

Each 303 note produces:
- A **note-on** with velocity 112 (for accent) or 80 (non-accent).
- A **note-off** at the step's end.
- For slides: the next note overlaps slightly with the current one. DAWs interpret the overlap as legato / portamento and handle it appropriately (glide to next note).

Velocity carries accent. If your external synth has velocity-sensitive filter or amp (most do), the accent translates naturally.

### Drums

Each drum hit produces:
- A **note-on on channel 10** (standard MIDI drum channel).
- General MIDI kit mapping:
  - BD (kick) = MIDI note 36
  - SD (snare) = 38
  - CH (closed hat) = 42
  - OH (open hat) = 46
  - CL (clap) = 39

These map to a GM-compatible drum kit on any MIDI drum module or DAW drum plugin.

### Clock

- **24 PPQN clock ticks** at the current BPM.
- **Start** when you press `Space` (play).
- **Stop** when you pause.

This lets external gear sync to acidflow's tempo without you having to manually set the BPM on each device.

## What MIDI sync in receives

When `Shift+I` is enabled:

- **Clock ticks** are averaged into a smoothed tempo estimate. acidflow's BPM display floats toward the external tempo rather than jumping — the display stays readable even with jittery incoming clock.
- **Start/Stop** drive the transport. Pressing Start on an external device starts acidflow. Stop stops it.

This lets you make acidflow a "slave" to another master (a DAW, a TR-8, whatever).

## Note-in (always on)

If you connect a MIDI keyboard to acidflow's input port, every key press you make on the keyboard plays the 303 voice. This is always active — no toggle required.

Velocity over 100 → the note is accented. Velocity under 100 → normal.

This is a much more expressive way to play the 303 than jam mode (Chapter 31). A real MIDI keyboard gives you proper note-off events, continuous velocity control, polyphonic aftertouch (though the 303 voice ignores aftertouch since it's monophonic), and a better physical interface.

## Setup examples

### acidflow slaved to a DAW

- acidflow: `Shift+I` to enable sync in.
- DAW: set its MIDI clock output to acidflow's ALSA port.
- In the DAW: press play. acidflow starts. Both are at the same tempo.

You can now record acidflow's audio output into the DAW (via JACK/PipeWire/loopback) while keeping everything in sync.

### acidflow driving an external synth

- acidflow: `Shift+O` to enable MIDI out.
- External synth: connect its MIDI input to acidflow's ALSA output.
- In acidflow: program a pattern. Press play.
- The external synth plays the same notes. You can A/B by muting acidflow's output.

This turns acidflow into a step sequencer for any MIDI synth. The 303-style accent/slide patterns drive a Moog, a Prophet, a Juno — whatever you connect.

### acidflow as a drum trigger

- acidflow: `Shift+O` enabled.
- External drum module: connected to acidflow's output.
- Program drums in acidflow. Press play.
- The external drum module plays GM-mapped kit sounds.

If you don't like acidflow's internal drum synthesis, trigger an external module instead.

### MIDI keyboard → acidflow bass voice

- Plug in a MIDI keyboard. Connect its output to acidflow's input.
- Play the keyboard. The 303 voice plays.
- Velocity maps to accent. Play softly for normal, hard for accent.

Now you can play the 303 properly — with velocity, with polyphonic key events (though only one note plays at a time), with longer sustained notes.

## MIDI and jam mode

Jam mode (Chapter 31) uses the computer keyboard. MIDI input uses a real keyboard. They don't conflict — MIDI input is always on; jam mode is separately toggleable.

If you have a MIDI keyboard connected AND jam mode on, both will play the 303. Whichever triggers most recently plays. Can be useful for two-handed performance (one hand on MIDI, one on computer keys).

## Clock jitter and tempo smoothing

External MIDI clocks are never perfectly stable. Even hardware masters (like a Tempi or a DAW) send clock with microsecond jitter.

If acidflow just took the raw clock as "tempo now," the BPM display would jitter wildly. Instead, acidflow smooths the incoming clock into a running tempo estimate. The display floats toward the new tempo over several seconds rather than snapping.

Audible effect: if the master tempo is 120 BPM, acidflow plays at 120 BPM. If the master changes to 125, acidflow glides to 125 over a few bars. Not instant, but musically smooth.

## Using MIDI to drive multiple instruments

A 303 and a kick drum were classic pairing. acidflow's 303 + external drum module = that pairing with modern interfaces.

For even more: stack two acidflow instances (if you can run multiple on your system), or route acidflow's MIDI out to multiple synths via a MIDI splitter. You can have acidflow drive a bass synth, a pad synth, and a drum module all at once.

## The 24 PPQN convention

MIDI clock is 24 pulses per quarter note. This is a MIDI standard since the 1980s. With 24 PPQN:

- One 16th note = 6 clock pulses (24/4).
- One 8th note = 12 clock pulses.
- One quarter note = 24 pulses.

acidflow's sequencer resolves timing at 16th-note accuracy (step length), but clock pulses go out at 24 PPQN for external sync.

## MIDI out and slides

The original TB-303 didn't send MIDI (it predates MIDI). Its slides were circuit-level events — no MIDI equivalent.

acidflow encodes slides as **note overlap**: the sliding note starts before the previous note ends. In MIDI terms:

- Note A: on at time T, off at time T + D.
- Note B (sliding from A): on at time T + D - overlap, off at time T + 2D.

The overlap period (a few ms) tells the receiver "these notes are legato." Most DAWs and synths interpret this as "glide from A to B." The glide time depends on the receiver's portamento setting — acidflow can't control that; it just signals the intent.

Non-MIDI-legato receivers will hear the overlap as a brief polyphonic moment. For monophonic synths, the note overlap triggers a retrigger-free slide; for polyphonic synths, you get both notes briefly sounding.

## The gateway drug problem

MIDI is powerful but can be distracting. You can spend hours routing, syncing, tweaking. Every new device adds complexity.

For most acid production, you don't need MIDI. acidflow alone produces finished tracks. Use MIDI when you genuinely need external gear or a DAW; otherwise, the built-in workflow is often faster.

Rule of thumb: start in acidflow alone. Only reach for MIDI when you hit a limit the built-in tools can't handle.

## Common issues

- **"aconnect can't find acidflow"**: make sure acidflow is running and the ALSA backend started successfully. Check stderr for backend errors.
- **"The clock is wrong"**: ensure the master device is actually sending clock, and acidflow is actually listening (`Shift+I` enabled). Clock send/receive settings on DAWs can be buried.
- **"MIDI notes are delayed"**: ALSA sequencer has some latency (milliseconds). For tight sync, route via JACK rather than plain ALSA MIDI.
- **"External synth doesn't slide"**: check its portamento settings. acidflow signals slides with note overlap, but the receiver has to interpret that.

## Try this (requires Linux + a MIDI device)

1. Start acidflow. Enable MIDI out with `Shift+O`.
2. In a terminal, run `aconnect -l`. You should see an acidflow client with input/output ports.
3. Connect acidflow's output to a MIDI monitor (like `aseqdump -p <port>`).
4. Program a pattern in acidflow. Press Space (play).
5. In the aseqdump terminal, watch the events fly by: note-on, note-off, clock ticks.
6. Now enable MIDI sync in with `Shift+I`. Connect an external clock source.
7. Press Start on the external clock. acidflow's transport should start playing too.

MIDI is deep. Full documentation is at the MIDI Association's specs. For acid, though, you usually only need basic note-on/off and clock sync. Start there; dive deeper when the need arises.

Next: the randomizer and mutation.
