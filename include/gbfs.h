#ifndef INCLUDE_GBFS_H
#define INCLUDE_GBFS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------
// Fixed-width types (GBA safe)
// --------------------------------------------------
typedef uint16_t u16;
typedef uint32_t u32;

// --------------------------------------------------
// Optional helper macro (GBFS space filler)
// --------------------------------------------------
#define GBFS_SPACE(filename, kbytes) \
const char filename[(kbytes) * 1024] __attribute__((aligned(16))) = \
"PinEightGBFSSpace-" #filename "-" #kbytes ;

// --------------------------------------------------
// GBFS FILE HEADER
// --------------------------------------------------
typedef struct GBFS_FILE
{
    char magic[16];     // "PinEightGBFS\r\n\032\n"
    u32 total_len;      // total archive size
    u16 dir_off;        // directory offset
    u16 dir_nmemb;      // number of entries
    char reserved[8];   // reserved
} GBFS_FILE;

// --------------------------------------------------
// DIRECTORY ENTRY
// --------------------------------------------------
typedef struct GBFS_ENTRY
{
    char name[64];      // file name (padded)
    u32 len;            // file size
    u32 data_offset;    // offset from start of archive
} GBFS_ENTRY;

// --------------------------------------------------
// GBFS API (C linkage ONLY for functions)
// --------------------------------------------------
const GBFS_FILE *find_first_gbfs_file(const void *start);

const void *skip_gbfs_file(const GBFS_FILE *file);

const void *gbfs_get_obj(const GBFS_FILE *file,
                         const char *name,
                         u32 *len);

const void *gbfs_get_nth_obj(const GBFS_FILE *file,
                             size_t n,
                             char *name,
                             u32 *len);

void *gbfs_copy_obj(void *dst,
                   const GBFS_FILE *file,
                   const char *name);

size_t gbfs_count_objs(const GBFS_FILE *file);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // INCLUDE_GBFS_H