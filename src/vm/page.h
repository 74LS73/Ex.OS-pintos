#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

#include "filesys/file.h"
#include "devices/block.h"

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
    _SPTE_FOR_SWAP,
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
    block_sector_t start_sector;  // 如果被放入交换区，记录起始sector
    bool writable;
    struct hash_elem elem;  // hash元素，参考hash.h

  };


typedef struct supplemental_page_table vm_spt;
typedef struct supplemental_page_table_entry vm_spte;

vm_spt *vm_spt_create ();
void vm_spt_destory (vm_spt *);
bool vm_spt_insert (vm_spt *, vm_spte *);
vm_spte *vm_spt_find (vm_spt *, uint8_t *);

vm_spte *vm_spte_create_for_file (uint8_t *, struct file *, off_t, uint32_t, uint32_t, bool);
vm_spte *vm_spte_create_for_stack (uint8_t *);

vm_spte *vm_spte_set_for_swap (vm_spt *, uint8_t *upage, block_sector_t);

bool vm_load_page_by_spte (vm_spte *spte);

#endif

