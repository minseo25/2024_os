#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096

void do_ksm() {
    int scanned, merged, freemem;
    freemem = ksm(&scanned, &merged);
    printf("ksm: scanned=%d, merged=%d, freemem=%d\n\n", scanned, merged, freemem);
}

void main() {
    char *dyn_mem = (char *)malloc(PGSIZE);
    if (dyn_mem == 0) {
        printf("malloc failed\n");
        exit(-1);
    }

    dyn_mem[0] = 'D';

    if (fork() == 0) { // Child process
        do_ksm();
        sleep(5);
        dyn_mem[0] = 'd';
        do_ksm(); // Perform KSM after modification
        printf("Child: %c\n", dyn_mem[0]);
        free(dyn_mem);
        exit(0);
    } else {
        sleep(1);
        do_ksm();
        wait(0);
        printf("Parent: %c\n", dyn_mem[0]);
        free(dyn_mem);
        exit(0);
    }
}
