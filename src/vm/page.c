#include "vm/page.h"

bool 
create_spte_from_exec(struct file *file, int32_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes)
{
  struct spte *spte = malloc(sizeof(struct spte));
  if (!spte)
    return false;

  spte->upage = upage;  
  spte->state = EXEC_FILE;

  spte->file = file;
  spte->offset = ofs;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  
  return (hash_insert(&thread_current()->spt, &spte->elem) == NULL);
}
