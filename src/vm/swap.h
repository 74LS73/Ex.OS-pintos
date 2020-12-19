#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "frame.h"
#include "devices/block.h"


void swap_init ();
block_sector_t fswap_put_frame(void *);
void fswap_get_frame(void *, block_sector_t);
void fswap_free_frame( block_sector_t);

#endif
