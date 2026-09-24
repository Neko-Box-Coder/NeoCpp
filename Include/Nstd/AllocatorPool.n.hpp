#ifndef NSTD_ALLOCATOR_POOL_N_HPP
#define NSTD_ALLOCATOR_POOL_N_HPP

/*
API:
```c++
template<typename TARGET_ALLOC>
struct AllocatorPool
{
    static inline n_result<AllocatorPool> Init(Allocator backingAlloc, uint64 initialSize);
    inline n_result<void> AddAllocator(uint64 allocSize);
    
    template<typename T>
    inline n_view<T> Malloc(uint64 count);
    inline void Free(void* mem);
    
    template<typename T>
    inline n_view<T> Realloc(void* data, usize count);
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
        Allocator BackingAllocator;
        
        static inline n_result<AllocatorPool> Init(Allocator backingAlloc, uint64 initialSize)
        {
            n_use_error_defer();
            n_error_defer { backingAlloc.Destroy(); };
            
            AllocatorPool pool = {};
            pool.BackingAllocator = backingAlloc;
            pool.Allocators = pool.Allocators.Init(pool.BackingAllocator, 16);
            pool.BackingSizes = pool.BackingSizes.Init(pool.BackingAllocator, 16);
            pool.AddAllocator(initialSize).n_try();
            
            return pool;
        }
        
        inline n_result<void> AddAllocator(uint64 allocSize)
        {
            n_use_error_defer();
            
            n_view<uint8> backing = BackingAllocator.Malloc<uint8>(allocSize);
            n_check_true((bool)backing);
            n_error_defer { BackingAllocator.Free(backing); };
            
            TARGET_ALLOC alloc = alloc.Init(backing).n_try();
            n_error_defer 
            {
                alloc.ContextDestroy(&alloc); 
                Allocators.Resize(0); 
                BackingSizes.Resize(0); 
            };
            
            Allocators.Add(alloc).n_try();
            BackingSizes.Add(allocSize).n_try();
            return {};
        }
        
        template<typename T>
        inline n_view<T> Malloc(uint64 count)
        {
            for(int i = 0; i < Allocators.Len; ++i)
            {
                if(Allocators.At(i).GetFreeBytes() > sizeof(T) * count)
                {
                    n_view<T> p = Allocators.At(i).template Malloc<T>(count);
                    if(p)
                        return p;
                }
            }
            
            n_assert(Allocators.Len == BackingSizes.Len);
            uint64 allocSize = sizeof(T) * count;
            if(BackingSizes.Len == 0 || BackingSizes.At(BackingSizes.Len - 1) * 2 < allocSize + 1 KB)
            {
                if(AddAllocator(allocSize + 1 KB).err)
                    return {};
            }
            else
            {
                if(AddAllocator(BackingSizes.At(BackingSizes.Len - 1) * 2).err)
                    return {};
            }
            
            return Allocators.At(Allocators.Len - 1).template Malloc<T>(count);
        }
        
        
        inline void Free(void* mem)
        {
            for(int i = 0; i < Allocators.Len; ++i)
            {
                if(Allocators.At(i).OwnsPtr(mem))
                {
                    Allocators.At(i).Free(mem);
                    break;
                }
            }
        }
        
        template<typename T>
        inline n_view<T> Realloc(void* data, usize count)
        {
            for(int i = 0; i < Allocators.Len; ++i)
            {
                if(Allocators.At(i).OwnsPtr(data))
                    return Allocators.At(i).template Realloc<T>(data, count);
            }
            return {};
        }
        
        inline void FreeAll()
        {
            for(int i = 0; i < Allocators.Len; ++i)
                Allocators.At(i).FreeAll();
        }
        
        inline void Destroy()
        {
            for(int i = 0; i < Allocators.Len; ++i)
                Allocators.At(i).Destroy();
        }
        
        
        static inline void* ContextMalloc(void* context, uint64 allocSize)
        {
            AllocatorPool* pool = (AllocatorPool*)context;
            return pool->Malloc<uint8>(allocSize).data;
        }
        
        static inline void ContextFree(void* context, void* mem)
        {
            AllocatorPool* pool = (AllocatorPool*)context;
            return pool->Free(mem);
        }
        
        static inline void* ContextRealloc(void* context, void* mem, usize allocSize)
        {
            AllocatorPool* pool = (AllocatorPool*)context;
            return pool->Realloc<uint8>(mem, allocSize).data;
        }
        
        static inline void ContextFreeAll(void* context)
        {
            AllocatorPool* pool = (AllocatorPool*)context;
            return pool->FreeAll();
        }
        
        static inline void ContextDestroy(void* context)
        {
            AllocatorPool* pool = (AllocatorPool*)context;
            return pool->Destroy();
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
    
    //static_assert(n_is_simple(AllocatorPool));
}

#endif
