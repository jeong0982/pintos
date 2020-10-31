#include <debug.h>
#include "devices/block.h"
#include "filesys/filesys.h"

struct bce {
    bool dirty_flag;
    bool clock;
    bool occupied;

    block_sector_t disk_sector;
    uint8_t buffer[BLOCK_SECTOR_SIZE];
};

void buffer_cache_init (void);
void buffer_cache_flush (struct bce *);
void buffer_cache_close (void);
static struct bce* buffer_cache_lookup (block_sector_t);
static struct bce* buffer_cache_victim (void);
void buffer_cache_read (block_sector_t, void*);
void buffer_cache_write (block_sector_t, void*);
