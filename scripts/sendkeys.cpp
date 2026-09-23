// sendkeys -- type into a BWindow without going through input_server.
//
// R Chromium's view reads a key press out of the BMessage it was dispatched
// from: "bytes" for the text, and "raw_char"/"key"/"modifiers" for the code
// (see HaikuContentView::DispatchKey). Nothing in that path cares whether the
// message came from the input server or from here, so a posted B_KEY_DOWN is
// indistinguishable from a real key -- and unlike an input_server add-on,
// this cannot take the keyboard away from whoever is using the machine.
//
// usage: sendkeys <team-id> type <text>
//        sendkeys <team-id> key <enter|tab|esc|back>
//
// Needed because DevTools does not come up on this port ("Cannot start http
// server for devtools") and content_shell never starts the pipe transport,
// so a login flow cannot otherwise be driven without a person at the keyboard.
#include <Message.h>
#include <Messenger.h>
#include <InterfaceDefs.h>
#include <OS.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool WindowMessenger(team_id team, BMessenger* out) {
  BMessenger app(NULL, team);
  if (!app.IsValid()) {
    fprintf(stderr, "no messenger for team %d\n", (int)team);
    return false;
  }
  // Try each window: the browser may own more than one, and only the one
  // with a focused view will take the keys.
  for (int32 i = 0; i < 8; i++) {
    BMessage get(B_GET_PROPERTY), reply;
    get.AddSpecifier("Messenger");
    get.AddSpecifier("Window", i);
    if (app.SendMessage(&get, &reply, 5000000, 5000000) != B_OK)
      continue;
    BMessenger win;
    if (reply.FindMessenger("result", &win) != B_OK || !win.IsValid())
      continue;
    // The window itself is the wrong target. BWindow::DispatchMessage only
    // calls BView::KeyDown when the message is dispatched *to a view*; sent
    // to the window handler it falls through to MessageReceived and the page
    // never sees the key. So ask the window for its view and aim there.
    BMessage getview(B_GET_PROPERTY), vreply;
    getview.AddSpecifier("Messenger");
    getview.AddSpecifier("View", (int32)0);
    if (win.SendMessage(&getview, &vreply, 5000000, 5000000) == B_OK) {
      BMessenger view;
      if (vreply.FindMessenger("result", &view) == B_OK && view.IsValid()) {
        *out = view;
        fprintf(stderr, "using window %d view 0\n", (int)i);
        return true;
      }
    }
    *out = win;
    fprintf(stderr, "using window %d (no view)\n", (int)i);
    return true;
  }
  fprintf(stderr, "no scriptable window in team %d\n", (int)team);
  return false;
}

static void SendKey(const BMessenger& win, const char* bytes, int32 raw_char,
                    int32 key, int32 modifiers) {
  for (int pass = 0; pass < 2; pass++) {
    BMessage msg(pass == 0 ? B_KEY_DOWN : B_KEY_UP);
    msg.AddInt64("when", system_time());
    msg.AddInt32("key", key);
    msg.AddInt32("modifiers", modifiers);
    msg.AddInt32("raw_char", raw_char);
    for (size_t i = 0; bytes[i] != '\0'; i++)
      msg.AddInt8("byte", (int8)bytes[i]);
    msg.AddString("bytes", bytes);
    win.SendMessage(&msg);
    snooze(pass == 0 ? 15000 : 45000);
  }
}

int main(int argc, char** argv) {
  if (argc < 4) {
    fprintf(stderr, "usage: sendkeys <team-id> type|key <text>\n");
    return 1;
  }
  team_id team = (team_id)atoi(argv[1]);
  BMessenger win;
  if (!WindowMessenger(team, &win))
    return 1;

  if (strcmp(argv[2], "type") == 0) {
    for (const char* p = argv[3]; *p != '\0'; p++) {
      char one[2] = {*p, '\0'};
      int32 raw = (unsigned char)*p;
      int32 mods = 0;
      // A capital letter arrives as the shifted form of its lowercase key.
      if (*p >= 'A' && *p <= 'Z') {
        raw = *p;
        mods = B_SHIFT_KEY;
      }
      SendKey(win, one, raw, 0, mods);
    }
  } else if (strcmp(argv[2], "key") == 0) {
    const char* name = argv[3];
    if (strcmp(name, "enter") == 0) {
      char nl[2] = {B_RETURN, '\0'};
      SendKey(win, nl, B_RETURN, 0, 0);
    } else if (strcmp(name, "tab") == 0) {
      char tb[2] = {B_TAB, '\0'};
      SendKey(win, tb, B_TAB, 0, 0);
    } else if (strcmp(name, "esc") == 0) {
      char es[2] = {B_ESCAPE, '\0'};
      SendKey(win, es, B_ESCAPE, 0, 0);
    } else if (strcmp(name, "back") == 0) {
      char bs[2] = {B_BACKSPACE, '\0'};
      SendKey(win, bs, B_BACKSPACE, 0, 0);
    } else {
      fprintf(stderr, "unknown key %s\n", name);
      return 1;
    }
  }
  return 0;
}
