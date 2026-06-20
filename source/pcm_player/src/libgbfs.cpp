/*
 * libgbfs.cpp
 * made by yewgamer
*/

#include <stdint.h>
#include <stddef.h>
#include <cstring>
#include <cstdlib>
#include "gbfs.h"

// -------------------- TYPES --------------------
typedef uint16_t u16;
typedef uint32_t u32;

// -------------------- FIND GBFS --------------------
const GBFS_FILE *find_first_gbfs_file(const void *start)
{
    const uint32_t *here = (const uint32_t *)
        ((uintptr_t)start & ~(uintptr_t)0xFF);

    const char rest_of_magic[] = "ightGBFS\r\n\x1a\n";

    while(here < (const uint32_t *)0x02040000)
    {
        if(*here == 0x456e6950)
        {
            if(!memcmp(here + 1, rest_of_magic, 12))
                return (const GBFS_FILE *)here;
        }
        here += 256 / sizeof(*here);
    }

    if(here < (const uint32_t *)0x08000000)
        here = (const uint32_t *)0x08000000;

    while(here < (const uint32_t *)0x0a000000)
    {
        if(*here == 0x456e6950)
        {
            if(!memcmp(here + 1, rest_of_magic, 12))
                return (const GBFS_FILE *)here;
        }
        here += 256 / sizeof(*here);
    }

    return nullptr;
}

// -------------------- SKIP --------------------
const void *skip_gbfs_file(const GBFS_FILE *file)
{
    return (const uint8_t *)file + file->total_len;
}

// -------------------- NAME COMPARE --------------------
static int namecmp(const void *a, const void *b)
{
    return memcmp(a, b, 64);
}

// -------------------- GET OBJECT --------------------
const void *gbfs_get_obj(const GBFS_FILE *file,
                         const char *name,
                         u32 *len)
{
    char key[64] = {0};
    memcpy(key, name, 64);

    const GBFS_ENTRY *dirbase =
        (const GBFS_ENTRY *)((const uint8_t *)file + file->dir_off);

    const GBFS_ENTRY *here = (const GBFS_ENTRY *)
        bsearch(key, dirbase, file->dir_nmemb, sizeof(GBFS_ENTRY), namecmp);

    if(!here)
        return nullptr;

    if(len)
        *len = here->len;

    return (const uint8_t *)file + here->data_offset;
}

// -------------------- NTH OBJECT --------------------
const void *gbfs_get_nth_obj(const GBFS_FILE *file,
                             size_t n,
                             char *name,
                             u32 *len)
{
    const GBFS_ENTRY *dirbase =
        (const GBFS_ENTRY *)((const uint8_t *)file + file->dir_off);

    if(n >= file->dir_nmemb)
        return nullptr;

    const GBFS_ENTRY *here = dirbase + n;

    if(name)
    {
        memcpy(name, here->name, 64);
        name[63] = 0;
    }

    if(len)
        *len = here->len;

    return (const uint8_t *)file + here->data_offset;
}

// -------------------- COPY --------------------
void *gbfs_copy_obj(void *dst,
                   const GBFS_FILE *file,
                   const char *name)
{
    u32 len;
    const void *src = gbfs_get_obj(file, name, &len);

    if(!src)
        return nullptr;

    memcpy(dst, src, len);
    return dst;
}

// -------------------- COUNT --------------------
size_t gbfs_count_objs(const GBFS_FILE *file)
{
    return file ? file->dir_nmemb : 0;
}