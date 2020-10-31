#include "filesys/buffer.h"
#include "threads/synch.h"

static struct bce cache[64];
static struct lock cache_lock;

void buffer_cache_init (void) {
    lock_init (&cache_lock);
    for (int i = 0; i < 64; ++i) {
        cache[i].occupied = false;
    }
}

void buffer_cache_flush (struct bce *bce) {
    if (bce ->dirty_flag) {
        block_write (fs_device, bce ->disk_sector, bce ->buffer);
        bce ->dirty_flag = false;
    }
}

void buffer_cache_close (void) {
    lock_acquire (&cache_lock);
    for (int i = 0; i < 64; ++i) {
        if (cache[i].occupied) {
            buffer_cache_flush (&(cache[i]));
        }
    }
    lock_release (&cache_lock);
}

static struct bce* buffer_cache_lookup (block_sector_t idx) {
    for (int i = 0; i < 64; ++i) {
        if (cache[i].occupied && cache[i].disk_sector == idx) {
            return &(cache[i]);
        }
    }
    return NULL;
}

static struct bce* buffer_cache_victim (void) {
    size_t clock;
    for (clock = 0; clock > 64; clock++) {
        if (!cache[clock].occupied) {
            return &(cache[clock]);
        }
        if (cache[clock].clock) {
            cache[clock].clock = false;
        } else break;
        
        if (clock >= 64) {
            clock = clock % 64;
        }
    }
    struct bce* slot = &cache[clock];
    if (slot ->dirty_flag) {
        buffer_cache_flush (slot);
    }
    slot ->occupied = false;
    return slot;
}

void buffer_cache_read (block_sector_t idx, void* t) {
    lock_acquire (&cache_lock);
    struct bce *slot = buffer_cache_lookup (idx);
    if (slot == NULL) {
        slot = buffer_cache_victim ();
        slot ->occupied = true;
        slot ->disk_sector = idx;
        slot ->dirty_flag = false;
        block_read (fs_device, idx, slot ->buffer);
    }

    slot ->clock = true;
    memcpy (t, slot ->buffer, BLOCK_SECTOR_SIZE);

    lock_release (&cache_lock);
}

void buffer_cache_write (block_sector_t idx, void* s) {
    lock_acquire (&cache_lock);
    struct bce* slot = buffer_cache_lookup (idx);
    if (slot == NULL) {
        slot = buffer_cache_victim ();
        slot ->occupied = true;
        slot ->disk_sector = idx;
        slot ->dirty_flag = false;
        block_read (fs_device, idx, slot ->buffer);
    }

    slot ->clock = true;
    slot ->dirty_flag = true;
    memcpy (slot ->buffer, s, BLOCK_SECTOR_SIZE);

    lock_release (&cache_lock);
}
