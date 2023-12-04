#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "vm/page.h"

void init_SwapTable();
void swap_load(struct spt_entry *page, void *kva);
int swap_evict(void *kva);

#endif