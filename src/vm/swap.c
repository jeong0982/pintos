#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include <debug.h>
static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;

static size_t swap_size;

void swap_table_init (void) {
  // Initialize the swap disk
  swap_disk = block_get_role(BLOCK_SWAP);
  if(swap_disk == NULL) {
    PANIC ("Error: Can't initialize swap block");
    NOT_REACHED ();
  }

  // Initialize swap_available, with all entry true
  // each single bit of `swap_available` corresponds to a block region,
  // which consists of contiguous [SECTORS_PER_PAGE] sectors,
  // their total size being equal to PGSIZE.
  swap_size = block_size(swap_disk) / SECTORS_PER_PAGE;
  swap_table = bitmap_create(swap_size);
  bitmap_set_all(swap_table, true);
}

block_sector_t swap_out (void *victim_frame) {
    printf ("swap out start\n");
    lock_acquire (&swap_lock);
    printf ("swap out - 1\n");
    block_sector_t free_index = bitmap_scan_and_flip (swap_table, 0, 8, false);
    printf ("swap out - 2\n");
    if (free_index == BITMAP_ERROR) ASSERT ("No free index in swap disk");
    printf ("swap out - 3\n");
    for (int i = 0; i < 8; i++) {
        block_write (swap_disk, free_index + i, (uint8_t *) victim_frame + i * BLOCK_SECTOR_SIZE);
    }
    printf ("swap out - 4\n");
    lock_release (&swap_lock);
    printf ("swap out finish\n");
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

