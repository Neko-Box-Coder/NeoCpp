#ifndef NSTD_HEAP_ALLOCATOR_N_HPP
#define NSTD_HEAP_ALLOCATOR_N_HPP

/*
API:
```c++
struct HeapAllocator
{
    static inline n_result<HeapAllocator> Init(uint64 allocCount);
    inline bool OwnsPtr(void* ptr);
    
    template<typename T>
    inline n_view<T> Malloc(uint64 count);
    inline void Free(void* data);
    
    template<typename T>
    inline n_view<T> Realloc(void* data, uint64 count);
    inline void FreeAll();
    inline void Destroy();
    inline Allocator MakeAllocator();
};
```

Usage:
```c++
{
    Nstd::HeapAllocator h = h.Init(32);
    Nstd::Allocator alloc = h.MakeAllocator();
    n_defer { alloc.Destroy(); };

    n_view<int> ints = alloc.Malloc<int>(16);
    alloc.Free(ints);
}
```
*/

#include "ncpp.n.hpp"
#include "./Allocator.n.hpp"

#if !defined(NSTD_ALLOC_MALLOC) && !defined(NSTD_ALLOC_FREE) && !defined(NSTD_ALLOC_REALLOC)
    #include <stdlib.h>
    #define NSTD_ALLOC_MALLOC(sz) malloc(sz)
    #define NSTD_ALLOC_FREE(p) free(p)
    #define NSTD_ALLOC_REALLOC(p, sz) realloc(p, sz)
#elif !defined(NSTD_ALLOC_MALLOC) || !defined(NSTD_ALLOC_FREE) || !defined(NSTD_ALLOC_REALLOC)
    #error "You cannot partially define custom memory allocation macros"
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
    
    struct HeapAllocator
    {
        void** MemLookup;
        uint32 Len;
        uint32 Cap;
        
        static inline n_result<HeapAllocator> Init(uint64 allocCount)
        {
            HeapAllocator alloc = {};
            alloc.MemLookup = (void**)Intern_Calloc(allocCount * sizeof(void*));
            n_check_true(alloc.MemLookup);
            alloc.Len = 0;
            alloc.Cap = allocCount;
            return alloc;
        }
        
        static inline uint32 Intern_GetPossibleKey(void* ptr, uint32 cap)
        {
            uint halfBits = sizeof(uintptr_t) / 8 / 2;
            uintptr_t k = (uintptr_t)ptr;
            uintptr_t s1 = k;
            uintptr_t s2 = k;
            do
            {
                s1 >>= halfBits;
                s2 <<= halfBits;
                k = k ^ s1 ^ s2;
                halfBits /= 2;
            }
            while(halfBits > 0);
            return (uint32)(k % cap);
        }
        
        inline bool Intern_Rehash()
        {
            void** newLookup = (void**)Intern_Calloc(Cap * 2 * sizeof(void*));
            if(!newLookup)
                return false;
            
            for(uint32 i = 0; i < Cap; ++i)
            {
                if(MemLookup[i])
                {
                    uint32 key = Intern_GetPossibleKey(MemLookup[i], Cap * 2);
                    while(newLookup[key])
                    {
                        ++key;
                        key %= Cap * 2;
                    }
                    newLookup[key] = MemLookup[i];
                }
            }
            Cap *= 2;
            NSTD_ALLOC_FREE(MemLookup);
            MemLookup = newLookup;
            return true;
        }
        
        inline uint32 Intern_NullKey(void* ptr)
        {
            uint32 key = Intern_GetPossibleKey(ptr, Cap);
            while(MemLookup[key])
            {
                ++key;
                key %= Cap;
            }
            return key;
        }
        
        inline uint32 Intern_GetKey(void* ptr)
        {
            uint32 key = Intern_GetPossibleKey(ptr, Cap);
            uint32 oriKey = key;
             do
            {
                if(MemLookup[key] == ptr) 
                    return key;
                ++key; 
                if(key >= Cap) 
                    key = 0;
            } while(key != oriKey);
            return Cap;
        }
        
        inline bool OwnsPtr(void* ptr)
        {
            if(!ptr)
                return false;
            
            uint32 k = Intern_GetKey(ptr);
            if(k == Cap)
                return false;
            return true;
        }
        
        template<typename T>
        inline n_view<T> Malloc(uint64 count)
        {
            if(Len + 1 >= Cap / 2)
            {
                if(!Intern_Rehash())
                    return {};
            }
            
            void* m = NSTD_ALLOC_MALLOC(count * sizeof(T));
            uint32 k = Intern_NullKey(m);
            MemLookup[k] = m;
            ++(Len);
            return {(T*)m, count};
        }
        
        inline void Free(void* data)
        {
            if(!data)
                return;
            
            uint32 k = Intern_GetKey(data);
            if(k == Cap)
                return;
            
            NSTD_ALLOC_FREE(MemLookup[k]);
            MemLookup[k] = NULL;
            --Len;
        }
        
        template<typename T>
        inline n_view<T> Realloc(void* data, uint64 count)
        {
            uint32 k = Intern_GetKey(data);
            if(k == Cap)
                return { (T*)data, count };
            
            MemLookup[k] = NULL;
            void* p = NSTD_ALLOC_REALLOC((char*)data, count);
            if(!p)
            {
                k = Intern_NullKey(data);
                MemLookup[k] = data;
                return {};
            }
            
            k = Intern_NullKey(p);
            MemLookup[k] = p;
            return { (T*)p, count };
        }
        
        inline void FreeAll()
        {
            for(uint32 i = 0; i < Cap; ++i)
            {
                if(MemLookup[i])
                {
                    NSTD_ALLOC_FREE(MemLookup[i]);
                    MemLookup[i] = NULL;
                }
            }
        }
        
        inline void Destroy()
        {
            FreeAll();
            NSTD_ALLOC_FREE(MemLookup);
            memset(this, 0, sizeof(HeapAllocator));
        }
        
        static void* ContextMalloc(void* c, uint64 size)
        {
            HeapAllocator* context = (HeapAllocator*)c;
            return context->Malloc<uint8>(size).data;
        }
        
        static void ContextFree(void* c, void* ptr)
        {
            HeapAllocator* context = (HeapAllocator*)c;
            context->Free(ptr);
        }
        
        static void* ContextRealloc(void* c, void* ptr, uint64 size)
        {
            HeapAllocator* context = (HeapAllocator*)c;
            return context->Realloc<uint8>(ptr, size).data;
        }
        
        static void ContextFreeAll(void* c)
        {
            HeapAllocator* context = (HeapAllocator*)c;
            context->FreeAll();
        }
        
        static void ContextDestroy(void* c) 
        {
            HeapAllocator* context = (HeapAllocator*)c;
            context->Destroy();
        }
        
        inline Allocator MakeAllocator()
        {
            return Allocator::Init( ContextMalloc, 
                                    ContextFree, 
                                    ContextRealloc, 
                                    ContextFreeAll, 
                                    ContextDestroy, 
                                    NULL, 
                                    this);
        }
    };
    
    static_assert(n_is_simple(HeapAllocator));
}


#endif
