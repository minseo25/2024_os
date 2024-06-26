//----------------------------------------------------------------
//
//  4190.307 Operating Systems (Spring 2024)
//
//  Project #4: KSM (Kernel Samepage Merging)
//
//  May 7, 2024
//
//  Dept. of Computer Science and Engineering
//  Seoul National University
//
//----------------------------------------------------------------

#ifdef SNU
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "memlayout.h"

#define MAX_KSM_S_NODES 1024
#define MAX_KSM_US_NODES 8192

extern int freemem;

struct reverse_node {
  int pid;
  void* addr_VA; // page aligned, using PGROUNDDOWN
};

struct ksm_node {
  void* addr_PA; // page aligned, using PGROUNDDOWN
  uint64 hash_value;
  uint refcnt;
  struct reverse_node reverse[16]; // not used in zero-page
  pte_t *pte; // PTE of the page for merging, used only in unstable node
};

#endif
