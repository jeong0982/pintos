#include "vm/swap.h"
#include "threads/synch.h"
#include <debug.h>

block_sector_t swap_out (void *victim_frame) {
    lock_acquire (&swap_lock);

    block_sector_t free_index = bitmap_scan_and_flip (swap_table, 0, 8, false);

    if (free_index == BITMAP_ERROR) ASSERT ("No free index in swap disk");

    for (int i = 0; i < 8; i++) {
        block_write (swap_disk, free_index + i, (uint8_t *) victim_frame + i * BLOCK_SECTOR_SIZE);
    }
    lock_release (&swap_lock);
    return free_index;
}

void swap_in (block_sector_t swap_index, void* frame) {  
    lock_acquire(&swap_lock);
    if (bitmap_test(swap_table, swap_index) == false) ASSERT ("Trying to swap in a free block.");
   
    bitmap_flip(swap_table, swap_index); //bitmap 초기화

    for (int i = 0; i < 8; i++) {
        block_read (swap_disk, swap_index + i, (uint8_t *) frame + i * BLOCK_SECTOR_SIZE);
    }
    lock_release(&swap_lock);
}

