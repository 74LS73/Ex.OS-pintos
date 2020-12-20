#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include "threads/palloc.h"

typedef struct frame_table_entry vm_fte;

// 全局页框
void frame_init (); 
void *falloc_get_frame (enum palloc_flags, void *);
void falloc_free_frame(void *kpage);

vm_fte *fmap_remove_fte (void *kpage);

bool frame_pin (void *kpage);
bool frame_unpin (void *kpage);

#endif


