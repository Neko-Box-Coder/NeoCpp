#ifndef NSTD_ALLOCATOR_N_HPP
#define NSTD_ALLOCATOR_N_HPP

/*
API:
```c++
struct Allocator
{
    static inline Allocator Init(   MallocSig contextMalloc,
                                    FreeSig contextFree,
                                    ReallocSig contextRealloc,
                                    FreeAllSig contextFreeAll,
                                    DestroySig contextDestroy,
                                    GetFreeBytesSig contextGetFreeBytes,
                                    void* context);
    
    template<typename T>
    inline n_view<T> Malloc(uint64 count);
    
    template<typename T>
    inline void Free(n_view<T> mem);
    inline void Free(void* mem);
    
    template<typename T>
    inline n_view<T> Realloc(n_view<T> mem, uint64 count);
    inline void FreeAll();
    inline void Destroy();
    inline uint64 GetFreeBytes() const;
    
    template<typename T>
    inline n_view<T> Calloc(uint64 count);
};
```

Usage:
```c++
{
    Nstd::HeapAllocator h = h.Init(32);
    Nstd::Allocator alloc = h.MakeAllocator();
    n_defer { alloc.Destroy(); };

    n_view<int64_t> ints = alloc.Malloc<int64_t>(16);
    ints = alloc.Realloc<int64_t>(ints, 64);
    alloc.Free(ints);
    alloc.FreeAll();
}
```
*/

#include "ncpp.n.hpp"

#include <string.h>
#include <stddef.h>

#if 0
    #if !defined(NSTD_ALLOC_MALLOC) && !defined(NSTD_ALLOC_FREE) && !defined(NSTD_ALLOC_REALLOC)
        #include <stdlib.h>
        #define NSTD_ALLOC_MALLOC(sz) malloc(sz)
        #define NSTD_ALLOC_FREE(p) free(p)
        #define NSTD_ALLOC_REALLOC(p, sz) realloc(p, sz)
    #elif !defined(NSTD_ALLOC_MALLOC) || !defined(NSTD_ALLOC_FREE) || !defined(NSTD_ALLOC_REALLOC)
        #error "You cannot partially define custom memory allocation macros"
    #endif
#endif

#ifndef KB
    #define KB * 1000
#endif

#ifndef MB
    #define MB * 1000000
#endif

#ifndef GB
    #define GB * 1000000000
#endif

namespace Nstd
{
    //inline void* Intern_Calloc(usize sz)
    //{
    //    void* p = NSTD_ALLOC_MALLOC(sz);
    //    if(!p)
    //        return NULL;
    //    memset(p, 0, sz);
    //    return p;
    //}
    
    //using ReserveAheadSig = void (*)(void* context, uint64 size);
    using MallocSig = void* (*)(void* context, uint64 size);
    using FreeSig = void (*)(void* context, void* ptr);
    using ReallocSig = void* (*)(void* context, void* ptr, uint64 size);
    using FreeAllSig = void (*)(void* context);
    using DestroySig = void (*)(void* context);
    using GetFreeBytesSig = uint64 (*)(const void* context);
    
    struct Allocator
    {
        MallocSig ContextMalloc;
        FreeSig ContextFree;
        ReallocSig ContextRealloc;
        FreeAllSig ContextFreeAll;
        DestroySig ContextDestroy;
        GetFreeBytesSig ContextGetFreeBytes;
        void* Context;
        
        static inline Allocator Init(   MallocSig contextMalloc,
                                        FreeSig contextFree,
                                        ReallocSig contextRealloc,
                                        FreeAllSig contextFreeAll,
                                        DestroySig contextDestroy,
                                        GetFreeBytesSig contextGetFreeBytes,
                                        void* context)
        {
            Allocator alloc = {};
            alloc.ContextMalloc = contextMalloc;
            alloc.ContextFree = contextFree;
            alloc.ContextRealloc = contextRealloc;
            alloc.ContextFreeAll = contextFreeAll;
            alloc.ContextDestroy = contextDestroy;
            alloc.ContextGetFreeBytes = contextGetFreeBytes;
            alloc.Context = context;
            return alloc;
        }
        
        //template<typename T>
        //inline void ReserveAhead(uint64 count) { ContextReserveAhead(Context, sizeof(T) * count); }
        
        template<typename T>
        inline n_view<T> Malloc(uint64 count)
        {
            T* retPtr = (T*)ContextMalloc(Context, sizeof(T) * count);
            return n_view<T>(retPtr, retPtr ? count : 0);
        }
        
        template<typename T>
        inline void Free(n_view<T> mem) { return ContextFree(Context, mem.data); }
        
        inline void Free(void* mem) { return ContextFree(Context, mem); }
        
        template<typename T>
        inline n_view<T> Realloc(n_view<T> mem, uint64 count)
        {
            T* retPtr = (T*)ContextRealloc(Context, mem.data, sizeof(T) * count); 
            return n_view<T>(retPtr, retPtr ? count : 0);
        }
        
        inline void FreeAll() { return ContextFreeAll(Context); }
        inline void Destroy() { return ContextDestroy(Context); }
        inline uint64 GetFreeBytes() const
        {
            if(ContextGetFreeBytes) 
                return ContextGetFreeBytes(Context); 
            return UINT64_MAX;
        }
        
        template<typename T>
        inline n_view<T> Calloc(uint64 count)
        {
            n_view<T> t = Malloc<T>(count);
            if(!t)
                return {};
            t.zero();
            return t;
        }
    };
    
    static_assert(n_is_simple(Allocator));
}

#endif
