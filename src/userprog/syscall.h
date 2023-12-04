#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#define PHYS_BASE 0xc0000000

#include "lib/user/syscall.h"

struct file 
  {
    struct inode *inode;        /* File's inode. */
    int pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };

void syscall_init (void);
void halt (void);
void exit (int status);
pid_t exec (const char *cmd_line);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

#endif /* userprog/syscall.h */
