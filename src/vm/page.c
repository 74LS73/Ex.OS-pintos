#include "page.h"

#include <debug.h>
#include "threads/palloc.h"
#include "userprog/process.h"

unsigned spt_hash_hash_func (const struct hash_elem *e, void *aux);
bool spt_hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux);
void spt_hash_destory_func (struct hash_elem *e, void *aux);

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
vm_spte_create (uint8_t *upage, struct file *file, off_t file_ofs, uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable)
{
  vm_spte* spte = malloc (sizeof (vm_spte));
  spte->upage = upage;
  spte->file = file;
  spte->file_ofs = file_ofs;
  spte->page_read_bytes = page_read_bytes;
  spte->page_zero_bytes = page_zero_bytes;
  spte->writable = writable;
  return spte;
}

bool
vm_load_page_by_spte (vm_spte *spte) 
{
  int8_t *upage = spte->upage;
  struct file *file = spte->file;
  off_t ofs = spte->file_ofs;
  uint32_t page_read_bytes = spte->page_read_bytes;
  uint32_t page_zero_bytes = spte->page_zero_bytes;
  bool writable = spte->writable;

  uint8_t *kpage = palloc_get_page (PAL_USER);
  if (kpage == NULL)
    return false;

  /* Load this page. */
  file_seek (file, ofs);
  if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
    {
      palloc_free_page (kpage);
      return false; 
    }
  memset (kpage + page_read_bytes, 0, page_zero_bytes);

  /* Add the page to the process's address space. */
  struct thread *t = thread_current ();
  bool flag = pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable);
  if (!flag) 
    {
      palloc_free_page (kpage);
      return false; 
    }
  return true;
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
