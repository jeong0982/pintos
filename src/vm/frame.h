#include <list.h>
#include "vm/page.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/palloc.h"

struct list frame_table;
struct lock frame_table_lock;

struct fte {
    void *frame;
    struct spte *spte;
    struct thread *thread;
    struct list_elem elem;
};

uint8_t *frame_alloc (enum palloc_flags, struct spte*);
// init_frame_table
// frame_free
struct fte *find_victim (void);
// create_fte
void frame_table_update (struct fte*, struct spte*, struct thread*);
// destroy_frame_table