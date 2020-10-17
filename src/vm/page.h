#include <hash.h>
#include "devices/block.h"
#include "filesys/file.h"
#include "threads/thread.h"

enum status {
    SWAP_DISK, MEMORY, EXEC_FILE, STACK
};

struct spte {
    struct hash_elem elem;


    enum status state;
    void *upage;

    // for lazy loading
    struct file *file;
    size_t offset;
    size_t read_bytes;
    size_t zero_bytes;

    bool writable;
    bool is_loaded;

    // swap_out 된 경우, swap disk에서의 page의 위치
    block_sector_t swap_location;
};

void init_spt (struct hash*);
bool spte_less (const struct hash_elem*, const struct hash_elem*, void* UNUSED);
unsigned spte_hash_func (const struct hash_elem *, void* UNUSED);
struct spte *get_spte (void *);
struct spte* create_spte_for_stack (void *);
void update_spte_to_swap(struct spte *, block_sector_t);
static void destroy_spte (struct hash_elem *, void * UNUSED);
void destroy_spt (struct hash *);
bool spt_insert_file (struct file *, off_t, uint8_t *, uint32_t, uint32_t, bool);