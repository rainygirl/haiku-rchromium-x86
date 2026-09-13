#ifndef RCHROMIUM_HAIKU_BEAPI_LOOPER_H_
#define RCHROMIUM_HAIKU_BEAPI_LOOPER_H_

#include "base/callback.h"

class BLooper;

namespace ui {

// Every BLooper adds itself to Haiku's global looper list when it is
// constructed, and BApplication::_WindowAt() walks that list with
// dynamic_cast<BWindow*> on each entry -- which BLooper::resolve_specifier()
// reaches whenever a scripting message arrives, as one does the moment
// anything asks the application about its windows. A BLooper subclass
// compiled with -fno-rtti has a null typeinfo pointer in its vtable, so that
// cast faults at offset 4 and kills the process.
//
// So the clipboard's watcher looper is defined in haiku_beapi_views.cc, which
// is compiled with RTTI, and reached from the ordinary ozone target only
// through these two functions. See the comment at the top of that file.
BLooper* CreateClipboardChangeWatcher(base::RepeatingClosure on_changed);

// Stops watching the system clipboard and asks the looper to quit. The looper
// deletes itself once it does, so the caller must not free it.
void StopClipboardChangeWatcher(BLooper* watcher);

}  // namespace ui

#endif
