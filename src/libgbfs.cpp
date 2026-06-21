/*
 * libgbfs.cpp
 * Access objects in a GBFS file archive safely.
 */

#include "gbfs.h"
#include <cstring>
#include <cstdlib>  // <-- FIX: Brings in bsearch

// -------------------- SEARCH CONFIGURATION --------------------
#define GBFS_1ST_SEARCH_LIMIT ((const uint32_t *)0x02040000)
#define GBFS_2ND_SEARCH_START ((const uint32_t *)0x08000000)
#define GBFS_2ND_SEARCH_LIMIT ((const uint32_t *)0x0a000000)
#define GBFS_ALIGNMENT 256

// -------------------- FIND GBFS ARCHIVE --------------------
const GBFS_FILE *find_first_gbfs_file(const void *start)
{
    const uint32_t *here = (const uint32_t *)((uintptr_t)start & -(uintptr_t)GBFS_ALIGNMENT);
    const char rest_of_magic[] = "ightGBFS\r\n\x1a\n";

    // 1. Scan Multiboot/EWRAM Space
    while(here < GBFS_1ST_SEARCH_LIMIT)
    {
        if(*here == 0x456e6950) // Little-endian "PinE"
        {
            if(!std::memcmp(here + 1, rest_of_magic, 12))
            {
                return reinterpret_cast<const GBFS_FILE *>(here);
            }
        }
        here += GBFS_ALIGNMENT / sizeof(*here);
    }

    // 2. Scan Game Pak ROM Space
    if(here < GBFS_2ND_SEARCH_START)
    {
        here = GBFS_2ND_SEARCH_START;
    }

    while(here < GBFS_2ND_SEARCH_LIMIT)
    {
        if(*here == 0x456e6950)
        {
            if(!std::memcmp(here + 1, rest_of_magic, 12))
            {
                return reinterpret_cast<const GBFS_FILE *>(here);
            }
        }
        here += GBFS_ALIGNMENT / sizeof(*here);
    }

    return nullptr;
}

// -------------------- SKIP ARCHIVE --------------------
const void *skip_gbfs_file(const GBFS_FILE *file)
{
    if(!file) return nullptr;
    return reinterpret_cast<const uint8_t *>(file) + file->total_len;
}

// -------------------- BSEARCH UTILITIES --------------------
static int namecmp(const void *a, const void *b)
{
    return std::memcmp(a, b, 64);
}

// -------------------- GET OBJECT BY NAME --------------------
const void *gbfs_get_obj(const GBFS_FILE *file, const char *name, uint32_t *len)
{
    if(!file || !name) return nullptr;

    char key[64] = {0};
    for(int i = 0; i < 64 && name[i] != '\0'; ++i)
    {
        key[i] = name[i];
    }

    const GBFS_ENTRY *dirbase = reinterpret_cast<const GBFS_ENTRY *>(
        reinterpret_cast<const uint8_t *>(file) + file->dir_off
    );

    // Using standard global bsearch to comply smoothly across toolchains
    const GBFS_ENTRY *here = reinterpret_cast<const GBFS_ENTRY *>(
        bsearch(key, dirbase, file->dir_nmemb, sizeof(GBFS_ENTRY), namecmp)
    );

    if(!here) return nullptr;

    if(len)
    {
        *len = here->len;
    }

    return reinterpret_cast<const uint8_t *>(file) + here->data_offset;
}

// -------------------- GET OBJECT BY INDEX --------------------
const void *gbfs_get_nth_obj(const GBFS_FILE *file, size_t n, char *name, uint32_t *len)
{
    if(!file) return nullptr;

    if(n >= file->dir_nmemb) return nullptr;

    const GBFS_ENTRY *dirbase = reinterpret_cast<const GBFS_ENTRY *>(
        reinterpret_cast<const uint8_t *>(file) + file->dir_off
    );
    
    const GBFS_ENTRY *here = dirbase + n;

    if(name)
    {
        for(int i = 0; i < 64; ++i)
        {
            name[i] = here->name[i];
        }
        name[63] = '\0';
    }

    if(len)
    {
        *len = here->len;
    }

    return reinterpret_cast<const uint8_t *>(file) + here->data_offset;
}

// -------------------- COPY OBJECT TO MEMORY --------------------
void *gbfs_copy_obj(void *dst, const GBFS_FILE *file, const char *name)
{
    if(!dst || !file || !name) return nullptr;

    uint32_t len = 0;
    const void *src = gbfs_get_obj(file, name, &len);

    if(!src) return nullptr;

    std::memcpy(dst, src, len);
    return dst;
}

// -------------------- TOTAL OBJECT COUNT --------------------
size_t gbfs_count_objs(const GBFS_FILE *file)
{
    return file ? file->dir_nmemb : 0;
}