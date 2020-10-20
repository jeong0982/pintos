#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/synch.h"

uint8_t* 
frame_alloc (enum palloc_flags flags, struct spte *spte){
    if ( (flags & PAL_USER) == 0 )
        return NULL;
  
    void *frame = palloc_get_page(flags);
    if (!frame){
        lock_acquire(&frame_table_lock);
        struct fte *fte = find_victim();
        lock_release(&frame_table_lock);

        frame = fte->frame;
        block_sector_t idx = swap_out(frame);
        // swap_out된 frame의 spte, pte 업데이트
        update_spte_to_swap (fte->spte, idx);
        frame_table_update(fte, spte, thread_current());
    } else {
        create_fte(frame, spte);
    }
    return frame;
}

struct fte *find_victim (void) {
    struct list_elem *evict_elem = list_pop_back (&frame_table);
    list_push_front (&frame_table, evict_elem);
    struct fte* evict = list_entry (evict_elem, struct fte, elem);
    return evict;
}

void frame_table_init () {
    list_init (&frame_table);
    lock_init (&frame_table_lock);
}

void create_fte (void *frame, struct spte *spte) {
    struct fte *fte;
    fte = calloc (1, sizeof(struct fte));
    if (fte == NULL)
        return;
    fte ->thread = thread_current ();
    fte ->frame = frame;
    fte ->spte = spte;
    lock_acquire (&frame_table_lock);
    list_push_front (&frame_table, &fte ->elem);
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

void remove_frame_by_spte (struct spte* spte) {
    struct fte *f;
    struct list_elem *e;
    void *frame;
    lock_acquire (&frame_table_lock);
    e = list_head (&frame_table);
    e = list_next (e);
    while (e != list_tail (&frame_table)) {
        f = list_entry (e, struct fte, elem);
        if (f ->spte == spte) {
            list_remove (e);
            frame = f ->frame;
            free (f);
            break;
        }
        e = list_next (e);
    }
    lock_release (&frame_table_lock);
    palloc_free_page (frame);
}

void frame_free (void *frame) {
    remove_frame (frame);
    palloc_free_page (frame);
}

void frame_table_update (struct fte* frame, struct spte* spte, struct thread* t) {
    frame ->spte = spte;
    frame ->thread = t;
}

void* find_frame (struct spte *spte) {
    struct fte *f;
    struct list_elem *e;

    lock_acquire (&frame_table_lock);
    e = list_begin (&frame_table);
    while (e != list_tail (&frame_table)) {
        f = list_entry (e, struct fte, elem);
        if (f ->spte == spte) {
            break;
        }
        e = list_next (e);
    }
    lock_release (&frame_table_lock);
    return f->frame;
}
