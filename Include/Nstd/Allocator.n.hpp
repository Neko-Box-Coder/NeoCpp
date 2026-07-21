#ifndef NSTD_ALLOCATOR_N_HPP
#define NSTD_ALLOCATOR_N_HPP

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
#include <stdlib.h>

namespace Nstd
{
    struct HeapAllocator
    {
        void* Allocations;
        uint64 Size;
        uint64 Cap;
        
        template<typename T>
        static inline HeapAllocator Init(uint64 reserveSize)
        {
            return {};
        }
        
        template<typename T>
        static inline void Reserve(uint64 reserveSize)
        {
            return;
        }
        
        template<typename T>
        inline T* Malloc(uint64 size)
        {
            return (T*)malloc(sizeof(T) * size);
        }
        
        template<typename T>
        inline void Free(T* ptr)
        {
            free(ptr);
        }
        
        template<typename T>
        inline T* Realloc(T* ptr, uint64 size)
        {
            return (T*)realloc(ptr, sizeof(T) * size);
        }
        
        template<typename T>
        inline T* Calloc(uint64 size)
        {
            return (T*)calloc(size, sizeof(T));
        }
        
        inline void FreeAll() {}
        
        inline void Destroy() {}
    };
    
    struct ArenaAllocator
    {
    };
    
    struct CustomAllocator
    {
    };
    
    struct Allocator
    {
        TaggedUnion<HeapAllocator, ArenaAllocator, CustomAllocator, Allocator*> Impl;
        
        template<typename T, typename AllocType>
        static inline Allocator Init(uint64 reserveSize) n_defer_with(Destroy(ret_val))
        {
            Allocator a = {};
            a.Impl = a.Impl.template Init<AllocType>( AllocType::template Init<T>(reserveSize) );
            return a;
        }
        
        template<typename T>
        static inline Allocator InitProxy(  Allocator* alloc, 
                                            uint64 reserveSize) n_defer_with(Destroy(ret_val))
        {
            Allocator a = {};
            a.Impl = a.Impl.template Init<Allocator*>(alloc);
            return a;
        }
        
        #define INTERN_NSTD_DISPATCH(action, tempRet) \
            do \
            { \
                switch(Impl.Index) \
                { \
                    case n_typeof(Impl)::GetIndex<HeapAllocator>(): \
                        return Impl.Get<HeapAllocator>().action; \
                    case n_typeof(Impl)::GetIndex<ArenaAllocator>(): \
                    case n_typeof(Impl)::GetIndex<CustomAllocator>(): \
                    case n_typeof(Impl)::GetIndex<Allocator*>(): \
                        tempRet; \
                    default: \
                        tempRet; \
                } \
            } while(0)
        
        template<typename T>
        inline void Reserve(uint64 size)
        {
            INTERN_NSTD_DISPATCH(Reserve<T>(size), return);
        }
        
        template<typename T>
        inline T* Malloc(uint64 size)
        {
            INTERN_NSTD_DISPATCH(Malloc<T>(size), return NULL);
        }
        
        template<typename T>
        inline void Free(T* ptr)
        {
            INTERN_NSTD_DISPATCH(Free<T>(ptr), return);
        }
        
        template<typename T>
        inline T* Realloc(T* ptr, uint64 size)
        {
            INTERN_NSTD_DISPATCH(Realloc<T>(ptr, size), return NULL);
        }
        
        template<typename T>
        inline T* Calloc(uint64 size)
        {
            INTERN_NSTD_DISPATCH(Calloc<T>(size), return NULL);
        }
        
        inline void FreeAll() 
        {
            INTERN_NSTD_DISPATCH(FreeAll(), return);
        }
        
        inline void Destroy() 
        {
            INTERN_NSTD_DISPATCH(Destroy(), return);
            //memset(this, 0, sizeof(*this));
        }
        
        #undef INTERN_NSTD_DISPATCH
    };
    
}



#endif
