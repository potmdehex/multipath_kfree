//
//  extra_recipe_utils.h
//  multipath_kfree
//
//  Created by John Åkerblom on 6/1/18.
//  Copyright © 2018 kjljkla. All rights reserved.
//

#ifndef extra_recipe_utils_h
#define extra_recipe_utils_h

#include <mach/mach.h>
#include <stdint.h>

mach_port_t prealloc_port(int size);
void prepare_prealloc_port(mach_port_t port);
void send_prealloc_msg(mach_port_t port, uint64_t* buf, int n);
uint64_t* receive_prealloc_msg(mach_port_t port);

typedef mach_port_t io_service_t;
typedef mach_port_t io_connect_t;
io_connect_t alloc_userclient();

// Kernel RWX

void kx_setup(io_connect_t *ucs, mach_port_t *lazy_ports, uint64_t kaslr_shift, uint64_t kernel_buffer_base);
void kx3(uint64_t fptr, uint64_t arg0, uint64_t arg1, uint64_t arg2);

void kread(uint64_t addr, uint8_t *userspace, int n);
uint32_t kread32(uint64_t addr);
uint64_t kread64(uint64_t addr);

void kwrite(uint64_t addr, uint8_t *userspace, int n);
void kwrite32(uint64_t addr, uint32_t value);
void kwrite64(uint64_t addr, uint64_t value);

#endif /* extra_recipe_utils_h */
