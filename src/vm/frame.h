#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"

// 全局页框
void frame_init (); 
void *falloc_get_frame (enum palloc_flags, void *);
void falloc_free_frame(void *page);

#endif

