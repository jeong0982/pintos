#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include <devices/shutdown.h>
#include <threads/thread.h>
#include <filesys/filesys.h>
#include "threads/vaddr.h"
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


/*_____부가적인 함수들_________________________________________________________________*/
void check_address (void *addr) {
  if (0x8048000 > addr || 0xc0000000 < addr) {
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
  check_address ((void*) sp);
  // printf("syscall : %d\n", *(uint32_t *)(f->esp));
  // hex_dump(f->esp, f->esp, PHYS_BASE - f->esp, 1); 
  switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT: {
      halt ();
      NOT_REACHED ();
      break;
    }
    case SYS_EXIT: {
      get_argument (sp, arg, 1); //Exit을 위한 argument는 하나
      exit(arg[0]);
      break;
    }
    case SYS_EXEC: {
      get_argument (sp, arg, 1);
      // check_address ((void *) arg[0]);
      f -> eax = exec ((const char *) arg[0]);
      break;
    }
    case SYS_WAIT: {
      get_argument (sp, arg, 1);
      f-> eax = (uint32_t) wait((tid_t)arg[0]);
      break;
    }
    case SYS_CREATE: {
      bool status;
      get_argument (sp, arg, 2);
      status = create((const char*) arg[0], arg[1]);
      f ->eax = status;
      break;
    }
    case SYS_REMOVE: {
      bool status;
      get_argument (sp, arg, 1);
      status = remove((const char*) arg[0]);
      f ->eax = status;
      break;
    }
    case SYS_OPEN: {
      get_argument (sp, arg, 1);
      f ->eax = open ((const char*) arg[0]);
      break;
    }
    case SYS_FILESIZE: {
      get_argument (sp, arg, 1);
      f ->eax = filesize(arg[0]);
      break;
    }
    case SYS_SEEK: {
      get_argument (sp, arg, 2);
      seek (arg[0], arg[1]);
      break;
    }
    case SYS_TELL: {
      get_argument (sp, arg, 1);
      f ->eax = tell (arg[0]);
      break;
    }
    case SYS_CLOSE: {
      get_argument (sp, arg, 1);
      close (arg[0]);
      break;
    }
    case SYS_READ: {
      get_argument (sp, arg, 3);
      f->eax = read(arg[0], (void *) arg[1], arg[2]);
      break;
    }
    case SYS_WRITE: {
      get_argument (sp, arg, 3);
      f ->eax = write(arg[0], (void *) arg[1], arg[2]);
      break;
    }

    default:
      exit (-1);
  }
  palloc_free_page (arg);
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
  // for (int i = 0; i < 128; i++) {
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
  check_address (file);
  if (file == NULL) {
    return -1;
  }
  struct file *f = filesys_open (file);
  if (strcmp (thread_name(), file) == 0)
    file_deny_write (f);
  if (f == NULL) {
    return -1;
  }
  return process_add_file (f);
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
  lock_acquire (&filesys_lock);
  int i;
  check_address (buffer);
  check_address ((const uint8_t*) buffer + size - 1);
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
  lock_acquire (&filesys_lock);
  check_address (buffer);
  check_address ((const uint8_t*) buffer + size - 1);
  if (fd == 1) {
    putbuf(buffer, size);
    lock_release (&filesys_lock);
    return size;
  } else if (fd > 1) {
    off_t offset = file_write (thread_current () -> fd[fd], buffer, size);
    lock_release (&filesys_lock);
    return offset;
  }
  lock_release (&filesys_lock);
  return -1;
}

void seek (int fd, unsigned position) {
  file_seek(thread_current()->fd[fd], position);
}

unsigned tell (int fd) {
  return file_tell(thread_current()->fd[fd]);
}

void close (int fd) {
  return process_close_file (fd);
}
