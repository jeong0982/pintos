#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

void
init_spt (struct hash* h) {
  hash_init(h, *spte_hash_func, *spte_less, NULL);
}

bool
spte_less(const struct hash_elem* ha, const struct hash_elem* hb, void* aux UNUSED) {
  struct spte* a = hash_entry(ha, struct spte, elem);
  struct spte* b = hash_entry(hb, struct spte, elem);
  return (a->upage > b->upage);
}

unsigned
spte_hash_func(const struct hash_elem* he, void *aux UNUSED) {
  const struct spte *vspte;
  vspte = hash_entry(he, struct spte, elem);
  return hash_bytes(&vspte->upage, sizeof vspte->upage);
}

struct spte*
get_spte(void *upage) {
  struct spte spte;
  struct hash_elem *e;
  void *new_upage = pg_round_down (upage);
  spte.upage = new_upage;
  e = hash_find (&thread_current()->spt, &spte.elem);
  if (e == NULL) {
    return NULL;
  }
  
  return hash_entry(e, struct spte, elem);
}

struct spte* 
create_spte_for_stack(void *upage)
{
  struct spte *spte = malloc(sizeof(struct spte));
  void *new_upage = pg_round_down (upage);
  if (!spte)
    return false;

  spte->upage = new_upage;  
  spte->state = STACK;
  spte->writable = true;

  return spte;
}

static void destroy_spte (struct hash_elem *e, void *aux UNUSED) {
  struct spte *spte;
  spte = hash_entry (e, struct spte, elem);
  if (spte ->state & SWAP_DISK) {
    swap_table_free (spte ->swap_location);  
  }
  free (spte);
}

void destroy_spt (struct hash *spt) {
  hash_destroy (spt, destroy_spte);
}

void update_spte_to_swap(struct spte *spte, block_sector_t idx) {
  spte ->state = SWAP_DISK;
  spte ->swap_location = idx;
  pagedir_clear_page (thread_current () ->pagedir, spte ->upage);
}

bool spt_insert_file (struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
  struct spte *spte;
  struct hash_elem *hash_result;

  spte = malloc(sizeof(struct spte));
  
  if (spte == NULL)
    return false;

  spte->upage = upage;
  spte->state = EXEC_FILE;
  spte->file = file;
  spte->offset = ofs;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;
  spte->is_loaded = false;

  hash_result = hash_insert (&thread_current ()->spt, &spte->elem);
  if (hash_result != NULL)
    return false;
  
  return true;

}
