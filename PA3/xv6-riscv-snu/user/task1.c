//----------------------------------------------------------------
//
//  4190.307 Operating Systems (Spring 2024)
//
//  Project #3: The EDF Scheduler
//
//  April 11, 2024
//
//  Jin-Soo Kim (jinsoo.kim@snu.ac.kr)
//  Systems Software & Architecture Laboratory
//  Dept. of Computer Science and Engineering
//  Seoul National University
//
//----------------------------------------------------------------

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

uint64 
simpleHash(uint64 x)
{
  for (int i = 0; i < 100000; i++)
  {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
  }
  return x;
}

uint64 
run_task(int n)
{
  uint64 x = 0;

  for (int i = 0; i < n; i++)
    x += simpleHash(i);
  return x;
}

int
do_fork(int n)
{
  int x = 0;

  for (int i = 0; i < 10; i++)
  {
    printf("%l %d starts\n", time()/100000, getpid());
    x += run_task(n);
    printf("%l %d ends\n", time()/100000, getpid());
    sched_yield();
  }
  return x;
}

int
main(int argc, char *argv[])
{
  uint64 x = 0;
  int pid1, pid2;

  sleep(1);
  if ((pid1 = fork()))
  {
    if ((pid2 = fork()))
    {
      sched_setattr(pid1, 1, 3);
      sched_setattr(pid2, 1, 4);
      wait(0);
      wait(0);
      exit(0);
    }
    else
    {
      // pid2
      x = do_fork(50);
      exit(x);
    }
  }
  else
  {
    // pid1
    x = do_fork(80);
    exit(x);
  }
}

