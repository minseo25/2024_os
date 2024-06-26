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

void
main(int argc, char *argv[])
{
  int scanned, merged, freemem;

  while (1)
  {
    sleep(5);
	  freemem = ksm(&scanned, &merged);
    printf("ksmd: scanned %d, merged %d, freemem %d\n",
        scanned, merged, freemem);
  }
}
