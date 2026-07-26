#ifndef NSTD_HEAP_ALLOCATOR_POOL_N_HPP
#define NSTD_HEAP_ALLOCATOR_POOL_N_HPP

#include "ncpp.n.hpp"
#include "./AllocatorPool.n.hpp"


namespace Nstd
{
    struct HeapAllocatorPool
    {
        void** MemLookup;
        uint32 Len;
        uint32 Cap;
        
        inline void Init(uint64 allocCount)
        {
            MemLookup = (void**)Intern_Calloc(allocCount * sizeof(void*));
            Len = 0;
            Cap = allocCount;
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
                    uint32 key = (uintptr_t)MemLookup[i] % (Cap * 2);
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
            uint32 key = (uintptr_t)ptr % Cap;
            while(MemLookup[key])
            {
                ++key;
                key %= Cap;
            }
            return key;
        }
        
        inline uint32 Intern_GetKey(void* ptr)
        {
            uint32 key = (uintptr_t)ptr % Cap;
            uint32 oriKey = key;
            while(MemLookup[key] != ptr && key != oriKey)
            {
                ++key;
                key %= Cap;
            }
            if(MemLookup[key] != ptr && key == oriKey)
                return Cap;
            return key;
        }
        
        static void ReserveAhead(void*, uint64) {}
        
        static void* Malloc(void* c, uint64 size)
        {
            HeapAllocatorPool* context = (HeapAllocatorPool*)c;
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
            HeapAllocatorPool* context = (HeapAllocatorPool*)c;
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
            HeapAllocatorPool* context = (HeapAllocatorPool*)c;
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
            HeapAllocatorPool* context = (HeapAllocatorPool*)c;
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
            HeapAllocatorPool* context = (HeapAllocatorPool*)c;
            FreeAll(context);
            NSTD_ALLOC_FREE(context->MemLookup);
            memset(context, 0, sizeof(HeapAllocatorPool));
        }
        
        inline AllocatorPool MakeAllocatorPool()
        {
            AllocatorPool retAlloc = {};
            retAlloc.Init(ReserveAhead, Malloc, Free, Realloc, FreeAll, Destroy, this, true);
            return retAlloc;
        }
    };
    
    static_assert(n_is_simple(HeapAllocatorPool));
}


#endif
