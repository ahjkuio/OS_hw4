
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char buf[4*4096];  // MSGBUFLEN из kernel/msgbuf.h
  
  if(dmesg(buf) < 0){
    fprintf(2, "dmesg failed\n");
    exit(1);
  }
  
  printf("%s", buf);
  exit(0);
}
