#include "haiku_event_translation.h"

#include <InterfaceDefs.h>
// B_PRIMARY_MOUSE_BUTTON and friends live in View.h, not InterfaceDefs.h.
#include <View.h>

#include "ui/events/event_constants.h"

namespace ui {

int EventFlagsFromHaiku(int32 modifiers, int32 buttons) {
  int flags = EF_NONE;

  if ((modifiers & B_SHIFT_KEY) != 0)
    flags |= EF_SHIFT_DOWN;
  // See the policy note in the header: both of Haiku's shortcut-capable
  // modifiers report as Control so that Alt+L and Ctrl+L both work.
  if ((modifiers & (B_COMMAND_KEY | B_CONTROL_KEY)) != 0)
    flags |= EF_CONTROL_DOWN;
  if ((modifiers & B_OPTION_KEY) != 0)
    flags |= EF_ALT_DOWN;
  if ((modifiers & B_CAPS_LOCK) != 0)
    flags |= EF_CAPS_LOCK_ON;

  if ((buttons & B_PRIMARY_MOUSE_BUTTON) != 0)
    flags |= EF_LEFT_MOUSE_BUTTON;
  if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0)
    flags |= EF_RIGHT_MOUSE_BUTTON;
  if ((buttons & B_TERTIARY_MOUSE_BUTTON) != 0)
    flags |= EF_MIDDLE_MOUSE_BUTTON;

  return flags;
}

KeyboardCode KeyboardCodeFromHaiku(int32 raw_char, int32 raw_key) {
  // The function keys all arrive with raw_char == B_FUNCTION_KEY and are only
  // distinguishable by the raw key code.
  if (raw_char == B_FUNCTION_KEY) {
    switch (raw_key) {
      case B_F1_KEY: return VKEY_F1;
      case B_F2_KEY: return VKEY_F2;
      case B_F3_KEY: return VKEY_F3;
      case B_F4_KEY: return VKEY_F4;
      case B_F5_KEY: return VKEY_F5;
      case B_F6_KEY: return VKEY_F6;
      case B_F7_KEY: return VKEY_F7;
      case B_F8_KEY: return VKEY_F8;
      case B_F9_KEY: return VKEY_F9;
      case B_F10_KEY: return VKEY_F10;
      case B_F11_KEY: return VKEY_F11;
      case B_F12_KEY: return VKEY_F12;
      case B_PRINT_KEY: return VKEY_PRINT;
      case B_SCROLL_KEY: return VKEY_SCROLL;
      case B_PAUSE_KEY: return VKEY_PAUSE;
      default: return VKEY_UNKNOWN;
    }
  }

  switch (raw_char) {
    case B_BACKSPACE: return VKEY_BACK;
    case B_TAB: return VKEY_TAB;
    case B_RETURN: return VKEY_RETURN;
    case B_ESCAPE: return VKEY_ESCAPE;
    case B_SPACE: return VKEY_SPACE;
    case B_DELETE: return VKEY_DELETE;
    case B_INSERT: return VKEY_INSERT;
    case B_HOME: return VKEY_HOME;
    case B_END: return VKEY_END;
    case B_PAGE_UP: return VKEY_PRIOR;
    case B_PAGE_DOWN: return VKEY_NEXT;
    case B_LEFT_ARROW: return VKEY_LEFT;
    case B_RIGHT_ARROW: return VKEY_RIGHT;
    case B_UP_ARROW: return VKEY_UP;
    case B_DOWN_ARROW: return VKEY_DOWN;
    default: break;
  }

  if (raw_char >= 'a' && raw_char <= 'z')
    return static_cast<KeyboardCode>(VKEY_A + (raw_char - 'a'));
  if (raw_char >= 'A' && raw_char <= 'Z')
    return static_cast<KeyboardCode>(VKEY_A + (raw_char - 'A'));
  if (raw_char >= '0' && raw_char <= '9')
    return static_cast<KeyboardCode>(VKEY_0 + (raw_char - '0'));

  switch (raw_char) {
    case '-': return VKEY_OEM_MINUS;
    case '=': return VKEY_OEM_PLUS;
    case ',': return VKEY_OEM_COMMA;
    case '.': return VKEY_OEM_PERIOD;
    case '/': return VKEY_OEM_2;
    case ';': return VKEY_OEM_1;
    case '\'': return VKEY_OEM_7;
    case '[': return VKEY_OEM_4;
    case ']': return VKEY_OEM_6;
    case '\\': return VKEY_OEM_5;
    case '`': return VKEY_OEM_3;
    default: return VKEY_UNKNOWN;
  }
}

}  // namespace ui
