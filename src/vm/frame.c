#include "vm/frame.h"
#incldue "vm/page.h"

void* 
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
        spte_update (fte->spte);
        frame_table_update(fte, spte, thread_current());
    } else {
        create_fte(frame);    
    }
    return frame;
}
