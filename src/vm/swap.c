#include "swap.h"

#include <bitmap.h>
#include "threads/vaddr.h"

// 一个页面需要的sector数量
#define SWAP_PAGE_SIZE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct block *swap_block;
static struct bitmap *sector_map;

void 
swap_init () 
{
  swap_block = block_get_role (BLOCK_SWAP);
  ASSERT (swap_block != NULL);
  // 如何在c中设置private属性：block->size
  sector_map = bitmap_create (block_size (swap_block));
  bitmap_set_all (sector_map, true);
}

// 返回一个组可用的SWAP_PAGE的第一个sector的序号
block_sector_t
swap_get_free_sectors ()
{
  return bitmap_scan (sector_map, 0, SWAP_PAGE_SIZE, true);
}

// 把frame写入交换区
block_sector_t 
fswap_put_frame(void *kpage) 
{
  // 写入
  block_sector_t start_sector = swap_get_free_sectors ();
  int i;
  for (i = 0; i < SWAP_PAGE_SIZE; i++)
    {
      block_write (swap_block, start_sector + i, kpage + i * BLOCK_SECTOR_SIZE);
    }
  
  bitmap_set_multiple (sector_map, start_sector, SWAP_PAGE_SIZE, false);
  return start_sector;
}

// 把frame从交换区读出
void 
fswap_get_frame(void *kpage, block_sector_t start_sector) 
{
  // 读出
  int i;
  for (i = 0; i < SWAP_PAGE_SIZE; i++)
    {
      block_read (swap_block, start_sector + i, kpage + i * BLOCK_SECTOR_SIZE);
    }
  bitmap_set_multiple (sector_map, start_sector, SWAP_PAGE_SIZE, true);
  return;
}
