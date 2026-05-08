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

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void bedrockseed$crack(int size, block_entry* entries, int mode);

#ifdef __cplusplus
}
#endif
