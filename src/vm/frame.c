#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/synch.h"

uint8_t* 
frame_alloc (enum palloc_flags flags, struct spte *spte){
    if ( (flags & PAL_USER) == 0 )
        return NULL;
  
    void *frame = palloc_get_page(flags);
    printf ("%p : frame alloc\n", frame);
    if (!frame){
        lock_acquire(&frame_table_lock);
        struct fte *fte = find_victim();
        lock_release(&frame_table_lock);   

        frame = fte->frame;
        swap_out(frame);

        // swap_out된 frame의 spte, pte 업데이트
        update_spte (fte->spte);
        frame_table_update(fte, spte, thread_current());
    } else {
        create_fte(frame);
        lock_acquire(&frame_table_lock);
        struct fte* fte = list_entry (list_end (&frame_table), struct fte, elem);
        fte ->spte = spte;
        lock_release(&frame_table_lock); 
    }
    return frame;
}

struct fte *find_victim (void) {
    struct list_elem *evict_elem = list_pop_back (&frame_table);
    list_push_front (&frame_table, evict_elem);
    return list_entry (evict_elem, struct fte, elem);
}

void frame_table_init () {
    list_init (&frame_table);
    lock_init (&frame_table_lock);
}

void create_fte (void *frame) {
    struct fte *fte;
    fte = calloc (1, sizeof *fte);
    if (fte == NULL)
        return;
    fte ->thread = thread_current ();
    fte ->frame = frame;
    lock_acquire (&frame_table_lock);
    list_push_back (&frame_table, &fte ->elem);
    lock_release (&frame_table_lock);
    return;
}

void remove_frame (void *frame) {
    struct fte *f;
    struct list_elem *e;

    lock_acquire (&frame_table_lock);
    e = list_head (&frame_table);
    e = list_next (e);
    while (e != list_tail (&frame_table)) {
        f = list_entry (e, struct fte, elem);
        if (f ->frame == frame) {
            list_remove (e);
            free (f);
            break;
        }
        e = list_next (e);
    }
    lock_release (&frame_table_lock);
}

void frame_free (void *frame) {
    remove_frame (frame);
    palloc_free_page (frame);
}

void frame_table_update (struct fte* frame, struct spte* spte, struct thread* t) {

}
