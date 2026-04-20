#pragma once
// Focus sections used by main.cpp + the help-bar widget. Declared in its own
// header because both live on different sides of the widget boundary and
// neither should have to depend on the other.

namespace tb303 {

enum class Section { Knobs, FX, Sequencer, Drums, Transport };

} // namespace tb303
