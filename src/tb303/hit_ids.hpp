#pragma once
// Mouse targets. Widgets tag boxes with `| dsl::hit(hit_id(kind, index))` and
// main.cpp resolves clicks with maya::hit_test, so the geometry always comes
// from the frame that was actually painted.

#include <cstdint>

namespace tb303 {

enum HitKind : std::uint32_t {
    kHitTitle = 1,
    kHitHelpBar,
    kHitHelpOverlay,
    kHitKnob,        // index = knob idx
    kHitFx,          // index = fx idx
    kHitSeqRow,      // a sequencer data row; step comes from the x offset
    kHitDrumRow,     // index = drum voice; step comes from the x offset
    kHitTransport,
    kHitScope,
    kHitPanel,       // index = Section, for hover focus over borders/padding
};

// Column layout inside the tagged rows, used to turn an x offset into a step.
inline constexpr int kSeqCellX0 = 8;   // 7-col label + left wall
inline constexpr int kSeqCellW  = 4;   // 3-col cell + wall
inline constexpr int kDrumCellX0 = 4;  // "BD: "
inline constexpr int kDrumCellW  = 3;

} // namespace tb303
