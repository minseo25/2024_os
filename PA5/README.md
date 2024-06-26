# 4190.307 Operating Systems (Spring 2024)
# Project #5: FATty File System
### Due: 11:59 PM, June 22 (Saturday)

## Introduction

Microsoft's early FAT file system has a unique file index structure based on the ___File Allocation Table (FAT)___. The goal of this project is to implement the "FATty" file system, which modifies the file index structure of the existing `xv6` file system to resemble that of the FAT file system.

## Background

### Microsoft FAT File System

The FAT file system, developed by Microsoft in 1977, is one of the earliest and simplest file systems. FAT uses a table at the beginning of a disk to manage files and directories. Each entry in the table corresponds to a cluster (i.e., a block), a fixed-size unit of disk storage. The table maintains pointers to the next cluster in a file, allowing for sequential access and easy file allocation.

FAT exists in several versions, including FAT12, FAT16, and FAT32, each extending the maximum storage capacity and improving performance. FAT32, for example, supports larger disks and smaller cluster sizes, making it suitable for a wider range of applications. Despite being somewhat outdated, FAT remains relevant in specific contexts, such as USB drives and memory cards, due to its broad operating system support and simplicity. For more information on the FAT file system, you can visit [this page](https://en.wikipedia.org/wiki/File_Allocation_Table).

### File Allocation Table (FAT)

The name of the FAT file system itself stems from a unique data structure called the File Allocation Table (FAT). This table, located at the beginning of the disk, contains information on the file index, specifically the locations of the blocks belonging to each file or directory. 
Assume that the first block number for the file `foo` is 2, and for the file `bar` is 5. In the FAT file system, this information on the first block number is recorded in the directory entry.

Let's consider the following FAT example. The FAT has an entry for each block, and each entry points to the next block number in the file. From the directory entry, we know that the file `foo` starts at  block 2. Then, `FAT[2]` has the next block number for the file, which is 3 in this case. Similarly, the subsequent block numbers can be found by looking up the FAT in sequence: `FAT[3] = 4`, `FAT[4] = 7`, and `FAT[7] = 8`. Finally, `FAT[8]` has the value of -1 which denotes the end of the file. Therefore, we can see that the file `foo` is stored in five blocks: 2, 3, 4, 7, and 8. In the same way, we can find that the file `bar` is located at blocks 5, 6, 9, and 10. The FAT entry marked as zero indicates that the corresponding block has not been allocated to any file yet. Additionally, reserved blocks have the value of -2 in the corresponding FAT entries. The size of each FAT entry is 12 bits, 16 bits, and 32 bits in FAT12, FAT16, and FAT32, respectively.


```
FAT[0]  FAT[1]  FAT[2]  FAT[3]  FAT[4]  FAT[5]  FAT[6]  FAT[7]
 -2      -2       3       4       7       6       9       8 

FAT[8]  FAT[9]  FAT[10] FAT[11] FAT[12] FAT[13] FAT[14] FAT[15]
 -1       10     -1       0       0       0       0       0 

...
``` 

## The FATty File System

### The On-disk Layout

The following illustrates the on-disk layout of the current `xv6` file system when the file system size is set to 2000 blocks (`FSSIZE` in `kernel/param.h`). 

```
+---+---+-------+------+---+----------------------------------------+
| B | S |   L   |  I   | M |                D                       |
+---+---+-------+------+---+----------------------------------------+
```
* `B`: Boot block (1 block) -- Not used
* `S`: Superblock (1 block)
* `L`: Log blocks (30 blocks) (cf. `LOGSIZE` @ `kernel/param.h`)
* `I`: Inode blocks (13 blocks) (cf. `NINODES` inodes @ `kernel/param.h`)
* `M`: Free bitmap blocks (1 block) (cf. `FSSIZE` blocks @ `kernel/param.h`)
* `D`: Data blocks (1954 blocks) 

In contrast, the on-disk layout of the new FATty file system is as follows. After the Log (`L`) blocks, the FATty file system has a region designated for FAT (`F`) blocks. FAT entries cover the entire file system blocks, with each FAT entry being 32 bits long. Therefore, one FAT block is required for every 256 file system blocks. (Note: The block size is set to 1024 bytes in `xv6`. Refer to `BSIZE` in `kernel/fs.h`.)

Unlike the original `xv6` file system, the FATty file system does not require the Free bitmap (`M`) blocks because the FAT blocks contain the information on the free data blocks. 

```
+---+---+-------+------+---+----------------------------------------+
| B | S |   L   |  F   | I |                D                       |
+---+---+-------+------+---+----------------------------------------+
```
* `B`: Boot block (1 block) -- Not used
* `S`: Superblock (1 block) 
* `L`: Log blocks (30 blocks) (cf. `LOGSIZE` @ `kernel/param.h`)
* `F`: FAT blocks (8 blocks) (cf. `FSSIZE` blocks @ `kernel/param.h`)
* `I`: Inode blocks (4 blocks) (cf. `NINODES` inodes @ `kernel/param.h`)
* `D`: Data blocks (1956 blocks) 

### Minor Changes from the FAT File System

0. The FATty file system has a magic number `0x46415459`, where each byte represents 'F', 'A', 'T', and 'Y', respectively.

1. In the original FAT file system, the File Allocation Table (FAT) is duplicated to provide redundancy and reliability, protecting against data corruption. In the FATty file system, however, we maintain only one copy of the FAT blocks for simplicity.

3. Each FAT entry is encoded as a signed 32-bit integer with the following values:
   * Positive values (> 0): denote the next block number
   * Zero (0): denotes the end of the file
   * Negative one (-1): indicates that the corresponding blocks are reserved (this applies to the entries for the boot block, superblock, log blocks, FAT blocks, and inode blocks)

3. In the original FAT file system, the first block number is kept in the directory entry. However, in the FATty file system, this information is stored in the inode's `startblk` field.

4. The unallocated (free) data blocks are also linked together via FAT entries. The head of the free block list is maintained in the `freehead` field of the superblock. The total number of free blocks is stored in the superblock's `freeblks` field.

5. When the FATty file system is mounted, the superblock and FAT blocks are read into the memory. During file system operations, only the in-memory versions of the superblock and FAT blocks are updated. To make these updates persistent, users must explicitly call the `sync()` system call. If not, any modifications to the superblock and FAT blocks will be lost.
   

### Superblocks and Inode for the FATty file system

The superblock of the FATty file system contains the following information.

```C
// @ kernel/fs.h
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
#ifdef SNU
  uint nfat;         // Number of FAT blocks
  uint fatstart;     // Block number of first FAT block
  uint freehead;     // Head of the free block list
  uint freeblks;     // Number of free data blocks
#else
  uint bmapstart;    // Block number of first free map block -- Not needed for PA5
#endif
};
```

The inode of the FATty file system contains the following information.

```C
// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
#ifdef SNU
  uint startblk;        // First block number
#else
  uint addrs[NDIRECT+1];   // Data block addresses -- Not needed for PA5
#endif
};
```


## Problem specification

### 1. Modify the `mkfs` tool to create the correct on-disk image (20 points)

To set up the FATty file system, you need to start by modifying the `mkfs/mkfs.c` file to generate the correct image file, `fs.img`. This image file should include the new FAT blocks and exclude the Free bitmap blocks. The FAT blocks must be positioned between the Log blocks and the Inode blocks. Additionally, you should correctly initialize the free block list and the corresponding superblock fields such as `freehead` and `freeblks`. Please note that the file system size (`FSSIZE` in `kernel/param.h`), the number of inodes (`NINODES` in `kernel/param.h`), or the number/size of pre-installed programs (`UPROGS` in `Makefile`) can vary. 

### 2. Replace the file index structure with FAT (60 points)

The current `xv6` file system uses 12 direct pointers and one indirect pointer within the inode to store the locations of data blocks associated with a file or directory. In the FATty file system, however, each inode only contains a pointer (`startblk`) to the first data block. The subsequent block locations should be looked up in the FAT. Additionally, you need to modify the data block allocation and deallocation functions to utilize the new free list. When a data block is allocated or deallocated, ensure that the superblock's `freeblks` value is updated accordingly. The skeleton code includes  functionality to print this value whenever you press `^f` (`ctrl-f`) in the console. This value will be checked to determine whether your implementation has space leaks or not during various file system operations. 

We provide you with a sample FATty file system image, `fs.img-fatty`, in the skeleton code so that you can begin Part 2 of this project before completing Part 1.

### 3. Implement the `sync()` system call (10 points)

The final task is to implement the `sync()` system call. The system call number of `sync()` is already assigned to 22 in the `./kernel/syscall.h` file. When a file or directory is created, deleted, or updated, the associated inode blocks and data blocks are written into the disk immediately, with the help of the Logging Layer (see Chap. 8.4 of the [xv6 book](http://csl.snu.ac.kr/courses/4190.307/2024-1/book-riscv-rev3.pdf)). However, for the superblock and FAT blocks, we chose to update only their in-memory versions to improve overall performance. The role of the `sync()` system call is to write the contents of the superblock and FAT blocks to the disk to make them persistent. You don't need to care about sudden power failures during the `sync()` system call. 

__SYNOPSYS__
```
    void sync(void);
```

__DESCRIPTION__

The `sync()` system call causes all pending modifications to the superblock and FAT blocks to be written to the disk. 

__RETURN VALUE__

* The `sync()` system call is always successful.

### 4. Design document (10 points)

You need to prepare and submit the design document (in a single PDF file) for your implementation. Your design document should include the following:

1. New data structures
   * Describe any new data structures introduced for this project.
   * Explain the purpose and functionality of each data structure.
   * Provide insights into why these data structures were necessary and how they contribute to the implementation.

2. Algorithm design
   * Describe the algorithm used for implementing the FATty file system.
   * Describe the strategy used to synchronize simultaneous accesses to the FAT.
   * Explain any corner cases you have considered and the strategies you used to address them.
   * Discuss any efforts you made to enhance the code efficiency, both in terms of time and space.

3. Testing and validation
   * Describe the testing strategy used to verify the implementation of the FATty file system.
   * Discuss how you verify the correct handling of the corner cases highlighted in Section 2.


## Restrictions

* Your implementation should pass `usertests` on multi-processor RISC-V systems (i.e., `CPUS` > 1).
* There should be no space leaks in the file system. 
* You only need to modify files in the `./kernel` and `./mkfs` directories. Any changes made outside these directories will be ignored during grading.

## Skeleton code

The skeleton code for this project assignment (PA5) is available as a branch named `pa5`. Therefore, you should work on the `pa5` branch as follows:

```
$ git clone https://github.com/snu-csl/xv6-riscv-snu
$ git checkout pa5
```

After downloading, you must first set your `STUDENTID` in the `Makefile` again.

The `pa5` branch includes a sample FATty file system image, `fs-fatty.img`. Using this image file, you can start Part 2 without completing Part 1 of this project. If you want to use this image file, copy it to `fs.img` before running `xv6`.

Note: The current skeleton code is unable to build the kernel image due to the changes in the inode and superblock structures.


## Tips

* Read Chap. 8 of the [xv6 book](http://csl.snu.ac.kr/courses/4190.307/2024-1/book-riscv-rev3.pdf) to understand the file system in `xv6`.

* For your reference, the following roughly shows the amount of changes you need to make for this project assignment. Each `+` symbol indicates 1~5 lines of code that should be added, deleted, or altered.
```
 kernel/defs.h    | +
 kernel/fs.c      | +++++++++++++++++
 mkfs/mkfs.c      | ++++++++++++++++++
```
  
## Hand in instructions

* First, make sure you are on the `pa5` branch in your `xv6-riscv-snu` directory. And then perform the `make submit` command to generate a compressed tar file named `xv6-{PANUM}-{STUDENTID}.tar.gz` in the `../xv6-riscv-snu` directory. Upload this file with your design document (in PDF format) to the submission server.
  
* The total number of submissions for this project assignment will be limited to 50. Only the version marked as `FINAL` will be considered for the project score. Please remember to designate the version you wish to submit using the `FINAL` button. 
  
* Note that the submission server is only accessible inside the SNU campus network. If you want off-campus access (from home, cafe, etc.), you can add your IP address by submitting a Google Form whose URL is available in the eTL. Now, adding your new IP address is automated by a script that periodically checks the Google Form at minutes 0, 20, and 40 at hours between 09:00 and 00:40 the following day and at every hour (minute 0) between 01:00 and 09:00.
     + If you cannot reach the server a minute after the update time, send the request again, as you might have sent the wrong IP address.
     + If you still cannot access the server after a while, that is likely due to an error in the automated process. The TAs will check if the script is properly running, but that is a ___manual___ process, so please do not expect it to be completed immediately.


## Logistics

* You will work on this project alone.
* Only the upload submitted before the deadline will receive the full credit. 25% of the credit will be deducted for every single day delayed.
* __You can use up to _3 slip days_ during this semester__. If your submission is delayed by one day and you decide to use one slip day, there will be no penalty. In this case, you should explicitly declare the number of slip days you want to use in the QnA board of the submission server right after each submission. Once slip days have been used, they cannot be canceled later, so saving them for later projects is highly recommended!
* Any attempt to copy others' work will result in a heavy penalty (for both the copier and the originator). Don't take a risk.

Have fun!

[Jin-Soo Kim](mailto:jinsoo.kim_AT_snu.ac.kr)  
[Systems Software and Architecture Laboratory](http://csl.snu.ac.kr)  
[Dept. of Computer Science and Engineering](http://cse.snu.ac.kr)  
[Seoul National University](http://www.snu.ac.kr)
