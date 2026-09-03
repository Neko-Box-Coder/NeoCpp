#ifndef NSTD_HEAP_ALLOCATOR_N_HPP
#define NSTD_HEAP_ALLOCATOR_N_HPP

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
        
        static inline HeapAllocator Init(uint64 allocCount)
        {
            HeapAllocator alloc = {};
            alloc.MemLookup = (void**)Intern_Calloc(allocCount * sizeof(void*));
            alloc.Len = 0;
            alloc.Cap = allocCount;
            return alloc;
        }
        
        static inline uint32 GetKey(void* ptr, uint32 cap)
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
        
        inline bool Rehash()
        {
            void** newLookup = (void**)Intern_Calloc(Cap * 2 * sizeof(void*));
            if(!newLookup)
                return false;
            
            for(uint32 i = 0; i < Cap; ++i)
            {
                if(MemLookup[i])
                {
                    uint32 key = GetKey(MemLookup[i], Cap * 2);
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
            uint32 key = GetKey(ptr, Cap);
            while(MemLookup[key])
            {
                ++key;
                key %= Cap;
            }
            return key;
        }
        
        inline uint32 Intern_GetKey(void* ptr)
        {
            uint32 key = GetKey(ptr, Cap);
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
        
        static void* Malloc(void* c, uint64 size)
        {
            HeapAllocator* context = (HeapAllocator*)c;
            if(context->Len + 1 >= context->Cap / 2)
            {
                if(!context->Rehash())
                    return NULL;
            }
            
            void* m = NSTD_ALLOC_MALLOC(size);
            uint32 k = context->Intern_NullKey(m);
            context->MemLookup[k] = m;
            ++(context->Len);
            return m;
        }
        
        static void Free(void* c, void* ptr)
        {
            HeapAllocator* context = (HeapAllocator*)c;
            if(!ptr)
                return;
            
            uint32 k = context->Intern_GetKey(ptr);
            if(k == context->Cap)
                return;
            
            NSTD_ALLOC_FREE(context->MemLookup[k]);
            context->MemLookup[k] = NULL;
            --(context->Len);
        }
        
        static void* Realloc(void* c, void* ptr, uint64 size)
        {
            HeapAllocator* context = (HeapAllocator*)c;
            uint32 k = context->Intern_GetKey(ptr);
            if(k == context->Cap)
                return ptr;
            
            context->MemLookup[k] = NULL;
            void* p = NSTD_ALLOC_REALLOC((char*)ptr, size);
            if(!p)
            {
                k = context->Intern_NullKey(ptr);
                context->MemLookup[k] = ptr;
                return NULL;
            }
            
            k = context->Intern_NullKey(p);
            context->MemLookup[k] = p;
            return p;
        }
        
        static void FreeAll(void* c)
        {
            HeapAllocator* context = (HeapAllocator*)c;
            for(uint32 i = 0; i < context->Cap; ++i)
            {
                if(context->MemLookup[i])
                {
                    NSTD_ALLOC_FREE(context->MemLookup[i]);
                    context->MemLookup[i] = NULL;
                }
            }
        }
        
        static void Destroy(void* c) 
        {
            HeapAllocator* context = (HeapAllocator*)c;
            FreeAll(context);
            NSTD_ALLOC_FREE(context->MemLookup);
            memset(context, 0, sizeof(HeapAllocator));
        }
        
        inline Allocator MakeAllocator()
        {
            return Allocator::Init(Malloc, Free, Realloc, FreeAll, Destroy, NULL, this);
        }
    };
    
    static_assert(n_is_simple(HeapAllocator));
}


#endif
