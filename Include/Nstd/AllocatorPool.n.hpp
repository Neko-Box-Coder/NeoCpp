#ifndef NSTD_ALLOCATOR_POOL_N_HPP
#define NSTD_ALLOCATOR_POOL_N_HPP

/*
API:
```c++
template<typename TARGET_ALLOC>
struct AllocatorPool
{
    List<TARGET_ALLOC> Allocators;
    List<uint64> BackingSizes;
    Allocator* BackingAllocator;

    static inline n_result<AllocatorPool> Init(n_ref Allocator& backingAlloc, uint64 initialSize);
    inline n_result<void> AddAllocator(uint64 allocSize);
    inline Allocator MakeAllocator();
};
```

Usage:
```c++
{
    Nstd::HeapAllocator h = h.Init(32);
    Nstd::Allocator alloc = h.MakeAllocator();
    n_defer { alloc.Destroy(); };
    
    //NOTE: AllocatorPool with FastAllocator
    Nstd::AllocatorPool<Nstd::FastAllocator<>> pool = pool.Init(n_ref alloc, 64).n_try();
    Nstd::Allocator poolAlloc = pool.MakeAllocator();
}
```
*/

#include "ncpp.n.hpp"

#include "./Allocator.n.hpp"
#include "./List.n.hpp"

#include <string.h>
#include <stddef.h>

namespace Nstd
{
    template<typename TARGET_ALLOC>
    struct AllocatorPool
    {
        List<TARGET_ALLOC> Allocators;
        List<uint64> BackingSizes;
        Allocator* BackingAllocator;
        
        static inline n_result<AllocatorPool> Init(n_ref Allocator& backingAlloc, uint64 initialSize)
        {
            n_use_error_defer();
            
            n_error_defer { backingAlloc.Destroy(); };
            AllocatorPool pool = {};
            pool.BackingAllocator = &backingAlloc;
            pool.Allocators = pool.Allocators.Init(n_ref *pool.BackingAllocator, 16);
            pool.BackingSizes = pool.BackingSizes.Init(n_ref *pool.BackingAllocator, 16);
            pool.AddAllocator(initialSize).n_try();
            
            return pool;
        }
        
        inline n_result<void> AddAllocator(uint64 allocSize)
        {
            n_use_error_defer();
            n_check_true(BackingAllocator);
            
            n_view<uint8> backing = BackingAllocator->Malloc<uint8>(allocSize);
            n_check_true((bool)backing);
            n_error_defer { BackingAllocator->Free(backing); };
            
            TARGET_ALLOC alloc = {};
            alloc.Init(backing).n_try();
            n_error_defer 
            {
                alloc.Destroy(&alloc); 
                Allocators.Resize(0); 
                BackingSizes.Resize(0); 
            };
            
            Allocators.Add(alloc).n_try();
            BackingSizes.Add(allocSize).n_try();
            return {};
        }
        
        inline void* InternMalloc(uint64 allocSize)
        {
            for(int i = 0; i < Allocators.Len; ++i)
            {
                if(Allocators.At(i).GetFreeBytes(&Allocators.At(i)) > allocSize)
                {
                    void* p = Allocators.At(i).Malloc(&Allocators.At(i), allocSize);
                    if(p)
                        return p;
                }
            }
            
            n_assert(Allocators.Len == BackingSizes.Len);
            if(BackingSizes.Len == 0 || BackingSizes.At(BackingSizes.Len - 1) * 2 < allocSize + 1 KB)
            {
                if(AddAllocator(allocSize + 1 KB).err)
                    return NULL;
            }
            else
            {
                if(AddAllocator(BackingSizes.At(BackingSizes.Len - 1) * 2).err)
                    return NULL;
            }
            
            return Allocators.At(Allocators.Len - 1).Malloc(&Allocators.At(Allocators.Len - 1), 
                                                            allocSize);
        }
        
        inline void InternFree(void* mem)
        {
            for(int i = 0; i < Allocators.Len; ++i)
            {
                if(Allocators.At(i).OwnsPtr(mem))
                {
                    Allocators.At(i).Free(&Allocators.At(i), mem);
                    break;
                }
            }
        }
        
        inline void* InternRealloc(void* mem, usize allocSize)
        {
            for(int i = 0; i < Allocators.Len; ++i)
            {
                if(Allocators.At(i).OwnsPtr(mem))
                    return Allocators.At(i).Realloc(&Allocators.At(i), mem, allocSize);
            }
            return NULL;
        }
        
        inline void InternFreeAll()
        {
            for(int i = 0; i < Allocators.Len; ++i)
                Allocators.At(i).FreeAll(&Allocators.At(i));
        }
        
        inline void InternDestroy()
        {
            for(int i = 0; i < Allocators.Len; ++i)
                Allocators.At(i).Destroy(&Allocators.At(i));
        }
        
        static inline void* Malloc(void* context, uint64 allocSize)
        {
            AllocatorPool* pool = (AllocatorPool*)context;
            return pool->InternMalloc(allocSize);
        }
        
        static inline void Free(void* context, void* mem)
        {
            AllocatorPool* pool = (AllocatorPool*)context;
            return pool->InternFree(mem);
        }
        
        static inline void* Realloc(void* context, void* mem, usize allocSize)
        {
            AllocatorPool* pool = (AllocatorPool*)context;
            return pool->InternRealloc(mem, allocSize);
        }
        
        static inline void FreeAll(void* context)
        {
            AllocatorPool* pool = (AllocatorPool*)context;
            return pool->InternFreeAll();
        }
        
        static inline void Destroy(void* context)
        {
            AllocatorPool* pool = (AllocatorPool*)context;
            return pool->InternDestroy();
        }
        
        inline Allocator MakeAllocator()
        {
            return Allocator::Init(Malloc, Free, Realloc, FreeAll, Destroy, NULL, this);
        }
    };
    
    //static_assert(n_is_simple(AllocatorPool));
}

#endif
