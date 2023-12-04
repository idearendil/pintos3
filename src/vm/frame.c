#include "vm/frame.h"
#include "threads/synch.h"
#include "vm/swap.h"

static struct lock ft_lock;
static struct list ft_lst;
static struct ft_entry *clock_pointer;

void
FrameTable_init()
{
  lock_init(&ft_lock);
  list_init(&ft_lst);
  clock_pointer = NULL;
}

void *
falloc_get_page(void *upage, enum palloc_flags flags)
{
  lock_acquire(&ft_lock);

  void *kpage;
  kpage = palloc_get_page(flags);
  if (kpage == NULL)
  {
    evict();
    kpage = palloc_get_page(flags);
    if (kpage == NULL)
      return NULL;
  }
  
  struct ft_entry *temp_entry;
  temp_entry =(struct ft_entry *)malloc(sizeof *temp_entry);
  temp_entry->kpage = kpage;
  temp_entry->upage = upage;
  temp_entry->t = thread_current ();
  list_push_back(&ft_lst, &temp_entry->list_elem);

  lock_release(&ft_lock);

  return kpage;
}

void
falloc_free_page(void *kpage)
{
  bool flag = false;
  if(!lock_held_by_current_thread(&ft_lock)) {
    lock_acquire(&ft_lock);
    flag = true;
  }

  struct ft_entry *temp_entry;
  temp_entry = get_frame_table_entry(kpage);
  if(temp_entry == NULL)
    exit (-1);

  palloc_free_page(temp_entry->kpage);
  pagedir_clear_page(temp_entry->t->pagedir, temp_entry->upage);
  list_remove(&temp_entry->list_elem);
  free(temp_entry);

  if(flag)  lock_release(&ft_lock);
}

struct ft_entry*
get_frame_table_entry(void* kpage)
{
  struct list_elem *e;
  for (e = list_begin(&ft_lst); e != list_end(&ft_lst); e = list_next(e))
    if (list_entry(e, struct ft_entry, list_elem)->kpage == kpage)
      return list_entry(e, struct ft_entry, list_elem);
  return NULL;
}

void evict() {

  struct ft_entry *temp_entry = clock_pointer;
  do {
    if(clock_pointer == NULL) temp_entry = list_entry(list_begin(&ft_lst), struct ft_entry, list_elem);
    else  {
      pagedir_set_accessed(temp_entry->t->pagedir, temp_entry->upage, false);

      if (list_next(&clock_pointer->list_elem) == list_end(&ft_lst)) {
        temp_entry = list_entry(list_begin(&ft_lst), struct ft_entry, list_elem);
      } else {
        temp_entry = list_next(temp_entry);
      }
    }
  } while(!pagedir_is_accessed(temp_entry->t->pagedir, temp_entry->upage));

  struct spt_entry *s;
  s = get_spt_entry(&thread_current()->sp_table, temp_entry->upage);
  s->state = IN_SWAP;
  s->swap_id = swap_evict(temp_entry->kpage);

  falloc_free_page(temp_entry->kpage);
}