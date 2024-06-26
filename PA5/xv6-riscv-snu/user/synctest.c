#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void testSyncCall() {
    int fd;

    fd = open("tempfile", O_CREATE | O_RDWR);
    if(fd >= 0) {
        write(fd, "Hello, xv6!", 11);
        close(fd);
        printf("File created and written.\n");

        sync();
        printf("Sync called: File changes are now persistent.\n");
    } else {
        printf("Failed to create file.\n");
    }
}

void testWithoutSyncCall() {
    int fd;

    fd = open("tempfile2", O_CREATE | O_RDWR);
    if(fd >= 0) {
        write(fd, "Hello, again!", 13);
        close(fd);
        printf("File created and written without sync.\n");
    } else {
        printf("Failed to create file.\n");
    }
}

int main(void) {
    testSyncCall();
    // testWithoutSyncCall();
    exit(0);
}