//
// Copyright(C) 2025 Roman Fomin
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <sys/mman.h>
  #include <unistd.h>
#endif

#include <stddef.h>
#include <stdint.h>

#include "doomtype.h"

static size_t GetPageSize(void)
{
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
#else
    return (size_t)sysconf(_SC_PAGESIZE);
#endif
}

static size_t RoundUp(size_t size, size_t multiple)
{
    if (multiple == 0)
    {
        return size;
    }

    size_t remainder = size % multiple;
    if (remainder == 0)
    {
        return size;
    }

    return size + multiple - remainder;
}

static void AdjustToPageBoundaries(void **ptr, size_t *size, size_t page_size)
{
    if (*size == 0)
    {
        return;
    }
    uintptr_t uptr = (uintptr_t)*ptr;
    uintptr_t mask = page_size - 1;
    uintptr_t start = uptr;
    uintptr_t end = start + *size;
    uintptr_t start_page = start & ~mask;
    uintptr_t end_page = (end + mask) & ~mask;
    *ptr = (void *)start_page;
    *size = (size_t)(end_page - start_page);
}

void *I_ReserveRegion(size_t size)
{
    size_t page_size = GetPageSize();
    size_t rounded_size = RoundUp(size, page_size);

#ifdef _WIN32
    return VirtualAlloc(NULL, rounded_size, MEM_RESERVE, PAGE_NOACCESS);
#else
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  #ifdef MAP_NORESERVE
    flags |= MAP_NORESERVE;
  #endif
    void *ptr = mmap(NULL, rounded_size, PROT_NONE, flags, -1, 0);
    if (ptr == MAP_FAILED)
    {
        return NULL;
    }
    return ptr;
#endif
}

boolean I_ReleaseRegion(void *ptr, size_t size)
{
#ifdef _WIN32
    return VirtualFree(ptr, 0, MEM_RELEASE);
#else
    size_t page_size = GetPageSize();
    size_t rounded_size = RoundUp(size, page_size);
    return munmap(ptr, rounded_size) == 0;
#endif
}

boolean I_CommitRegion(void *ptr, size_t size)
{
    size_t page_size = GetPageSize();
    void *adjusted_ptr = ptr;
    size_t adjusted_size = size;

    AdjustToPageBoundaries(&adjusted_ptr, &adjusted_size, page_size);

    if (adjusted_size == 0)
    {
        return true;
    }

#ifdef _WIN32
    return VirtualAlloc(adjusted_ptr, adjusted_size, MEM_COMMIT, PAGE_READWRITE)
           != NULL;
#else
    return mprotect(adjusted_ptr, adjusted_size, PROT_READ | PROT_WRITE) == 0;
#endif
}
