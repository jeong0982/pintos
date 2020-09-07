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
    thread_exit ();
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
  printf("syscall num : %d\n", *(uint32_t *)(f->esp));
  printf ("system call!\n");

  int *arg;
  void *esp = malloc(4);

  switch (*(uint32_t *)(f->esp)) {
    case SYS_HALT: {
      halt ();
      NOT_REACHED ();
      break;
    }
    case SYS_EXIT: {
      get_argument (f->esp, &arg[0], 1); //Exit을 위한 argument는 하나
      exit(arg[0]);
      break;
    }
    case SYS_EXEC: {

    }
    case SYS_WAIT: {

    }
    case SYS_CREATE: {

    }
    case SYS_REMOVE: {

    }
    case SYS_OPEN: {

    }

    default:
      thread_exit ();
  }
  thread_exit ();
}

/*__Function of the Round Table___________________________________________________________*/
void halt(void) {
  shutdown_power_off ();
}

void exit (int status) {
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit ();
}

tid_t exec (const char *cmd_line) {
  tid_t tid = process_execute (cmd_line);
  

}

bool create (const char *file, unsigned initial_size) {
  return filesys_create (file, initial_size);
}

bool remove (const char *file) {
  return filesys_remove (file);
}


