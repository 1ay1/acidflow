# 7 · Getting acidflow running

The rest of this book assumes acidflow is open in front of you. This chapter ensures that's true. It's deliberately short; if you already have the program running, skip to the next one.

## Install

acidflow is a terminal program. You compile it from source; there are no pre-built binaries.

### Prerequisites

- A C++26-capable compiler (recent GCC or Clang on Unix; Visual Studio 2022 or 2026 on Windows — Windows is a C++23 fallback).
- CMake ≥ 3.28.
- A sibling checkout of [maya](https://github.com/1ay1/maya) — the TUI framework acidflow is built on.
- On Linux: ALSA development headers.
- On macOS: CoreAudio (built into the SDK; no install needed).
- On Windows: Windows SDK (built into Visual Studio).

### Unix (Linux / macOS)

```bash
# Linux — install ALSA dev headers
sudo apt-get install libasound2-dev    # Debian/Ubuntu
sudo dnf install alsa-lib-devel        # Fedora
sudo pacman -S alsa-lib                # Arch

# Clone maya as a sibling of acidflow
git clone https://github.com/1ay1/maya.git
git clone https://github.com/1ay1/acidflow.git
cd acidflow

cmake -B build
cmake --build build -j
./build/acidflow
```

### Windows

```bat
cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
.\build\Release\acidflow.exe
```

Use Windows Terminal, WezTerm, or Alacritty — `cmd.exe` won't render the UI correctly.

## First boot

When acidflow starts, you see the full-screen TUI. From top to bottom:

- **Title bar** with the current preset name, BPM, pattern length, swing, and status badges (SONG mode, JAM mode, MIDI OUT, MIDI SYNC).
- **Knobs panel** — nine knobs: TUNE, WAVE, CUTOFF, RES, ENVMOD, DECAY, ACCENT, DRIVE, VOL.
- **FX rack** — seven knobs: FX DRIVE, DLY TIME, DLY FB, DLY MIX, REV SIZE, REV DAMP, REV MIX.
- **Sequencer** — the 16-step grid for the 303 voice.
- **Drum grid** — 5 voices × 16 steps.
- **Transport strip** — preset selector, tempo, swing, length.
- **Scope / filter heatmap** — live audio visualization.
- **Help bar** — scrolling hints at the bottom.

The currently-focused panel has a highlighted border. Press **Tab** to cycle focus (Knobs → FX → Sequencer → Drums → Transport). Each section's keyboard shortcuts apply only when it's focused.

## Make sure sound is working

Press `Space`. The sequencer starts running. If you hear audio, you're done.

If you don't hear audio:

- **Linux**: ALSA may be routing to the wrong device. Try `aplay -L` to list devices. acidflow uses the default device (`hw:0` or similar). If your output is somewhere else, you'll need to reconfigure ALSA at the system level (this is a Linux issue, not acidflow's).
- **macOS**: CoreAudio should pick your current default output. If you've changed outputs since launching acidflow, restart the program.
- **Windows**: WASAPI picks the system default. Check your volume mixer and make sure acidflow isn't muted.

If the sequencer is running but silent, check that VOL is above 0. Seriously. It happens.

## The help overlay

Press `?` at any time for a scrollable keyboard reference inside the program. Every keybind is listed. Use `↑`/`↓` or Page Up / Page Down to scroll. Press `?` or `Esc` to close.

The overlay is the canonical source of truth for keybindings. If anything in this book ever drifts from what the overlay says, believe the overlay.

## Picking a preset

Press `Tab` until the Transport panel is focused. Use `↑`/`↓` to browse the 16 built-in presets. Press `Enter` (or just wait — selection is live) to load one. Try:

- **1 Acid Tracks** — the canonical Phuture pattern.
- **5 Voodoo Ray** — A Guy Called Gerald's classic, restrained melody.
- **7 Plastikman** — minimal, filter-heavy.
- **13 Ambient Acid** — slow, dubby, long decay.

Press `Space` to start it playing. Welcome.

## Saving your work

acidflow has nine user-save slots. To save the current state, press `Shift-1` through `Shift-9`. To load, press `1` through `9`. Save slots write to `~/.config/acidflow/slots/*.afslot` on Unix or `%APPDATA%\acidflow\slots\*.afslot` on Windows. They persist between sessions.

If you press `Shift-1` while on a preset, your customized version is saved to slot 1. The presets themselves are read-only — you can always get back to the factory version by selecting from the transport list.

## Exports

- `e` — export the current loop as a 4-bar WAV file.
- `E` — export the pattern as a standard MIDI file (.mid). Drums are NOT included in the MIDI export (a deliberate choice; see Chapter 32).
- `p` — export a shareable text representation of the pattern.
- `P` — import a text representation. Paste or load one from a friend.

All exports write to the current working directory unless you're running acidflow from a path with no write permission (in which case they go to `$HOME` or `%USERPROFILE%`).

## Quit

`q`. You lose any unsaved changes (hence the save slots).

## What to do next

If you're new to synthesis or acid, read Parts I–II in order. If you're synth-literate, the most rewarding chapter is the one on accent (Chapter 12). If you just want to start making music, skip to Chapter 37.

But seriously — before you read another chapter, make some noise. Press a preset. Press Space. Turn the CUTOFF knob while the pattern plays. Hit `Shift-R` to randomize everything three times in a row and see what comes up. Press `R` to randomize the pattern keeping the same knobs. The book teaches; the instrument is how you actually learn.
