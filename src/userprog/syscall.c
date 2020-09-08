#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include <devices/shutdown.h>
#include <threads/thread.h>
#include <filesys/filesys.h>
/*___선언_____________________________________________________________________________*/
static void syscall_handler (struct intr_frame *);


void syscall_init (void);
void get_argument (void *, int *, int);
bool create (const char *, unsigned);
bool remove (const char*);


void halt (void);
void exit (int);
tid_t exec (const char *);

/*_____부가적인 함수들_________________________________________________________________*/
void check_address (void *addr) {
  if (0x8048000 > addr || 0xc0000000 < addr) {
    exit (-1);
  }
}

void
syscall_init (void) 
{
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
  // hex_dump(f->esp, f->esp, 100, 1); 
  switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT: {
      halt ();
      NOT_REACHED ();
      break;
    }
    case SYS_EXIT: {
      get_argument (f->esp, arg, 1); //Exit을 위한 argument는 하나
      exit(arg[0]);
      break;
    }
    case SYS_EXEC: {
      get_argument (sp, arg, 1);
      check_address ((void *) arg[0]);
      f -> eax = exec ((const char *) arg[0]);
      break;
    }
    case SYS_WAIT: {
      f-> eax = wait((tid_t)*(uint32_t *)(f->esp + 4));
      break;
    }
    case SYS_CREATE: {
      break;
    }
    case SYS_REMOVE: {
      break;
    }
    case SYS_OPEN: {
      break;
    }
    case SYS_READ: {
      f->eax = read((int)*(uint32_t *)(f->esp+20), (void *)*(uint32_t *)(f->esp + 24), (unsigned)*((uint32_t *)(f->esp + 28)));
      break;
    }

    case SYS_WRITE: {
      write((int)*(uint32_t *)(f->esp+4), (void *)*(uint32_t *)(f->esp + 8), (unsigned)*((uint32_t *)(f->esp + 12)));
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
  cur ->exit_status = status;
  printf("%s: exit(%d)\n", cur->name, status); 
  thread_exit ();
}

tid_t exec (const char *cmd_line) {
  tid_t tid = process_execute (cmd_line);
  struct thread *child = get_child_process (tid);
  struct thread *cur = thread_current ();
  sema_down (&child ->load_sema);
  if (child-> is_done) {
    return tid;
  } else {
    return -1;
  }
  // return process_execute(cmd_line);
}

bool create (const char *file, unsigned initial_size) {
  return filesys_create (file, initial_size);
}

bool remove (const char *file) {
  return filesys_remove (file);
}

int wait (tid_t tid) {
  return process_wait(tid);
}

int read (int fd, void* buffer, unsigned size) {
  int i;
  if (fd == 0) {
    for (i = 0; i < size; i ++) {
      if (((char *)buffer)[i] == '\0') {
        break;
      }
    }
  }
  return i;
}

int write (int fd, const void *buffer, unsigned size) {
  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  return -1;
}

