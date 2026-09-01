#ifndef NSTD_FAST_ALLOCATOR_N_HPP
#define NSTD_FAST_ALLOCATOR_N_HPP

#include "ncpp.n.hpp"
#include "./AllocatorPool.n.hpp"

#include <string.h>
#include <stddef.h>

namespace Nstd
{

    struct FastAllocatorHeader
    {
        uint32 Next;
        uint32 ClassId; //0-5 = small class, >=6 = large block byte count
    };

    template<usize BLOCK_SIZE = 8>
    struct FastAllocator
    {
        static constexpr uint32 SMALL_CLASS_COUNT = 6;
        static constexpr uint32 SMALL_MAX_SIZE = 256;
        
        static_assert(BLOCK_SIZE >= sizeof(FastAllocatorHeader), "BLOCK_SIZE too small for header");

        uint8* Memory;
        usize TotalBytes;
        usize BumpIndex;
        uint64 UsedBytes;
        
        //Per-class free lists (head offset into pool, 0 = empty)
        uint32 FreeListHead[SMALL_CLASS_COUNT];

        //Large block free list head (offset into pool, 0 = empty)
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
            return RoundUpToBlock(SIZES[classId] + sizeof(FastAllocatorHeader));
        }

        //Get header pointer from user data pointer
        inline FastAllocatorHeader* GetHeader(void* ptr)
        {
            return (FastAllocatorHeader*)((uint8*)ptr - sizeof(FastAllocatorHeader));
        }

        inline n_result<void> Init(usize reserveSize)
        {
            if(reserveSize == 0)
                reserveSize = 1 * 1024 * 1024; //Default: 1MB

            Memory = (uint8*)NSTD_ALLOC_MALLOC(reserveSize);
            if(!Memory)
                return n_error_msg("Failed to malloc reserve (%zu bytes)", reserveSize);

            TotalBytes = reserveSize;
            BumpIndex = 0;
            UsedBytes = 0;

            for(int i = 0; i < SMALL_CLASS_COUNT; ++i)
                FreeListHead[i] = 0;

            LargeFreeHead = 0;
            return {};
        }

        inline void* MallocSmall(uint64 byteSize)
        {
            int cls = GetClass(byteSize);

            //Try free list first — O(1) pop
            uint32 head = FreeListHead[cls];
            if(head != 0)
            {
                FastAllocatorHeader* h = (FastAllocatorHeader*)(Memory + head);
                FreeListHead[cls] = h->Next;

                UsedBytes += byteSize;
                return (uint8*)h + sizeof(FastAllocatorHeader);
            }

            //Bump fallback
            usize slotSize = SlotSize(cls);
            if(BumpIndex + slotSize > TotalBytes)
                return NULL;

            FastAllocatorHeader* h = (FastAllocatorHeader*)(Memory + BumpIndex);
            h->ClassId = (uint32)cls;
            BumpIndex += slotSize;

            UsedBytes += byteSize;
            return (uint8*)h + sizeof(FastAllocatorHeader);
        }

        inline void* MallocLarge(uint64 byteSize)
        {
            //Try large free list first
            uint32 head = LargeFreeHead;
            uint32 prev = 0;
            while(head != 0)
            {
                FastAllocatorHeader* h = (FastAllocatorHeader*)(Memory + head);
                uint32 slotBytes = h->ClassId; //Large blocks store byte count in ClassId

                if(slotBytes >= (uint32)byteSize)
                {
                    //Found a fit, remove from list
                    if(prev == 0)
                        LargeFreeHead = h->Next;
                    else
                    {
                        FastAllocatorHeader* ph = (FastAllocatorHeader*)(Memory + prev);
                        ph->Next = h->Next;
                    }

                    //If slot is big enough to split, carve off remainder
                    usize usedBytes = RoundUpToBlock(byteSize + sizeof(FastAllocatorHeader));
                    if(slotBytes > (uint32)(usedBytes + sizeof(FastAllocatorHeader)))
                    {
                        uint32 remStart = head + (uint32)usedBytes;
                        uint32 remBytes = slotBytes - (uint32)usedBytes;
                        FastAllocatorHeader* rh = (FastAllocatorHeader*)(Memory + remStart);
                        rh->ClassId = remBytes; //Large block with remainder size
                        
                        //Insert remainder at head of large free list
                        rh->Next = LargeFreeHead;
                        LargeFreeHead = remStart;
                    }

                    h->ClassId = (uint32)byteSize;
                    UsedBytes += byteSize;
                    return (uint8*)h + sizeof(FastAllocatorHeader);
                }

                prev = head;
                head = h->Next;
            }

            //Bump fallback
            usize totalBytes = RoundUpToBlock(byteSize + sizeof(FastAllocatorHeader));
            if(BumpIndex + totalBytes > TotalBytes)
                return NULL;

            FastAllocatorHeader* h = (FastAllocatorHeader*)(Memory + BumpIndex);
            h->ClassId = (uint32)byteSize;
            BumpIndex += totalBytes;

            UsedBytes += byteSize;
            return (uint8*)h + sizeof(FastAllocatorHeader);
        }

        inline void* MallocInternal(uint64 byteSize)
        {
            if(byteSize == 0) return NULL;
            if(byteSize <= SMALL_MAX_SIZE)
                return MallocSmall(byteSize);
            return MallocLarge(byteSize);
        }

        inline void Free(void* ptr)
        {
            if(!ptr) return;

            uint8* p = (uint8*)ptr;
            if(p < Memory || p >= Memory + TotalBytes) return;

            FastAllocatorHeader* h = GetHeader(ptr);
            uint32 classId = h->ClassId;

            uint32 idx = (uint32)((uint8*)h - Memory);

            static const uint32 SmallSizes[] = { 8, 16, 32, 64, 128, 256 };
            if(classId <= 5) //Small block, push to class free list
            {
                UsedBytes -= (uint64)SmallSizes[classId];
                int cls = (int)classId;
                h->Next = FreeListHead[cls];
                FreeListHead[cls] = idx;
            }
            else //Large block, push to large free list
            {
                UsedBytes -= (uint64)classId;
                h->Next = LargeFreeHead;
                LargeFreeHead = idx;
            }
        }

        inline void* Realloc(void* oldPtr, uint64 byteSize)
        {
            if(!oldPtr)
                return MallocInternal(byteSize);
            if(byteSize == 0)
            {
                Free(oldPtr);
                return NULL;
            }

            FastAllocatorHeader* h = GetHeader(oldPtr);
            uint32 classId = h->ClassId;

            static const uint32 SmallSizes[] = { 8, 16, 32, 64, 128, 256 };
            uint64 currentSize;
            if(classId <= 5)
                currentSize = (uint64)SmallSizes[classId];
            else
                currentSize = classId;

            if(byteSize <= currentSize) return oldPtr; //No grow needed

            void* newPtr = MallocInternal(byteSize);
            if(!newPtr) return NULL;
            memcpy(newPtr, oldPtr, (byteSize < currentSize) ? byteSize : currentSize);
            Free(oldPtr);
            return newPtr;
        }

        static void ReserveAhead(void*, uint64) {}

        static void* MallocCallback(void* c, uint64 byteSize)
        {
            if(!byteSize)
                return NULL;
            FastAllocator* context = (FastAllocator*)c;
            return context->MallocInternal(byteSize);
        }

        static void FreeCallback(void* c, void* ptr)
        {
            FastAllocator* context = (FastAllocator*)c;
            context->Free(ptr);
        }

        static void* ReallocCallback(void* c, void* p, uint64 byteSize)
        {
            if(!p)
                return MallocCallback(c, byteSize);
            if(byteSize == 0)
            {
                FreeCallback(c, p);
                return NULL;
            }

            FastAllocator* context = (FastAllocator*)c;
            return context->Realloc(p, byteSize);
        }

        static void FreeAll(void* c)
        {
            FastAllocator* context = (FastAllocator*)c;
            context->BumpIndex = 0;
            context->UsedBytes = 0;
            for(int i = 0; i < SMALL_CLASS_COUNT; ++i)
                context->FreeListHead[i] = 0;
            context->LargeFreeHead = 0;
        }

        static void Destroy(void* c)
        {
            FastAllocator* context = (FastAllocator*)c;
            if(context->Memory)
            {
                NSTD_ALLOC_FREE(context->Memory);
                context->Memory = NULL;
            }
        }

        static uint64 GetFreeBytes(const void* c)
        {
            const FastAllocator* context = (FastAllocator*)c;
            return context->TotalBytes - context->UsedBytes;
        }

        inline AllocatorPool MakeAllocatorPool()
        {
            AllocatorPool retAlloc = {};
            retAlloc.Init(  ReserveAhead, 
                            MallocCallback, 
                            FreeCallback, 
                            ReallocCallback, 
                            FreeAll, 
                            Destroy, 
                            GetFreeBytes, 
                            this, 
                            true);
            return retAlloc;
        }
    };

    static_assert(n_is_simple(FastAllocator<>));
}

#endif
