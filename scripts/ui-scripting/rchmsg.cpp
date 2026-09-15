// Send a scripted BMessage into R Chromium exactly as a button click would.
//   rchmsg WHAT  <window> <view-outer> [<view-inner> ...]      e.g. rchmsg rchA 0 "rchromium chrome"
//   rchmsg settext "<value>" <window> <view-outer> [<view-inner> ...]
// Specifiers are added innermost-first (property, then views inner->outer, then window).
#include <Messenger.h>
#include <Message.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
static void add_window(BMessage& m, const char* w) { if (isdigit(w[0])) m.AddSpecifier("Window", (int32)atoi(w)); else m.AddSpecifier("Window", w); }
int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage\n"); return 2; }
    BMessenger app("application/x-vnd.rchromium-native");
    if (!app.IsValid()) { printf("app not running\n"); return 1; }
    BMessage msg; int vi;
    if (strcmp(argv[1], "settext") == 0) {
        msg.what = B_SET_PROPERTY; msg.AddString("data", argv[2]); vi = 4;
        // BTextView accepts Text only through a range specifier: replace [0, 10000).
        BMessage range(B_RANGE_SPECIFIER); range.AddString("property", "Text"); range.AddInt32("index", 0); range.AddInt32("range", 10000); msg.AddSpecifier(&range);
        for (int i = argc - 1; i >= vi; --i) msg.AddSpecifier("View", argv[i]);   // inner -> outer
        add_window(msg, argv[3]);
    } else {
        const char* w = argv[1]; msg.what = ((uint32)(uint8)w[0] << 24) | ((uint32)(uint8)w[1] << 16) | ((uint32)(uint8)w[2] << 8) | (uint8)w[3]; vi = 3;
        for (int i = argc - 1; i >= vi; --i) msg.AddSpecifier("View", argv[i]);
        add_window(msg, argv[2]);
    }
    BMessage reply; status_t s = app.SendMessage(&msg, &reply, 3000000, 3000000);
    int32 err = 0; reply.FindInt32("error", &err);
    printf("send=%s reply=0x%08lx%s\n", strerror(s), (unsigned long)reply.what, err ? " (error in reply)" : "");
    return s == B_OK ? 0 : 1;
}
