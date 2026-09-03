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
                if(Allocators.at(i).GetFreeBytes(&Allocators).at(i) > allocSize)
                {
                    void* p = Allocators.at(i).Malloc(Allocators.at(i), allocSize);
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
            
            return Allocators.at(Allocators.Len - 1).Malloc(allocSize);
        }
        
        inline void InternFree(void* mem)
        {
            for(int i = 0; i < Allocators.Len; ++i)
            {
                if(Allocators.at(i).OwnsPtr(mem))
                {
                    Allocators.at(i).Free(&Allocators.at(i), mem);
                    break;
                }
            }
        }
        
        inline void* InternRealloc(void* mem, usize allocSize)
        {
            for(int i = 0; i < Allocators.Len; ++i)
            {
                if(Allocators.at(i).OwnsPtr(mem))
                    return Allocators.at(i).Realloc(&Allocators.at(i), mem, allocSize);
            }
            return NULL;
        }
        
        inline void InternFreeAll()
        {
            for(int i = 0; i < Allocators.Len; ++i)
                Allocators.at(i).FreeAll(&Allocators.at(i));
        }
        
        inline void InternDestroy()
        {
            for(int i = 0; i < Allocators.Len; ++i)
                Allocators.at(i).Destroy(&Allocators.at(i));
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
