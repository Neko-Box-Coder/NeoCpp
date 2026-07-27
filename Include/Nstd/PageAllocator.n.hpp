#ifndef NSTD_PAGE_ALLOCATOR_N_HPP
#define NSTD_PAGE_ALLOCATOR_N_HPP

#include "ncpp.n.hpp"
#include "./BitView.n.hpp"
#include "./AllocatorPool.n.hpp"

#include <string.h>
#include <stddef.h>

namespace Nstd
{
    template<usize BLOCK_SIZE = 16, bool SINGLE = false>
    struct PageAllocator
    {
        static constexpr usize PAGE_SIZE = BLOCK_SIZE * 8 * BLOCK_SIZE;
        
        uint8* Blocks;
        BitView Control;
        BitView Key;
        uint16 BlocksCount;
        uint8 PageCount;
        bool Backing;
        
        inline n_result<void> Intern_Init(TaggedUnion<n_view<char>, usize> arg)
        {
            usize reserveSize;
            n_use_error_defer();
            
            if(arg.Is<n_view<char>>())
            {
                n_check_true((bool)arg.Get<n_view<char>>());
                reserveSize = arg.Get<n_view<char>>().len;
            }
            else
                reserveSize = arg.Get<usize>();
            
            usize pageCount = reserveSize / PAGE_SIZE;
            if(pageCount > 64 || BLOCK_SIZE * 8 * pageCount >= UINT16_MAX)
            {
                return n_error_msg( "Block size too small for this much memory. "
                                    "BLOCK SIZE: %zu, Memory: %" PRIu64, BLOCK_SIZE, reserveSize);
            }
            
            PageCount = pageCount;
            if(arg.Is<n_view<char>>())
                Blocks = (uint8*)arg.Get<n_view<char>>().data;
            else
            {
                Blocks = (uint8*)NSTD_ALLOC_MALLOC(PAGE_SIZE * PageCount);
                if(!Blocks)
                    return n_error_msg("Failed to malloc");
            }
            
            n_error_defer { if(arg.Is<usize>()) NSTD_ALLOC_FREE(Blocks); };
            
            memset(Blocks, 0, BLOCK_SIZE * PageCount * 2);
            Control = Control.Init({ Blocks, BLOCK_SIZE * PageCount });
            if(!SINGLE)
            {
                Key = Key.Init({ Blocks + BLOCK_SIZE * PageCount, BLOCK_SIZE * PageCount });
                Control.SetBitsAt<1>(0, PageCount * 2).n_try();
                Key.SetBit<1>(0);
                BlocksCount = PageCount * 2;
            }
            else
            {
                Control.SetBitsAt<1>(0, PageCount).n_try();
                BlocksCount = PageCount;
            }
            
            Backing = arg.Is<n_view<char>>();
            return {};
        }
        
        inline n_result<void> InitWithBacking(n_view<char> backing)
        {
            Intern_Init(TaggedUnion<n_view<char>, usize>::Init<n_view<char>>(backing)).n_try();
            return {};
        }
        
        inline n_result<void> Init(usize reserveSize)
        {
            Intern_Init(TaggedUnion<n_view<char>, usize>::Init<usize>(reserveSize)).n_try();
            return {};
        }
        
        inline uint16 FitBlocks(uint64 fitByteSize)
        {
            if(SINGLE)
            {
                if(fitByteSize > BLOCK_SIZE)
                    return Control.Len();
                
                for(int i = PageCount; i < Control.Len(); ++i)
                {
                    if(!Control.GetBit(i))
                        return i;
                }
                
                return Control.Len();
            }
            
            const uint16 fitBlockCount = (fitByteSize + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
            if(Control.Len() - PageCount * 2 < fitBlockCount)
                return Control.Len();
            
            usize curIndex = PageCount * 2;
            bool curBit = Control.GetBit(curIndex);
            do
            {
                n_result<usize> f = Control.GetBitsUntilFlipped(curIndex);
                n_assert(!f.err);
                n_assert(f.value <= Control.Len());
                if(!curBit)
                {
                    if(f.value - curIndex >= fitBlockCount)
                        return curIndex;
                }
                curIndex = f.value;
                n_assert(curIndex == Control.Len() || Control.GetBit(curIndex) == !curBit);
                curBit = !curBit;
            }
            while(curIndex != Control.Len());
            
            return Control.Len();
        }
        
        template<bool OVERLAP = false>
        inline void* UseBlocks(uint16 index, uint64 bytes)
        {
            if(SINGLE)
            {
                n_assert(index >= PageCount && index < Control.Len());
                if(!OVERLAP)
                    n_assert(!Control.GetBit(index));
                Control.SetBit<true>(index);
                ++BlocksCount;
                return &Blocks[index * BLOCK_SIZE];
            }
            else
                n_assert(index >= PageCount * 2 && index < Control.Len());
            
            if(!OVERLAP)
            {
                n_assert(!Control.GetBit(index));
                n_assert(!Key.GetBit(index));
            }
            
            const usize blocksNeeded = (bytes + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
            n_assert(blocksNeeded <= Control.Len() - BlocksCount);
            
            const uint32 endIndex = index + blocksNeeded; (void)endIndex;
            n_assert(endIndex <= Control.Len());
            n_result<void> r = Control.SetBitsAt<true>(index, blocksNeeded); (void)r;
            Key.SetBit<true>(index);
            n_assert(!r.err);
            BlocksCount += blocksNeeded;
            return &Blocks[index * BLOCK_SIZE];
        }
        
        inline uint16 FindIndex(void* ptr)
        {
            if(ptr < Blocks || ptr >= Blocks + PAGE_SIZE * PageCount)
                return Control.Len();
            
            n_assert(((ptrdiff_t)ptr - (ptrdiff_t)Blocks) % BLOCK_SIZE == 0);
            return ((ptrdiff_t)ptr - (ptrdiff_t)Blocks) / BLOCK_SIZE;
        }
        
        #if 0
        inline uint16 GetUsedBlocksCount(uint16 index)
        {
            if(index >= Control.Len())
                return 0;
            if(SINGLE)
                return 1;
            
            n_assert(Control.GetBit(index));
            n_result<usize> f = Control.GetBitsUntilFlipped(index);
            n_assert(!f.err);
            return f.value - index;
        }
        #endif
        
        inline void FreeBlocks(uint16 index)
        {
            if(index >= Control.Len())
                return;
            
            n_assert(Control.GetBit(index));
            if(SINGLE)
            {
                Control.SetBit<false>(index);
                --BlocksCount;
                return;
            }
            n_assert(Key.GetBit(index));
            
            if(index != Control.Len() - 1)
            {
                usize endIndex = index;
                
                //First round
                usize startByteIndex = (index + 1) / 8;
                usize startBitIndex = (index + 1) % 8;
                uint8 mixedByte =   Control.ByteViews.data[startByteIndex] ^ 
                                    Key.ByteViews.data[startByteIndex];
                for(int i = startBitIndex; i < 8; ++i)
                {
                    if(!((mixedByte >> i) & 0x01)) //Either Key + Control or No control bit
                    {
                        endIndex += (i - startBitIndex) + 1;
                        break;
                    }
                }
                
                n_assert(startByteIndex != Control.ByteViews.len - 1 || endIndex != index);
                if(endIndex == index) //Not in first round
                {
                    for(int64 i = startByteIndex + 1; i < Control.ByteViews.len; ++i)
                    {
                        uint8 mixedByte = Control.ByteViews.data[i] ^ Key.ByteViews.data[i];
                        for(int j = 0; j < 8; ++j)
                        {
                            if(!((mixedByte >> j) & 0x01))
                            {
                                endIndex = i * 8 + j;
                                break;
                            }
                        }
                        if(endIndex != index)
                            break;
                    }
                    
                    if(endIndex == index)
                        endIndex = Control.Len();
                }
                
                n_assert(   endIndex == Control.Len() || 
                            !Control.GetBit(endIndex) || 
                            Key.GetBit(endIndex));
                n_assert(BlocksCount >= endIndex - index + PageCount);
                Control.SetBits<false>(index, endIndex - index);
                BlocksCount -= endIndex - index;
            }
            else
            {
                n_assert(BlocksCount > PageCount);
                Control.SetBit<false>(index);
                --BlocksCount;
            }
            
            Key.SetBit<false>(index);
        }
        
        inline uint16 ReallocBlocks(uint16 index, uint64 bytes)
        {
            n_assert(index < Control.Len());
            n_assert(Control.GetBit(index));
            if(SINGLE)
            {
                if(bytes > BLOCK_SIZE)
                    return Control.Len();
                return index;
            }
                        
            n_assert(Key.GetBit(index));
            if(bytes > (PAGE_SIZE - BLOCK_SIZE) * PageCount)
                return Control.Len();
            
            uint16 blocksOccupied = 0;
            if(index < Control.Len() - 1)
            {
                if(Key.GetBit(index + 1))
                    blocksOccupied = 1;
                else
                {
                    n_result<usize> f = Key.GetBitsUntilFlipped(index + 1);
                    n_assert(!f.err);
                    n_assert(f.value > index);
                    if(f.value != Control.Len())
                        n_assert(Key.GetBit(f.value));
                    blocksOccupied = f.value - index;
                }
            }
            else
                blocksOccupied = 1;
            
            const uint32 totalBlocksNeeded = ((bytes + BLOCK_SIZE - 1) / BLOCK_SIZE);
            if(totalBlocksNeeded == blocksOccupied)
                return index;
            
            if(totalBlocksNeeded < blocksOccupied) //Shrink
            {
                Control.SetBits<false>(index + totalBlocksNeeded, blocksOccupied - totalBlocksNeeded);
                BlocksCount += blocksOccupied - totalBlocksNeeded;
                return index;
            }
            else //Grow
            {
                //Existing blocks are not at the end and has empty space after
                if( index + blocksOccupied != Control.Len() &&
                    !Key.GetBit(index + blocksOccupied))
                {
                    n_assert(Control.GetBit(index + blocksOccupied) == 0);
                    n_result<usize> f = Control.GetBitsUntilFlipped(index + blocksOccupied);
                    n_assert(!f.err);
                    if(f.value != Control.Len())
                        n_assert(Key.GetBit(f.value));
                    uint16 totalBlocksFree = f.value - index;
                    if(totalBlocksFree >= totalBlocksNeeded) //If we have enough free blocks
                    {
                        UseBlocks<true>(index, bytes);
                        BlocksCount -= blocksOccupied;
                        return index;
                    }
                }
                
                //Try refitting...
                uint16 fi = FitBlocks(bytes);
                if(fi == Control.Len())
                    return fi;
                
                void* p = UseBlocks<false>(fi, bytes);
                memcpy(p, &Blocks[index], blocksOccupied * BLOCK_SIZE);
                
                FreeBlocks(index);
                return fi;
            }
        }
        
        static void FreeAll(void* c)
        {
            PageAllocator* context = (PageAllocator*)c;
            if(SINGLE)
            {
                memset(context->Blocks, 0, BLOCK_SIZE * context->PageCount);
                context->Control.SetBits<1>(0, context->PageCount);
                context->BlocksCount = context->PageCount;
            }
            else
            {
                memset(context->Blocks, 0, BLOCK_SIZE * context->PageCount * 2);
                context->Control.SetBits<1>(0, context->PageCount * 2);
                context->Key.SetBit<1>(0);
                context->BlocksCount = context->PageCount * 2;
            }
        }
        
        static void Destroy(void* c)
        {
            PageAllocator* context = (PageAllocator*)c;
            if(!context->Backing)
                NSTD_ALLOC_FREE(context->Blocks);
            memset(context, 0, sizeof(PageAllocator<BLOCK_SIZE, SINGLE>));
        }
        
        static void* Malloc(void* c, uint64 byteSize)
        {
            PageAllocator* context = (PageAllocator*)c;
            if(!byteSize)
                return NULL;
            
            uint16 index = context->FitBlocks(byteSize);
            if(index == context->Control.Len())
                return NULL;
            
            return context->UseBlocks<false>(index, byteSize);
        }
        
        static void Free(void* c, void* p)
        {
            PageAllocator* context = (PageAllocator*)c;
            if(!p)
                return;
            context->FreeBlocks(context->FindIndex(p));
        }
        
        static void* Realloc(void* c, void* p, uint64 byteSize)
        {
            PageAllocator* context = (PageAllocator*)c;
            if(!p)
                return NULL;
            
            uint16 i = context->FindIndex(p);
            if(i == context->Control.Len())
                return NULL;
            
            uint16 ri = context->ReallocBlocks(i, byteSize);
            if(ri == context->Control.Len())
                return NULL;
            return &context->Blocks[ri * BLOCK_SIZE];
        }
        
        static void ReserveAhead(void*, uint64) {}
        
        inline AllocatorPool MakeAllocatorPool()
        {
            AllocatorPool retAlloc = {};
            retAlloc.Init(ReserveAhead, Malloc, Free, Realloc, FreeAll, Destroy, this, true);
            return retAlloc;
        }
    };
    
    static_assert(n_is_simple(PageAllocator<>));
}

#endif
