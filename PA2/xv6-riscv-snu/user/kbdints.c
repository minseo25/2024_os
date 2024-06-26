#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int n;

  n = kbdints();
  printf("kbdints: %d\n", n);
  exit(0);
}
