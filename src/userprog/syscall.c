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
#include "vm/page.h"

struct file *find_f (int fd); //to find file by fd
static void syscall_handler (struct intr_frame *f);
struct lock file_lock;

bool check_user_vaddr(void *addr)
{
  return addr < PHYS_BASE && addr != 0;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

void
syscall_handler (struct intr_frame *f) 
{
  if (!check_user_vaddr (f->esp))
  {
    exit (-1);
  }
  void *sp = f->esp;
  //arguemnt require ==> change to right data type
  //if return value ==> to eax
  switch (*(uint32_t *)sp)
  {
    case SYS_HALT:
      halt ();
      break;

    case SYS_EXIT:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      exit( *(uint32_t *)(sp + 4) );
      break;

    case SYS_EXEC:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      f->eax = exec ( (const char *)*(uint32_t *)(sp + 4) );
      break;

    case SYS_WAIT:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      f->eax = wait ( (pid_t *)*(uint32_t *)(sp + 4) );
      break;

    case SYS_CREATE:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      f->eax = create ( (const char *)*(uint32_t *)(sp + 4),  (const char *)*(uint32_t *)(sp + 8) );
      break;

    case SYS_REMOVE:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      f->eax = remove ( (const char *)*(uint32_t *)(sp + 4) );
      break;

    case SYS_OPEN:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      f->eax = open ( (const char *)*(uint32_t *)(sp + 4) );
      break;

    case SYS_FILESIZE:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      f->eax = filesize ( (int)*(uint32_t *)(sp + 4) );
      break;

    case SYS_READ:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      if (!check_user_vaddr((int*)sp + 2)) exit(-1);
      if (!check_user_vaddr((int*)sp + 3)) exit(-1);
      f->eax = read ( (int)*(uint32_t *)(sp + 4), (void *)*(uint32_t *)(sp + 8), (unsigned)*((uint32_t *)(sp + 12)) );
      break;

    case SYS_WRITE:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      if (!check_user_vaddr((int*)sp + 2)) exit(-1);
      if (!check_user_vaddr((int*)sp + 3)) exit(-1);
      f->eax = write( (int)*(uint32_t *)(sp + 4), (void *)*(uint32_t *)(sp + 8), (unsigned)*((uint32_t *)(sp + 12)) );
      break;

    case SYS_SEEK:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      if (!check_user_vaddr((int*)sp + 2)) exit(-1);
      seek ( (int)*(uint32_t *)(sp + 4), (unsigned)*((uint32_t *)(sp + 8)) );
      break;

    case SYS_TELL:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      f->eax = tell ( (int)*(uint32_t *)(sp + 4) );
      break;

    case SYS_CLOSE:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      close ( (int)*(uint32_t *)(sp + 4) );
      break;
    
    case SYS_MMAP:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      if (!check_user_vaddr((int*)sp + 2)) exit(-1);
      f->eax = mmap((int)*(uint32_t *)(sp + 4), (void *)*(uint32_t *)(sp + 8));
      break;

    case SYS_MUNMAP:
      if (!check_user_vaddr((int*)sp + 1)) exit(-1);
      munmap((int)*(uint32_t *)(sp + 4));
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
  if (file == NULL || !check_user_vaddr(file))
    exit(-1);
  return filesys_create (file, initial_size);
}

bool
remove (const char *file)
{
  if (file == NULL || !check_user_vaddr(file))
    exit(-1);
  return filesys_remove (file);
}

int
open (const char *file)
{
  lock_acquire (&file_lock);
  if (file == NULL || !check_user_vaddr(file)) {
    lock_release(&file_lock);
    exit(-1);
  }
  struct file *return_file = filesys_open (file);
  if (return_file == NULL)  {
    lock_release (&file_lock);
    return -1;
  }
  else
  {
    for (int i=3; i<128; i++)
    {
      if (find_f(i) == NULL)
      {
        if (strcmp (thread_current()->name, file) == false)
          file_deny_write (return_file);

        thread_current()->fd_list[i] = return_file;
        lock_release (&file_lock);
        return i;
      }
    }
  }
  lock_release (&file_lock);
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
  if (!check_user_vaddr(buffer)) {
    exit (-1);
  }

  int bytes_read;

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
      lock_acquire (&file_lock);
      bytes_read = file_read (f, buffer, size);
      lock_release (&file_lock);
      return bytes_read;
    }
  }
}


int
write (int fd, const void *buffer, unsigned size)
{
  check_user_vaddr (buffer);
  if (fd == 1) //STDOUT
  {
    lock_acquire (&file_lock);
    putbuf (buffer, size);
    lock_release (&file_lock);
    return size;
  }
  else
  {
    int bytes_written;
    struct file *f = find_f (fd);
    if (f == NULL)
    {
      exit(-1);
    }
    if (f->deny_write)
    {
      file_deny_write (f);
    }
    lock_acquire (&file_lock);
    bytes_written = file_write (f, buffer, size);
    lock_release (&file_lock);

    return bytes_written;
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

int 
mmap(int fd, void *addr)
{
  struct file *f = find_f(fd);
  struct file *opened_f;
  struct mmf *mmf;

  if (addr == NULL || (int)addr % PGSIZE != 0)  return -1;

  if (f == NULL)  return -1;

  lock_acquire (&file_lock);

  opened_f = file_reopen(f);
  if (opened_f == NULL)
  {
    lock_release(&file_lock);
    return -1;
  }

  mmf = init_mmf(thread_current()->map_cnt, opened_f, addr);
  thread_current()->map_cnt++;
  if (mmf == NULL)
  {
    lock_release(&file_lock);
    return -1;
  }

  lock_release(&file_lock);

  return mmf->id;
}

void 
munmap(int mapid)
{
  struct thread* t = thread_current();
  struct mmf* mmf;

  if(mapid >= t->map_cnt)  return;

  struct list_elem* e;
  for (e = list_begin(&t->mmf_lst); e != list_end(&t->mmf_lst); e = list_next(e))
  {
    mmf = list_entry(e, struct mmf, list_elem);
    if (mmf->id == mapid)
      break;
  }
  if (e == list_end(&t->mmf_lst))  return;

  lock_acquire(&file_lock);
  
  off_t max_length = file_length(mmf->file);
  off_t ofs = 0;
  while(ofs < max_length) {
    struct spt_entry *temp_entry = get_spt_entry(&t->sp_table, mmf->upage + ofs);

    if(pagedir_is_dirty(t->pagedir, mmf->upage + ofs))  {
      void* kpage = pagedir_get_page(t->pagedir, mmf->upage + ofs);
      file_write_at(temp_entry->file, kpage, temp_entry->read_bytes, temp_entry->ofs);
    }

    delete_a_page(&t->sp_table, temp_entry);

    ofs += PGSIZE;
  }
  list_remove(e);

  lock_release(&file_lock);
}

struct file
*find_f (int fd)
{
  return (thread_current()->fd_list[fd]);
}