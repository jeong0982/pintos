#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/swap.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  char *saveptr = NULL;
  const char *file_name_usr = NULL;
  char *file_name_copy = NULL;
  file_name_copy = palloc_get_page (0);
  if (file_name_copy == NULL) {
    return TID_ERROR;
  }
  strlcpy (file_name_copy, file_name, PGSIZE);

  file_name_usr = strtok_r (file_name_copy, " ", &saveptr);

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL) {
    if (file_name_copy) {
      palloc_free_page (file_name_copy);
    }
    return TID_ERROR;
  }
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name_usr, PRI_DEFAULT, start_process, fn_copy);
  if (file_name_copy) palloc_free_page (file_name_copy);
  if (tid == TID_ERROR) {
    if (fn_copy) palloc_free_page (fn_copy);
  }
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  init_spt (&thread_current () ->spt);
  const char **tokens = (const char**) palloc_get_page(0);
  if (tokens == NULL) {
    palloc_free_page (file_name);
    exit (-1);
  }
  char *saveptr;
  char *token;
  int index = 0;
  int count;

  token = strtok_r (file_name, " ", &saveptr);
  tokens[index++] = token;
  strlcpy (file_name, token, strlen (token) + 1);     // 이거 되냐?

  while(token != NULL) {
    token = strtok_r (NULL, " ", &saveptr);
    tokens[index++] = token;
  }

  count = index - 1;

  // 이제 tokens에 모든 것들이 다 저장되었다
  void* argv_addr[count]; // 각각 스트링들의 주소를 저장

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);
  sema_up (&thread_current () ->load_sema);
  if (success) {
  /*___________일단 실제 단어부터 넣어버림_____________________________________________________*/
    int i, j, len = 0;
    for (i = count - 1; i > -1; i--) {
      for (j = strlen(tokens[i]); j > -1; j--) { // 여기서 NULL부터 시작하는건가?
        if_.esp = if_.esp - 1;
        ** (char **) &if_.esp  = tokens[i][j];    // 이거 & 맞냐
        // memcpy(if_.esp, tokens[i][j], 1);
      }
      argv_addr[i] = if_.esp; // 방금 넣은 스트링의 주소를 저장
    }

  /*__________________이제 4의 배수로 내림을 해버린다________________________________________*/
    if_.esp = (void*) ( (unsigned int) (if_.esp) & 0xfffffffc);

  /*___각 스트링들의 주소를 가리키는 포인터를 counter 개 삽입하기 전에 마지막에 NULL을 넣어준다___*/
    if_.esp -= 4;
    *((uint32_t*) if_.esp) = 0;

  /*___________이제 스택에 주소들을 포인터로 넣어주자_______________________________________*/
    for (i = count - 1; i > -1; i--) {
      if_.esp -= 4;
      *((void**) if_.esp) = argv_addr[i];
    }
  /*__________________그 다음 더블 포인터를 넣어준다__________________________________*/
    if_.esp -= 4;
    *((void**) if_.esp) = (if_.esp + 4); // 줄이기 전에 것이 2차원 배열의 시작

  /*_________________ 맨 아래에서 두번째에는 count를 넣어주자________________________________________*/
    if_.esp -= 4;
    *((int*) if_.esp) = count;

  /*_________________ 맨 위에 return addr________________________________________*/
    if_.esp -= 4;
    *((int*) if_.esp) = 0;
  }
  palloc_free_page (tokens);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) {
    thread_current () ->load_success = 0;
    exit (-1);
  }
  thread_current () ->load_success = 1;
  sema_up (&thread_current () ->load_sema);
  sema_down (&thread_current () -> meml_sema);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

int process_add_file (struct file *f) {
  struct thread *cur = thread_current ();
  int i = 0;
  bool success = false;
  for (i = 2; i < 128; i++) {
    if (cur ->fd[i] == NULL) {
      cur ->fd[i] = f;
      success = true;
      break;
    }
  }
  if (!success) {
    printf ("fail in process add file\n");
    return -1;
  }
  return i;
}

struct file *process_get_file (int fd) {
  struct thread *cur = thread_current ();
  // printf ("%d : fd\n", fd);
  return cur -> fd[fd];
}

void process_close_file (int fd) {
  struct thread *cur = thread_current ();
  if (cur ->fd[fd]) {
    file_close (cur ->fd[fd]);
  }
  cur ->fd[fd] = NULL;
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  struct thread *child = get_child_process(child_tid);
  if (child == NULL) {
    return -1;
  }
  sema_up (&child -> meml_sema);
  sema_down (&child -> wait_sema);
  // if (child ->is_done != 1) {
  //   return -1;
  // }
  int status = child ->exit_status;
  remove_child_process (child);
  return status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  for (int i = 0; i < 128; i++) {
    file_close (cur ->fd[i]);
    cur ->fd[i] = NULL;
  }
  while (!list_empty (&cur ->mmap_list)) {
    struct list_elem *e = list_begin (&cur ->mmap_list);
    struct mmap_file *mmap_f = list_entry (e, struct mmap_file, elem);

    munmap (mmap_f ->mid);
  }
  struct list_elem *e;
  struct list *child_list = &cur ->children;
  while (!list_empty (child_list)) {
    e = list_begin (child_list);
    struct thread *t = list_entry (e, struct thread, child_elem);
    process_wait (t ->tid);
  }
  destroy_spt (&cur ->spt);
  dir_close (cur ->cwd);
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  // file_close (cur ->file_running);
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);
static bool load_segment_lazily (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment_lazily (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
              }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);


bool load_from_exec (struct spte *spte)
{
  enum palloc_flags flags = PAL_USER;
  uint8_t *frame = frame_alloc(flags, spte);
  if (!frame) return false;
  if ((int) spte->read_bytes != file_read_at(spte->file, frame, spte->read_bytes, spte->offset)){
    frame_free(frame);
    return false;
  }
  memset(frame + spte->read_bytes, 0, spte->zero_bytes);
  // printf ("load_from_exec : %p: install page\n", spte ->upage);
  // printf ("%d : writable\n", spte ->writable);
  if (!install_page(spte->upage, frame, spte->writable)) {
      frame_free(frame);
      return false;
  }
  pagedir_set_dirty (thread_current () ->pagedir, frame, false);
  spte->state = MEMORY;
  frame_unfix (frame);
  return true;
}

bool load_from_swap (struct spte *spte)
{
    // printf ("load from swap\n");
    uint8_t *frame = frame_alloc (PAL_USER, spte);
    if (!frame) {
        // printf ("no frame\n");
        return false;
    }
    if (!install_page(spte->upage, frame, spte->writable)){
        // printf ("install fail\n");
        frame_free (frame);
        return false;
    }
    swap_in(spte->swap_location, frame);
    spte->state = MEMORY;
    pagedir_set_dirty (thread_current () ->pagedir, frame, false);
    return true;
}

bool stack_growth (void* fault_addr)
{
  struct spte *new_spte = create_spte_for_stack (fault_addr);
  uint8_t *frame = frame_alloc (PAL_USER | PAL_ZERO, new_spte);
  if (!install_page (new_spte ->upage, frame, new_spte ->writable)) {
    frame_free (frame);
    return false;
  }
  pagedir_set_dirty (thread_current () ->pagedir, frame, false);
  return true;
}

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);

      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

static bool
load_segment_lazily (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
  file_seek (file, ofs);

  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      if (spt_insert_file (file, ofs, upage, page_read_bytes, page_zero_bytes, writable) == NULL) 
        {
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      ofs += PGSIZE;
    }
  return true;
}


/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      struct spte* spte_stack = create_spte_for_stack (((uint8_t *) PHYS_BASE) - PGSIZE);
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
