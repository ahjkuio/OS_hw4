#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char *buf;

  // Выделяем память динамически, +1 для нулевого терминатора
  buf = malloc(MSGBUFLEN + 1);
  if (buf == 0) {
    fprintf(2, "dmesg: malloc failed\n");
    exit(1);
  }

  if(dmesg(buf) < 0){
    fprintf(2, "dmesg failed\n");
    free(buf);
    exit(1);
  }

  printf("%s", buf);

  free(buf);
  exit(0);
}