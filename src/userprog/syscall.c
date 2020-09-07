#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <devices/shutdown.h>
#include <threads/thread.h>
#include <filesys/filesys.h>

static void syscall_handler (struct intr_frame *);

void halt (void);
void exit (int);

void check_address (void *addr) {
  if (0x8048000 > addr || 0xc0000000 < addr) {
    thread_exit ();
  }
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf("syscall num : %d\n", *(uint32_t *)(f->esp));
  printf ("system call!\n");

  int *arg;
  void *esp = malloc(4);

  switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT:
      {
        halt ();
        NOT_REACHED ();
        break;
      }
    case SYS_EXIT:
      {

      }
    default:
      thread_exit ();
  }
  thread_exit ();
}

void halt(void) {
  shutdown_power_off ();
}

void exit (int status) {
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit ();
}

bool create (const char *file, unsigned initial_size) {
  return filesys_create (file, initial_size);
}

bool remove (const char *file) {
  return filesys_remove (file);
}
