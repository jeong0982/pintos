#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct lock filesys_lock;
void syscall_init (void);
void exit (int);

#endif /* userprog/syscall.h */
