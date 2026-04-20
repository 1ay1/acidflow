# 41 · Glossary

Terms used throughout this book, defined.

## A

**Accent** — A per-step flag that boosts the note's volume, filter envelope depth, and apparent punch. In the TB-303 circuit, accents charge a dedicated capacitor that stacks across consecutive accented notes, producing rising squelch. In acidflow: toggle with `a` on a step.

**Acid** — The subgenre of electronic music defined by TB-303-style resonant filter squelch. Originated in 1987 with Phuture's *Acid Tracks*. Also used loosely for any 303-like sound.

**Aliasing** — Digital audio artifact where frequencies above half the sample rate wrap into audible range as fake low frequencies. Naive oscillators alias heavily; PolyBLEP corrections mitigate it.

**Allpass filter** — A filter that passes all frequencies at equal amplitude but shifts phase. Used in reverb to smear discrete echoes into diffuse wash.

**Archetype** — acidflow's term for preset parameter region used by the randomizer. Knob archetypes: Classic, Squelch, Driving, Dubby. Pattern archetypes: Pedal, Driving, Melodic, Dub.

**Arpeggio** — A melodic pattern that plays notes of a chord in sequence rather than simultaneously. Acid lines often arpeggiate minor chords.

## B

**Backbeat** — Beats 2 and 4 in a 4/4 meter. Where the snare and/or clap traditionally hit. Steps 5 and 13 in acidflow's 16-step grid.

**Ballistic envelope** — An envelope (attack/decay shape) that responds quickly to incoming signal, like a needle on a VU meter. Acid filter envelopes are somewhat ballistic — fast attack, slower decay.

**BD** — Bass Drum, i.e., kick drum. acidflow's first drum voice.

**BPM** — Beats Per Minute. Standard tempo unit. Acid house is typically 120-128 BPM.

**Breakdown** — An arrangement section where elements are removed, creating contrast before the drop.

## C

**Capacitor** — Electronic component that stores charge. In the TB-303, capacitors hold the filter envelope and accent envelope levels; their time constants determine decay shape.

**Chorus** — A modulation effect that thickens sound. Not in acidflow. Sometimes confused with ensemble/unison effects.

**CL** — Clap. acidflow's fifth drum voice. Synthesized as multiple noise bursts with a tail.

**Clip / Clipping** — When a signal exceeds the ceiling of the output, its peaks are cut off. Hard clip = instant flat. Soft clip = smooth saturation (tanh curve).

**Comb filter** — A filter that mixes a signal with a delayed copy, producing peaks at regular frequency intervals. Used in reverb as the "feedback" component.

**CH** — Closed Hi-hat. acidflow's third drum voice.

**Cutoff** — The frequency above which a lowpass filter attenuates. Raising cutoff = more highs pass through = brighter sound. Lowering = less highs = darker sound.

**CV** — Control Voltage. Analog synth parameter control. Not present in acidflow (digital), but the concept (knobs controlling continuous parameters) is.

## D

**Damp (reverb)** — How quickly high frequencies die in the reverb tail. More damp = darker tail.

**DAW** — Digital Audio Workstation. Software for multitrack recording and mixing (Ableton, Logic, Reaper, FL Studio).

**Decay** — The time it takes an envelope to fall from peak to zero (or sustain level). Short decay = snappy. Long decay = sustained.

**Delay** — A time-based effect that plays back the signal after a delay, optionally with feedback for multiple echoes.

**Delay division** — In tempo-synced delay, the musical subdivision to lock to (1/16, 1/8, etc.).

**Detroit** — Detroit techno subgenre, 1988 onward. Originators include Derrick May, Kevin Saunderson, Juan Atkins.

**Distortion** — Non-linear processing that adds harmonics. Drive, saturation, fuzz, overdrive — all forms.

**DLY** — Abbreviation for delay in acidflow's UI.

**Drive** — Saturation / overdrive effect. Adds harmonics and compresses peaks. acidflow has two drives: voice-level (pre-filter) and FX-level (post-mix).

**Drop** — The arrangement moment when the drums / bass / full energy enters after a build.

**Drum bus** — The aggregate output of all drum voices, summed before reaching the FX rack.

**Dubby** — An archetype characterized by long decays, low cutoff, moderate resonance, and reverb-heavy aesthetic.

**Duty cycle** — For square waves, the proportion of each cycle spent "high." 50% = symmetric square. TB-303 square has pitch-dependent duty (71% low pitches, 45% high).

## E

**Echo** — Single-repeat delay. Often used interchangeably with "delay."

**Envelope** — A curve that shapes a parameter over time after a note trigger. Common types: ADSR (attack-decay-sustain-release), AD (attack-decay), one-shot.

**ENVMOD** — Envelope Modulation. Depth of the filter envelope's modulation of cutoff. Higher = more dramatic filter sweep per note.

**EQ** — Equalizer. A filter or set of filters that boosts/cuts specific frequency ranges. Not in acidflow.

## F

**Feedback** — In delay: the amount of the delayed signal fed back to the input, producing multiple repeats. In filters: the signal returned to the filter's input, creating resonance.

**Filter** — A circuit or algorithm that attenuates or boosts specific frequencies. Lowpass passes lows, cuts highs. Highpass passes highs, cuts lows. Bandpass passes a specific range.

**FM** — Frequency Modulation. Synthesis technique where one oscillator modulates another's frequency. Not in acidflow.

**FX** — Effects. acidflow's FX rack: drive, delay, reverb.

## G

**Gate** — A signal that controls when a note is "on" (key held) or "off" (key released). Envelopes start on gate-on, release on gate-off.

**Glide** — See slide.

## H

**Hardfloor** — A German production duo (Ramon Zenker, Oliver Bondzio) who defined the aggressive acid techno sound of 1992-1996. Also a subgenre label.

**Harmonic** — A frequency that is a whole-number multiple of the fundamental. A sawtooth has all harmonics; a square has only odd harmonics.

**HP / Highpass** — A filter that passes high frequencies and attenuates low frequencies.

## I

**IDM** — Intelligent Dance Music. A loose genre including Aphex Twin, Plastikman, Autechre. Often incorporates acid.

**Impulse response** — The output of a system when fed a single impulse (click). Characterizes the system completely. Used in convolution reverbs.

## J

**Jam mode** — acidflow's live-keyboard mode. The computer keyboard becomes a two-octave piano routed to the 303 voice.

## K

**Knob** — A rotary control for a continuous parameter. acidflow has 9 voice knobs and 7 FX knobs.

## L

**Ladder filter** — A filter topology consisting of 4 stages in series with global feedback. Made famous by the Moog ladder filter. The TB-303 uses a diode ladder filter.

**Legato** — A performance style where notes are connected smoothly, without gaps. The 303's slide is a form of legato.

**LFO** — Low Frequency Oscillator. A slow oscillator used to modulate parameters over time. Not prominent in acidflow; the envelope is the main modulation source.

**LP / Lowpass** — A filter that passes low frequencies and attenuates high frequencies. The TB-303's filter is lowpass.

**LUFS** — Loudness Units Full Scale. A measure of perceived loudness. Streaming services normalize to around -14 LUFS.

## M

**Master bus** — The final mixing point before the output. All signals pass through it.

**MIDI** — Musical Instrument Digital Interface. Protocol for electronic instruments to communicate. Used for note data, control changes, and clock sync.

**Minor pentatonic** — A 5-note scale: root, minor 3rd, perfect 4th, perfect 5th, minor 7th. The default acid scale.

**Mixdown** — The process of combining multiple tracks into a stereo mix.

**Modulation** — Varying a parameter over time. Envelopes, LFOs, and manual knob moves are modulation sources.

**Monophonic** — Only one note playable at a time. The 303 and acidflow voice are monophonic.

**Mutation** — A small change to the pattern. acidflow's `M` key applies one mutation.

## N

**Note-on / note-off** — MIDI messages that start and stop a note.

## O

**Octave** — 12 semitones. A note an octave higher is twice the frequency.

**OH** — Open Hi-hat. acidflow's fourth drum voice.

**Oscillator** — The primary sound source. Produces a periodic waveform (saw, square, sine, etc.).

**Oversampling** — Processing audio at a higher sample rate than needed, then downsampling. Reduces aliasing. acidflow uses 2× oversampling for the filter.

## P

**Pattern** — A sequence of notes. In acidflow, 16 steps of note data.

**Pedal point** — A pattern centered on a single pitch (the pedal). Most classic acid lines are pedal points.

**PolyBLEP** — Polynomial Band-Limited Step. A technique for anti-aliasing edges in digital oscillators. acidflow uses it for saw and square.

**Polymeter** — Two or more patterns of different lengths playing simultaneously. If 303 is 12 steps and drums are 16, they drift relative to each other.

**Polyphonic** — Multiple notes playable simultaneously. acidflow is monophonic.

**P-lock** — Parameter lock. Per-step override of a knob's value.

**PPQN** — Pulses Per Quarter Note. MIDI clock resolution: 24 PPQN standard.

**Preset** — A pre-saved set of parameters (pattern + knobs). acidflow ships with 16.

**Probability** — The chance that a step plays when the sequencer reaches it. acidflow: 25/50/75/100%.

## R

**Ratchet** — A step that retriggers multiple times within its duration. acidflow: 1-4 sub-triggers.

**RES / Resonance** — The emphasis at the filter's cutoff frequency. Low RES = flat response. High RES = pronounced peak. Very high RES = self-oscillation.

**Reverb** — An effect that simulates the acoustic reflections of a space. Adds depth and diffuse tails.

**REV** — Abbreviation for reverb in acidflow's UI.

**Root** — The tonal center of a scale or melody. Most acid lines are in A minor or C minor, with A or C as the root.

**RT60** — Reverb time for decay to -60 dB. A rough measure of reverb length.

## S

**Sample rate** — How many audio samples are stored per second. CD quality = 44.1 kHz. Most systems today use 48 kHz.

**Sawtooth / Saw** — A waveform that ramps up and snaps down. Rich in harmonics. The 303's main waveform.

**Schroeder reverb** — A reverb algorithm using comb filters and allpass filters. acidflow's reverb is Schroeder-style.

**SD** — Snare Drum. acidflow's second drum voice.

**Self-oscillation** — When a filter's resonance is so high that it produces its own oscillation without input. Threshold around 80-90% RES in acidflow.

**Semitone** — One step on the Western 12-tone equal temperament scale. An octave = 12 semitones.

**Sequencer** — The step-based note programmer. In acidflow: 16 steps.

**Slide** — A smooth pitch glide from one note to the next. Preserves the envelope (no retrigger). Triggered by flagging a step with slide.

**Song mode** — Chaining saved slots into an arrangement.

**Square wave** — A waveform alternating between high and low. Only odd harmonics. 303's second waveform.

**Staccato** — Notes with short duration, separated by silence. Opposite of legato.

**Step** — A single slot in the 16-step sequencer grid.

**Subtractive synthesis** — Synthesis technique: start with a harmonically rich oscillator, remove harmonics with a filter. The 303 is subtractive.

**Sweep** — Manually moving a knob over time. Filter sweep = changing cutoff.

**Swing** — Delaying even-numbered steps to create groove. acidflow: 50-75%.

**Synthesis** — The process of generating audio from scratch (as opposed to sampling).

## T

**Tanh** — Hyperbolic tangent. A smooth S-curve used for soft clipping. Both the TB-303 and acidflow use tanh saturation.

**Tape delay** — A delay effect that emulates analog tape with high-frequency rolloff on each pass.

**TB-303** — Transistor Bass 303. Roland's 1981 bass synth that accidentally became the acid instrument.

**Tempo** — Speed of music, measured in BPM.

**Tonic** — The root note of a scale.

**Tracker** — A style of sequencer software (Amiga/DOS era). Vertical pattern layout, keyboard-as-piano input.

**Transient** — The initial brief burst of energy at a note's start. Kicks, claps, and hats have prominent transients.

## V

**VCA** — Voltage-Controlled Amplifier. An amp whose gain is controlled by a CV signal. In acidflow, the VCA is controlled by the volume envelope.

**Velocity** — In MIDI, the intensity of a note (0-127). Acid uses binary accent rather than continuous velocity.

**VCO** — Voltage-Controlled Oscillator. An oscillator whose pitch is controlled by a CV. acidflow's oscillator is digital but emulates VCO behavior.

**Voice** — An independent sound-generating element. The 303 voice is monophonic; there are 5 drum voices.

**Voltage-per-octave** — A tuning convention where 1 volt = 1 octave change. acidflow emulates this for oscillator pitch.

**VOL** — Volume.

## W

**Waveform** — The shape of an oscillator's output. Saw, square, sine, triangle.

**WAV** — Waveform Audio File. Uncompressed audio format. acidflow exports WAV.

## Z

**Zero-delay feedback (ZDF)** — A filter topology that avoids the 1-sample delay of traditional digital filters. acidflow's filter is ZDF.

## Cross-references

For deeper coverage of any term, find the chapter listed:

- Accent: Chapter 12.
- Slide: Chapter 13.
- Filter: Chapter 10.
- Envelope: Chapter 11.
- Reverb: Chapter 29.
- Song mode: Chapter 30.
- ZDF/Tanh/PolyBLEP: Chapter 6.

Next: afterword and further listening.
