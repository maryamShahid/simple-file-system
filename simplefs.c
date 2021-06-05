#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include "simplefs.h"
#include "blocks.c"

#define DIR_ENTRY_SIZE 110
#define MAX_FILES 128
#define MAX_OPENED_FILES 16
#define MAX_OPENED_BITS 15
#define BITS 13
#define SUPERBLOCK_START 0
#define SUPERBLOCK_COUNT 1
#define BITMAP_START 1
#define DIR_START 5
#define DIR_COUNT 4
#define FCB_START 9
#define FCB_COUNT 4
#define FCB_SIZE 128
#define MAX_FCB 32768
#define MAX_FILE_SIZE 32768
#define DISK_POINTER_SIZE 4096
#define DIR_ENTRY_PER_BLOCK 32
#define MAX_BITS_SIZE 1024
#define MAX_BIT_SIZE_ACTUAL 1023

// Global Variables =======================================
int vdisk_fd; // Global virtual disk file descriptor. Global within the library.
              // Will be assigned with the vsfs_mount call.
              // Any function in this file can use this.
              // Applications will not use  this directly. 

struct FILES oft[MAX_OPENED_FILES];

// ========================================================

// read block k from disk (virtual disk) into buffer block.
// size of the block is BLOCKSIZE.
// space for block must be allocated outside of this function.
// block numbers start from 0 in the virtual disk. 
int read_block (void *block, int k) {
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = read(vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
        printf("read error\n");
        return -1;
    }
    return (0);
}

// write block k into the virtual disk. 
int write_block (void *block, int k) {
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = write(vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
        printf("write error\n");
        return (-1);
    }
    return 0;
}

/**********************************************************************
   The following functions are to be called by applications directly. 
***********************************************************************/

int create_format_vdisk (char *vdiskname, unsigned int m) {
    struct timeval current_time;
    gettimeofday(&current_time,NULL);
    double starting = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
    
    char command[1000];
    int totals;
    int blocked = 1;
    int iterate;
    int current;
    totals  = blocked << m;
    iterate = totals / BLOCKSIZE;
    sprintf (command, "dd if=/dev/zero of=%s bs=%d count=%d",
             vdiskname, BLOCKSIZE, iterate);
    system (command);
    void *medianBM = calloc(BLOCKSIZE, BLOCKSIZE);
    vdisk_fd = open(vdiskname, O_RDWR);
    void *packet = calloc(BLOCKSIZE, BLOCKSIZE);
    *(int *) packet = totals;
    current = write_block(packet, SUPERBLOCK_START);
    free(packet);
    if (current == -1) {
        free(medianBM);
        return -1;
    }
    unsigned int outLoop;
    for (outLoop = SUPERBLOCK_START; outLoop < BITS; ++outLoop)
        set_bit(medianBM, SUPERBLOCK_START, outLoop);
    current = write_block(medianBM, BITMAP_START);
    if (current == -1){
        free(medianBM);
        return -1;
    }
    packet = calloc(BLOCKSIZE / MAX_OPENED_FILES, BLOCKSIZE);
    void *existing = packet;
    for (outLoop = SUPERBLOCK_START; outLoop < MAX_FILES; ++outLoop){
        struct DIR dir;
        memset(dir.name, SUPERBLOCK_START, sizeof(dir.name));
        dir.index = -1;
        *(struct DIR *) existing = dir;
        existing += MAX_FILES;
    }
    for (outLoop = SUPERBLOCK_START; outLoop < DIR_COUNT; ++outLoop){
        current = write_block(packet, (outLoop + DIR_START));
        if (current == -1){
            free(packet);
            free(medianBM);
            return -1;
        }
    }
    free(packet);
    packet = calloc(BLOCKSIZE / MAX_FILES, BLOCKSIZE);
    existing = packet;
    for (outLoop = SUPERBLOCK_START; outLoop < FCB_SIZE; ++outLoop){
        struct FCB fcb;
        fcb.available = SUPERBLOCK_START;
        *(struct FCB *) existing = fcb;
        existing += FCB_SIZE;
    }
    for (outLoop = SUPERBLOCK_START; outLoop < FCB_COUNT; ++outLoop){
        current = write_block(packet, (outLoop + FCB_START));
        if (current == -1){
            free(packet);
            free(medianBM);
            return -1;
        }   
    }
    free(packet);
    fsync(vdisk_fd);
    close(vdisk_fd);
    free(medianBM);
    gettimeofday(&current_time,NULL);
    double ending = current_time.tv_usec * 0.001 + (current_time.tv_sec * 1000);
    printf("%s of size %u created in time: %lf miliseconds\n", vdiskname, 1 << m, ending - starting);
    return (0); 
}

int sfs_mount (char *vdiskname) {
    // simply open the Linux file vdiskname and in this
    // way make it ready to be used for other operations.
    // vdisk_fd is global; hence other function can use it.
    int i;
    for (i = 0; i < MAX_OPENED_FILES; ++i){
        oft[i].index = -1;
        oft[i].free = SUPERBLOCK_START;
        oft[i].size = SUPERBLOCK_START;
    }
    vdisk_fd = open(vdiskname, O_RDWR); 
    return(0);
}

int sfs_umount () {
    fsync (vdisk_fd);
    close (vdisk_fd);
    return (0); 
}

int sfs_create(char *filename) {
    int traverse = BLOCKSIZE * DIR_START;
    void *median = calloc(SUPERBLOCK_COUNT, MAX_FILES);
    lseek(vdisk_fd, (off_t) traverse, SEEK_SET);

    int size = 0;
    while (size < MAX_FILES) {
        read(vdisk_fd, median, MAX_FILES);
        if ((*(struct DIR *) median).index == -1) {
            strcpy((*(struct DIR *) median).name, filename);

            traverse = BLOCKSIZE * FCB_START;
            void *medianTwo = calloc(SUPERBLOCK_COUNT, FCB_SIZE);
            lseek(vdisk_fd, (off_t) traverse, SEEK_SET);

            int size2 = 0;
            while (size2 < FCB_SIZE) {
                read(vdisk_fd, medianTwo, FCB_SIZE);
                if ((*(struct FCB *) medianTwo).available == 0) {
                    (*(struct DIR *) median).index = size2;
                    (*(struct FCB *) medianTwo).available = 1;

                    void *medianThree = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
                    for (int iterateOut = SUPERBLOCK_COUNT; iterateOut <= FCB_COUNT; ++iterateOut) {
                        read_block(medianThree, iterateOut);
                        for (unsigned int iterate = SUPERBLOCK_START; iterate < MAX_FCB; ++iterate) {
                            if (get_bit(medianThree, SUPERBLOCK_START, iterate) == SUPERBLOCK_START) {
                                set_bit(medianThree, SUPERBLOCK_START, iterate);
                                write_block(medianThree, iterateOut);
                                (*(struct FCB *) medianTwo).index_block =
                                        iterate + ((iterateOut - SUPERBLOCK_COUNT) * (SUPERBLOCK_COUNT << MAX_OPENED_BITS));
                                (*(struct FCB *) medianTwo).size = SUPERBLOCK_START;

                                void *index_buff = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
                                void *index_curr = index_buff;
                                int k;
                                for (k = 0; k < (BLOCKSIZE / FCB_COUNT); ++k) {
                                    *(int *) index_curr = -1;
                                    index_curr += sizeof(int);
                                }

                                write_block(index_buff, (*(struct FCB *) medianTwo).index_block);
                                free(index_buff);
                                traverse = (FCB_START * BLOCKSIZE) + (FCB_SIZE * size2);
                                lseek(vdisk_fd, (off_t) traverse, SEEK_SET);
                                write(vdisk_fd, medianTwo, FCB_SIZE);
                                traverse = (DIR_START * BLOCKSIZE) + (FCB_SIZE * size);
                                lseek(vdisk_fd, (off_t) traverse, SEEK_SET);
                                write(vdisk_fd, median, FCB_SIZE);
                                free(medianThree);
                                free(medianTwo);
                                free(median);
                                return 0;
                            }
                        }
                    }
                    free(medianThree);
                    free(medianTwo);
                    free(median);
                    return -1;
                }
                size2++;
            }
            free(medianTwo);
            free(median);
            return -1;
        }
        size++;
    }
    free(median);
    return -1;
}

int sfs_open(char *file, int mode) {
    int inLoop;
    for (inLoop = SUPERBLOCK_START; inLoop < MAX_FILES; ++inLoop) {
        if (oft[inLoop].index == SUPERBLOCK_START) {
            void *pack = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
            void *traverse;
            int outLoop;
            for (outLoop = SUPERBLOCK_START; outLoop < FCB_COUNT; ++outLoop) {
                traverse = pack;
                read_block(traverse, outLoop + DIR_START);
                int midLoop;
                for (midLoop = SUPERBLOCK_START; midLoop < MAX_FILES; ++midLoop) {
                    if (strcmp((*(struct DIR *) traverse).name, file) == SUPERBLOCK_START &&
                        (*(struct DIR *) traverse).index != -1) {
                        oft[inLoop].index = (*(struct DIR *) traverse).index;
                        int positions = (*(struct DIR *) traverse).index / DIR_ENTRY_PER_BLOCK;
                        void *medFB = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
                        read_block(medFB, (positions + FCB_START));
                        oft[inLoop].size = (*(struct FCB *) (medFB + ((*(struct DIR *) traverse).index % DIR_ENTRY_PER_BLOCK) * FCB_SIZE)).size;
                        break;
                    }
                    traverse += FCB_SIZE;
                }
                if (oft[inLoop].index != -1) {
                    break;
                }
            }
            free(pack);
            if (outLoop >= FCB_COUNT)
                return -1;
            strcpy(oft[inLoop].name, file);
            oft[inLoop].mode = mode;
            oft[inLoop].free = SUPERBLOCK_COUNT;
            if (mode == MODE_READ)
                oft[inLoop].offset = SUPERBLOCK_START;
            else
                oft[inLoop].offset = oft[inLoop].size % DISK_POINTER_SIZE;
            oft[inLoop].total = SUPERBLOCK_START;
            break;
        }
    }
    if (inLoop >= MAX_OPENED_FILES)
        return -1;
    else
        return inLoop;
}

int sfs_close(int fd) {
    oft[fd].index = SUPERBLOCK_START;
    int index = oft[fd].index;
    int packing = index / DIR_ENTRY_PER_BLOCK;
    void *medians = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
    read_block(medians, (packing + FCB_START));
    (*(struct FCB *) (medians + MAX_FILES * (index % DIR_ENTRY_PER_BLOCK))).size = sfs_getsize(fd);
    write_block(medians, (packing + FCB_START));
    free(medians);
    return 0;
}

int sfs_getsize (int  fd) {
    int index = oft[fd].index;
    int blkPos = index / DIR_ENTRY_PER_BLOCK;
    int traverseFCB = (blkPos + FCB_START) * BLOCKSIZE + (index % DIR_ENTRY_PER_BLOCK) * FCB_SIZE;
    void *medianFC = calloc(SUPERBLOCK_COUNT, FCB_SIZE);
    lseek(vdisk_fd, (off_t) traverseFCB, SEEK_SET);
    read(vdisk_fd, medianFC, FCB_SIZE);
    int blocking = (*(struct FCB *) medianFC).index_block;
    free(medianFC);
    void *medianPos = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
    read_block(medianPos, blocking);
    void *locate = medianPos;
    int used = SUPERBLOCK_START;
    while ((used * FCB_COUNT) < BLOCKSIZE && *(int *) (locate) != -1) {
        used++;
        locate += sizeof(int);
    }
    int trv = oft[fd].offset;
    free(medianPos);
    if (trv != SUPERBLOCK_START)
        return (used - SUPERBLOCK_COUNT) * BLOCKSIZE + trv;
    return used * BLOCKSIZE;
}

int sfs_read(int fd, void *buf, int n) {
    if (oft[fd].mode == MODE_READ) {
        int indexing = oft[fd].index;
        int block_num = indexing / DIR_ENTRY_PER_BLOCK;
        int traverseFCB = (block_num + FCB_START) * BLOCKSIZE + (indexing % DIR_ENTRY_PER_BLOCK) * FCB_SIZE;
        void *medianFCB = calloc(SUPERBLOCK_COUNT, FCB_SIZE);
        lseek(vdisk_fd, (off_t) traverseFCB, SEEK_SET);
        read(vdisk_fd, medianFCB, FCB_SIZE);
        int index_block = (*(struct FCB *) medianFCB).index_block;
        free(medianFCB);
        void *receiveIn = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
        read_block(receiveIn, index_block);
        int initial = SUPERBLOCK_START;
        for (int iter = SUPERBLOCK_START; iter < MAX_BITS_SIZE && initial < (oft[fd].total); iter++) {
            if (*(((int *) receiveIn) + iter) != -1) {
                initial++;
            } else {
                return -1;
            }
        }
        int sizeLeft = oft[fd].size - (oft[fd].total * BLOCKSIZE + oft[fd].offset);
        int readLeft = n;
        int startingS = sizeLeft;
        if (sizeLeft == SUPERBLOCK_START) {
            return 0;
        }
        void *checking = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
        for (int i = initial; i < MAX_BITS_SIZE; i++) {
            if (*(((int *) receiveIn) + i) != -1) {
                read_block(checking, i);
                if (readLeft < sizeLeft) {
                    if (DISK_POINTER_SIZE - oft[fd].offset <= readLeft) {
                        memcpy(buf + (n - readLeft), checking + oft[fd].offset,
                               DISK_POINTER_SIZE - oft[fd].offset);
                        sizeLeft -= DISK_POINTER_SIZE - oft[fd].offset;
                        readLeft -= DISK_POINTER_SIZE - oft[fd].offset;
                        oft[fd].offset = SUPERBLOCK_START;
                        oft[fd].total++;
                    } else {
                        memcpy(buf + (n - readLeft), checking + oft[fd].offset, readLeft);
                        oft[fd].offset = oft[fd].offset + readLeft;
                        free(checking);
                        return n;
                    }
                } else {
                    if (sizeLeft > DISK_POINTER_SIZE - oft[fd].offset) {
                        memcpy(buf + (n - readLeft), checking + oft[fd].offset,
                               DISK_POINTER_SIZE - oft[fd].offset);
                        sizeLeft -= DISK_POINTER_SIZE - oft[fd].offset;
                        readLeft -= DISK_POINTER_SIZE - oft[fd].offset;
                        oft[fd].offset = SUPERBLOCK_START;
                        oft[fd].total++;
                    } else {
                        memcpy(buf + (n - readLeft), checking + oft[fd].offset, readLeft);
                        oft[fd].offset += readLeft;
                        free(checking);
                        return startingS;
                    }
                }
            } else {
                printf("Error\n");
                free(checking);
                return -1;
            }
        }
    }
    return -1;
}

int sfs_append(int fd, void *buf, int n) {
    if (oft[fd].mode == MODE_APPEND) {
        int index = oft[fd].index;
        int blkIndex = index / DIR_ENTRY_PER_BLOCK;
        int traverseFCB = (blkIndex + FCB_START) * BLOCKSIZE + (index % DIR_ENTRY_PER_BLOCK) * FCB_SIZE;
        void *medianFCB = calloc(SUPERBLOCK_COUNT, FCB_SIZE);
        lseek(vdisk_fd, (off_t) traverseFCB, SEEK_SET);
        read(vdisk_fd, medianFCB, FCB_SIZE);
        int blkNum = (*(struct FCB *) medianFCB).index_block;
        free(medianFCB);
        int blx = n;
        void *median = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
        read_block(median, blkNum);
        int loop;
        for (loop = SUPERBLOCK_START; blx > SUPERBLOCK_START && loop < MAX_BIT_SIZE_ACTUAL; ++loop) {
            if ((oft[fd].offset == SUPERBLOCK_START && *(int *) (median + loop * sizeof(int)) == -1) ||
                (oft[fd].offset > SUPERBLOCK_START && *(int *) (median + loop * sizeof(int)) != -1 &&
                 *(int *) (median + (loop + SUPERBLOCK_COUNT) * sizeof(int)) == -1)) {
                if (*(int *) (median + loop * sizeof(int)) == -1) {
                    void *medianBmp = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
                    int cuz, alpha = SUPERBLOCK_COUNT, beta = SUPERBLOCK_START;
                    while (alpha <= FCB_COUNT && !beta) {
                        read_block(medianBmp, alpha);
                        for (cuz = SUPERBLOCK_START; cuz < MAX_FILE_SIZE; cuz++) {
                            if (get_bit(medianBmp, SUPERBLOCK_START, cuz) == SUPERBLOCK_START) {
                                set_bit(medianBmp, SUPERBLOCK_START, cuz);
                                write_block(medianBmp, alpha);
                                int *setting = (int *) median;
                                setting[loop] = cuz;
                                beta = SUPERBLOCK_COUNT;
                                write_block(median, blkNum);
                                break;
                            }
                        }
                        alpha++;
                        free(medianBmp);
                    }
                    if (!beta) {
                        printf("ERROR - Disk Full\n");
                        free(median);
                        return -1;
                    }
                }
                void *textEdit = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
                int *checkPos = (int *) median;
                read_block(textEdit, checkPos[loop]);
                if (blx <= DISK_POINTER_SIZE - oft[fd].offset) {
                    memcpy(textEdit + oft[fd].offset, buf + n - blx, blx);
                    oft[fd].offset = (oft[fd].offset + blx) % DISK_POINTER_SIZE;
                    write_block(textEdit, checkPos[loop]);
                    blx = SUPERBLOCK_START;
                    free(textEdit);
                    free(median);
                    return n;
                } else {
                    memcpy(textEdit + oft[fd].offset, buf + n - blx,
                           DISK_POINTER_SIZE - oft[fd].offset);
                    blx = blx - DISK_POINTER_SIZE + oft[fd].offset;
                    oft[fd].offset = SUPERBLOCK_START;
                    write_block(textEdit, checkPos[loop]);
                    free(textEdit);
                }
            }
        }
        if (*(int *) (median + MAX_BIT_SIZE_ACTUAL * sizeof(int)) == -1) {
            void *medianBmp = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
            int cuz, alpha = SUPERBLOCK_COUNT, beta = SUPERBLOCK_START;
            while (alpha <= FCB_COUNT && !beta) {
                read_block(medianBmp, alpha);
                for (cuz = SUPERBLOCK_START; cuz < MAX_FILE_SIZE; cuz++) {
                    if (get_bit(medianBmp, SUPERBLOCK_START, cuz) == SUPERBLOCK_START) {
                        set_bit(medianBmp, SUPERBLOCK_START, cuz);
                        write_block(medianBmp, alpha);
                        int *as = (int *) median;
                        as[loop] = cuz;
                        beta = SUPERBLOCK_COUNT;
                        write_block(median, blkNum);
                        break;
                    }
                }
                alpha++;
                free(medianBmp);
            }
            if (!beta) {
                printf("ERROR - DISK FULL\n");
                free(median);
                return -1;
            }
            void *textEdit = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
            read_block(textEdit, *(int *) (median + MAX_BIT_SIZE_ACTUAL * sizeof(int)));
            if (blx > DISK_POINTER_SIZE) {
                memcpy(textEdit, buf + n - blx, DISK_POINTER_SIZE);
                blx -= DISK_POINTER_SIZE;
                write_block(textEdit, *(int *) (median + MAX_BIT_SIZE_ACTUAL * sizeof(int)));
                free(textEdit);
                free(median);
                return n - blx;
            }
            memcpy(textEdit, buf + n - blx, blx);
            oft[fd].offset = blx;
            write_block(textEdit, *(int *) (median + MAX_BIT_SIZE_ACTUAL * sizeof(int)));
            free(textEdit);
            free(median);
            return n;
        } else if ((oft[fd].offset > SUPERBLOCK_START && *(int *) (median + MAX_BIT_SIZE_ACTUAL * sizeof(int)) != -1)) {
            void *textEdit = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
            read_block(textEdit, *(int *) (median + MAX_BIT_SIZE_ACTUAL * sizeof(int)));
            if (blx > DISK_POINTER_SIZE - oft[fd].offset) {
                memcpy(textEdit + oft[fd].offset, buf + n - blx,
                       DISK_POINTER_SIZE - oft[fd].offset);
                oft[fd].offset = SUPERBLOCK_START;
                blx = blx - DISK_POINTER_SIZE + oft[fd].offset;
                write_block(textEdit, *(int *) (median + MAX_BIT_SIZE_ACTUAL * sizeof(int)));
                free(textEdit);
                free(median);
                return n - blx;
            } else {
                memcpy(textEdit + oft[fd].offset, buf + n - blx, blx);
                write_block(textEdit, *(int *) (median + MAX_BIT_SIZE_ACTUAL * sizeof(int)));
                free(textEdit);
                free(median);
                return n;
            }
        }
        free(median);
        return n - blx;
    } else
        return -1;
}

int sfs_delete(char *filename) {
    void *median = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
    int outLoop;
    for (outLoop = SUPERBLOCK_START; outLoop < FCB_COUNT; ++outLoop) {
        read_block(median, outLoop + DIR_START);
        int inLoop;
        for (inLoop = SUPERBLOCK_START; inLoop < DIR_ENTRY_PER_BLOCK; ++inLoop) {
            if (strcmp((*(struct DIR *) (median + inLoop * FCB_SIZE)).name, filename) == SUPERBLOCK_START) {
                int index = (*(struct DIR *) (median + inLoop * FCB_SIZE)).index;
                (*(struct DIR *) (median + inLoop * FCB_SIZE)).index = -1;
                write_block(median, outLoop + DIR_START);
                int blkInd = index / DIR_ENTRY_PER_BLOCK;
                int blkTraverse = index % DIR_ENTRY_PER_BLOCK;
                read_block(median, blkInd + FCB_START);
                int posBlk = (*(struct FCB *) (median + FCB_SIZE * blkTraverse)).index_block;
                (*(struct FCB *) (median + FCB_SIZE * blkTraverse)).available = SUPERBLOCK_START;
                write_block(median, blkInd + FCB_START);
                read_block(median, posBlk);
                int *iterate = (int *) median;
                int led;
                for (led = SUPERBLOCK_START; led < MAX_BITS_SIZE && iterate[led] != -1; ++led) {
                    int bmpOne = iterate[led];
                    int bmpTwo = bmpOne / (SUPERBLOCK_COUNT << MAX_OPENED_BITS);
                    int bmpThree = bmpOne % (SUPERBLOCK_COUNT << MAX_OPENED_BITS);
                    void *bmpMedian = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
                    read_block(bmpMedian, bmpTwo + SUPERBLOCK_COUNT);
                    clear_bit(bmpMedian, SUPERBLOCK_START, bmpThree);
                    write_block(bmpMedian, bmpTwo + SUPERBLOCK_COUNT);
                    free(bmpMedian);
                    iterate[led] = -1;
                }
                int bmpOne = posBlk;
                int bmpTwo = bmpOne / (SUPERBLOCK_COUNT << MAX_OPENED_BITS);
                int bmpThree = bmpOne % (SUPERBLOCK_COUNT << MAX_OPENED_BITS);
                void *bmpMedian = calloc(SUPERBLOCK_COUNT, BLOCKSIZE);
                read_block(bmpMedian, bmpTwo + SUPERBLOCK_COUNT);
                clear_bit(bmpMedian, SUPERBLOCK_START, bmpThree);
                write_block(bmpMedian, bmpTwo + SUPERBLOCK_COUNT);
                free(bmpMedian);
                write_block(median, posBlk);
                free(median);
                return 0;
            }
        }
    }
    free(median);
    return (-1);
}