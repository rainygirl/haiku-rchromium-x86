#ifndef RCHROMIUM_HAIKU_EVENT_TRANSLATION_H_
#define RCHROMIUM_HAIKU_EVENT_TRANSLATION_H_

#include <SupportDefs.h>

#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

// Translates a Haiku `modifiers()` bitmask and a Haiku mouse button mask into
// Chromium event flags.
//
// Modifier policy, which is a deliberate choice rather than a mechanical
// mapping: Haiku's primary shortcut modifier is B_COMMAND_KEY, which the
// default keymap puts on the physical Alt key, so a Haiku user reaches for
// Alt+L where Chromium's shortcut table expects Ctrl+L. B_COMMAND_KEY is
// therefore reported as EF_CONTROL_DOWN. B_CONTROL_KEY is reported the same
// way so that muscle memory from other systems also works, and B_OPTION_KEY
// carries EF_ALT_DOWN for the rare cases that genuinely want Alt.
//
// The cost of this policy is that a real Alt chord cannot be expressed with
// the Alt key itself. If that turns out to matter more than native-feeling
// shortcuts, this function is the single place to change.
int EventFlagsFromHaiku(int32 modifiers, int32 buttons);

// Maps a Haiku key event to a Chromium key code. `raw_char` is the message's
// "raw_char" field (the unmodified character) and `raw_key` is its "key" field,
// which is the only way to tell the function keys apart because they all
// arrive as B_FUNCTION_KEY.
//
// Returns VKEY_UNKNOWN when nothing sensible maps, which is not an error: the
// character payload is delivered separately, so text still reaches the page.
KeyboardCode KeyboardCodeFromHaiku(int32 raw_char, int32 raw_key);

}  // namespace ui

#endif
