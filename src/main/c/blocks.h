#pragma once

#include <jni.h>

enum block_type {
    BEDROCK,
    RAW,
    ORE,
    FILL,
    STONE,
    DEEPSLATE,
};

typedef struct block_entry block_entry;
struct block_entry {
    int x, y, z;
    int typ;
};

extern "C" JNIEXPORT void crack(int size, block_entry* entries, int mode);
