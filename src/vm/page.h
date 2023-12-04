#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/file.h"
#include "filesys/off_t.h"

#define ONLY_ZERO 0
#define IN_FRAME 1
#define IN_SWAP 2
#define IN_FILE 3

struct spt_entry
  {
    void *upage;
    void *kpage;
  
    int state;
  
    struct file *file;
    uint32_t read_bytes;
    uint32_t padding;
    off_t ofs;
    bool writable;
    
    int swap_id;

    struct hash_elem hash_elem;
  };

void init_SupplementalPageTable(struct hash *);
void destroy_SupplementalPageTable(struct hash *);
void init_zero_spt_entry(struct hash *, void *);
struct spt_entry *get_spt_entry(struct hash *, void *);
void init_frame_spt_entry(struct hash *, void *, void *);
struct spt_entry *init_file_spt_entry(struct hash *, void *, struct file *, off_t, uint32_t, uint32_t, bool);
bool load_a_page(struct hash *, void *);
void delete_a_page(struct hash *spt, struct spt_entry *entry);

#endif
