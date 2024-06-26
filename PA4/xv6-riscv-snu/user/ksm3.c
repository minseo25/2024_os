//----------------------------------------------------------------
//
//  4190.307 Operating Systems (Spring 2024)
//
//  Project #4: KSM (Kernel Samepage Merging)
//
//  May 7, 2024
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

#define N  16 
#define LOOP  100000

// run this program with ksmd as follows:
// $ ksmd &; ksm3

char dummy[4096-16] = "ksm3";
int a[N][1024];                 // &a[0][0] = 0x2000

int 
stress_test(void)
{
  int i, j, k;
  int sum = 0;
  int ok = 1;

  for (k = 0; k < LOOP; k++)
    for (j = 0; j < 1024; j++)
      for (i = 0; i < N; i++)
        a[i][j]++;

  for (i = 0; i < N; i++)
  {
    for (j = 0; j < 1024; j++)
    {
      if (a[i][j] != LOOP)
      {
        printf("a[%d][%d] = %d\n", i, j, a[i][j]);
        ok = 0;
      }
      sum += a[i][j];
    }
  }
  printf("%s: %s\n", dummy, (ok)? "OK" : "WRONG");
  return sum;
}

int
main(void)
{
  int x;
  uint64 begin, end;

  sleep(5);
  begin = time(); 
  x =  stress_test();
  end = time();
  printf("%s: elapsed time = %l\n", dummy, end - begin);
  return x;
}
