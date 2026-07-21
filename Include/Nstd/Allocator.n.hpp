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
#include <stddef.h>

namespace Nstd
{
    /*
    struct Backtrace
    {
        void* Allocations;
    }
    */
    //template<usize BLOCK_SIZE = 32>
    
    
    struct HeapAllocator
    {
        struct Intern_Bucket
        {
            void** Data;
            uint32 MaxLen;
            uint32 Cap;
            uint32 LastFree;
        };
        
        Intern_Bucket* Buckets;
        uint32 Len;
        uint32 LastFree;
        
        static inline usize MinOverhead()
        {
            return  alignof(max_align_t) >= sizeof(uint32) ? 
                    alignof(max_align_t) : 
                    alignof(max_align_t) * ((sizeof(uint32) + alignof(max_align_t) - 1) / 
                                            alignof(max_align_t));
        }
        
        template<typename T>
        static inline HeapAllocator Init(uint64 reserveSize)
        {
            return {};
        }
        
        template<typename T>
        static inline void ReserveAhead(uint64 reserveSize)
        {
            return;
        }
        
        template<typename T>
        inline T* Malloc(uint64 size)
        {
            uint32 chosen = Len;
            uint32 slot = 0;
            if(Buckets)
            {
                //Use free one
                if(LastFree != Len)
                {
                    chosen = LastFree;
                    if(Buckets[chosen].LastFree != Buckets[chosen].MaxLen)
                    {
                        slot = Buckets[chosen].LastFree;
                        Buckets[chosen].LastFree = Buckets[chosen].MaxLen;
                    }
                    else
                        slot = Buckets[chosen].MaxLen++;
                }
                //No free
                else
                {
                    //Find any buckets that are free
                    for(uint32 i = 0; i < Len; ++i)
                    {
                        if(Buckets[i].LastFree != Buckets[i].MaxLen)
                        {
                            chosen = i;
                            slot = Buckets[i].LastFree;
                            break;
                        }
                        else if(Buckets[i].MaxLen < Buckets[i].Cap)
                        {
                            chosen = i;
                            slot = Buckets[i].MaxLen++;
                            break;
                        }
                    }
                    
                    //Create new one if no space
                    if(chosen == Len)
                    {
                        {
                            void* t = realloc(Buckets, (Len + 1) * sizeof(Intern_Bucket));
                            if(!t)
                                return NULL;
                            Buckets = (Intern_Bucket*)t;
                        }
                        
                        Buckets[Len].Data = (void**)calloc(Buckets[Len - 1].Cap * 2, sizeof(void*));
                        Buckets[Len].MaxLen = 1;
                        Buckets[Len].Cap = Buckets[Len - 1].Cap * 2;
                        Buckets[Len++].LastFree = 1;
                        slot = 0;
                        chosen = Len - 1;
                    }
                }
            } //if(Buckets)
            else
            {
                //First initialization
                Buckets = (Intern_Bucket*)malloc(sizeof(Intern_Bucket));
                memset(Buckets, 0, sizeof(Intern_Bucket));
                Len = 1;
                LastFree = 0;
                
                Buckets[0].Data = (void**)calloc(64, sizeof(void*));
                Buckets[0].MaxLen = 1;
                Buckets[0].Cap = 64;
                Buckets[0].LastFree = 1;
                chosen = 0;
                slot = 0;
            }
            
            n_assert(chosen != Len);
            
            Intern_Bucket& bucket = Buckets[chosen];
            void* retP = NULL;
            n_assert(bucket.Data[slot] == NULL);
            bucket.Data[slot] = malloc(sizeof(T) * size + MinOverhead());
            if(!bucket.Data[slot])
                return NULL;
            memcpy(bucket.Data[slot], &chosen, sizeof(chosen));
            retP = (char*)bucket.Data[slot] + MinOverhead();
            bucket.LastFree = bucket.MaxLen;
            
            if(bucket.MaxLen >= bucket.Cap)
                LastFree = Len;
            else
                LastFree = chosen;
            
            return (T*)retP;
        }
        
        template<typename T>
        inline void Free(T* ptr)
        {
            if(!ptr)
                return;
            
            void* p = (char*)ptr - MinOverhead();
            
            uint32 chosen = 0;
            memcpy(&chosen, p, sizeof(chosen));
            n_assert(chosen < Len);
            
            Intern_Bucket& bucket = Buckets[chosen];
            uint32 f = bucket.MaxLen;
            for(uint32 i = 0; i < bucket.MaxLen; ++i)
            {
                if(bucket.Data[i] == p)
                {
                    f = i;
                    break;
                }
            }
            
            n_assert(f != bucket.MaxLen);
            free(bucket.Data[f]);
            bucket.Data[f] = NULL;
            
            if(f == bucket.MaxLen)
            {
                --bucket.MaxLen;
                bucket.LastFree = bucket.MaxLen;
            }
            else
                bucket.LastFree = f;
            
            LastFree = chosen;
        }
        
        template<typename T>
        inline T* Realloc(T* ptr, uint64 size)
        {
            void* p = realloc((char*)ptr - MinOverhead(), sizeof(T) * size + MinOverhead());
            if(!p)
                return NULL;
            return (T*)((char*)p + MinOverhead());
        }
        
        inline void FreeAll()
        {
            //TODO
        }
        
        inline void Destroy() 
        {
            //TODO
        }
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
        inline void ReserveAhead(uint64 size)
        {
            INTERN_NSTD_DISPATCH(ReserveAhead<T>(size), return);
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
            T* t = Malloc<T>(size);
            if(!t)
                return t;
            memset(t, 0, sizeof(T) * size);
            return t;
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
