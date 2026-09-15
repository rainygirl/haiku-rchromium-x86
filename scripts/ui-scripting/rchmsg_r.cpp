#include <Messenger.h>
#include <Message.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char**argv){ BMessenger app("application/x-vnd.rchromium-native"); BMessage msg(B_SET_PROPERTY); msg.AddString("data",argv[1]);
 BMessage range(B_RANGE_SPECIFIER); range.AddString("property","Text"); range.AddInt32("index",0); range.AddInt32("range",atoi(argv[2])); msg.AddSpecifier(&range);
 msg.AddSpecifier("View","_input_"); msg.AddSpecifier("View","search"); msg.AddSpecifier("Window","Bookmarks");
 BMessage reply; status_t s=app.SendMessage(&msg,&reply,3000000,3000000); printf("settext(\"%s\",replace %s) -> %s reply=0x%08lx\n",argv[1],argv[2],strerror(s),(unsigned long)reply.what); return 0; }
