#include <list.h>
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/thread.h"
#include "threads/palloc.h"

static struct list frame_table;
struct lock frame_table_lock;

struct fte {
    bool fixed;
    void *frame;
    struct spte *spte;
    struct thread *thread;
    struct list_elem elem;
};

uint8_t *frame_alloc (enum palloc_flags, struct spte*);
void frame_table_init (void);
void frame_free (void *);
struct fte *find_victim (void);
void create_fte (void *, struct spte*);
void frame_table_update (struct fte*, struct spte*, struct thread*);
void remove_frame (void *);
void remove_frame_by_spte (struct spte*);
void* find_frame (struct spte*);
void frame_unfix (void*);
void frame_fix (void*);
