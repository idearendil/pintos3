#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/off_t.h"
#include "devices/block.h"

struct file *find_f (int fd); //to find file by fd
static void syscall_handler (struct intr_frame *f);
void check_user_vaddr (const void *vaddr); //check it is within user address

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
syscall_handler (struct intr_frame *f) 
{
  void *sp = f->esp;
  //arguemnt require ==> change to right data type
  //if return value ==> to eax
  switch (*(uint32_t *)sp)
  {
    case SYS_HALT:
      halt ();
      break;

    case SYS_EXIT:
      check_user_vaddr (sp + 4);
      exit( *(uint32_t *)(sp + 4) );
      break;

    case SYS_EXEC:
      check_user_vaddr (sp + 4);
      f->eax = exec ( (const char *)*(uint32_t *)(sp + 4) );
      break;

    case SYS_WAIT:
      check_user_vaddr (sp + 4);
      f->eax = wait ( (pid_t *)*(uint32_t *)(sp + 4) );
      break;

    case SYS_CREATE:
      check_user_vaddr (sp + 4);
      f->eax = create ( (const char *)*(uint32_t *)(sp + 4),  (const char *)*(uint32_t *)(sp + 8) );
      break;

    case SYS_REMOVE:
      check_user_vaddr (sp + 4);
      f->eax = remove ( (const char *)*(uint32_t *)(sp + 4) );
      break;

    case SYS_OPEN:
      check_user_vaddr (sp + 4);
      f->eax = open ( (const char *)*(uint32_t *)(sp + 4) );
      break;

    case SYS_FILESIZE:
      check_user_vaddr (sp + 4);
      f->eax = filesize ( (int)*(uint32_t *)(sp + 4) );
      break;

    case SYS_READ:
      check_user_vaddr (sp + 4);
      f->eax = read ( (int)*(uint32_t *)(sp + 4), (void *)*(uint32_t *)(sp + 8), (unsigned)*((uint32_t *)(sp + 12)) );
      break;

    case SYS_WRITE:
      check_user_vaddr (sp + 4);
      f->eax = write( (int)*(uint32_t *)(sp + 4), (void *)*(uint32_t *)(sp + 8), (unsigned)*((uint32_t *)(sp + 12)) );
      break;

    case SYS_SEEK:
      check_user_vaddr (sp + 4);
      seek ( (int)*(uint32_t *)(sp + 4), (unsigned)*((uint32_t *)(sp + 8)) );
      break;

    case SYS_TELL:
      check_user_vaddr (sp + 4);
      f->eax = tell ( (int)*(uint32_t *)(sp + 4) );
      break;

    case SYS_CLOSE:
      check_user_vaddr (sp + 4);
      close ( (int)*(uint32_t *)(sp + 4) );
      break;
  }
  // thread_exit ();
}

void
halt (void)
{
  shutdown_power_off ();
}

void
exit (int status)
{
  printf("%s: exit(%d)\n", thread_name(), status); //terminated message
  thread_current() -> exit_status = status;
  //close all files
  for (int i=3; i<128; i++) 
  {
    if (find_f(i) != NULL)
      close(i);
  }
  thread_exit();
}

pid_t
exec (const char *cmd_line)
{
  return process_execute (cmd_line);
}

int
wait (pid_t pid)
{
  return process_wait (pid);
}

bool
create(const char *file, unsigned initial_size)
{
  if (file == NULL)
    exit(-1);
  return filesys_create (file, initial_size);
}

bool
remove (const char *file)
{
  if (file == NULL)
    exit(-1);
  return filesys_remove (file);
}

int
open (const char *file)
{
  if (file == NULL)
    exit(-1);
  check_user_vaddr (file);
  struct file *return_file = filesys_open (file);
  if (return_file == NULL)
    return -1;
  else
  {
    for (int i=3; i<128; i++)
    {
      if (find_f(i) == NULL)
      {
        if (strcmp (thread_current()->name, file) == false)
          file_deny_write (return_file);

        thread_current()->fd_list[i] = return_file;
        return i;
      }
    }
  }
  return -1;
}

int
filesize (int fd)
{
  struct file *f = find_f (fd);
  if (f == NULL)
    exit(-1);
  else
    return file_length (f);
}

int
read (int fd, void *buffer, unsigned size)
{
  check_user_vaddr (buffer);
  if (fd == 0) //STDIN
  {
    int i;
    for (i=0; i<size; i++)
    {
      ((char *)buffer)[i] = input_getc();
    }
    return size;
  }
  else //other case
  {
    struct file *f = find_f (fd);
    if (f == NULL)
      exit(-1);
    else
    {
      return file_read (f, buffer, size);
    }
  }
}


int
write (int fd, const void *buffer, unsigned size)
{
  check_user_vaddr (buffer);
  if (fd == 1) //STDOUT
  {
    putbuf (buffer, size);
    return size;
  }
  else
  {
    struct file *f = find_f (fd);
    if (f == NULL)
    {
      exit(-1);
    }
    if (f->deny_write)
    {
      file_deny_write (f);
    }
    return file_write (f, buffer, size);
  }
}

void
seek (int fd, unsigned position)
{
  struct file *f = find_f (fd);
  if (f == NULL)
    exit(-1);
  else
    return file_seek (f, position);
}

unsigned
tell (int fd)
{
  struct file *f = find_f (fd);
  if (f == NULL)
    exit(-1);
  else
    return file_tell (f);
}

void
close (int fd)
{
  struct file *f = find_f (fd);
  if (f == NULL)
    exit(-1);
  else
  {
    f = NULL;
    file_close (f);
  }
}

struct file
*find_f (int fd)
{
  return (thread_current()->fd_list[fd]);
}

void
check_user_vaddr (const void *vaddr)
{
  if (!is_user_vaddr (vaddr))
    exit(-1);
}
