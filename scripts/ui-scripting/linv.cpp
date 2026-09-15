#include <Messenger.h>
#include <Message.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char**argv){ BMessenger app("application/x-vnd.rchromium-native"); BMessage msg(B_EXECUTE_PROPERTY);
 BMessage idx(B_INDEX_SPECIFIER); idx.AddString("property","Item"); idx.AddInt32("index",atoi(argv[1])); msg.AddSpecifier(&idx);
 msg.AddSpecifier("View","bookmarks"); msg.AddSpecifier("View","scroller"); msg.AddSpecifier("Window","Bookmarks");
 BMessage r; status_t s=app.SendMessage(&msg,&r,3000000,3000000); int32 e=0; r.FindInt32("error",&e); printf("execute Item %s -> %s reply=0x%08lx err=%ld\n",argv[1],strerror(s),(unsigned long)r.what,(long)e); return 0; }
