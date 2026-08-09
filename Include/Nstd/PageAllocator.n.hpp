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
    
    template<usize BLOCK_SIZE = 16, n_enable_if(BLOCK_SIZE >= sizeof(FreeNode))>
    struct PageAllocator
    {
        static constexpr usize PAGE_SIZE = BLOCK_SIZE * 8 * BLOCK_SIZE;
        
        n_view<uint8> Blocks;
        BitView Control;
        BitView Key;
        uint32 FreeNodeHead;
        uint32 BlocksCount;
        uint16 PageCount;
        bool Backing;
        
        #define DATA_START_INDEX PageCount * 2
        #define ASSERT_NODE(nodeRef) \
            do \
            { \
                n_assert(nodeRef.Next >= DATA_START_INDEX && nodeRef.Next < Control.Len()); \
                n_assert(nodeRef.Prev >= DATA_START_INDEX && nodeRef.Prev < Control.Len()); \
                n_assert(nodeRef.Blocks < Control.Len()); \
            } while(0)
        
        bool MergeNodeBefore(FreeNode& cur, uint32 curIndex)
        {
            ASSERT_NODE(cur);
            if(cur.Prev == curIndex)
                return false;
            
            FreeNode prev = Blocks.read<FreeNode>(BLOCK_SIZE * cur.Prev);
            ASSERT_NODE(prev);
            if(cur.Prev + prev.Blocks != curIndex)
                return false;
            
            if(cur.Next != curIndex)
            {
                n_assert(cur.Next > curIndex);
                FreeNode next = Blocks.read<FreeNode>(BLOCK_SIZE * cur.Next);
                ASSERT_NODE(next);
                n_assert(next.Prev == curIndex);
                next.Prev = cur.Prev;
                Blocks.write(BLOCK_SIZE * cur.Next, next);
            }
            
            prev.Blocks += cur.Blocks;
            prev.Next = cur.Next == curIndex ? cur.Prev : cur.Next;
            Blocks.write(BLOCK_SIZE * cur.Prev, prev);
            return true;
        }
        
        bool MergeNodeAfter(FreeNode& cur, uint32 curIndex)
        {
            ASSERT_NODE(cur);
            if(cur.Next == curIndex)
                return false;
            
            FreeNode next = Blocks.read<FreeNode>(BLOCK_SIZE * cur.Next);
            ASSERT_NODE(next);
            if(curIndex + cur.Blocks != cur.Next)
                return false;
            
            if(next.Next != cur.Next)
            {
                n_assert(next.Next > cur.Next);
                FreeNode nextNext = Blocks.read<FreeNode>(BLOCK_SIZE * next.Next);
                ASSERT_NODE(nextNext);
                n_assert(nextNext.Prev == cur.Next);
                nextNext.Prev = curIndex;
                Blocks.write(BLOCK_SIZE * next.Next, nextNext);
            }
            
            cur.Blocks += next.Blocks;
            cur.Next = next.Next == cur.Next ? curIndex : next.Next;
            Blocks.write(BLOCK_SIZE * curIndex, cur);
            return true;
        }
        
        bool InsertFreeNodeAfter(FreeNode& cur, uint32 curIndex, FreeNode& newNode, uint32 newIndex)
        {
            n_assert(curIndex < newIndex);
            n_assert(curIndex + cur.Blocks <= newIndex);
            ASSERT_NODE(cur);
            if(cur.Next == curIndex)
            {
                cur.Next = newIndex;
                newNode.Prev = curIndex;
                newNode.Next = newIndex;
                Blocks.write(BLOCK_SIZE * curIndex, cur);
                Blocks.write(BLOCK_SIZE * newIndex, newNode);
                return true;
            }
            
            FreeNode next = Blocks.read<FreeNode>(BLOCK_SIZE * cur.Next);
            ASSERT_NODE(next);
            n_assert(next.Prev == curIndex);
            if(newIndex + newNode.Blocks > cur.Next)
                return false;
            
            newNode.Prev = curIndex;
            newNode.Next = cur.Next;
            cur.Next = newIndex;
            next.Prev = newIndex;
            Blocks.write(BLOCK_SIZE * curIndex, cur);
            Blocks.write(BLOCK_SIZE * newIndex, newNode);
            Blocks.write(BLOCK_SIZE * newNode.Next, next);
            return true;
        }
        
        bool InsertFreeNodeBefore(FreeNode& cur, uint32 curIndex, FreeNode& newNode, uint32 newIndex)
        {
            n_assert(curIndex > newIndex);
            n_assert(newIndex + newNode.Blocks <= curIndex);
            ASSERT_NODE(cur);
            if(cur.Prev == curIndex)
            {
                n_assert(FreeNodeHead == curIndex);
                cur.Prev = newIndex;
                newNode.Prev = newIndex;
                newNode.Next = curIndex;
                Blocks.write(BLOCK_SIZE * curIndex, cur);
                Blocks.write(BLOCK_SIZE * newIndex, newNode);
                FreeNodeHead = newIndex;
                return true;
            }
            
            FreeNode prev = Blocks.read<FreeNode>(BLOCK_SIZE * cur.Prev);
            ASSERT_NODE(prev);
            n_assert(prev.Next == curIndex);
            if(cur.Prev + prev.Blocks > newIndex)
                return false;
            
            newNode.Next = curIndex;
            newNode.Prev = cur.Prev;
            cur.Prev = newIndex;
            prev.Next = newIndex;
            Blocks.write(BLOCK_SIZE * curIndex, cur);
            Blocks.write(BLOCK_SIZE * newIndex, newNode);
            Blocks.write(BLOCK_SIZE * newNode.Prev, prev);
            return true;
        }
        
        FreeNode SplitFreeNode(FreeNode& cur, uint32 curIndex, uint32 splitIndex)
        {
            ASSERT_NODE(cur);
            n_assert(splitIndex > curIndex && splitIndex < curIndex + splitIndex);

            FreeNode splitNode = cur;
            uint32 frontLen = splitIndex - curIndex;
            cur.Blocks = frontLen;
            splitNode.Blocks -= frontLen;

            splitNode.Prev = curIndex;
            splitNode.Next = cur.Next == curIndex ? splitIndex : cur.Next;

            if(cur.Next != curIndex) //Update next free node to point back to split node
            {
                FreeNode next = Blocks.read<FreeNode>(BLOCK_SIZE * cur.Next);
                ASSERT_NODE(next);
                next.Prev = splitIndex;
                Blocks.write(BLOCK_SIZE * cur.Next, next);
            }
            cur.Next = splitIndex;
            Blocks.write(BLOCK_SIZE * curIndex, cur);

            ASSERT_NODE(splitNode);
            Blocks.write(BLOCK_SIZE * splitIndex, splitNode);
            return splitNode;
        }
        
        
        void RemoveFreeNode(FreeNode& cur, uint32 curIndex)
        {
            ASSERT_NODE(cur);
            
            bool isHead = cur.Prev == curIndex;
            bool isTail = cur.Next == curIndex;
            
            if(isHead && isTail) //Single node — only one in the list
            {
                FreeNodeHead = Control.Len();
                return;
            }
            
             if(isTail) //Removing the tail — prev becomes the new tail
            {
                FreeNode prev = Blocks.read<FreeNode>(BLOCK_SIZE * cur.Prev);
                ASSERT_NODE(prev);
                n_assert(prev.Next == curIndex);
                prev.Next = cur.Prev;
                Blocks.write(BLOCK_SIZE * cur.Prev, prev);
                return;
            }
            
            FreeNode next = Blocks.read<FreeNode>(BLOCK_SIZE * cur.Next);
            ASSERT_NODE(next);
            n_assert(next.Prev == curIndex);
            
            if(isHead) //Removing the head — next becomes the new head
            {
                next.Prev = cur.Next;
                FreeNodeHead = cur.Next;
            }
            else //Middle node — link prev to next
            {
                next.Prev = cur.Prev;
                FreeNode prev = Blocks.read<FreeNode>(BLOCK_SIZE * cur.Prev);
                ASSERT_NODE(prev);
                n_assert(prev.Next == curIndex);
                prev.Next = cur.Next;
                Blocks.write(BLOCK_SIZE * cur.Prev, prev);
            }
            
            Blocks.write(BLOCK_SIZE * cur.Next, next);
        }
        
        
        
        //TODO: Use CharBacking only
        using CharBacking = n_view<char>;
        inline n_result<void> Intern_Init(TaggedUnion<CharBacking, usize> arg)
        {
            usize reserveSize;
            n_use_error_defer();
            
            if(arg.Is<CharBacking>())
            {
                n_check_true((bool)arg.Get<CharBacking>());
                reserveSize = arg.Get<CharBacking>().len;
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
            if(arg.Is<CharBacking>())
                Blocks = arg.Get<CharBacking>().as<uint8>();
            else
            {
                Blocks = { (uint8*)NSTD_ALLOC_MALLOC(PAGE_SIZE * PageCount), PAGE_SIZE * PageCount };
                if(!Blocks)
                    return n_error_msg("Failed to malloc");
            }
            
            n_error_defer 
            { 
                if(arg.Is<usize>())
                {
                    NSTD_ALLOC_FREE(Blocks.data); 
                    Blocks = {};
                }
            };
            
            n_assert(BLOCK_SIZE * DATA_START_INDEX < Blocks.len);
            Blocks.sub(0, BLOCK_SIZE * DATA_START_INDEX).zero();
            
            Control = Control.Init({ Blocks.data, BLOCK_SIZE * PageCount });
            Key = Key.Init({ &Blocks[BLOCK_SIZE * PageCount], BLOCK_SIZE * PageCount });
            Control.SetBitsAt<1>(0, DATA_START_INDEX).n_try();
            Key.SetBit<1>(0);
            BlocksCount = DATA_START_INDEX;
            Backing = arg.Is<CharBacking>();
            
            FreeNodeHead = DATA_START_INDEX;
            FreeNode freeNode = { FreeNodeHead, FreeNodeHead, Control.Len() - FreeNodeHead };
            Blocks.write(BLOCK_SIZE * FreeNodeHead, freeNode);
            return {};
        }
        
        inline n_result<void> InitWithBacking(CharBacking backing)
        {
            Intern_Init(TaggedUnion<CharBacking, usize>::Init<CharBacking>(backing)).n_try();
            return {};
        }
        
        inline n_result<void> Init(usize reserveSize)
        {
            Intern_Init(TaggedUnion<CharBacking, usize>::Init<usize>(reserveSize)).n_try();
            return {};
        }
        
        inline uint32 FitBlocks(uint64 fitByteSize)
        {
            const uint32 fitBlocksCount = (fitByteSize + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
            if(Control.Len() - DATA_START_INDEX < fitBlocksCount)
                return Control.Len();
            
            if(FreeNodeHead == Control.Len())
                return Control.Len();
            
            n_assert(FreeNodeHead >= DATA_START_INDEX);
            
            FreeNode curNode;
            uint32 curIndex = FreeNodeHead;
            while(true)
            {
                curNode = Blocks.read<FreeNode>(BLOCK_SIZE * curIndex);
                ASSERT_NODE(curNode);
                if(curNode.Blocks >= fitBlocksCount)
                    return curIndex;

                if(curNode.Next == curIndex)
                    break;

                curIndex = curNode.Next;
            }
            
            return Control.Len(); //No free slots
        }
        
        //NOTE: caller will set Control/Key bits
        inline bool UseFreeNode(uint32 index, uint32 blocks)
        {
            n_assert(index >= DATA_START_INDEX && index < Control.Len());
            
            FreeNode curFreeNode = Blocks.read<FreeNode>(BLOCK_SIZE * index);
            ASSERT_NODE(curFreeNode);
            if(blocks > curFreeNode.Blocks)
                return false;
            
            if(blocks == curFreeNode.Blocks) //Use whole block, remove from list
            {
                RemoveFreeNode(curFreeNode, index);
                return true;
            }
            
            uint32 newIndex = index + blocks;
            SplitFreeNode(curFreeNode, index, newIndex);
            
            RemoveFreeNode(curFreeNode, index);
            return true;
        }
        
        //NOTE: BlocksCount and FreeNode not maintained if OVERLAP is true. It's the caller 
        //      responsibility
        template<bool OVERLAP = false>
        inline void* UseBlocks(uint32 index, uint64 bytes)
        {
            n_assert(index >= DATA_START_INDEX && index < Control.Len());
            const usize blocksNeeded = (bytes + (BLOCK_SIZE - 1)) / BLOCK_SIZE;
            n_assert(index + blocksNeeded <= Control.Len());
            if(!OVERLAP)
            {
                n_assert(!Control.GetBit(index));
                n_assert(!Key.GetBit(index));
                n_assert(Control.GetBit(index - 1));
                if(!UseFreeNode(index, blocksNeeded))
                    return NULL;
                
                n_assert(BlocksCount >= DATA_START_INDEX && Control.Len() >= BlocksCount + blocksNeeded);
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
            if(ptr < Blocks.data || ptr >= Blocks.data + PAGE_SIZE * PageCount)
                return Control.Len();
            
            n_assert(((ptrdiff_t)ptr - (ptrdiff_t)Blocks.data) % BLOCK_SIZE == 0);
            return ((ptrdiff_t)ptr - (ptrdiff_t)Blocks.data) / BLOCK_SIZE;
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
                uint8 mixedByte = Control.ByteViews[startByteIndex] ^ Key.ByteViews[startByteIndex];
                for(int i = startBitIndex; i < 8; ++i)
                {
                    if(!((mixedByte >> i) & 0x01)) //Either Key + Control or No control bit
                    {
                        endIndex += (i - startBitIndex) + 1;
                        break;
                    }
                }
                
                if(startByteIndex == Control.ByteViews.len - 1)
                {
                    if(endIndex == index)
                        endIndex = Control.Len();
                }
                
                if(endIndex == index) //Not in first round
                {
                    for(int64 i = startByteIndex + 1; i < Control.ByteViews.len; ++i)
                    {
                        uint8 mixedByte = Control.ByteViews[i] ^ Key.ByteViews[i];
                        if(mixedByte == UINT8_MAX)
                            continue;
                        
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
                
                //Sanity check to make sure we end at start of next used blocks or free blocks
                if(endIndex != Control.Len())
                    n_assert(!Control.GetBit(endIndex) || Key.GetBit(endIndex));
                
                n_assert(BlocksCount >= (endIndex - index) + DATA_START_INDEX);
            } //else
            
            uint32 freeBlocks = endIndex - index;
            
            //Check for adjacent free neighbors
            bool hasPrev = index > DATA_START_INDEX && !Control.GetBit(index - 1);
            uint32 prevIndex = 0;
            if(hasPrev)
            {
                n_result<ssize> r = Control.GetBitsUntilFlipped<true>(index - 1);
                n_assert(!r.err);
                n_assert(r.value >= 0);
                prevIndex = (uint32)(r.value + 1);
            }

            bool hasNext = endIndex < Control.Len() && !Control.GetBit(endIndex);

            Key.SetBit<false>(index);
            Control.SetBits<false>(index, freeBlocks);
            BlocksCount -= freeBlocks;

            if(hasPrev) //Insert after prev, merge with prev, then handle next
            {
                FreeNode prevNode = Blocks.read<FreeNode>(BLOCK_SIZE * prevIndex);
                ASSERT_NODE(prevNode);

                FreeNode newNode;
                newNode.Blocks = freeBlocks;

                InsertFreeNodeAfter(prevNode, prevIndex, newNode, index);

                MergeNodeAfter(prevNode, prevIndex);

                if(hasNext)
                {
                    MergeNodeAfter(prevNode, prevIndex);
                }
                return;
            }
            
            FreeNode cur;
            cur.Blocks = freeBlocks;

            if(hasNext)
            {
                FreeNode nextNode = Blocks.read<FreeNode>(BLOCK_SIZE * endIndex);
                ASSERT_NODE(nextNode);
                InsertFreeNodeBefore(nextNode, endIndex, cur, index);
                MergeNodeAfter(cur, index);
                return;
            }
            
            //No spatially adjacent predecessor — find insertion point
            if(FreeNodeHead == Control.Len()) //Empty list
            {
                cur.Next = index;
                cur.Prev = index;
                FreeNodeHead = index;
                Blocks.write(BLOCK_SIZE * index, cur);
                return;
            }
            
            if(index < FreeNodeHead) //Before head — become new head
            {
                FreeNode oldHead = Blocks.read<FreeNode>(BLOCK_SIZE * FreeNodeHead);
                ASSERT_NODE(oldHead);
                InsertFreeNodeBefore(oldHead, FreeNodeHead, cur, index);
                FreeNodeHead = index;
                return;
            }
            
            //Between or after head — scan bitmap for successor
            n_result<ssize> r = Control.GetBitsUntilFlipped(endIndex);
            n_assert(!r.err);

            if((uint32)r.value < Control.Len()) //Scan ahead
            {
                FreeNode nextNode = Blocks.read<FreeNode>(BLOCK_SIZE * r.value);
                ASSERT_NODE(nextNode);
                InsertFreeNodeBefore(nextNode, (uint32)r.value, cur, index);
            }
            else //Scan backwards
            {
                n_result<ssize> r2 = Control.GetBitsUntilFlipped<true>(index - 1);
                n_assert(!r2.err);

                n_result<ssize> r3 = Control.GetBitsUntilFlipped<true>((uint32)r2.value);
                n_assert(!r3.err);

                uint32 tailIndex;
                if(r3.value < 0)
                    tailIndex = DATA_START_INDEX;
                else
                    tailIndex = (uint32)(r3.value + 1);

                FreeNode tail = Blocks.read<FreeNode>(BLOCK_SIZE * tailIndex);
                ASSERT_NODE(tail);
                InsertFreeNodeAfter(tail, tailIndex, cur, index);
            }
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
                uint32 shrinkStart = index + totalBlocksNeeded;
                uint32 shrinkCount = blocksOccupied - totalBlocksNeeded;

                Control.SetBits<false>(shrinkStart, shrinkCount);
                BlocksCount -= shrinkCount;

                uint32 shrinkEnd = shrinkStart + shrinkCount;
                if(shrinkEnd < Control.Len() && !Control.GetBit(shrinkEnd))
                {
                    // Merge with existing free node at shrinkEnd using helpers
                    FreeNode cur;
                    cur.Blocks = shrinkCount;

                    FreeNode nextFree = Blocks.read<FreeNode>(BLOCK_SIZE * shrinkEnd);
                    ASSERT_NODE(nextFree);

                    InsertFreeNodeBefore(nextFree, shrinkEnd, cur, shrinkStart);
                    MergeNodeAfter(cur, shrinkStart);
                }
                else
                {
                    // Standalone free node — find insertion point using bitmap
                    FreeNode cur;
                    cur.Blocks = shrinkCount;

                    if(FreeNodeHead == Control.Len())
                    {
                        cur.Next = shrinkStart;
                        cur.Prev = shrinkStart;
                        FreeNodeHead = shrinkStart;
                    }
                    else if(shrinkStart < FreeNodeHead)
                    {
                        FreeNode oldHead = Blocks.read<FreeNode>(BLOCK_SIZE * FreeNodeHead);
                        ASSERT_NODE(oldHead);
                        InsertFreeNodeBefore(oldHead, FreeNodeHead, cur, shrinkStart);
                        FreeNodeHead = shrinkStart;
                    }
                    else
                    {
                        uint32 endIndex = shrinkStart + shrinkCount;
                        n_result<ssize> r = Control.GetBitsUntilFlipped(endIndex);
                        n_assert(!r.err);

                        if((uint32)r.value < Control.Len())
                        {
                            FreeNode nextNode = Blocks.read<FreeNode>(BLOCK_SIZE * r.value);
                            ASSERT_NODE(nextNode);
                            InsertFreeNodeBefore(nextNode, (uint32)r.value, cur, shrinkStart);
                        }
                        else
                        {
                            n_result<ssize> r2 = Control.GetBitsUntilFlipped<true>(shrinkStart - 1);
                            n_assert(!r2.err);

                            n_result<ssize> r3 = Control.GetBitsUntilFlipped<true>((uint32)r2.value);
                            n_assert(!r3.err);

                            uint32 tailIndex;
                            if(r3.value < 0)
                                tailIndex = DATA_START_INDEX;
                            else
                                tailIndex = (uint32)(r3.value + 1);

                            FreeNode tail = Blocks.read<FreeNode>(BLOCK_SIZE * tailIndex);
                            ASSERT_NODE(tail);
                            InsertFreeNodeAfter(tail, tailIndex, cur, shrinkStart);
                        }
                    }
                }

                Key.SetBit<false>(shrinkStart);

                return index;
            }
            
            //Grow
            //Existing blocks are not at the end and has empty space after
            if( index + blocksOccupied < Control.Len() &&
                !Key.GetBit(index + blocksOccupied))
            {
                n_assert(Control.GetBit(index + blocksOccupied) == 0);
                
                uint32 nextFreeIndex = index + blocksOccupied;
                FreeNode nextFree = Blocks.read<FreeNode>(BLOCK_SIZE * nextFreeIndex);
                ASSERT_NODE(nextFree);
                
                uint32 totalBlocksFree = blocksOccupied + nextFree.Blocks;
                if(totalBlocksFree >= totalBlocksNeeded) //If we have enough free blocks
                {
                    uint32 growBlocks = totalBlocksNeeded - blocksOccupied;
                    if(nextFree.Blocks == growBlocks) //Used up all the free blocks in next free
                        RemoveFreeNode(nextFree, nextFreeIndex);
                    else //Partial consumption
                    {
                        uint32 newNextFreeIndex = index + totalBlocksNeeded;
                        n_assert(newNextFreeIndex > nextFreeIndex);
                        n_assert(newNextFreeIndex < nextFreeIndex + nextFree.Blocks);
                        (void)SplitFreeNode(nextFree, nextFreeIndex, newNextFreeIndex);
                        n_assert(nextFree.Blocks == growBlocks);
                        RemoveFreeNode(nextFree, nextFreeIndex);
                    }
                    
                    UseBlocks<true>(index, bytes);
                    BlocksCount += growBlocks;
                    return index;
                }
            }
            
            //Try refitting...
            uint32 fi = FitBlocks(bytes);
            if(fi == Control.Len())
                return fi;
            
            void* p = UseBlocks<false>(fi, bytes);
            memcpy(p, &Blocks[index], blocksOccupied * BLOCK_SIZE);
            
            FreeBlocks(index);
            return fi;
        }

        static void FreeAll(void* c)
        {
            PageAllocator* context = (PageAllocator*)c;
            memset(context->Blocks.data, 0, BLOCK_SIZE * context->DATA_START_INDEX);
            context->Control.SetBits<1>(0, context->DATA_START_INDEX);
            context->Key.SetBit<1>(0);
            context->BlocksCount = context->DATA_START_INDEX;
            
            context->FreeNodeHead = context->DATA_START_INDEX;
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
                NSTD_ALLOC_FREE(context->Blocks.data);
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
    
        #undef DATA_START_INDEX
    };
    
    static_assert(n_is_simple(PageAllocator<>));
}

#endif
