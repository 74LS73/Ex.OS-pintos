#include "page.h"

#include <debug.h>
#include "threads/palloc.h"
#include "userprog/process.h"

unsigned spt_hash_hash_func (const struct hash_elem *e, void *aux);
bool spt_hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux);
void spt_hash_destory_func (struct hash_elem *e, void *aux);

static bool install_page (void *upage, void *kpage, bool writable);
// 创建SPT
vm_spt * 
vm_spt_create () 
{
  vm_spt *spt = malloc (sizeof (vm_spt));
  hash_init (&spt->page_map, spt_hash_hash_func, spt_hash_less_func, NULL);
  return spt; 
}


// 销毁SPT
void 
vm_spt_destory (vm_spt *spt)
{
  hash_destroy (&spt->page_map, spt_hash_destory_func);
  free (spt);
}


bool 
vm_spt_insert (vm_spt *spt, vm_spte *spte) 
{
  if (vm_spt_find (spt, spte->upage) != NULL) return false;
  struct hash_elem *old = hash_insert (&spt->page_map, &spte->elem);
  return old == NULL;
}

vm_spte *
vm_spt_find (vm_spt *spt, uint8_t *upage) 
{
  // 只需要upage就可以找SPTE，不过需要一个hash_elem
  vm_spte *tmp = malloc (sizeof (vm_spte));
  tmp->upage = upage;
  struct hash_elem *e = hash_find (&spt->page_map, &tmp->elem);
  if (e == NULL) return NULL;
  return hash_entry(e, vm_spte, elem);
}

vm_spte *
vm_spte_create_for_file (uint8_t *upage, struct file *file, off_t file_ofs, uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable)
{
  vm_spte* spte = malloc (sizeof (vm_spte));
  spte->type = _SPTE_FOR_FILE;
  spte->upage = upage;
  spte->file = file;
  spte->file_ofs = file_ofs;
  spte->page_read_bytes = page_read_bytes;
  spte->page_zero_bytes = page_zero_bytes;
  spte->writable = writable;
  return spte;
}

vm_spte *
vm_spte_create_for_stack (uint8_t *upage)
{
  vm_spte* spte = malloc (sizeof (vm_spte));
  spte->type = _SPTE_FOR_STACK;
  spte->upage = upage;
  spte->writable = true;
  return spte;
}

// 状态改为放到交换区
bool
vm_spte_set_for_swap (vm_spt *spt, uint8_t *upage, block_sector_t start_sector)
{
  vm_spte* spte = vm_spt_find (spt, upage);
  if (spte == NULL) return false;
  spte->type = _SPTE_FOR_SWAP;
  spte->upage = upage;
  spte->start_sector = start_sector;
  return true;
}

bool
vm_load_page_by_spte (vm_spte *spte) 
{
  switch (spte->type)
    {
    case _SPTE_FOR_FILE:
      {
        uint8_t *upage = spte->upage;
        struct file *file = spte->file;
        off_t ofs = spte->file_ofs;
        uint32_t page_read_bytes = spte->page_read_bytes;
        uint32_t page_zero_bytes = spte->page_zero_bytes;
        bool writable = spte->writable;

        uint8_t *kpage = falloc_get_frame (PAL_USER, upage);
        if (kpage == NULL)
          return false;

        /* Load this page. */
        file_seek (file, ofs);
        if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
          {
            falloc_free_frame (kpage);
            return false; 
          }
        memset (kpage + page_read_bytes, 0, page_zero_bytes);

        if (!install_page (upage, kpage, writable)) 
          {
            falloc_free_frame (kpage);
            return false; 
          }
        return true;
      }
    case _SPTE_FOR_STACK:
      {
        uint8_t *upage = spte->upage;
        bool writable = spte->writable;
        uint8_t *kpage = falloc_get_frame (PAL_USER | PAL_ZERO, upage);
        if (!install_page (upage, kpage, writable)) 
          {
            falloc_free_frame (kpage);
            return false; 
          }
        return true;
      }
    case _SPTE_FOR_SWAP:
      {
        uint8_t *upage = spte->upage;
        bool writable = spte->writable;
        uint8_t *kpage = falloc_get_frame (PAL_USER, upage);
        fswap_get_frame (kpage, spte->start_sector);
        
        if (!install_page (upage, kpage, writable)) 
          {
            falloc_free_frame (kpage);
            return false; 
          }
        // spte->type = _SPTE_FOR_STACK;
        return true;
      }
    default:
      {}
    }

  NOT_REACHED ();
  return false;
}

// 这个似乎主要在这边用到了
/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

// 哈希函数，根据SPTE的upage来hash
unsigned
spt_hash_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  vm_spte *spte = hash_entry(e, vm_spte, elem);
  return hash_bytes (&spte->upage, sizeof (uint8_t *));
}

// 比较函数
bool 
spt_hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  vm_spte *sptea = hash_entry(a, vm_spte, elem);
  vm_spte *spteb = hash_entry(b, vm_spte, elem);
  return sptea->upage < spteb->upage;
}

// 销毁哈希表的元素
void spt_hash_destory_func (struct hash_elem *e, void *aux)
{
  vm_spte *spte = hash_entry(e, vm_spte, elem);
  // TODO

  free (spte);
}


