#define DIR_ENTRY_SIZE 110
#define BITMAP_SIZE 4096
#define BIT_OFFSET 8

struct DIR {
    char name[DIR_ENTRY_SIZE];
    int index;
};

struct FCB {
    int available;
    int index_block;
    int size;
};

struct FILES {
    int free;
    char name[DIR_ENTRY_SIZE];
    int index;
    int mode;
    int offset;
    int size;
    int total;
};

void set_bit(void *bitmap, int nblock, unsigned int bit_index) {
    void *curr = bitmap + BITMAP_SIZE * nblock;
    int char_index = bit_index / BIT_OFFSET;
    int index = 7 - (bit_index % BIT_OFFSET);
    curr = curr + char_index * sizeof(char);
    *(char *) curr |= 1UL << index;
}

int get_bit(void *bitmap, int nblock, unsigned int bit_index) {
    void *curr = bitmap + BITMAP_SIZE * nblock;
    int char_index = bit_index / BIT_OFFSET;
    int index = 7 - (bit_index % BIT_OFFSET);
    curr = curr + char_index * sizeof(char);
    if ((*(char *) curr & 1UL << index) == 0)
        return 0;
    else
        return 1;
}

void clear_bit(void *bitmap, int nblock, unsigned int bit_index) {
    void *curr = bitmap + BITMAP_SIZE * nblock;
    int char_index = bit_index / BIT_OFFSET;
    int index = 7 - (bit_index % BIT_OFFSET);
    curr = curr + char_index * sizeof(char);
    *(char *) curr &= ~(1UL << index);
}