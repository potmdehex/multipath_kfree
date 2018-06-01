// Created by John Åkerblom 2018-06-01

#include <stdint.h>

#ifndef multipath_kfree_h
#define multipath_kfree_h

/* multipath_kfree: cause GC to free a kernel address.
   NOTE: closes mp_sock */
void multipath_kfree(int mp_sock, uint64_t addr);

/* multipath_kfree_nearby_self: cause GC to free a "nearby" kernel address.
   NOTE: closes mp_sock */
void multipath_kfree_nearby_self(int mp_sock, uint16_t addr_lowest_part);

#endif
