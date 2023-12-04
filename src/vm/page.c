#include "vm/page.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include <string.h>
#include "threads/vaddr.h"

extern struct lock file_lock;

static unsigned
hash_hash_func_spt(const struct hash_elem* elem, void* aux) {
  struct spt_entry *p = hash_entry(elem, struct spt_entry, hash_elem);
  return hash_bytes(&p->upage, sizeof(p->upage));
}

static bool 
hash_less_func_spt(const struct hash_elem* a, const struct hash_elem* b, void* aux) {
  return hash_entry(a, struct spt_entry, hash_elem)->upage < hash_entry(b, struct spt_entry, hash_elem)->upage;
}

static void
page_destructor(struct hash_elem* elem, void* aux) {
  struct spt_entry* e = hash_entry(elem, struct spt_entry, hash_elem);
  free(e);
}

void
init_SupplementalPageTable(struct hash* spt) {
  hash_init(spt, hash_hash_func_spt, hash_less_func_spt, NULL);
}

void
destroy_SupplementalPageTable(struct hash* spt) {
  hash_destroy(spt, page_destructor);
}

void
init_zero_spt_entry(struct hash* sp_hash_table, void* upage)
{
  struct spt_entry* e;
  e = (struct spt_entry*) malloc(sizeof *e);
  
  e->upage = upage;
  e->kpage = NULL;
  e->state = ONLY_ZERO;
  e->file = NULL;
  e->writable = true;
  
  hash_insert(sp_hash_table, &e->hash_elem);
}

void
init_frame_spt_entry(struct hash* sp_hash_table, void* upage, void* kpage)
{
  struct spt_entry* e;
  e = (struct spt_entry*) malloc(sizeof *e);

  e->upage = upage;
  e->kpage = kpage;
  e->state = IN_FRAME;
  e->file = NULL;
  e->writable = true;
  
  hash_insert(sp_hash_table, &e->hash_elem);
}

struct spt_entry*
init_file_spt_entry(struct hash* sp_hash_table, void* upage, struct file* file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  struct spt_entry *e;
  
  e = (struct spt_entry *) malloc(sizeof *e);

  e->upage = upage;
  e->kpage = NULL;
  e->state = IN_FILE;
  e->file = file;
  e->ofs = ofs;
  e->read_bytes = read_bytes;
  e->zero_bytes = zero_bytes;
  e->writable = writable;
  
  hash_insert(sp_hash_table, &e->hash_elem);
  
  return e;
}

bool
load_a_page(struct hash* sp_hash_table, void* upage)
{
  struct spt_entry* e;
  e = get_spt_entry(sp_hash_table, upage);
  if (e == NULL)  exit (-1);

  void* kpage;
  kpage = falloc_get_page(PAL_USER, upage);
  if (kpage == NULL)  exit (-1);

  bool flag = false;

  switch (e->state)
  {
  case ONLY_ZERO:
    memset (kpage, 0, PGSIZE);
    break;
  case IN_SWAP:
    swap_load(e, kpage);
    break;
  case IN_FILE:
    if(!lock_held_by_current_thread(&file_lock))  {
      lock_acquire(&file_lock);
      flag = true;
    }
    if (file_read_at(e->file, kpage, e->read_bytes, e->ofs) != e->read_bytes)
    {
      falloc_free_page (kpage);
      lock_release (&file_lock);
      exit (-1);
    }
    memset (kpage + e->read_bytes, 0, e->zero_bytes);
    if(flag) lock_release(&file_lock);
    break;
  default:
    exit (-1);
  }

  uint32_t* pagedir = thread_current()->pagedir;

  if (!pagedir_set_page(pagedir, upage, kpage, e->writable))
  {
    falloc_free_page(kpage);
    exit(-1);
  }

  e->kpage = kpage;
  e->state = IN_FRAME;

  return true;
}

struct spt_entry*
get_spt_entry(struct hash* sp_hash_table, void* upage)
{
  struct spt_entry e;
  e.upage = upage;

  struct hash_elem* elem = hash_find(sp_hash_table, &e.hash_elem);

  return elem != NULL ? hash_entry(elem, struct spt_entry, hash_elem) : NULL;
}

void 
delete_a_page(struct hash *sp_hash_table, struct spt_entry *entry)
{
  hash_delete(sp_hash_table, &entry->hash_elem);
  free(entry);
}