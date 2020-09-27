#include <list.h>
#include "vm/page.h"
#include "threads/thread.h"
#include "threads/synch.h"

struct list frame_table;
struct lock frame_table_lock;

struct fte {
    void *frame;
    struct spte *spte;
    struct thread *thread;
    struct list_elem elem;
};

init_frame_table
frame_alloc
frame_free
find_victim
create_fte
frame_table_update
destroy_frame_table