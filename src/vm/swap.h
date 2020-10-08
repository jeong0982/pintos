#include <bitmap.h>
#include <devices/block.h>

struct bitmap *swap_table;
struct block *swap_disk;
struct lock swap_lock;

void swap_table_init (void);
block_sector_t swap_out (void *);
void swap_in (block_sector_t, void *);