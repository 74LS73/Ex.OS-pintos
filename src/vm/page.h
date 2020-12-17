#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

#include "filesys/file.h"

#ifndef VM
#define VM
#endif



// SPT：需要一个哈希表，储存补充页表项
struct supplemental_page_table
  {
    struct hash page_map;
  };

enum supplemental_page_table_entry_type
  {
    _SPTE_FOR_FILE,
    _SPTE_FOR_STACK,
  };

typedef enum supplemental_page_table_entry_type vm_spte_type;
// SPTE：补充页表项
struct supplemental_page_table_entry
  {
    uint8_t *upage;
    vm_spte_type type;
    struct file *file;
    off_t file_ofs;
    uint32_t page_read_bytes; 
    uint32_t page_zero_bytes; 
    bool writable;
    struct hash_elem elem;  // hash元素，参考hash.h

  };


typedef struct supplemental_page_table vm_spt;
typedef struct supplemental_page_table_entry vm_spte;
typedef uint32_t mapid_t;

vm_spt *vm_spt_create ();
void vm_spt_destory (vm_spt *);
bool vm_spt_insert (vm_spt *, vm_spte *);
vm_spte *vm_spt_find (vm_spt *, uint8_t *);

vm_spte *vm_spte_create_for_file (uint8_t *, struct file *, off_t, uint32_t, uint32_t, bool);
vm_spte *vm_spte_create_for_stack (uint8_t *);

bool vm_load_page_by_spte (vm_spte *spte);

#endif
