#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  uint64 t_start, t_end;

  t_start = time();
  sleep(1);
  t_end = time();
  printf("%l %l %l\n", t_start, t_end, t_end - t_start);
  exit(0);
}
