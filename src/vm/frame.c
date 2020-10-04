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
        swap_out(frame);

        // swap_out된 frame의 spte, pte 업데이트
        update_spte (fte->spte);
        frame_table_update(fte, spte, thread_current());
    } else {
        create_fte(frame);    
    }
    return frame;
}

struct fte *find_victim (void) {
    struct list_elem *evict_elem = list_pop_back (&frame_table);
    list_push_front (&frame_table, evict_elem);
    return list_entry (evict_elem, struct fte, elem);
}

void create_fte (void *frame) {

}

void frame_table_update (struct fte* frame, struct spte* spte, struct thread* t) {

}
