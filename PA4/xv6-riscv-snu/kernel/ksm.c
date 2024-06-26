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

#include "ksm.h"

extern struct proc proc[NPROC];
struct proc* cur_proc;

int scanned;
int merged;
struct ksm_node ksm_stable[MAX_KSM_S_NODES]; // stable ksm nodes, W permission removed, shared
struct ksm_node ksm_unstable[MAX_KSM_US_NODES]; // candidates for merging, W permission kept, not shared
int ksm_stable_cnt;
int ksm_unstable_cnt;

void
ksm_kfree(void *pa, int pid, uint64 va)
{
  int idx;
  va = PGROUNDDOWN(va);
  pa = (void*)PGROUNDDOWN((uint64)pa);

  for(idx = 0; idx<ksm_stable_cnt; idx++) {
    if(ksm_stable[idx].addr_PA == pa) break;
  }

  // if exists in stable list
  if(idx < ksm_stable_cnt) {
    // remove reverse mapping
    ksm_stable[idx].refcnt--;

    if(idx > 0) {
      int i;
      for(i=0; i<=ksm_stable[idx].refcnt; i++) {
        if(ksm_stable[idx].reverse[i].pid == pid && ksm_stable[idx].reverse[i].addr_VA == (void*)va) {
          ksm_stable[idx].reverse[i] = ksm_stable[idx].reverse[ksm_stable[idx].refcnt];
          ksm_stable[idx].reverse[ksm_stable[idx].refcnt] = (struct reverse_node){0,};
          break;
        }
      }
      if(i > ksm_stable[idx].refcnt)
        panic("ksm_kfree: reverse mapping not found");
    }

    if(ksm_stable[idx].refcnt <= 0 && idx > 0) {
      ksm_stable[idx] = ksm_stable[--ksm_stable_cnt];
      ksm_stable[ksm_stable_cnt] = (struct ksm_node){0,};
      kfree(pa);
    }
    return;
  }

  for(idx = 0; idx<ksm_unstable_cnt; idx++) {
    if(ksm_unstable[idx].addr_PA == pa) break;
  }

  // if exists in unstable list
  if(idx < ksm_unstable_cnt) {
    if(ksm_unstable[idx].reverse[0].pid != pid || ksm_unstable[idx].reverse[0].addr_VA != (void*)va)
      panic("ksm_kfree: reverse mapping not found");

    ksm_unstable[idx] = ksm_unstable[--ksm_unstable_cnt];
    ksm_unstable[ksm_unstable_cnt] = (struct ksm_node){0,};
    kfree(pa);
    return;
  }

  // not found in both stable and unstable list
  kfree(pa);
}

void
debug_nodes(void)
{
  printf("-------------------------stable nodes--------------------------\n");
  for(int i=0; i<ksm_stable_cnt; i++) {
    printf("PA: %p, RC: %d, hash: %p\n", ksm_stable[i].addr_PA, ksm_stable[i].refcnt, ksm_stable[i].hash_value);
    if(i==0) {
      printf("  No reverse mapping in zero-page\n");
      continue;
    }
    for(int j=0; j<ksm_stable[i].refcnt; j++) {
      printf("  pid: %d, VA: %p\n", ksm_stable[i].reverse[j].pid, ksm_stable[i].reverse[j].addr_VA);
    }
  }
  printf("------------------------unstable nodes-------------------------\n");
  for(int i=0; i<ksm_unstable_cnt; i++) {
    printf("PA: %p, RC: %d, hash: %p\n", ksm_unstable[i].addr_PA, ksm_unstable[i].refcnt, ksm_unstable[i].hash_value);
    printf("  pid: %d, VA: %p, ", ksm_unstable[i].reverse[0].pid, ksm_unstable[i].reverse[0].addr_VA);
    uint permissions = PTE_FLAGS(*(ksm_unstable[i].pte));
    printf("permission(URWX): %d%d%d%d\n", permissions & PTE_U ? 1 : 0, permissions & PTE_R ? 1 : 0, permissions & PTE_W ? 1 : 0, permissions & PTE_X ? 1 : 0);
  }
  printf("---------------------------------------------------------------\n");
}

int
copy_on_write(pagetable_t pagetable, int pid, uint64 va)
{
  va = PGROUNDDOWN(va);
  pte_t *pte = walk(pagetable, va, 0);
  uint64 pa;
  int idx;
  char *mem;

  if(pte == 0 || !(*pte & PTE_V) || !(*pte & PTE_U) || !(*pte & PTE_COW)) // invalid pte
    return -1;

  pa = PTE2PA(*pte);
  for(idx = 0; idx < ksm_stable_cnt; idx++) {
    if(ksm_stable[idx].addr_PA == (void*)pa) break;
  }
  if(idx >= ksm_stable_cnt) {// not found in stable list (not COW protected)
    return -1;
  }

  // allocate new page, copy content, and update PTE with W permission
  mem = kalloc();
  memmove(mem, (char*)pa, PGSIZE);
  *pte = PA2PTE(mem) | (PTE_FLAGS(*pte) & ~PTE_COW) | PTE_W;
  sfence_vma(); // flush TLB

  // update stable node data (delete reverse mapping)
  ksm_stable[idx].refcnt--;
  if(idx > 0) {
    for(int i=0; i<=ksm_stable[idx].refcnt; i++) {
      if(ksm_stable[idx].reverse[i].pid == pid && ksm_stable[idx].reverse[i].addr_VA == (void*)va) {
        ksm_stable[idx].reverse[i] = ksm_stable[idx].reverse[ksm_stable[idx].refcnt];
        ksm_stable[idx].reverse[ksm_stable[idx].refcnt] = (struct reverse_node){0,};
        break;
      }
    }

    // delete stable node if refcnt is 0
    if(ksm_stable[idx].refcnt <= 0 && idx > 0) {
      kfree((void *)pa);
      ksm_stable[idx] = ksm_stable[--ksm_stable_cnt];
      ksm_stable[ksm_stable_cnt] = (struct ksm_node){0,};
    }
  }

  intr_on();
  return 0;
}

void
add_zero_page(void* pa)
{
  // add stable ksm node for zero-page
  ksm_stable[ksm_stable_cnt].addr_PA = pa;
  ksm_stable[ksm_stable_cnt].hash_value = xxh64(pa, PGSIZE);
  ksm_stable[ksm_stable_cnt].refcnt = 1;
  ksm_stable[ksm_stable_cnt].reverse[0] = (struct reverse_node){0,}; // no reverse mapping for zero-page
  ksm_stable[ksm_stable_cnt].pte = 0;

  ksm_stable_cnt = 1;
  ksm_unstable_cnt = 0;
}

void
delete_ksm_unstable_node(void* pa)
{
  int idx;
  for(idx = 0; idx < ksm_unstable_cnt; idx++) {
    if(ksm_unstable[idx].addr_PA == pa) break;
  }

  if(idx == ksm_unstable_cnt) return;

  ksm_unstable[idx] = ksm_unstable[--ksm_unstable_cnt];
  ksm_unstable[ksm_unstable_cnt] = (struct ksm_node){0,};
}

void
move_ksm_unstable_to_stable(int idx, int pid, uint64 va)
{
  ksm_unstable[idx].refcnt++;
  ksm_unstable[idx].reverse[ksm_unstable[idx].refcnt-1].pid = pid;
  ksm_unstable[idx].reverse[ksm_unstable[idx].refcnt-1].addr_VA = (void*)va;
  ksm_stable[ksm_stable_cnt++] = ksm_unstable[idx];

  ksm_unstable[idx] = ksm_unstable[--ksm_unstable_cnt];
  ksm_unstable[ksm_unstable_cnt] = (struct ksm_node){0,};
}

void
insert_ksm_node(void* pa, pagetable_t pagetable, int pid, uint64 va)
{
  uint64 hash_value = xxh64(pa, PGSIZE);
  int is, ius;

  // printf("[KSM] Scanning VPN(%d): ", va/PGSIZE);
  scanned++;
  
  // search stable list
  for(is = 0; is < ksm_stable_cnt; is++) {
    if(ksm_stable[is].hash_value == hash_value) {
      if(ksm_stable[is].addr_PA == pa) {
        // printf("\n");
        return;
      }
      
      // update coresponding PTE to points to shared addr, reclaim original frame
      pte_t *pte = walk(pagetable, va, 0);
      if(pte ==0 || (*pte & PTE_V) == 0)
        panic("insert_ksm_node: pte");

      if(is == 0) {
        ksm_stable[0].refcnt++;
        // printf("Mapped to zero page\n");
      } else {
        ksm_stable[is].refcnt++;
        ksm_stable[is].reverse[ksm_stable[is].refcnt-1].pid = pid;
        ksm_stable[is].reverse[ksm_stable[is].refcnt-1].addr_VA = (void*)va;
        // printf("Hash match, (old pa: %p -> new pa: %p) (RC -> %d)\n", pa, ksm_stable[is].addr_PA, ksm_stable[is].refcnt);
      }

      // update PTE
      int writable = PTE_FLAGS(*pte) & PTE_W;
      *pte = PA2PTE(ksm_stable[is].addr_PA) | (PTE_FLAGS(*pte) & ~PTE_W) | (writable ? PTE_COW : 0);
      sfence_vma(); // flush TLB
      kfree(pa);
      merged++;

      // delete if it's from unstable ksm node
      delete_ksm_unstable_node(pa);

      return;
    }
  }

  // search unstable list
  for(ius = 0; ius < ksm_unstable_cnt; ius++) {
    if(ksm_unstable[ius].hash_value == hash_value) {
      if(ksm_unstable[ius].addr_PA == pa) {
        // printf("\n");
        return;
      }
      // update corresponding PTE to points to shared addr
      pte_t *pte = walk(pagetable, va, 0);
      if(pte ==0 || (*pte & PTE_V) == 0)
        panic("insert_ksm_node: pte");

      // update PTE
      int writable = PTE_FLAGS(*(ksm_unstable[ius].pte)) & PTE_W;
      int writable2 = PTE_FLAGS(*pte) & PTE_W;
      *(ksm_unstable[ius].pte) = (*(ksm_unstable[ius].pte) & ~PTE_W) | (writable ? PTE_COW : 0);
      *pte = PA2PTE(ksm_unstable[ius].addr_PA) | (PTE_FLAGS(*pte) & ~PTE_W) | (writable2 ? PTE_COW : 0);
      sfence_vma(); // flush TLB
      kfree(pa);
      merged++;

      // add to stable list , delete unstable node if necessary
      move_ksm_unstable_to_stable(ius, pid, va);
      delete_ksm_unstable_node(pa);

      // printf("Hash match, (old pa: %p -> new pa: %p) (RC -> %d)\n", pa, ksm_unstable[ius].addr_PA, ksm_unstable[ius].refcnt+1);
      return;
    }
  }

  // if unstable node is changed
  for(ius = 0; ius < ksm_unstable_cnt; ius++) {
    if(ksm_unstable[ius].addr_PA == pa) {
      ksm_unstable[ius].hash_value = hash_value;
      // printf("Additional change in Existing Page (PA: %p)\n", pa);
      return;
    }
  }

  // add new unstable node
  // printf("Unique Page (PA: %p)\n", pa);
  ksm_unstable[ksm_unstable_cnt].addr_PA = pa;
  ksm_unstable[ksm_unstable_cnt].hash_value = hash_value;
  ksm_unstable[ksm_unstable_cnt].refcnt = 1;
  ksm_unstable[ksm_unstable_cnt].pte = walk(pagetable, va, 0);
  ksm_unstable[ksm_unstable_cnt].reverse[0].pid = pid;
  ksm_unstable[ksm_unstable_cnt].reverse[0].addr_VA = (void*)va;
  ksm_unstable_cnt++;
}

void
ksmd(void)
{
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->pid == 1 || p->pid == 2 || p->pid == cur_proc->pid) 
      continue;
    if(p->state != RUNNABLE && p->state != SLEEPING)
      continue;
    // printf("[KSM] ================= Scanning Process[%d] =================\n", p->pid);

    pagetable_t pagetable = p->pagetable;
    uint64 a, last = PGROUNDUP(p->sz);
    pte_t *pte;
    void *pa;

    for(a = 0; a < last; a += PGSIZE) {
      if((pte = walk(pagetable, a, 0)) == 0)
        panic("not mapped yet..?");
      pa = (void *)PTE2PA(*pte);
      
      insert_ksm_node(pa, p->pagetable, p->pid, a);
    }
  }
}

uint64
sys_ksm(void)
{
  uint64 arg_scanned;
  uint64 arg_merged;
  argaddr(0, &arg_scanned);
  argaddr(1, &arg_merged);

  scanned = merged = 0;
  cur_proc = myproc();
  ksmd();

  // copy from kernel space to user space
  copyout(cur_proc->pagetable, arg_scanned, (char*)&scanned, sizeof(scanned));
  copyout(cur_proc->pagetable, arg_merged, (char*)&merged, sizeof(merged));

  // debug_nodes();
  return freemem;
}