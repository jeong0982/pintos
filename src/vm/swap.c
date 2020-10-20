#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include <debug.h>

static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;

static size_t swap_table_size;

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
  swap_table_size = block_size(swap_disk);
  swap_table = bitmap_create(swap_table_size);
  bitmap_set_all(swap_table, true);
  lock_init (&swap_lock);
}

void swap_table_free (block_sector_t swap_index) {
  lock_acquire (&swap_lock);
  if (bitmap_test (swap_table, swap_index) == true) {
    PANIC ("Error");
  }
  bitmap_set_multiple (swap_table, swap_index, 8, true);
  lock_release (&swap_lock);
}

block_sector_t swap_out (void *victim_frame) {
  lock_acquire (&swap_lock);

  block_sector_t free_index = bitmap_scan_and_flip (swap_table, 0, 8, true);
  if (free_index == BITMAP_ERROR) ASSERT ("No free index in swap disk");
  for (int i = 0; i < 8; i++) {
      block_write (swap_disk, free_index + i, (uint8_t *) victim_frame + i * BLOCK_SECTOR_SIZE);
  }
  lock_release (&swap_lock);
  return free_index;
}

void swap_in (block_sector_t idx, void* frame) {  
  lock_acquire(&swap_lock);
  if (bitmap_test(swap_table, idx) == true) ASSERT ("Trying to swap in a free block.");

  bitmap_set_multiple (swap_table, idx, 8, true); //bitmap 초기화

  for (int i = 0; i < 8; i++) {
      block_read (swap_disk, idx + i, (uint8_t *) frame + i * BLOCK_SECTOR_SIZE);
  }
  lock_release(&swap_lock);
}

