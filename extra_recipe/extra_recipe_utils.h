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

#endif /* extra_recipe_utils_h */
