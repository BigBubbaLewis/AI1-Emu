#ifndef MMAN_H
#define MMAN_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <switch.h>
#include <stdlib.h>

#define PROT_READ 0b001
#define PROT_WRITE 0b010
#define PROT_EXEC 0b100
#define MAP_PRIVATE 2
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20

#define MAP_FAILED ((void *)-1)
void* ptr_rw = NULL;
static void* ai1_rx = NULL;
static size_t ai1_size = 0;

static inline void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    (void)fd;
    (void)offset;

    size_t size = (len + 0xFFF) &~ 0xFFF;
    if (ptr_rw) return ai1_rx == addr && ai1_size == size ? ptr_rw : MAP_FAILED;
    if (!envGetOwnProcessHandle()) return MAP_FAILED;
    /* libnx 4.x replaces virtmemReserve with a locked find-and-map API. */
    virtmemLock();
    void* alias = virtmemFindAslr(size, 0x1000);
    Result result = alias ? svcMapProcessMemory(alias, envGetOwnProcessHandle(), (u64)addr, size) : MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    virtmemUnlock();
    if (R_SUCCEEDED(result))
    {
        ptr_rw = alias; ai1_rx = addr; ai1_size = size;
        return ptr_rw;
    }
    else
    {
        printf("[NXJIT]: Jit failed!\n");
        return (void*)-1;
    }
}

static inline int mprotect(void *addr, size_t len, int prot)
{
    return 0;
}

static inline int munmap(void *addr, size_t len)
{
    (void)addr; (void)len;
    if (!ptr_rw) return 0;
    Result result = svcUnmapProcessMemory(ptr_rw, envGetOwnProcessHandle(), (u64)ai1_rx, ai1_size);
    if (R_FAILED(result)) return -1;
    ptr_rw = NULL; ai1_rx = NULL; ai1_size = 0;
    printf("[NXJIT]: Jit closed\n");
    
    return 0;
}

#ifdef __cplusplus
};
#endif

#endif // MMAN_H
