#ifndef NSTD_PAGE_ALLOCATOR_N_HPP
#define NSTD_PAGE_ALLOCATOR_N_HPP

#include "ncpp.n.hpp"
#include "./BitView.n.hpp"
#include "./AllocatorPool.n.hpp"

#include <string.h>
#include <stddef.h>

namespace Nstd
{
    struct FreeNode
    {
        uint32 Next;
        uint32 Prev;
        uint32 Blocks;
    };
    
    template<usize BLOCK_SIZE = 16>
    struct PageAllocator
    {
        static constexpr usize PAGE_SIZE = BLOCK_SIZE * 8 * BLOCK_SIZE;
        
        uint8* Blocks;
        BitView Control;
        BitView Key;
        uint32 FreeNodeHead;
        uint32 BlocksCount;
        uint16 PageCount;
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
            if(pageCount > UINT16_MAX / 2 || BLOCK_SIZE * 8 * pageCount >= UINT32_MAX)
            {
                return n_error_msg( "Block size too small for this much memory. "
                                    "BLOCK SIZE: %zu, Memory: %" PRIu64 ", pageCount: %zu", 
                                    BLOCK_SIZE, 
                                    reserveSize,
                                    pageCount);
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
            Key = Key.Init({ Blocks + BLOCK_SIZE * PageCount, BLOCK_SIZE * PageCount });
            Control.SetBitsAt<1>(0, PageCount * 2).n_try();
            Key.SetBit<1>(0);
            BlocksCount = PageCount * 2;
            Backing = arg.Is<n_view<char>>();
            
            FreeNodeHead = PageCount * 2;
            FreeNode freeNode = { FreeNodeHead, FreeNodeHead, Control.Len() - FreeNodeHead };
            memcpy(&Blocks[BLOCK_SIZE * FreeNodeHead], &freeNode, sizeof(freeNode));
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
        
        #define ASSERT_NODE(nodeRef) \
            do \
            { \
                n_assert(nodeRef.Next >= PageCount * 2 && nodeRef.Next < Control.Len()); \
                n_assert(nodeRef.Prev >= PageCount * 2 && nodeRef.Prev < Control.Len()); \
                n_assert(nodeRef.Blocks < Control.Len()); \
            } while(0)
        
        inline uint32 FitBlocks(uint64 fitByteSize)
        {
            const uint32 fitBlocksCount = (fitByteSize + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
            if(Control.Len() - PageCount * 2 < fitBlocksCount)
                return Control.Len();
            
            if(FreeNodeHead == Control.Len())
                return Control.Len();
            
            n_assert(FreeNodeHead >= PageCount * 2);
            
            FreeNode curNode;
            uint32 curIndex = FreeNodeHead;
            do
            {
                memcpy(&curNode, &Blocks[BLOCK_SIZE * curIndex], sizeof(curNode));
                ASSERT_NODE(curNode);
                if(curNode.Blocks >= fitBlocksCount)
                    return curIndex;
                curIndex = curNode.Next;
            }
            while(curNode.Next != curIndex);
            
            return Control.Len(); //No free slots
        }
        
        inline bool UseFreeNode(uint32 index, uint32 blocks)
        {
            n_assert(index >= PageCount * 2 && index < Control.Len());
            
            FreeNode curFreeNode;
            memcpy(&curFreeNode, &Blocks[BLOCK_SIZE * index], sizeof(FreeNode));
            ASSERT_NODE(curFreeNode);
            if(blocks > curFreeNode.Blocks)
                return false;
            
            n_optional<FreeNode> prevNode = {};
            n_optional<FreeNode> nextNode = {};
            if(curFreeNode.Prev != index)
            {
                prevNode = FreeNode {};
                memcpy(&prevNode.value, &Blocks[BLOCK_SIZE * curFreeNode.Prev], sizeof(FreeNode));
                ASSERT_NODE(prevNode.value);
            }
            
            if(curFreeNode.Next != index)
            {
                nextNode = FreeNode {};
                memcpy(&nextNode.value, &Blocks[BLOCK_SIZE * curFreeNode.Next], sizeof(FreeNode));
                ASSERT_NODE(nextNode.value);
            }
            
            //TODO(NOW): Something wrong when splitting or merging free nodes? There's a cycle...
            if(blocks == curFreeNode.Blocks)
            {
                if(index == FreeNodeHead)
                {
                    if(nextNode)
                        FreeNodeHead = curFreeNode.Next;
                    else
                        FreeNodeHead = Control.Len();
                }
                
                if(prevNode)
                {
                    prevNode->Next = !nextNode ? curFreeNode.Prev : curFreeNode.Next;
                    memcpy(&Blocks[BLOCK_SIZE * curFreeNode.Prev], &prevNode.value, sizeof(FreeNode));
                }
                if(nextNode)
                {
                    nextNode->Prev = !prevNode ? curFreeNode.Next : curFreeNode.Prev;
                    memcpy(&Blocks[BLOCK_SIZE * curFreeNode.Next], &nextNode.value, sizeof(FreeNode));
                }
                return true;
            }
            
            uint32 newIndex = index + blocks;
            n_assert(newIndex < Control.Len() && !Control.GetBit(newIndex));
            
            FreeNode newNode = curFreeNode;
            if(prevNode)
            {
                prevNode->Next = newIndex;
                memcpy(&Blocks[BLOCK_SIZE * curFreeNode.Prev], &prevNode.value, sizeof(FreeNode));
            }
            if(nextNode)
            {
                nextNode->Prev = newIndex;
                memcpy(&Blocks[BLOCK_SIZE * curFreeNode.Next], &nextNode.value, sizeof(FreeNode));
            }
            newNode.Blocks -= blocks;
            memcpy(&Blocks[BLOCK_SIZE * newIndex], &newNode, sizeof(FreeNode));
            
            if(FreeNodeHead == index)
                FreeNodeHead = newIndex;
            
            return true;
        }
        
        //NOTE: BlocksCount and FreeNode not maintained if OVERLAP is true. It's the caller 
        //      responsibility
        template<bool OVERLAP = false>
        inline void* UseBlocks(uint32 index, uint64 bytes)
        {
            n_assert(index >= PageCount * 2 && index < Control.Len());
            const usize blocksNeeded = (bytes + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
            n_assert(blocksNeeded <= Control.Len() - BlocksCount);
            if(!OVERLAP)
            {
                n_assert(!Control.GetBit(index));
                n_assert(!Key.GetBit(index));
                n_assert(Control.GetBit(index - 1));
                if(!UseFreeNode(index, blocksNeeded))
                    return NULL;
                
                n_assert(BlocksCount >= PageCount * 2 && Control.Len() >= BlocksCount + blocksNeeded);
                BlocksCount += blocksNeeded;
            }
            
            const uint32 endIndex = index + blocksNeeded; (void)endIndex;
            n_assert(endIndex <= Control.Len());
            n_result<void> r = Control.SetBitsAt<true>(index, blocksNeeded); (void)r;
            Key.SetBit<true>(index);
            n_assert(!r.err);
            return &Blocks[index * BLOCK_SIZE];
        }
        
        inline uint32 FindIndex(void* ptr)
        {
            if(ptr < Blocks || ptr >= Blocks + PAGE_SIZE * PageCount)
                return Control.Len();
            
            n_assert(((ptrdiff_t)ptr - (ptrdiff_t)Blocks) % BLOCK_SIZE == 0);
            return ((ptrdiff_t)ptr - (ptrdiff_t)Blocks) / BLOCK_SIZE;
        }
        
        #if 0
        inline uint32 GetUsedBlocksCount(uint32 index)
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
        
        inline void FreeBlocks(uint32 index)
        {
            if(index >= Control.Len())
                return;
            
            n_assert(Control.GetBit(index));
            n_assert(Key.GetBit(index));
            
            usize endIndex = index;
            if(index == Control.Len() - 1)
                endIndex = Control.Len();
            else //Walk the occupied blocks to see how many we are using
            {
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
                
                if(startByteIndex == Control.ByteViews.len - 1)
                    n_assert(endIndex != index);
                
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
                
                if(endIndex != Control.Len())
                    n_assert(!Control.GetBit(endIndex) || Key.GetBit(endIndex));
                
                n_assert(BlocksCount >= (endIndex - index) + PageCount);
            }
            
            uint32 freeBlocks = endIndex - index;
            
            //Check if we have free neighbor space
            n_optional<FreeNode> prevFree = n_none;
            uint32 prevIndex = 0;
            n_optional<FreeNode> nextFree = n_none;
            uint32 nextIndex = endIndex;
            if(index != PageCount * 2 && !Control.GetBit(index - 1))
            {
                n_result<ssize> r = Control.GetBitsUntilFlipped<true>(index - 1);
                n_assert(!r.err);
                n_assert(r.value >= 0);
                prevIndex = r.value;
                prevFree = FreeNode {};
                memcpy(&prevFree.value, &Blocks[BLOCK_SIZE * (prevIndex + 1)], sizeof(FreeNode));
                ASSERT_NODE(prevFree.value);
            }
            
            if(endIndex != Control.Len() && !Control.GetBit(endIndex))
            {
                nextFree = FreeNode {};
                memcpy(&nextFree.value, &Blocks[BLOCK_SIZE * endIndex], sizeof(FreeNode));
                ASSERT_NODE(nextFree.value);
            }
            
            //Merge free nodes
            if(prevFree && nextFree) //Merge to prev free
            {
                prevFree->Blocks += freeBlocks + nextFree->Blocks;
                if(nextFree->Next == nextIndex)
                    prevFree->Next = prevIndex;
                else
                {
                    prevFree->Next = nextFree->Next;
                    
                    FreeNode nextAfterMerged;
                    memcpy(&nextAfterMerged, &Blocks[BLOCK_SIZE * nextFree->Next], sizeof(FreeNode));
                    ASSERT_NODE(nextAfterMerged);
                    nextAfterMerged.Prev = prevIndex;
                    memcpy(&Blocks[BLOCK_SIZE * nextFree->Next], &nextAfterMerged, sizeof(FreeNode));
                }
                memcpy(&Blocks[BLOCK_SIZE * prevIndex], &prevFree.value, sizeof(FreeNode));
            }
            else if(prevFree) //Merge to prev free
            {
                prevFree->Blocks += freeBlocks;
                memcpy(&Blocks[BLOCK_SIZE * prevIndex], &prevFree.value, sizeof(FreeNode));
            }
            else if(nextFree) //Merge to next free
            {
                FreeNode curFree = *nextFree;
                if(curFree.Next == nextIndex)
                    curFree.Next = index;
                else
                {
                    FreeNode nextAfterMerged;
                    memcpy(&nextAfterMerged, &Blocks[BLOCK_SIZE * curFree.Next], sizeof(FreeNode));
                    ASSERT_NODE(nextAfterMerged);
                    nextAfterMerged.Prev = index;
                    memcpy(&Blocks[BLOCK_SIZE * curFree.Next], &nextAfterMerged, sizeof(FreeNode));
                }
                
                if(curFree.Prev != nextIndex)
                {
                    FreeNode prevAfterMerged;
                    memcpy(&prevAfterMerged, &Blocks[BLOCK_SIZE * curFree.Prev], sizeof(FreeNode));
                    ASSERT_NODE(prevAfterMerged);
                    prevAfterMerged.Next = index;
                    memcpy(&Blocks[BLOCK_SIZE * curFree.Prev], &prevAfterMerged, sizeof(FreeNode));
                }
                
                if(FreeNodeHead == nextIndex)
                    FreeNodeHead = index;
                
                curFree.Blocks += freeBlocks;
                memcpy(&Blocks[BLOCK_SIZE * index], &curFree, sizeof(FreeNode));
            }
            else //Otherwise, create a free node and add to free node head
            {
                FreeNode curFree;
                curFree.Next = FreeNodeHead == Control.Len() ? index : FreeNodeHead;
                curFree.Prev = index;
                curFree.Blocks = freeBlocks;
                memcpy(&Blocks[BLOCK_SIZE * index], &curFree, sizeof(FreeNode));
            }
            
            Key.SetBit<false>(index);
            Control.SetBits<false>(index, endIndex - index);
            BlocksCount -= freeBlocks;
        }
        
        inline uint32 ReallocBlocks(uint32 index, uint64 bytes)
        {
            n_assert(index < Control.Len());
            n_assert(Control.GetBit(index));
            n_assert(Key.GetBit(index));
            if(bytes > (PAGE_SIZE - BLOCK_SIZE) * PageCount)
                return Control.Len();
            
            uint32 blocksOccupied = 0;
            if(index < Control.Len() - 1)
            {
                if(Key.GetBit(index + 1))
                    blocksOccupied = 1;
                else
                {
                    n_result<ssize> f = Key.GetBitsUntilFlipped(index + 1);
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
                if( index + blocksOccupied < Control.Len() &&
                    !Key.GetBit(index + blocksOccupied))
                {
                    n_assert(Control.GetBit(index + blocksOccupied) == 0);
                    
                    FreeNode nextFree;
                    uint32 nextFreeIndex = index + blocksOccupied;
                    memcpy(&nextFree, &Blocks[BLOCK_SIZE * nextFreeIndex], sizeof(FreeNode));
                    ASSERT_NODE(nextFree);
                    uint32 totalBlocksFree = blocksOccupied + nextFree.Blocks;
                    if(totalBlocksFree >= totalBlocksNeeded) //If we have enough free blocks
                    {
                        uint32 growBlocks = totalBlocksNeeded - blocksOccupied;
                        nextFree.Blocks -= growBlocks;
                        
                        n_optional<FreeNode> nextFreeNext = n_none;
                        n_optional<FreeNode> nextFreePrev = n_none;
                        if(nextFree.Next != nextFreeIndex)
                        {
                            nextFreeNext = FreeNode {};
                            memcpy( &nextFreeNext.value, 
                                    &Blocks[BLOCK_SIZE * nextFree.Next], 
                                    sizeof(FreeNode));
                            ASSERT_NODE(nextFreeNext.value);
                        }
                        
                        if(nextFree.Prev != nextFreeIndex)
                        {
                            nextFreePrev = FreeNode {};
                            memcpy( &nextFreePrev.value, 
                                    &Blocks[BLOCK_SIZE * nextFree.Prev], 
                                    sizeof(FreeNode));
                            ASSERT_NODE(nextFreePrev.value);
                        }
                        
                        //Update neighboring free nodes
                        if(!nextFree.Blocks) //Used up all the free blocks in next free
                        {
                            if(nextFreePrev)
                            {
                                if(nextFreeNext)
                                    nextFreePrev->Next = nextFree.Next;
                                else
                                    nextFreePrev->Next = nextFree.Prev;
                                
                                memcpy( &Blocks[BLOCK_SIZE * nextFree.Prev], 
                                        &nextFreePrev.value,
                                        sizeof(FreeNode));
                            }
                            
                            if(nextFreeNext)
                            {
                                if(nextFreePrev)
                                    nextFreeNext->Prev = nextFree.Prev;
                                else
                                {
                                    nextFreeNext->Prev = nextFree.Next;
                                    if(FreeNodeHead == nextFreeIndex)
                                        FreeNodeHead = nextFree.Next;
                                }
                                
                                memcpy( &Blocks[BLOCK_SIZE * nextFree.Next], 
                                        &nextFreeNext.value,
                                        sizeof(FreeNode));
                            }
                            else if(FreeNodeHead == nextFreeIndex)
                                FreeNodeHead = Control.Len();
                        }
                        else
                        {
                            FreeNode newNextFree = nextFree;
                            uint32 newNextFreeIndex = index + totalBlocksNeeded;
                            
                            if(nextFreePrev)
                            {
                                nextFreePrev->Next = newNextFreeIndex;
                                memcpy( &Blocks[BLOCK_SIZE * nextFree.Prev], 
                                        &nextFreePrev.value,
                                        sizeof(FreeNode));
                            }
                            
                            if(nextFreeNext)
                            {
                                nextFreeNext->Prev = newNextFreeIndex;
                                memcpy( &Blocks[BLOCK_SIZE * nextFree.Next], 
                                        &nextFreeNext.value,
                                        sizeof(FreeNode));
                            }
                            
                            if(FreeNodeHead == nextFreeIndex)
                                FreeNodeHead = newNextFreeIndex;
                            
                            memcpy( &Blocks[BLOCK_SIZE * newNextFreeIndex], 
                                    &newNextFree,
                                    sizeof(FreeNode));
                        }
                        
                        UseBlocks<true>(index, bytes);
                        BlocksCount += growBlocks;
                        return index;
                    } //if(totalBlocksFree >= totalBlocksNeeded)
                } //if( index + blocksOccupied < Control.Len() &&
                  //    !Key.GetBit(index + blocksOccupied))
                
                
                //Try refitting...
                uint32 fi = FitBlocks(bytes);
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
            memset(context->Blocks, 0, BLOCK_SIZE * context->PageCount * 2);
            context->Control.SetBits<1>(0, context->PageCount * 2);
            context->Key.SetBit<1>(0);
            context->BlocksCount = context->PageCount * 2;
            
            context->FreeNodeHead = context->PageCount * 2;
            FreeNode freeNode = {
                                    context->FreeNodeHead, 
                                    context->FreeNodeHead, 
                                    context->Control.Len() - context->FreeNodeHead 
                                };
            memcpy(&context->Blocks[BLOCK_SIZE * context->FreeNodeHead], &freeNode, sizeof(freeNode));
        }
        
        static void Destroy(void* c)
        {
            PageAllocator* context = (PageAllocator*)c;
            if(!context->Backing)
                NSTD_ALLOC_FREE(context->Blocks);
            memset(context, 0, sizeof(PageAllocator<BLOCK_SIZE>));
        }
        
        static void* Malloc(void* c, uint64 byteSize)
        {
            PageAllocator* context = (PageAllocator*)c;
            if(!byteSize)
                return NULL;
            
            uint32 index = context->FitBlocks(byteSize);
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
            
            uint32 i = context->FindIndex(p);
            if(i == context->Control.Len())
                return NULL;
            
            uint32 ri = context->ReallocBlocks(i, byteSize);
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
        
        #undef ASSERT_NODE
    };
    
    static_assert(n_is_simple(PageAllocator<>));
}

#endif
