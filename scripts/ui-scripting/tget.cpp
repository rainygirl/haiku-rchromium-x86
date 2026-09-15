#include <Messenger.h>
#include <Message.h>
#include <stdio.h>
int main(){ BMessenger app("application/x-vnd.rchromium-native"); BMessage msg(B_GET_PROPERTY);
 BMessage range(B_RANGE_SPECIFIER); range.AddString("property","Text"); range.AddInt32("index",0); range.AddInt32("range",1000); msg.AddSpecifier(&range);
 msg.AddSpecifier("View","_input_"); msg.AddSpecifier("View","search"); msg.AddSpecifier("Window","Bookmarks");
 BMessage r; app.SendMessage(&msg,&r,3000000,3000000); const char* t=""; r.FindString("result",&t); printf("[%s]",t); return 0; }
