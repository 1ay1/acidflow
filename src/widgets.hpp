#pragma once
// TB-303 simulator — custom widgets designed specifically for TUI UX.
// Umbrella header: every widget lives in its own file under tb303/ and is
// re-included here so main.cpp can keep using `#include "widgets.hpp"`.
//
// Design principles:
//   * every widget is readable at a glance (value readouts are always visible)
//   * focused elements are visually distinct (bold + accent color + brighter
//     border), unfocused are dimmed
//   * layout is tight so 8 knobs fit across a 100-col terminal
//   * no emoji — unicode box/block glyphs only (work on any TERM)

#include "tb303/theme.hpp"
#include "tb303/step_data.hpp"
#include "tb303/section.hpp"
#include "tb303/knob.hpp"
#include "tb303/waveform_toggle.hpp"
#include "tb303/sequencer.hpp"
#include "tb303/drums.hpp"
#include "tb303/filter_response.hpp"
#include "tb303/scope.hpp"
#include "tb303/transport.hpp"
#include "tb303/help_bar.hpp"
#include "tb303/help_overlay.hpp"
#include "tb303/take_picker.hpp"
