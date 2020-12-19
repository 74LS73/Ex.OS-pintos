#include "frame.h"
#include <hash.h>
#include <list.h>
#include "debug.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "threads/thread.h"
#include "swap.h"

// FT：页框表
static struct hash frame_map;
// 时钟列表
static struct list frame_clock;
// 时针
static struct list_elem *clock_ptr;

// FTE：页框项
struct frame_table_entry
  {
    uint8_t *kpage;   // 逻辑地址
    uint8_t *upage;   // 虚拟地址
    struct thread *t;
    // block_sector_t start_sector;  // 如果被放入交换区，记录起始sector
    struct hash_elem helem;  // hash元素，参考hash.h
    struct list_elem lelem;  // 循环列表，for 时钟页面置换算法
  };

typedef struct frame_table_entry vm_fte;

unsigned frame_hash_hash_func (const struct hash_elem *e, void *aux);
bool frame_hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux);
void frame_hash_destory_func (struct hash_elem *e, void *aux);

void *feviction_get_fte (uint32_t *pagedir);

void 
frame_init () 
{
  hash_init (&frame_map, frame_hash_hash_func, frame_hash_less_func, NULL);
  list_init (&frame_clock);
  clock_ptr = list_end (&frame_clock);
}

void *
falloc_get_frame (enum palloc_flags flags, void *upage)
{
  struct thread *cur_thread = thread_current ();
  uint8_t *kpage = palloc_get_page (flags | PAL_USER);
  if (kpage == NULL)
    {
      // eviction
      feviction_get_fte (cur_thread->pagedir);
      kpage = palloc_get_page (flags | PAL_USER);
    }

  vm_fte *fte = malloc(sizeof (vm_fte));
  fte->kpage = kpage;
  fte->upage = upage;
  fte->t = thread_current ();
  struct hash_elem *old = hash_insert (&frame_map, &fte->helem);
  list_insert (clock_ptr, &fte->lelem);
  if (old != NULL)
    {
      // TODO 出错
    }   
  return kpage;
}


void 
falloc_free_frame (void *kpage)
{
  vm_fte *tmp = malloc(sizeof (vm_fte));
  tmp->kpage = kpage;
  struct hash_elem *he = hash_delete (&frame_map, &tmp->helem);
  if (he == NULL)
    {
      // TODO 出错
      // printf("arrive here!\n");
    }
  vm_fte *fte = hash_entry (he, vm_fte, helem);
  list_remove (&fte->lelem);
  palloc_free_page (kpage);
  return;
}

void 
clock_point_to_next ()
{
  if (clock_ptr == list_end (&frame_clock))
    clock_ptr = list_begin (&frame_clock);
  clock_ptr = list_next (clock_ptr);
  if (clock_ptr == list_end (&frame_clock))
    clock_ptr = list_begin (&frame_clock);
}

void *
feviction_get_fte (uint32_t *pagedir)
{
  while (true)
    {
      clock_point_to_next ();
      vm_fte *fte = list_entry (clock_ptr, vm_fte, lelem);
      void *upage = fte->upage;
      // uint32_t *pagedir = fte->t->pagedir;
      // is R == 1
      if (pagedir_is_accessed (pagedir, upage))
        {
          pagedir_set_accessed (pagedir, upage, false);
        }
      else 
        {
          block_sector_t start_sector = fswap_put_frame (fte->kpage);
          // palloc_free_page(fte->kpage);
          // 从页目录中删除
          pagedir_clear_page (fte->t->pagedir, fte->upage);
          // printf("arrive here!\n");
          clock_point_to_next ();
          falloc_free_frame (fte->kpage);
          vm_spte_set_for_swap (fte->t->spt, fte->upage, start_sector);
          // TODO
          return fte->kpage;
        }
    }
  
  NOT_REACHED ();
  return NULL;
}


// 哈希函数，根据FTE的kpage来hash
unsigned
frame_hash_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  vm_fte *fte = hash_entry (e, vm_fte, helem);
  return hash_bytes (&fte->kpage, sizeof (uint8_t *));
}

// 比较函数
bool 
frame_hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  vm_fte *ftea = hash_entry (a, vm_fte, helem);
  vm_fte *fteb = hash_entry (b, vm_fte, helem);
  return ftea->kpage < fteb->kpage;
}

// 销毁哈希表的元素
void 
frame_hash_destory_func (struct hash_elem *e, void *aux)
{
  vm_fte *fte = hash_entry (e, vm_fte, helem);
  // TODO
  free (fte);
}

