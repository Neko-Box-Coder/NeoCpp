#ifndef NSTD_ALLOCATOR_POOL_N_HPP
#define NSTD_ALLOCATOR_POOL_N_HPP

/*
Usage:
```c++
{
    Nstd::Allocator a = a.Init<int64, Nstd::HeapAllocator>(32);   //Reserve 32 int64
    ndefer { a.Destroy(); };
    int64* ints = a.Malloc<int64>(16);                          //Allocate 16 int64
    (void)ints;
    //...
    ints = a.Realloc<int64>(ints, 64);                            //Expands to 64 int64
    char* chars = a.Malloc<char>(16);
    (void)chars;
    a.Free(ints);
    a.FreeAll();
    chars = a.Malloc<char>(4);
}
```
*/

#include "ncpp.n.hpp"
#include "./TaggedUnion.n.hpp"

#include <string.h>
#include <stddef.h>

#if !defined(NSTD_ALLOC_MALLOC) && !defined(NSTD_ALLOC_FREE) && !defined(NSTD_ALLOC_REALLOC)
    #include <stdlib.h>
    #define NSTD_ALLOC_MALLOC(sz) malloc(sz)
    #define NSTD_ALLOC_FREE(p) free(p)
    #define NSTD_ALLOC_REALLOC(p, sz) realloc(p, sz)
#elif !defined(NSTD_ALLOC_MALLOC) || !defined(NSTD_ALLOC_FREE) || !defined(NSTD_ALLOC_REALLOC)
    #error "You cannot partially define custom memory allocation macros"
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
    inline void* Intern_Calloc(usize sz)
    {
        void* p = NSTD_ALLOC_MALLOC(sz);
        if(!p)
            return NULL;
        memset(p, 0, sz);
        return p;
    }
    
    using ReserveAheadSig = void (*)(void* context, uint64 size);
    using MallocSig = void* (*)(void* context, uint64 size);
    using FreeSig = void (*)(void* context, void* ptr);
    using ReallocSig = void* (*)(void* context, void* ptr, uint64 size);
    using FreeAllSig = void (*)(void* context);
    using DestroySig = void (*)(void* context);
    
    struct AllocatorPool
    {
        ReserveAheadSig ContextReserveAhead;
        MallocSig ContextMalloc;
        FreeSig ContextFree;
        ReallocSig ContextRealloc;
        FreeAllSig ContextFreeAll;
        DestroySig ContextDestroy;
        void* Context;
        bool Pool;
        
        inline void Init(   ReserveAheadSig contextReserveAhead,
                            MallocSig contextMalloc,
                            FreeSig contextFree,
                            ReallocSig contextRealloc,
                            FreeAllSig contextFreeAll,
                            DestroySig contextDestroy,
                            void* context,
                            bool pool)
        {
            ContextReserveAhead = contextReserveAhead;
            ContextMalloc = contextMalloc;
            ContextFree = contextFree;
            ContextRealloc = contextRealloc;
            ContextFreeAll = contextFreeAll;
            ContextDestroy = contextDestroy;
            Context = context;
            Pool = pool;
        }
        
        template<typename T>
        inline void ReserveAhead(uint64 count) { ContextReserveAhead(Context, sizeof(T) * count); }
        
        template<typename T>
        inline T* Malloc(uint64 count) { return (T*)ContextMalloc(Context, sizeof(T) * count); }
        
        template<typename T>
        inline void Free(T* ptr) { return ContextFree(Context, ptr); }
        
        template<typename T>
        inline T* Realloc(T* ptr, uint64 count) 
        {
            return (T*)ContextRealloc(Context, (void*)ptr, sizeof(T) * count); 
        }
        
        inline void FreeAll() { return ContextFreeAll(Context); }
        inline void Destroy() { return ContextDestroy(Context); }
        
        template<typename T>
        inline T* Calloc(uint64 count)
        {
            T* t = Malloc<T>(count);
            if(!t)
                return NULL;
            memset(t, 0, sizeof(T) * count);
            return t;
        }
    };
    
    #if 0
    struct LargeAllocs
    {
        uint8** Allocs;
        
        uint64* Sizes;
        bool* Used;
        
        uint16 Len;
        uint16 Cap;
        
        inline void Init(uint16 initialEntries)
        {
            Allocs = (uint8**)Intern_Calloc(initialEntries * sizeof(uint8*));
            Sizes = (uint64*)Intern_Calloc(initialEntries * sizeof(uint64));
            Used = (bool*)Intern_Calloc(initialEntries * sizeof(bool));
            
            if(!Allocs || !Sizes || !Used)
            {
                NSTD_ALLOC_FREE(Allocs);
                NSTD_ALLOC_FREE(Sizes);
                NSTD_ALLOC_FREE(Used);
                return;
            }
            Len = 0;
            Cap = initialEntries;
        }
        
        inline uint16 Fit(uint64 fitSize)
        {
            if(!Allocs || !Len)
                return Len;
        
            for(int i = 0; i < Len; ++i)
            {
                if(!Used[i] && Sizes[i] >= fitSize)
                    return i;
            }
            return Len;
        }
        
        inline uint8* Realloc(uint16 index, uint64 byteSizes)
        {
            if(index >= Len)
                return NULL;
            
            if(Sizes[index] >= byteSizes)
                return Allocs[index];
            
            uint8* t = (uint8*)NSTD_ALLOC_REALLOC(Allocs[index], byteSizes);
            if(!t)
                return NULL;
            
            Allocs[index] = t;
            Sizes[index] = byteSizes;
            return t;
        }
        
        inline void* Use(uint16 index)
        {
            n_assert(index < Len);
            Used[index] = true;
            return Allocs[index];
        }
        
        inline void NewEntry(uint64 allocSize)
        {
            if(Len == Cap)
            {
                uint8** a = (uint8**)NSTD_ALLOC_REALLOC(Allocs, (Cap * 2) * sizeof(uint8*));
                uint64* s = (uint64*)NSTD_ALLOC_REALLOC(Sizes, (Cap * 2) * sizeof(uint64));
                bool* u = (bool*)NSTD_ALLOC_REALLOC(Used, (Cap * 2) * sizeof(bool));
                if(!a || !s || !u)
                {
                    NSTD_ALLOC_FREE(a);
                    NSTD_ALLOC_FREE(s);
                    NSTD_ALLOC_FREE(u);
                    return;
                }
                
                Allocs = a;
                Sizes = s;
                Used = u;
                Cap *= 2;
            }
            
            uint8* m = (uint8*)NSTD_ALLOC_MALLOC(allocSize);
            Allocs[Len] = m;
            Sizes[Len] = allocSize;
            Used[Len] = false;
            ++Len;
        }
        
        inline void Free(uint16 index)
        {
            if(!Used || index >= Len)
                return;
            
            if(index == Len - 1)
            {
                Used[--Len] = false;
                return;
            }
            
            //Swap the freed one with the last one
            uint8* a = Allocs[Len - 1];
            uint64 s = Sizes[Len - 1];
            
            Allocs[Len - 1] = Allocs[index];
            Sizes[Len - 1] = Sizes[index];
            Used[Len - 1] = false;
            
            Allocs[index] = a;
            Sizes[index] = s;
        }
        
        inline void FreeAll()
        {
            if(!Allocs || !Sizes || !Used)
                return;
            
            for(uint16 i = 0; i < Len; ++i)
                Used[i] = false;
            Len = 0;
            return;
        }
        
        inline void Destroy()
        {
            if(!Allocs || !Sizes || !Used)
                return;
            
            for(uint16 i = 0; i < Len; ++i)
            {
                if(Sizes[i] > 0)
                    NSTD_ALLOC_FREE(Allocs[i]);
            }
            NSTD_ALLOC_FREE(Allocs);
            NSTD_ALLOC_FREE(Sizes);
            NSTD_ALLOC_FREE(Used);
            memset(this, 0, sizeof(LargeAllocs));
        }
    };
    #endif
    
    struct ArenaAllocator
    {
    };
    
    struct CustomAllocator
    {
    };
    
    static_assert(n_is_simple(AllocatorPool));
}

#endif
