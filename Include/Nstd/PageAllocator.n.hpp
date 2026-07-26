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
        
        inline n_result<void> InitWithBacking(n_view<char> backing)
        {
            n_check_true(backing);
            
            usize pageCount = backing.len / PAGE_SIZE;
            if(pageCount > 64)
            {
                return n_error_msg( "Block size too small for this much memory. "
                                    "BLOCK SIZE: %zu, Memory: %" PRIu64, BLOCK_SIZE, backing.len);
            }
            
            PageCount = pageCount;
            if(BLOCK_SIZE * 8 * PageCount >= UINT16_MAX)
            {
                return n_error_msg( "Block size too small for this much memory. "
                                    "BLOCK SIZE: %zu, Memory: %" PRIu64, BLOCK_SIZE, backing.len);
            }
            
            Blocks = backing.data;
            memset(Blocks, 0, BLOCK_SIZE * PageCount);
            Control = Control.Init({ Blocks, BLOCK_SIZE * PageCount });
            if(!SINGLE)
                Key = Key.Init({ Blocks + BLOCK_SIZE * PageCount, BLOCK_SIZE * PageCount });
            
            Control.SetBitsAt<1>(0, PageCount * 2).n_try();
            if(!SINGLE)
                Key.SetBit<1>(0);
            
            BlocksCount = PageCount * 2;
            Backing = true;
            return {};
        }
        
        inline n_result<void> Init(usize reserveSize)
        {
            n_use_error_defer();
            
            usize pageCount = (reserveSize + (PAGE_SIZE - 1)) / PAGE_SIZE;
            if(pageCount > 64)
            {
                return n_error_msg( "Block size too small for this much memory. "
                                    "BLOCK SIZE: %zu, Memory: %zu", BLOCK_SIZE, reserveSize);
            }
            
            PageCount = pageCount;
            if(BLOCK_SIZE * 8 * PageCount >= UINT16_MAX)
            {
                return n_error_msg( "Block size too small for this much memory. "
                                    "BLOCK SIZE: %zu, Memory: %zu", BLOCK_SIZE, reserveSize);
            }
            
            Blocks = (uint8*)NSTD_ALLOC_MALLOC(PAGE_SIZE * PageCount);
            if(!Blocks)
                return n_error_msg("Failed to malloc");
            
            n_error_defer { NSTD_ALLOC_FREE(Blocks); };
            
            memset(Blocks, 0, BLOCK_SIZE * PageCount);
            Control = Control.Init({ Blocks, BLOCK_SIZE * PageCount });
            if(!SINGLE)
                Key = Key.Init({ Blocks + BLOCK_SIZE * PageCount, BLOCK_SIZE * PageCount });
            
            Control.SetBitsAt<1>(0, PageCount * 2).n_try();
            if(!SINGLE)
                Key.SetBit<1>(0);
            
            BlocksCount = PageCount * 2;
            Backing = false;
            return {};
        }
        
        inline uint16 FitBlocks(uint64 fitByteSize)
        {
            if(SINGLE)
            {
                if(fitByteSize > BLOCK_SIZE)
                    return Control.Len();
                
                for(int i = PageCount * 2; i < Control.Len(); ++i)
                {
                    if(!Control.GetBit(i))
                        return i;
                }
                
                return Control.Len();
            }
            
            const uint16 fitBlockCount = (fitByteSize + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
            if(Control.Len() - PageCount * 2 < fitBlockCount)
                return Control.Len();
            
            uint16 curCount = 0;
            uint16 prevUsedIdx = PageCount * 2 - 1;
            for(uint32 i = PageCount * 2; i < Control.Len(); ++i)
            {
                if(Control.GetBit(i)) //Occupied
                {
                    curCount = 0;
                    prevUsedIdx = i;
                }
                else //Free
                {
                    ++curCount;
                    if(curCount == fitBlockCount)
                        return prevUsedIdx + 1;
                }
            }
            
            return Control.Len();
        }
        
        template<bool OVERLAP = false>
        inline void* UseBlocks(uint16 index, uint64 bytes)
        {
            n_assert(index >= PageCount * 2 && index < Control.Len());
            if(!OVERLAP)
                n_assert(!Control.GetBit(index));
            
            if(SINGLE)
            {
                Control.SetBit<true>(index);
                ++BlocksCount;
                return &Blocks[index * BLOCK_SIZE];
            }
            
            const usize blocksNeeded = (bytes + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
            n_assert(blocksNeeded <= Control.Len() - BlocksCount);
            n_assert(!Key.GetBit(index));
            
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
            }
            else
            {
                n_result<usize> f = Control.GetBitsUntilFlipped(index);
                n_assert(!f.err);
                n_assert(f.value > index);
                Control.SetBits<false>(index, f.value - index);
                Key.SetBit<false>(index);
                n_assert(BlocksCount >= f.value - index + PageCount);
                BlocksCount -= f.value - index;
            }
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
            
            if(bytes > (PAGE_SIZE - BLOCK_SIZE) * PageCount)
                return Control.Len();
            
            n_result<usize> f = Control.GetBitsUntilFlipped(index);
            n_assert(!f.err);
            n_assert(f.value > index);
            
            uint16 blocksOccupied = f.value - index;
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
                if(f.value != Control.Len()) //Existing blocks are not at the end
                {
                    n_assert(Control.GetBit(index + blocksOccupied) == 0);
                    f = Control.GetBitsUntilFlipped(index + blocksOccupied);
                    n_assert(!f.err);
                    uint16 totalBlocksFree = f.value - index;
                    if(totalBlocksFree >= totalBlocksNeeded) //If we have enough free blocks
                    {
                        BlocksCount -= blocksOccupied;
                        UseBlocks<true>(index, bytes);
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
            memset(context->Blocks, 0, BLOCK_SIZE * context->PageCount);
            context->Control.SetBitsAt<1>(0, context->PageCount).n_try_act(return);
            context->BlocksCount = context->PageCount;
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
