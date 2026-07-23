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
#include "./BitView.n.hpp"
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
    
    template<usize BLOCK_SIZE, bool SINGLE>
    struct Page
    {
        static constexpr usize PAGE_SIZE = BLOCK_SIZE * BLOCK_SIZE;
        
        uint8* Blocks;
        BitView Control;
        Page<BLOCK_SIZE, SINGLE>* Next;
        uint16 Count;
        
        inline void Init()
        {
            Blocks = (uint8*)NSTD_ALLOC_MALLOC(PAGE_SIZE);
            memset(&Blocks[0], 0, BLOCK_SIZE);
            Blocks[0] = 1;  //Control block
            Control = Control.Init( { Blocks, BLOCK_SIZE } );
            Next = NULL;
            Count = 1;
        }
        
        template<bool S = SINGLE, n_enable_if(S)>
        inline Page* FitSingleInChain(uint16 fitSize, n_out uint16& outFitIndex)
        {
            if(fitSize > BLOCK_SIZE)
                return NULL;
            
            Page* curPage = this;
            while(curPage)
            {
                for(int i = 1; i < curPage->Control.Len(); ++i)
                {
                    if(!curPage->Control.GetBit(i))
                    {
                        outFitIndex = i;
                        return curPage;
                    }
                }
                
                curPage = curPage->Next;
            }
            
            return NULL;
        }
    
        template<bool S = SINGLE, n_enable_if(!S)>
        inline Page* FitInChain(uint64 fitSize, n_out uint16& outFitIndex)
        {
            Page* curPage = this;
            const uint16 fitBlockCount = (fitSize + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
            while(curPage)
            {
                if(BLOCK_SIZE - curPage->Count < fitBlockCount)
                    goto nextPage;
                
                {
                    uint16 curCount = 0;
                    uint16 prevUsedIdx = 0;
                    for(uint32 i = 1; i < curPage->Control.Len(); ++i)
                    {
                        if(curPage->Control.GetBit(i)) //Occupied
                        {
                            curCount = 0;
                            prevUsedIdx = i;
                        }
                        //Leave gap between allocations
                        else if(i == 1 || (int)prevUsedIdx < i - 1)
                        {
                            ++curCount;
                            if( curCount > fitBlockCount ||
                                (curCount == fitBlockCount && i == curPage->Control.Len() - 1))
                            {
                                if(prevUsedIdx == 0)
                                    outFitIndex = 1;
                                else
                                    outFitIndex = prevUsedIdx + 2;
                                return curPage;
                            }
                        }
                    }
                }
                nextPage:;
                curPage = curPage->Next;
            }
            return NULL;
        }
        
        inline void AppendPageToChain()
        {
            Page* nextPage = (Page*)NSTD_ALLOC_MALLOC(sizeof(Page));
            nextPage->Init();
            
            //Walk to end
            Page* curPage = this;
            while(curPage->Next)
                curPage = curPage->Next;
            
            curPage->Next = nextPage;
        }
        
        template<bool S = SINGLE, n_enable_if(S)>
        inline void* UseSingle(uint16 index)
        {
            n_assert(index > 0 && index <= BLOCK_SIZE);
            n_assert(!Control.GetBit(index));
            Control.SetBit<true>(index);
            ++Count;
            return &Blocks[index * BLOCK_SIZE];
        }
        
        template<bool OVERLAP = false, bool S = SINGLE, n_enable_if(!S)>
        inline void* Use(uint16 index, uint64 bytes)
        {
            const uint32 blocksNeeded = (bytes + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
            const uint32 endIndex = index + blocksNeeded; (void)endIndex;
            n_assert(index > 0 && endIndex <= BLOCK_SIZE);
            n_result<void> r = Control.SetBitsAt<true>(index, blocksNeeded); (void)r;
            n_assert(!r.err);
            if(endIndex < BLOCK_SIZE) //Assert gap
                n_assert(!Control.GetBit(endIndex));
            
            Count += blocksNeeded;
            return &Blocks[index * BLOCK_SIZE];
        }
        
        inline uint16 FindIndex(void* ptr)
        {
            if(ptr < Blocks || ptr >= Blocks + PAGE_SIZE)
                return Count;
            
            n_assert((ptrdiff_t)ptr % BLOCK_SIZE == 0);
            return ((ptrdiff_t)ptr - (ptrdiff_t)Blocks) / BLOCK_SIZE;
        }
        
        template<bool S = SINGLE, n_enable_if(!S)>
        inline usize GetMaxSize(uint16 index)
        {
            if(index >= Count)
                return 0;
            n_assert(Control.GetBit(index));
            n_result<usize> f = Control.GetBitsUntilFlipped(index);
            n_assert(!f.err);
            return (f.value - index) * BLOCK_SIZE;
        }
        
        inline void Free(uint16 index)
        {
            if(index >= Count)
                return;
            
            if(SINGLE)
            {
                Control.SetBit<false>(index);
                --Count;
                return;
            }
            else
            {
                n_assert(Control.GetBit(index));
                n_result<usize> f = Control.GetBitsUntilFlipped(index);
                n_assert(!f.err);
                n_assert(f.value != Control.Len());
                n_assert(f.value > index);
                Control.SetBits<false>(index, f.value - index);
                return;
            }
        }
        
        
        template<bool S = SINGLE, n_enable_if(S)>
        inline uint16 ReallocSingle(uint16 index, uint64 bytes)
        {
            if(bytes > BLOCK_SIZE)
                return Count;
            return index;
        }
        
        template<bool S = SINGLE, n_enable_if(!S)>
        inline bool Realloc(uint16 index, uint64 bytes)
        {
            if(bytes > PAGE_SIZE - BLOCK_SIZE)
                return false;
            
            const uint32 blocksNeeded = ((bytes + BLOCK_SIZE - 1) / BLOCK_SIZE);
            
            n_assert(index < Count);
            n_result<usize> f = Control.GetBitsUntilFlipped(index);
            n_assert(!f.err);
            n_assert(f.value > index);
            
            uint16 blocksOccupied = f.value - index;
            if(blocksNeeded == blocksOccupied)
                return true;
            
            if(blocksNeeded < blocksOccupied) //Shrink
            {
                Control.SetBits<false>(index + blocksNeeded, blocksOccupied - blocksNeeded);
                Count += blocksOccupied - blocksNeeded;
                return true;
            }
            else //Grow
            {
                if(f.value == Count)
                    return false;
                
                n_assert(Control.GetBit(index + blocksOccupied) == 0);
                f = Control.GetBitsUntilFlipped(index + blocksOccupied);
                n_assert(!f.err);
                uint16 blocksFree = f.value - index;
                
                //Not enough
                if( blocksFree < blocksNeeded || 
                    (blocksFree == blocksNeeded && BLOCK_SIZE - index != blocksFree))
                {
                    return false;
                }
                
                Count -= blocksOccupied;
                Use<true>(index, bytes);
                return true;
            }
        }
        
        inline void FreeAllInChain()
        {
            Page* curPage = this;
            while(curPage)
            {
                curPage->Blocks[0] = 1; //Control block
                for(uint16 i = 1; i < BLOCK_SIZE; ++i)
                    curPage->Blocks[i] = 0;
                curPage->Count = 1;
                curPage = curPage->Next;
            }
        }
        
        inline void DestroyAllInChain()
        {
            Page* curPage = this->Next;
            while(curPage)
            {
                Page* nextPage = curPage->Next;
                NSTD_ALLOC_FREE(curPage);
                curPage = nextPage;
            }
            
            memset(this, 0, sizeof(Page));
        }
        
        inline Page* ChainTail()
        {
            Page* curPage = this;
            while(curPage->Next)
                curPage = curPage->Next;
            return curPage;
        }
    };
    
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
    
    
    template<usize SMALL_BLOCK_SIZE = 8, usize NORMAL_BLOCK_SIZE = 16>
    struct HeapAllocatorTemplate
    {
        //(8 * 8) * 8 max bytes where first block is control block
        Page<SMALL_BLOCK_SIZE, true> Small;
        
        //(16 * 8) * 16 max bytes where first block is control block
        Page<NORMAL_BLOCK_SIZE, false> Normal;
        LargeAllocs Large;
        
        static inline usize MinOverhead()
        {
            return  alignof(max_align_t) >= sizeof(uint32) ? 
                    alignof(max_align_t) : 
                    alignof(max_align_t) * ((sizeof(uint32) + alignof(max_align_t) - 1) / 
                                            alignof(max_align_t));
        }
        
        template<typename T>
        static inline HeapAllocatorTemplate Init(uint64 reserveSize)
        {
            HeapAllocatorTemplate h = {};
            h.Small.Init();
            h.Normal.Init();
            h.Large.Init(16);
            return h;
        }
        
        
        template<typename T>
        inline void ReserveAhead(uint64 reserveSize)
        {
            uint64 bytesNeeded = sizeof(T) * reserveSize;
            if(bytesNeeded < SMALL_BLOCK_SIZE) //Small
            {
                if(Small.Fit(reserveSize) != Small.Count)
                {
                    //n_trace_printf_track();
                    return;
                }
                
                //n_trace_printf_track();
                Small.AppendPageToChain();
                return;
            }
            
            if(bytesNeeded >= n_typeof(Normal)::PAGE_SIZE - NORMAL_BLOCK_SIZE) //Large allocation
            {
                if(Large.Fit(reserveSize) != Large.Len)
                {
                    //n_trace_printf_track();
                    return;
                }
                
                //n_trace_printf_track();
                Large.NewEntry(reserveSize);
                return;
            }
            
            //No need to reserve if it can fit into existing page
            uint16 fitIndex;
            if(Normal.Fit(reserveSize, fitIndex))
            {
                //n_trace_printf_track();
                return;
            }
            
            //n_trace_printf_track();
            Normal.AppendPageToChain();
            return;
        }
        
        template<typename T>
        inline T* Malloc(uint64 mallocSize)
        {
            uint64 bytesNeeded = sizeof(T) * mallocSize;
            if(bytesNeeded <= SMALL_BLOCK_SIZE) //Small
            {
                //n_trace_printf_track();
                uint16 fitIndex;
                if(!Small.FitSingleInChain(bytesNeeded, fitIndex))
                    return NULL;
                Small.AppendPageToChain();
                //n_trace_printf_track();
                return (T*)Small.ChainTail()->UseSingle(1);
            }
            
            if(bytesNeeded >= n_typeof(Normal)::PAGE_SIZE - NORMAL_BLOCK_SIZE) //Large allocation
            {
                //n_trace_printf_track();
                uint16 f = Large.Fit(bytesNeeded);
                if(f != Large.Len)
                {
                    //n_trace_printf_track();
                    //printf("f: %" PRIu16 "\n", f);
                    return (T*)Large.Use(f);
                }
                Large.NewEntry(bytesNeeded);
                
                f = Large.Fit(bytesNeeded);
                n_assert(f == Large.Len - 1);
                if(f != Large.Len)
                {
                    //n_trace_printf_track();
                    //printf("f: %" PRIu16 "\n", f);
                    return (T*)Large.Use(f);
                }
                
                //n_trace_printf_track();
                return NULL;
            }
            
            uint16 fitIndex;
            Page<NORMAL_BLOCK_SIZE, false>* p = Normal.FitInChain(bytesNeeded, fitIndex);
            if(p)
            {
                //n_trace_printf_track();
                return (T*)p->Use(fitIndex, bytesNeeded);
            }
            
            //n_trace_printf_track();
            Normal.AppendPageToChain();
            return (T*)Normal.ChainTail()->Use(1, bytesNeeded);
        }
        
        template<typename T>
        inline void Free(T* ptr)
        {
            if(!ptr)
                return;
            
            {
                Page<NORMAL_BLOCK_SIZE, false>* np = &Normal;
                while(np)
                {
                    uint16 i = np->FindIndex(ptr);
                    if(i != np->Count)
                    {
                        //n_trace_printf_track();
                        np->Free(i);
                        return;
                    }
                    np = np->Next;
                }
            }
            
            {
                Page<SMALL_BLOCK_SIZE, true>* sp = &Small;
                while(sp)
                {
                    uint16 i = sp->FindIndex(ptr);
                    if(i != sp->Count)
                    {
                        //n_trace_printf_track();
                        sp->Free(i);
                        return;
                    }
                    sp = sp->Next;
                }
            }
            
            for(uint16 i = 0; i < Large.Len; ++i)
            {
                if(Large.Allocs[i] == (uint8*)ptr)
                {
                    //n_trace_printf_track();
                    Large.Free(i);
                    return;
                }
            }
            
            n_assert(false);
        }
        
        template<typename T>
        inline T* Realloc(T* ptr, uint64 size)
        {
            if(!ptr)
                return NULL;
            
            {
                Page<NORMAL_BLOCK_SIZE, false>* np = &Normal;
                while(np)
                {
                    uint16 i = np->FindIndex(ptr);
                    if(i != np->Count)
                    {
                        //n_trace_printf_track();
                        if(!np->Realloc(i, sizeof(T) * size))
                        {
                            //n_trace_printf_track();
                            return ptr;
                        }
                        
                        //n_trace_printf_track();
                        T* n = Malloc<T>(size);
                        usize fs = np->GetMaxSize(i);
                        memcpy(n, ptr, fs);
                        np->Free(i);
                        return n;
                    }
                    np = np->Next;
                }
            }
            
            {
                Page<SMALL_BLOCK_SIZE, true>* sp = &Small;
                while(sp)
                {
                    uint16 i = sp->FindIndex(ptr);
                    if(i != sp->Count)
                    {
                        //n_trace_printf_track();
                        if(sizeof(T) * size <= SMALL_BLOCK_SIZE)
                        {
                            //n_trace_printf_track();
                            return ptr;
                        }
                        
                        //n_trace_printf_track();
                        T* n = Malloc<T>(size);
                        memcpy(n, ptr, SMALL_BLOCK_SIZE);
                        sp->Free(i);
                        return n;
                    }
                    sp = sp->Next;
                }
            }
            
            for(uint16 i = 0; i < Large.Len; ++i)
            {
                if(Large.Allocs[i] == (uint8*)ptr)
                {
                    //n_trace_printf_track();
                    return (T*)Large.Realloc(i, sizeof(T) * size);
                }
            }
            
            return NULL;
        }
        
        inline void FreeAll()
        {
            //n_trace_printf_track();
            Small.FreeAllInChain();
            Normal.FreeAllInChain();
            Large.FreeAll();
        }
        
        inline void Destroy() 
        {
            Small.DestroyAllInChain();
            Normal.DestroyAllInChain();
            Large.Destroy();
        }
    };
    
    static_assert(n_is_simple(HeapAllocatorTemplate<>));

    struct HeapAllocator
    {
        void** MemLookup;
        uint32 Len;
        uint32 Cap;
        
        template<typename T>
        static inline HeapAllocator Init(uint64 reserveSize)
        {
            HeapAllocator h = {};
            uint64 count = reserveSize / sizeof(T);
            count = count > 2048 ? 2048 : count;
            h.MemLookup = (void**)Intern_Calloc(count * sizeof(void*));
            h.Len = 0;
            h.Cap = count;
            return h;
        }
        
        template<typename T>
        static inline void ReserveAhead(uint64 reserveSize)
        {
            return;
        }
        
        inline bool Rehash()
        {
            void** newLookup = (void**)Intern_Calloc(Cap * 2 * sizeof(void*));
            if(!newLookup)
                return false;
            
            //printf("Rehashing...\n");
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
                    //printf("Rehash using key %" PRIu32 "\n", key);
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
        
        template<typename T>
        inline T* Malloc(uint64 size)
        {
            if(Len + 1 >= Cap / 2)
            {
                if(!Rehash())
                    return NULL;
            }
            
            void* m = NSTD_ALLOC_MALLOC(sizeof(T) * size);
            uint32 k = Intern_NullKey(m);
            //printf("Using k %" PRIu32 "\n", k);
            MemLookup[k] = m;
            ++Len;
            return (T*)m;
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
        
        template<typename T>
        inline void Free(T* ptr)
        {
            if(!ptr)
                return;
            
            uint32 k = Intern_GetKey(ptr);
            if(k == Cap)
                return;
            
            NSTD_ALLOC_FREE(MemLookup[k]);
            //printf("Freeing k %" PRIu32 "\n", k);
            MemLookup[k] = NULL;
            --Len;
        }
        
        template<typename T>
        inline T* Realloc(T* ptr, uint64 size)
        {
            uint32 k = Intern_GetKey(ptr);
            if(k == Cap)
                return ptr;
            //printf("Freeing k %" PRIu32 "\n", k);
            MemLookup[k] = NULL;
            
            void* p = NSTD_ALLOC_REALLOC((char*)ptr, sizeof(T) * size);
            if(!p)
            {
                k = Intern_NullKey(ptr);
                //printf("Using k %" PRIu32 "\n", k);
                MemLookup[k] = ptr;
                return NULL;
            }
            
            k = Intern_NullKey(p);
            //printf("Using k %" PRIu32 "\n", k);
            MemLookup[k] = p;
            return (T*)p;
        }
        
        inline void FreeAll()
        {
            //printf("FreeAll()\n");
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
            //printf("Allocator Init()\n");
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
            //printf("ReserveAhead with size %" PRIu64 "\n", size * sizeof(T));
            INTERN_NSTD_DISPATCH(ReserveAhead<T>(size), return);
        }
        
        template<typename T>
        inline T* Intern_Malloc(uint64 size)
        {
            INTERN_NSTD_DISPATCH(Malloc<T>(size), return NULL);
        }
        
        template<typename T>
        inline T* Malloc(uint64 size)
        {
            //printf("Malloc with size %" PRIu64 "\n", size * sizeof(T));
            T* p = Intern_Malloc<T>(size);
            //printf("Malloc returning %p\n", p);
            return p;
        }
        
        template<typename T>
        inline void Free(T* ptr)
        {
            //printf("Freeing %p\n", ptr);
            INTERN_NSTD_DISPATCH(Free<T>(ptr), return);
        }
        
        template<typename T>
        inline T* Intern_Realloc(T* ptr, uint64 size)
        {
            INTERN_NSTD_DISPATCH(Realloc<T>(ptr, size), return NULL);
        }
        
        template<typename T>
        inline T* Realloc(T* ptr, uint64 size)
        {
            //printf("Realloc with ptr %p and size %" PRIu64 "\n", ptr, size * sizeof(T));
            T* p = Intern_Realloc(ptr, size);
            //printf("Realloc returning %p\n", p);
            return p;
        }
        
        template<typename T>
        inline T* Calloc(uint64 size)
        {
            T* t = Malloc<T>(size);
            if(!t)
                return NULL;
            memset(t, 0, sizeof(T) * size);
            return t;
        }
        
        inline void FreeAll() 
        {
            //printf("Calling FreeAll()\n");
            INTERN_NSTD_DISPATCH(FreeAll(), return);
        }
        
        inline void Destroy() 
        {
            //printf("Calling Destroy()\n");
            INTERN_NSTD_DISPATCH(Destroy(), return);
            //memset(this, 0, sizeof(*this));
        }
        
        #undef INTERN_NSTD_DISPATCH
    };
    static_assert(n_is_simple(Allocator));
    
}

#endif
