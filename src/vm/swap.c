#include "vm/swap.h"
#include "threads/synch.h"

#define SECTORS_IN_PAGE (PGSIZE/BLOCK_SECTOR_SIZE)

static struct lock swap_lock;
static struct bitmap *SwapTable;
static struct block *swap_disk;

void init_SwapTable()
{
    lock_init(&swap_lock);

    swap_disk = block_get_role(BLOCK_SWAP);
    SwapTable = bitmap_create(block_size(swap_disk) / SECTORS_IN_PAGE);

    bitmap_set_all(SwapTable, true);
}

void swap_load(struct spt_entry *spt_entry, void *kpage)
{
    lock_acquire(&swap_lock);

    if (spt_entry->swap_id >= bitmap_size(SwapTable) || spt_entry->swap_id < 0)    exit(-1);
    if (bitmap_test(SwapTable, spt_entry->swap_id) == true)  exit(-1);

    bitmap_set(SwapTable, spt_entry->swap_id, true);

    lock_release(&swap_lock);

    for(int i = 0; i < SECTORS_IN_PAGE; i++) block_read(swap_disk, spt_entry->swap_id * SECTORS_IN_PAGE + i, kpage + (i * BLOCK_SECTOR_SIZE));
}

int swap_evict(void *kpage)
{
    int id;

    lock_acquire(&swap_lock);
    id = bitmap_scan_and_flip(SwapTable, 0, 1, true);
    lock_release(&swap_lock);

    for(int i = 0; i < SECTORS_IN_PAGE; ++i)    block_write(swap_disk, id * SECTORS_IN_PAGE + i, kpage + (BLOCK_SECTOR_SIZE * i));

    return id;
}
