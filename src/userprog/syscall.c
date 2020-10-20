#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include <devices/shutdown.h>
#include <threads/thread.h>
#include <filesys/filesys.h>
#include "threads/vaddr.h"
#include "vm/frame.h"

/*___선언_____________________________________________________________________________*/
static void syscall_handler (struct intr_frame *);


void syscall_init (void);
void get_argument (void *, int *, int);
bool create (const char *, unsigned);
bool remove (const char*);


void halt (void);
void exit (int);
tid_t exec (const char *);
int wait (tid_t);
int open (const char *);
int filesize (int);
int read (int, void*, unsigned);
int write (int, const void*, unsigned);
void seek (int, unsigned);
unsigned tell (int);
void close (int);
mapid_t mmap (int, void*);
void munmap (mapid_t);

/*_____부가적인 함수들_________________________________________________________________*/
void check_address (void *addr) {
  if (0xc0000000 <= addr) {
    if (lock_held_by_current_thread(&filesys_lock))
      lock_release (&filesys_lock);
    exit (-1);
  }
}

void
syscall_init (void) 
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
get_argument (void *esp, int *arg, int count) {
  int i;
  int *ptr;
  for (i = 0; i < count; i++)
  {
    ptr = (int *) esp + i + 1; // +1을 해줌으로서 ret_addr의 시작 부분이 온다?
    check_address ((const void *) ptr);
    arg[i] = *ptr;
  }
}
/*____________________________________________________________________________________*/
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // printf("syscall num : %d\n", *(uint32_t *)(f->esp));
  // printf ("system call!\n");
  // hex_dump(f->esp, f->esp, 100, 1);
  int *arg = (int *) palloc_get_page (0);
  uint32_t *sp = f -> esp;
  thread_current () ->user_esp = f ->esp;
  check_address ((void*) sp);
  switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT: {
      palloc_free_page (arg);
      halt ();
      NOT_REACHED ();
      break;
    }
    case SYS_EXIT: {
      get_argument (sp, arg, 1); //Exit을 위한 argument는 하나
      int status = arg[0];
      palloc_free_page (arg);
      exit(status);
      break;
    }
    case SYS_EXEC: {
      get_argument (sp, arg, 1);
      // check_address ((void *) arg[0]);
      const char* prog = (const char *) arg[0];
      palloc_free_page (arg);
      f -> eax = exec (prog);
      break;
    }
    case SYS_WAIT: {
      get_argument (sp, arg, 1);
      tid_t tid = arg[0];
      palloc_free_page (arg);
      f-> eax = (uint32_t) wait(tid);
      break;
    }
    case SYS_CREATE: {
      bool status;
      get_argument (sp, arg, 2);
      status = create((const char*) arg[0], arg[1]);
      f ->eax = status;
      palloc_free_page (arg);
      break;
    }
    case SYS_REMOVE: {
      bool status;
      get_argument (sp, arg, 1);
      status = remove((const char*) arg[0]);
      f ->eax = status;
      palloc_free_page (arg);
      break;
    }
    case SYS_OPEN: {
      
      get_argument (sp, arg, 1);
      int return_code = open ((const char*) arg[0]);
      // if (return_code == -1) {
      // printf ("%d : open\n", return_code);
      // }
      palloc_free_page (arg);
      f ->eax = return_code;
      break;
    }
    case SYS_FILESIZE: {
      get_argument (sp, arg, 1);
      f ->eax = filesize(arg[0]);
      palloc_free_page (arg);
      break;
    }
    case SYS_SEEK: {
      get_argument (sp, arg, 2);
      seek (arg[0], arg[1]);
      palloc_free_page (arg);
      break;
    }
    case SYS_TELL: {
      get_argument (sp, arg, 1);
      f ->eax = tell (arg[0]);
      palloc_free_page (arg);
      break;
    }
    case SYS_CLOSE: {
      get_argument (sp, arg, 1);
      close (arg[0]);
      palloc_free_page (arg);
      break;
    }
    case SYS_READ: {
      get_argument (sp, arg, 3);
      f->eax = read(arg[0], (void *) arg[1], arg[2]);
      palloc_free_page (arg);
      break;
    }
    case SYS_WRITE: {
      get_argument (sp, arg, 3);
      // printf ("%p %p %p\n", arg[0], arg[1], arg[2]);
      f ->eax = write(arg[0], (void *) arg[1], arg[2]);
      palloc_free_page (arg);
      break;
    }
    case SYS_MMAP: {
      get_argument (sp, arg, 2);
      mapid_t ret = mmap (arg[0], arg[1]);
      f ->eax = ret;
      palloc_free_page (arg);
      break;
    }
    case SYS_MUNMAP: {
      get_argument (sp, arg, 1);
      munmap (arg[0]);
      palloc_free_page (arg);
      break;
    }
    default:
      palloc_free_page (arg);
      exit (-1);
  }
  // thread_exit ();
}

/*__Function of the Round Table___________________________________________________________*/
void halt(void) {
  shutdown_power_off ();
}

void exit (int status) {
  struct thread *cur = thread_current ();
  struct list *children = &thread_current () -> children;
  struct list_elem *e;

  // if (status < 0) {
  //   status = -1;
  // }

  cur ->exit_status = status;
  // for (int i = 2; i < 128; i++) {
  //   file_close(cur ->fd[i]);
  // }
  // for (e = list_begin (children); e != list_end (children); e = list_next (e)) {
  //   struct thread *t = list_entry (e, struct thread, child_elem);
  //   process_wait(t ->tid);
  // }
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit ();
}

tid_t exec (const char *cmd_line) {
  // printf (cmd_line);
  // printf ("\n");
  tid_t tid = process_execute (cmd_line);

  struct thread *child = get_child_process (tid);
  struct thread *cur = thread_current ();
  if (child == NULL) {
    return -1;
  }
  sema_down (&child ->load_sema);
  
  if (child-> load_success) {
    return tid;
  } else {
    return -1;
  }
}

bool create (const char *file, unsigned initial_size) {
  if (file == NULL) {
    exit (-1);
  }
  return filesys_create (file, initial_size);
}

bool remove (const char *file) {
  if (file == NULL) {
    exit (-1);
  }
  return filesys_remove (file);
}

int wait (tid_t tid) {
  return process_wait(tid);
}

int open (const char *file) {
  struct thread *cur = thread_current ();
  // printf (file);
  // printf ("\n");
  check_address (file);
  if (file == NULL) {
    return -1;
  }
  
  struct file *f = filesys_open (file);
  if (strcmp (thread_name(), file) == 0) {
    file_deny_write (f);
  }
  if (f == NULL) {
    return -1;
  }
  int status = process_add_file (f);
  return status;
}

int filesize (int fd) {
  struct thread *cur = thread_current ();
  struct file *f = process_get_file (fd);
  if (f == NULL) {
    return -1;
  }
  return file_length (f);
}

int read (int fd, void* buffer, unsigned size) {
  check_address (buffer);
  check_address ((const uint8_t*) buffer + size - 1);
  lock_acquire (&filesys_lock);
  int i;
  if (fd == 0) {
    for (i = 0; i < size; i ++) {
      if (((char *)buffer)[i] == '\0') {
        break;
      }
    }
  } else if (fd > 1) {
    struct file *f = process_get_file (fd);
    i = file_read (f, buffer, size);
  }
  lock_release (&filesys_lock);
  return i;
}

int write (int fd, const void *buffer, unsigned size) {
  check_address (buffer);
  check_address ((const uint8_t*) buffer + size - 1);
  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  } else if (fd > 1) {
    lock_acquire (&filesys_lock);
    off_t offset = file_write (thread_current () -> fd[fd], buffer, size);
    lock_release (&filesys_lock);
    return offset;
  }
  return -1;
}

void seek (int fd, unsigned position) {
  file_seek(thread_current()->fd[fd], position);
}

unsigned tell (int fd) {
  return file_tell(thread_current()->fd[fd]);
}

void close (int fd) {
  process_close_file (fd);
}

mapid_t mmap (int fd, void* addr) {
  if (fd <= 1) return -1;
  if (addr == NULL || pg_ofs (addr) != 0) return -1;
  lock_acquire (&filesys_lock);
  struct file *file_origin = process_get_file (fd);
  struct file *map_f = file_reopen (file_origin);
  if (map_f == NULL) {
    lock_release (&filesys_lock);
    return -1;
  }

  size_t offset;
  struct mmap_file *mmap_f = (struct mmap_file*) malloc (sizeof (struct mmap_file));
  list_init (&mmap_f ->spte_list);

  for (offset = 0; offset < file_length (map_f); offset += PGSIZE) {
    void *vaddr = addr + offset;
    if (exist_spte (vaddr)) {
      free (mmap_f);
      lock_release (&filesys_lock);
      return -1;
    }
  }

  for (offset = 0; offset < file_length (map_f); offset += PGSIZE) {
    void *vaddr = addr + offset;
    size_t read_bytes = (offset + PGSIZE < file_length (map_f)? PGSIZE : file_length (map_f) - offset);
    size_t zero_bytes = PGSIZE - read_bytes;
    struct spte* spte = spt_insert_file (map_f, offset, vaddr, read_bytes, zero_bytes, true);
    // printf ("spte create finish %p, %p, %p\n", spte, &mmap_f ->spte_list, &spte ->mmap_elem);
    list_push_back (&mmap_f ->spte_list, &spte ->mmap_elem);
  }

  mapid_t mid;
  if (!list_empty (&thread_current () ->mmap_list)) {
    mid = list_entry (list_back (&thread_current () ->mmap_list), struct mmap_file, elem) ->mid + 1;
  } else mid = 0;
  
  mmap_f ->mid = mid;
  mmap_f ->file = map_f;

  list_push_back (&thread_current () ->mmap_list, &mmap_f ->elem);

  lock_release (&filesys_lock);
  return mid;
}

bool spte_unmap (struct spte* spte, struct file *f) {
  struct thread* t = thread_current ();
  switch (spte ->state) {
    case MEMORY: {
      void* frame = find_frame (spte);
      if (pagedir_is_dirty (t ->pagedir, spte ->upage) 
          || pagedir_is_dirty (t ->pagedir, frame)) {
        file_write_at (f, spte ->upage, spte ->read_bytes, spte ->offset);
      }
      remove_frame_by_spte (spte);
      pagedir_clear_page (t ->pagedir, spte ->upage);
      break;
    }
    case SWAP_DISK: {
      if (pagedir_is_dirty (t ->pagedir, spte ->upage)) {
        void *temp = palloc_get_page (0);
        swap_in (spte ->swap_location, temp);
        file_write_at (f, temp, PGSIZE, spte ->offset);
        palloc_free_page (temp);
      } else {
        swap_table_free (spte ->swap_location);
      }
      break;
    }
    default:
      break;
  }
  hash_delete (&thread_current () ->spt, &spte ->elem);
  return true;
}

void munmap (mapid_t mapping) {
  struct thread *t = thread_current ();
  struct list_elem *e;
  struct mmap_file *mmap_file = NULL;

  if (!list_empty(&t ->mmap_list)) {
    for (e = list_begin (&t ->mmap_list); e != list_end (&t ->mmap_list); e = list_next (e)) {
      struct mmap_file *entry = list_entry(e, struct mmap_file, elem);
      if (entry ->mid == mapping) {
        mmap_file = entry;
      }
    }
  }

  if (mmap_file == NULL) {
    return;
  }

  lock_acquire (&filesys_lock);
  
  while (!list_empty (&mmap_file ->spte_list)) {
    struct list_elem *spte_elem = list_begin (&mmap_file ->spte_list);

    struct spte* spte = list_entry (spte_elem, struct spte, mmap_elem);

    bool success = spte_unmap (spte, mmap_file ->file);
    list_remove (&spte ->mmap_elem);
  }
  list_remove (&mmap_file ->elem);
  // printf ("hello %p\n", mmap_file);
  file_close (mmap_file ->file);
  free (mmap_file);
  lock_release (&filesys_lock);
}
