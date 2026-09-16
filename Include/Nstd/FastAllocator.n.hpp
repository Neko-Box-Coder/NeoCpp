#ifndef NSTD_FAST_ALLOCATOR_N_HPP
#define NSTD_FAST_ALLOCATOR_N_HPP

/*
API:
```c++
template<usize BLOCK_SIZE = 16>
struct FastAllocator
{
    inline n_result<void> Init(n_view<uint8> backing);
    inline Allocator MakeAllocator();
};
```

Usage:
```c++
{
    n_array<uint8, 1024> backing;
    Nstd::FastAllocator<> fa = fa.Init(backing.to_view()).n_try();
    Nstd::Allocator alloc = fa.MakeAllocator();
}
```
*/

#include "ncpp.n.hpp"
#include "./Allocator.n.hpp"

#include <string.h>
#include <stddef.h>

namespace Nstd
{

    struct FastAllocatorHeader
    {
        uint32 Next;
        uint32 ClassId; //0-5 = small class, >=6 = large block byte count
    };

    template<usize BLOCK_SIZE = 16>
    struct FastAllocator
    {
        static constexpr uint32 SMALL_CLASS_COUNT = 6;
        static constexpr uint32 SMALL_MAX_SIZE = 256;

        static constexpr usize DATA_OFFSET =    (sizeof(FastAllocatorHeader) + BLOCK_SIZE - 1) / 
                                                BLOCK_SIZE * BLOCK_SIZE;

        static constexpr uint32 NIL = UINT32_MAX;

        static_assert(BLOCK_SIZE >= sizeof(FastAllocatorHeader), "BLOCK_SIZE too small for header");

        n_view<uint8> Memory;
        usize BumpIndex;
        uint64 UsedBytes;
        
        //Per-class free lists (head offset into pool, NIL = empty)
        uint32 FreeListHead[SMALL_CLASS_COUNT];

        //Large block free list head (offset into pool, NIL = empty)
        uint32 LargeFreeHead;

        inline usize RoundUpToBlock(usize bytes)
        {
            return (bytes + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE;
        }

        //Map user byte size -> class index. Returns SMALL_CLASS_COUNT if large.
        inline int GetClass(uint64 byteSize)
        {
            static const uint32 THRESHOLDS[] = { 8, 16, 32, 64, 128, 257 };
            for(int i = 0; i < SMALL_CLASS_COUNT; ++i)
            {
                if(byteSize <= THRESHOLDS[i])
                    return i;
            }
            return SMALL_CLASS_COUNT; //Large
        }

        inline usize SlotSize(uint32 classId)
        {
            static const uint32 SIZES[] = { 8, 16, 32, 64, 128, 256 };
            return RoundUpToBlock(SIZES[classId] + DATA_OFFSET);
        }

        inline uint32 GetHeaderIndex(void* ptr)
        {
            n_assert((uint8*)ptr >= (Memory.data + DATA_OFFSET));
            return ((uint32)((uint8*)ptr - Memory.data - DATA_OFFSET));
        }

        inline n_result<void> Init(n_view<uint8> backing)
        {
            if(!backing)
                return n_error_msg("Invalid backing");
            Memory = backing;
            BumpIndex = 0;
            UsedBytes = 0;

            for(int i = 0; i < SMALL_CLASS_COUNT; ++i)
                FreeListHead[i] = NIL;

            LargeFreeHead = NIL;
            return {};
        }

        inline void* MallocSmall(uint64 byteSize)
        {
            int cls = GetClass(byteSize);

            //Try free list first — O(1) pop
            uint32 head = FreeListHead[cls];
            if(head != NIL)
            {
                FastAllocatorHeader h = Memory.read<FastAllocatorHeader>(head);
                FreeListHead[cls] = h.Next;

                UsedBytes += SlotSize(cls);
                return &Memory[head + DATA_OFFSET];
            }

            //Bump fallback
            usize slotSize = SlotSize(cls);
            if(BumpIndex + slotSize > Memory.len)
                return NULL;

            FastAllocatorHeader h = Memory.read<FastAllocatorHeader>(BumpIndex);
            h.ClassId = (uint32)cls;
            Memory.write<FastAllocatorHeader>(BumpIndex, h);
            BumpIndex += slotSize;
            UsedBytes += slotSize;
            return &Memory[BumpIndex - slotSize + DATA_OFFSET];
        }

        inline void* MallocLarge(uint64 byteSize)
        {
            //Try large free list first
            uint32 head = LargeFreeHead;
            uint32 prev = NIL;
            while(head != NIL)
            {
                FastAllocatorHeader h = Memory.read<FastAllocatorHeader>(head);
                uint32 slotBytes = h.ClassId; //Large blocks store byte count in ClassId

                if(slotBytes >= (uint32)byteSize)
                {
                    //Found a fit, remove from list
                    if(prev == NIL)
                        LargeFreeHead = h.Next;
                    else
                    {
                        FastAllocatorHeader ph = Memory.read<FastAllocatorHeader>(prev);
                        ph.Next = h.Next;
                        Memory.write<FastAllocatorHeader>(prev, ph);
                    }

                    //If slot is big enough to split, carve off remainder
                    usize usedBytes = RoundUpToBlock(byteSize + DATA_OFFSET);
                    if(slotBytes > (uint32)(usedBytes + DATA_OFFSET))
                    {
                        uint32 remStart = head + (uint32)usedBytes;
                        uint32 remBytes = slotBytes - (uint32)usedBytes;
                        FastAllocatorHeader rh = Memory.read<FastAllocatorHeader>(remStart);
                        rh.ClassId = remBytes; //Large block with remainder size
                        
                        //Insert remainder at head of large free list
                        rh.Next = LargeFreeHead;
                        Memory.write<FastAllocatorHeader>(remStart, rh);
                        LargeFreeHead = remStart;
                    }

                    h.ClassId = (uint32)byteSize;
                    Memory.write<FastAllocatorHeader>(head, h);
                    UsedBytes += RoundUpToBlock(byteSize + DATA_OFFSET);
                    return &Memory[head + DATA_OFFSET];
                }

                prev = head;
                head = h.Next;
            }

            //Bump fallback
            usize totalBytes = RoundUpToBlock(byteSize + DATA_OFFSET);
            if(BumpIndex + totalBytes > Memory.len)
                return NULL;

            FastAllocatorHeader h = Memory.read<FastAllocatorHeader>(BumpIndex);
            h.ClassId = (uint32)byteSize;
            Memory.write<FastAllocatorHeader>(BumpIndex, h);
            BumpIndex += totalBytes;
            UsedBytes += totalBytes;
            return &Memory[BumpIndex - totalBytes + DATA_OFFSET];
        }

        inline void* InternMalloc(uint64 byteSize)
        {
            if(byteSize == 0)
                return NULL;
            if(byteSize <= SMALL_MAX_SIZE)
                return MallocSmall(byteSize);
            return MallocLarge(byteSize);
        }

        inline void InternFree(void* ptr)
        {
            if(!ptr)
                return;

            uint8* p = (uint8*)ptr;
            if(p < Memory.data || p >= Memory.data + Memory.len)
                return;

            uint32 idx = GetHeaderIndex(ptr);
            FastAllocatorHeader h = Memory.read<FastAllocatorHeader>(idx);
            uint32 classId = h.ClassId;

            if(classId <= 5) //Small block, push to class free list
            {
                UsedBytes -= (uint64)SlotSize(classId);
                int cls = (int)classId;
                h.Next = FreeListHead[cls];
                Memory.write<FastAllocatorHeader>(idx, h);
                FreeListHead[cls] = idx;
            }
            else //Large block, push to large free list
            {
                UsedBytes -= (uint64)RoundUpToBlock(classId + DATA_OFFSET);
                h.Next = LargeFreeHead;
                Memory.write<FastAllocatorHeader>(idx, h);
                LargeFreeHead = idx;
            }
        }

        inline void* InternRealloc(void* oldPtr, uint64 byteSize)
        {
            if(!oldPtr)
                return InternMalloc(byteSize);
            if(byteSize == 0)
            {
                InternFree(oldPtr);
                return NULL;
            }
            
            if(oldPtr < Memory.data || oldPtr >= Memory.data + Memory.len)
                return oldPtr;

            FastAllocatorHeader h = Memory.read<FastAllocatorHeader>(GetHeaderIndex(oldPtr));
            uint32 classId = h.ClassId;

            static const uint32 SmallSizes[] = { 8, 16, 32, 64, 128, 256 };
            uint64 currentSize;
            if(classId <= 5)
                currentSize = (uint64)SmallSizes[classId];
            else
                currentSize = classId;

            if(byteSize <= currentSize) return oldPtr; //No grow needed

            void* newPtr = InternMalloc(byteSize);
            if(!newPtr) return NULL;
            memcpy(newPtr, oldPtr, (byteSize < currentSize) ? byteSize : currentSize);
            InternFree(oldPtr);
            return newPtr;
        }

        bool OwnsPtr(void* ptr)
        {
            if(!ptr)
                return false;
            return ptr >= Memory.data && ptr < Memory.data + Memory.len;
        }

        static void* Malloc(void* c, uint64 byteSize)
        {
            if(!byteSize)
                return NULL;
            FastAllocator* context = (FastAllocator*)c;
            return context->InternMalloc(byteSize);
        }

        static void Free(void* c, void* ptr)
        {
            FastAllocator* context = (FastAllocator*)c;
            context->InternFree(ptr);
        }

        static void* Realloc(void* c, void* p, uint64 byteSize)
        {
            if(!p)
                return Malloc(c, byteSize);
            if(byteSize == 0)
            {
                Free(c, p);
                return NULL;
            }

            FastAllocator* context = (FastAllocator*)c;
            return context->InternRealloc(p, byteSize);
        }

        static void FreeAll(void* c)
        {
            FastAllocator* context = (FastAllocator*)c;
            context->BumpIndex = 0;
            context->UsedBytes = 0;
            for(int i = 0; i < SMALL_CLASS_COUNT; ++i)
                context->FreeListHead[i] = NIL;
            context->LargeFreeHead = NIL;
        }

        static void Destroy(void* c)
        {
            FastAllocator* context = (FastAllocator*)c;
            memset(context, 0, sizeof(FastAllocator));
        }

        static uint64 GetFreeBytes(const void* c)
        {
            const FastAllocator* context = (FastAllocator*)c;
            return context->Memory.len - context->UsedBytes;
        }

        inline Allocator MakeAllocator()
        {
            return Allocator::Init(Malloc, Free, Realloc, FreeAll, Destroy, GetFreeBytes, this);
        }
    };

    static_assert(n_is_simple(FastAllocator<>));
}

#endif
