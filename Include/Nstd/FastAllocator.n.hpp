#ifndef NSTD_FAST_ALLOCATOR_N_HPP
#define NSTD_FAST_ALLOCATOR_N_HPP

/*
API:
```c++
template<usize BLOCK_SIZE = 16>
struct FastAllocator
{
    static inline n_result<FastAllocator> Init(n_view<uint8> backing);
    
    template<typename T>
    inline n_view<T> Malloc(uint64 count);
    inline void Free(void* data);
    inline bool OwnsPtr(void* ptr);
    
    template<typename T>
    inline n_view<T> Realloc(void* data, uint64 count);
    inline void FreeAll();
    inline void Destroy();
    inline uint64 GetFreeBytes() const;
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

        inline usize Intern_RoundUpToBlock(usize bytes)
        {
            return (bytes + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE;
        }

        //Map user byte size -> class index. Returns SMALL_CLASS_COUNT if large.
        inline int Intern_GetClass(uint64 byteSize)
        {
            static const uint32 THRESHOLDS[] = { 8, 16, 32, 64, 128, 257 };
            for(int i = 0; i < SMALL_CLASS_COUNT; ++i)
            {
                if(byteSize <= THRESHOLDS[i])
                    return i;
            }
            return SMALL_CLASS_COUNT; //Large
        }

        inline usize Intern_SlotSize(uint32 classId)
        {
            static const uint32 SIZES[] = { 8, 16, 32, 64, 128, 256 };
            return Intern_RoundUpToBlock(SIZES[classId] + DATA_OFFSET);
        }

        inline uint32 Intern_GetHeaderIndex(void* ptr)
        {
            n_assert((uint8*)ptr >= (Memory.data + DATA_OFFSET));
            return ((uint32)((uint8*)ptr - Memory.data - DATA_OFFSET));
        }

        static inline n_result<FastAllocator> Init(n_view<uint8> backing)
        {
            if(!backing)
                return n_error_msg("Invalid backing");
            
            FastAllocator fa = {};
            fa.Memory = backing;
            fa.BumpIndex = 0;
            fa.UsedBytes = 0;

            for(int i = 0; i < SMALL_CLASS_COUNT; ++i)
                fa.FreeListHead[i] = NIL;

            fa.LargeFreeHead = NIL;
            return fa;
        }

        inline n_view<uint8> Intern_MallocSmall(uint64 byteSize)
        {
            int cls = Intern_GetClass(byteSize);

            //Try free list first — O(1) pop
            uint32 head = FreeListHead[cls];
            if(head != NIL)
            {
                FastAllocatorHeader h = Memory.read<FastAllocatorHeader>(head);
                FreeListHead[cls] = h.Next;

                UsedBytes += Intern_SlotSize(cls);
                return Memory.sub(head + DATA_OFFSET, byteSize);
            }

            //Bump fallback
            usize slotSize = Intern_SlotSize(cls);
            if(BumpIndex + slotSize > Memory.len)
                return {};

            FastAllocatorHeader h = Memory.read<FastAllocatorHeader>(BumpIndex);
            h.ClassId = (uint32)cls;
            Memory.write<FastAllocatorHeader>(BumpIndex, h);
            BumpIndex += slotSize;
            UsedBytes += slotSize;
            return Memory.sub(BumpIndex - slotSize + DATA_OFFSET, byteSize);
        }

        inline n_view<uint8> Intern_MallocLarge(uint64 byteSize)
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
                    usize usedBytes = Intern_RoundUpToBlock(byteSize + DATA_OFFSET);
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
                    UsedBytes += Intern_RoundUpToBlock(byteSize + DATA_OFFSET);
                    return Memory.sub(head + DATA_OFFSET, byteSize);
                }

                prev = head;
                head = h.Next;
            }

            //Bump fallback
            usize totalBytes = Intern_RoundUpToBlock(byteSize + DATA_OFFSET);
            if(BumpIndex + totalBytes > Memory.len)
                return {};

            FastAllocatorHeader h = Memory.read<FastAllocatorHeader>(BumpIndex);
            h.ClassId = (uint32)byteSize;
            Memory.write<FastAllocatorHeader>(BumpIndex, h);
            BumpIndex += totalBytes;
            UsedBytes += totalBytes;
            return Memory.sub(BumpIndex - totalBytes + DATA_OFFSET, byteSize);
        }

        template<typename T>
        inline n_view<T> Malloc(uint64 count)
        {
            if(count == 0)
                return {};
            if(count * sizeof(T) <= SMALL_MAX_SIZE)
                return Intern_MallocSmall(sizeof(T) * count).template as<T>();
            return Intern_MallocLarge(sizeof(T) * count).template as<T>();
        }

        inline void Free(void* data)
        {
            if(!data)
                return;

            uint8* p = (uint8*)data;
            if(p < Memory.data || p >= Memory.data + Memory.len)
                return;

            uint32 idx = Intern_GetHeaderIndex(p);
            FastAllocatorHeader h = Memory.read<FastAllocatorHeader>(idx);
            uint32 classId = h.ClassId;

            if(classId <= 5) //Small block, push to class free list
            {
                UsedBytes -= (uint64)Intern_SlotSize(classId);
                int cls = (int)classId;
                h.Next = FreeListHead[cls];
                Memory.write<FastAllocatorHeader>(idx, h);
                FreeListHead[cls] = idx;
            }
            else //Large block, push to large free list
            {
                UsedBytes -= (uint64)Intern_RoundUpToBlock(classId + DATA_OFFSET);
                h.Next = LargeFreeHead;
                Memory.write<FastAllocatorHeader>(idx, h);
                LargeFreeHead = idx;
            }
        }

        inline bool OwnsPtr(void* ptr)
        {
            if(!ptr)
                return false;
            return ptr >= Memory.data && ptr < Memory.data + Memory.len;
        }

        template<typename T>
        inline n_view<T> Realloc(void* data, uint64 count)
        {
            if(!data)
                return Malloc<T>(count);
            if(count == 0)
            {
                Free(data);
                return {};
            }
            
            if(!OwnsPtr(data))
                return {};

            FastAllocatorHeader h = Memory.read<FastAllocatorHeader>(Intern_GetHeaderIndex(data));
            uint32 classId = h.ClassId;

            static const uint32 SmallSizes[] = { 8, 16, 32, 64, 128, 256 };
            uint64 currentSize;
            if(classId <= 5)
                currentSize = (uint64)SmallSizes[classId];
            else
                currentSize = classId;

            usize byteSize = count * sizeof(T);
            if(byteSize <= currentSize) //No grow needed
                return { (T*)data, count };

            n_view<uint8> curData = { (uint8*)data, currentSize };
            n_view<uint8> newData = Malloc<uint8>(byteSize);
            if(!newData)
                return {};
            if(byteSize < currentSize)
                curData.sub(0, byteSize).copy_to(newData);
            else
                curData.copy_to(newData);
            
            Free(curData.data);
            return newData.as<T>();
        }
        
        inline void FreeAll()
        {
            BumpIndex = 0;
            UsedBytes = 0;
            for(int i = 0; i < SMALL_CLASS_COUNT; ++i)
                FreeListHead[i] = NIL;
            LargeFreeHead = NIL;
        }

        inline void Destroy()
        {
            memset(this, 0, sizeof(FastAllocator));
        }
        
        inline uint64 GetFreeBytes() const
        {
            return Memory.len - UsedBytes;
        }

        static void* ContextMalloc(void* c, uint64 byteSize)
        {
            if(!byteSize)
                return NULL;
            FastAllocator* context = (FastAllocator*)c;
            return context->Malloc<uint8>(byteSize).data;
        }

        static void ContextFree(void* c, void* ptr)
        {
            FastAllocator* context = (FastAllocator*)c;
            context->Free(ptr);
        }

        static void* ContextRealloc(void* c, void* p, uint64 byteSize)
        {
            FastAllocator* context = (FastAllocator*)c;
            return context->Realloc<uint8>(p, byteSize);
        }

        static void ContextFreeAll(void* c)
        {
            FastAllocator* context = (FastAllocator*)c;
            context->FreeAll();
        }

        static void ContextDestroy(void* c)
        {
            FastAllocator* context = (FastAllocator*)c;
            context->Destroy();
        }

        static uint64 ContextGetFreeBytes(const void* c)
        {
            const FastAllocator* context = (FastAllocator*)c;
            return context->GetFreeBytes();
        }

        inline Allocator MakeAllocator()
        {
            return Allocator::Init( ContextMalloc, 
                                    ContextFree, 
                                    ContextRealloc, 
                                    ContextFreeAll, 
                                    ContextDestroy, 
                                    ContextGetFreeBytes, 
                                    this);
        }
    };

    static_assert(n_is_simple(FastAllocator<>));
}

#endif
