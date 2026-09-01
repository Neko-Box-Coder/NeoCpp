#ifndef NSTD_NODE_ALLOCATOR_N_HPP
#define NSTD_NODE_ALLOCATOR_N_HPP

#include "ncpp.n.hpp"
#include "./AllocatorPool.n.hpp"

#include <string.h>
#include <stddef.h>
#include <stdio.h>


namespace Nstd
{
    struct NodeAllocatorNode
    {
        uint32 Next;
        uint32 Prev;
        uint32 Blocks;
    };

    template<usize BLOCK_SIZE = 16>
    struct NodeAllocator
    {
        static constexpr uint32 USED_FLAG = (uint32)1 << 31;

        //Data offset rounded to BLOCK_SIZE so user pointers are properly aligned
        static constexpr usize DATA_OFFSET =    (sizeof(NodeAllocatorNode) + BLOCK_SIZE - 1) / 
                                                BLOCK_SIZE * BLOCK_SIZE;

        static_assert(BLOCK_SIZE >= sizeof(NodeAllocatorNode), "BLOCK_SIZE too small for Node");

        static constexpr int BUCKET_COUNT = 8;
        static constexpr int BUCKET_SIZE  = 64;
        uint32 Cache[BUCKET_COUNT][BUCKET_SIZE];
        uint32 CacheBlocks[BUCKET_COUNT][BUCKET_SIZE]; //block size per cache entry
        int CacheHead[BUCKET_COUNT];
        int CacheCount[BUCKET_COUNT];

        static constexpr uint32 NIL = ~uint32(0);

        n_view<uint8> Memory;
        uint32 BlockCount;
        uint32 FreeBlockCount;
        uint32 FreeHead;
        uint32 BumpBlock;

        int BucketForSize(uint32 blocks) const
        {
            if(blocks <= 4)  return 0;
            if(blocks <= 8)  return 1;
            if(blocks <= 16) return 2;
            if(blocks <= 32) return 3;
            if(blocks <= 64) return 4;
            if(blocks <= 128) return 5;
            if(blocks <= 256) return 6;
            return 7;
        }

        bool IsUsed(uint32 b) const { return b & USED_FLAG; }
        uint32 BlockCountOf(uint32 b) const { return b & ~USED_FLAG; }

        uint32 GetFreeBlocks() const { return FreeBlockCount; }
        uint32 SetUsed(uint32 n) { return n | USED_FLAG; }
        
        inline NodeAllocatorNode ReadNode(uint32 i) const 
        {
            return Memory.read<NodeAllocatorNode>(i * BLOCK_SIZE);
        }
        
        void WriteNode(uint32 i, NodeAllocatorNode n) 
        { 
            Memory.write<NodeAllocatorNode>(i * BLOCK_SIZE, n);
        }

        uint32 BlockFromPtr(uint8* p)
        {
            return (uint32)((p - Memory.data - DATA_OFFSET) / BLOCK_SIZE);
        }

        uint64 UsableBytes(uint32 blocks) const
        {
            uint32 total = blocks * BLOCK_SIZE;
            if(total > DATA_OFFSET)
                return (uint64)(total - DATA_OFFSET);
            return 0;
        }

        void CacheInsert(uint32 idx, uint32 blocks)
        {
            int b = BucketForSize(blocks);
            int slot = (CacheHead[b] + CacheCount[b]) % BUCKET_SIZE;
            Cache[b][slot] = idx;
            CacheBlocks[b][slot] = blocks;
            if(CacheCount[b] >= BUCKET_SIZE)
                CacheHead[b] = (CacheHead[b] + 1) % BUCKET_SIZE;
            else
                CacheCount[b]++;
        }

        void CacheRemoveBucket(int bucket, uint32 removeIdx)
        {
            if(bucket < 0 || bucket >= BUCKET_COUNT) return;
            int cc = CacheCount[bucket];
            if(cc <= 0) return;

            int startSlot = CacheHead[bucket];
            for(int i = 0; i < cc; ++i)
            {
                int slot = (startSlot + i) % BUCKET_SIZE;
                if(Cache[bucket][slot] == removeIdx) //Swap with last entry
                {
                    int lastSlot = (startSlot + cc - 1) % BUCKET_SIZE;
                    Cache[bucket][slot] = Cache[bucket][lastSlot];
                    CacheBlocks[bucket][slot] = CacheBlocks[bucket][lastSlot];
                    CacheCount[bucket]--;
                    return;
                }
            }
        }

        void CacheRemove(int bucket, uint32 removeIdx)
        {
            if(bucket < 0 || bucket >= BUCKET_COUNT) return;
            CacheRemoveBucket(bucket, removeIdx);
        }

        void CacheRemoveAll(uint32 removeIdx)
        {
            for(int b = 0; b < BUCKET_COUNT; ++b)
            {
                CacheRemoveBucket(b, removeIdx);
            }
        }

        inline n_result<void> Init(n_view<uint8> backing)
        {
            if(!backing)
                return n_error_msg("Invalid backing");
            
            uint64 totalBlocks = backing.len / BLOCK_SIZE;
            if(totalBlocks < 2 || totalBlocks > UINT32_MAX)
                return n_error_msg("Invalid reserve size: %zu", backing.len);

            BlockCount = (uint32)totalBlocks;
            FreeBlockCount = BlockCount;

            NodeAllocatorNode initNode;
            initNode.Next   = NIL;
            initNode.Prev   = NIL;
            initNode.Blocks = BlockCount;
            WriteNode(0, initNode);

            FreeHead  = 0;
            BumpBlock = 1;
            for(int b = 0; b < BUCKET_COUNT; ++b)
            {
                CacheHead[b]     = 0;
                CacheCount[b]    = 0;
            }

            if(BlockCount >= 2) //Seed cache with the initial large free region
            {
                int bkt = BucketForSize(BlockCount);
                Cache[bkt][0]    = 0;
                CacheHead[bkt]   = 0;
                CacheCount[bkt]  = 1;
            }

            return {};
        }

        void* MallocBlocks(uint64 size)
        {
            if(size == 0)
                size = 1;

           uint32 neededBytes = (uint32)(DATA_OFFSET + size);
            uint32 totalBlocks = (neededBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
            if(totalBlocks < 1)
                totalBlocks = 1;

            //1. Bump pointer fast path
            if(BumpBlock < BlockCount)
            {
                NodeAllocatorNode node = ReadNode(BumpBlock);
                uint32 freeSize = BlockCountOf(node.Blocks);
                if(!IsUsed(node.Blocks) &&
                   freeSize >= totalBlocks &&
                   freeSize <= (BlockCount - BumpBlock))
                {
                    int bkt = BucketForSize(freeSize);
                    CacheRemove(bkt, BumpBlock);
                    return AllocAt(BumpBlock, node, totalBlocks);
                }
            }

            //2. Size-class cache: scan matching bucket and larger
            int startBucket = BucketForSize(totalBlocks);
            for(int b = startBucket; b < BUCKET_COUNT; ++b)
            {
                int cc = CacheCount[b];
                if(cc <= 0) 
                    continue;

                int startSlot = CacheHead[b];
                for(int scanned = 0; scanned < cc; ++scanned)
                {
                    int slot = (startSlot + scanned) % BUCKET_SIZE;
                    uint32 cidx = Cache[b][slot];
                    uint32 cachedBlocks = CacheBlocks[b][slot];

                    if(cachedBlocks < totalBlocks) //cached size too small for this request
                        continue;

                    NodeAllocatorNode node = ReadNode(cidx);
                    n_assert_debug(!IsUsed(node.Blocks));
                    
                    #if 0
                    //Should not happen
                    if(IsUsed(node.Blocks))
                    {
                        CacheRemove(b, cidx);
                        cc = CacheCount[b];
                        scanned--;
                        continue;
                    }
                    #endif

                    {
                        uint32 freeSize = BlockCountOf(node.Blocks);
                        if(freeSize >= totalBlocks && freeSize <= (BlockCount - cidx))
                        {
                            CacheRemove(b, cidx);
                            return AllocAt(cidx, node, totalBlocks);
                        }
                    }
                    ++scanned;
                }
            }

            //3. Final fallback: spatial chain walk
            uint32 cur = FreeHead;
            while(cur != NIL)
            {
                NodeAllocatorNode node = ReadNode(cur);
                if(!IsUsed(node.Blocks))
                {
                    uint32 freeSize = BlockCountOf(node.Blocks);
                    if(freeSize >= totalBlocks && freeSize <= (BlockCount - cur))
                    {
                        int bkt = BucketForSize(freeSize);
                        CacheRemove(bkt, cur);
                        return AllocAt(cur, node, totalBlocks);
                    }
                }
                cur = node.Next;
            }

            //Exhausted, report stats for diagnostics
            {
                uint32 chainNodes  = 0;
                uint32 chainBlocks = 0;
                cur = FreeHead;
                while(cur != NIL)
                {
                    NodeAllocatorNode cn = ReadNode(cur);
                    chainNodes++;
                    if(chainNodes > 100000) break;
                    if(!IsUsed(cn.Blocks))
                        chainBlocks += BlockCountOf(cn.Blocks);
                    cur = cn.Next;
                }

                uint64 largestFreeChainBlock = 0;
                cur = FreeHead;
                while(cur != NIL)
                {
                    NodeAllocatorNode cn = ReadNode(cur);
                    if(!IsUsed(cn.Blocks))
                    {
                        uint32 s = BlockCountOf(cn.Blocks);
                        if(s > largestFreeChainBlock)
                            largestFreeChainBlock = s;
                    }
                    cur = cn.Next;
                }

                uint64 totalChainBytes     = chainBlocks * BLOCK_SIZE;
                uint64 totalChainUsable    = UsableBytes(chainBlocks);

                fprintf(stderr, 
                        "NodeAllocator OOM: need %u blocks (%zu bytes usable), "
                        "chain has %u nodes / %u free blocks (%zu MB total, %zu MB usable), "
                        "max contiguous free=%zu blocks\n",
                        totalBlocks, 
                        (uint64)size,
                        chainNodes, 
                        chainBlocks,
                        (uint64)(totalChainBytes / (1024*1024)),
                        (uint64)(totalChainUsable / (1024*1024)),
                        largestFreeChainBlock);
            }

            return NULL;
        }

        void* AllocAt(uint32 blockIdx, NodeAllocatorNode node, uint32 totalBlocks)
        {
            uint32 freeSize = BlockCountOf(node.Blocks);
            FreeBlockCount -= totalBlocks;
            if(freeSize == totalBlocks) //Exact fit
            {
                node.Blocks = SetUsed(totalBlocks);
                WriteNode(blockIdx, node);
                return &Memory[blockIdx * BLOCK_SIZE + DATA_OFFSET];
            }

            //Split: allocate first totalBlocks, remainder stays free starting at 
            //blockIdx + totalBlocks
            uint32 remaining = freeSize - totalBlocks;
            NodeAllocatorNode remNode;
            remNode.Next   = node.Next;
            remNode.Prev   = blockIdx;
            remNode.Blocks = remaining; //Not used flag = free

            if(node.Next != NIL)
            {
                NodeAllocatorNode nextN = ReadNode(node.Next);
                nextN.Prev = blockIdx + totalBlocks;
                WriteNode(node.Next, nextN);
            }

            node.Blocks = SetUsed(totalBlocks);
            node.Next   = blockIdx + totalBlocks;
            WriteNode(blockIdx, node);
            WriteNode(blockIdx + totalBlocks, remNode);

            if(BumpBlock == blockIdx) //Update bump pointer to split point
                BumpBlock = blockIdx + totalBlocks;

            //Insert remainder into cache (fresh index from split, no stale entries possible)
            CacheInsert(blockIdx + totalBlocks, remaining);
            return &Memory[blockIdx * BLOCK_SIZE + DATA_OFFSET];
        }

        void FreeBlocks(void* ptr)
        {
            if(!ptr)
                return;

            uint8* mem = (uint8*)ptr;
            if(mem < Memory.data || mem >= Memory.data + Memory.len)
                return;

            uint32 idx = BlockFromPtr(mem);
            NodeAllocatorNode node = ReadNode(idx);

            if(!IsUsed(node.Blocks)) //Already free
                return;

            //Mark as free and merge with adjacent free blocks
            uint32 origBlocks  = BlockCountOf(node.Blocks);
            uint32 mergeIdx    = idx;
            uint32 mergeBlocks = origBlocks;

            FreeBlockCount += origBlocks;
            CacheRemove(BucketForSize(mergeBlocks), idx);

            if(node.Prev != NIL) //Merge with predecessor if it's free
            {
                NodeAllocatorNode predN = ReadNode(node.Prev);
                if(!IsUsed(predN.Blocks))
                {
                    uint32 predBlocks = BlockCountOf(predN.Blocks);
                    //Evict absorbed predecessor from its cache bucket BEFORE links change
                    CacheRemove(BucketForSize(predBlocks), node.Prev);

                    mergeIdx    = node.Prev;
                    mergeBlocks += predBlocks;

                    if(predN.Prev != NIL) //Wire grandparent to merged block
                    {
                        NodeAllocatorNode gpN = ReadNode(predN.Prev);
                        gpN.Next = mergeIdx;
                        WriteNode(predN.Prev, gpN);
                    }
                    else
                        FreeHead = mergeIdx;

                    node.Prev = predN.Prev;

                    //Update downstream successor's Prev pointer, it may still point to the old 
                    //absorbed index
                    if(node.Next != NIL)
                    {
                        NodeAllocatorNode downN = ReadNode(node.Next);
                        downN.Prev = mergeIdx;
                        WriteNode(node.Next, downN);
                    }
                }
            }
            else if(FreeHead == idx)
                FreeHead = mergeIdx;

            //Merge with successor
            uint32 succIdx = mergeIdx + mergeBlocks;
            if(succIdx < BlockCount)
            {
                NodeAllocatorNode succN = ReadNode(succIdx);
                if(!IsUsed(succN.Blocks))
                {
                    uint32 succBlocks = BlockCountOf(succN.Blocks);
                    //Evict absorbed successor from its cache bucket BEFORE links change
                    CacheRemove(BucketForSize(succBlocks), succIdx);

                    mergeBlocks += succBlocks;
                    node.Next = succN.Next;
                    if(succN.Next != NIL)
                    {
                        NodeAllocatorNode nnN = ReadNode(succN.Next);
                        nnN.Prev = mergeIdx;
                        WriteNode(succN.Next, nnN);
                    }
                }
            }
            
            //Ensure FreeHead can reach this block: if we freed something before FreeHead,
            //walk back from FreeHead to find our merged block is reachable. If not, update
            //FreeHead so chain traversal starts from a position that can see all frees.
            if(mergeIdx < FreeHead)
            {
                //Walk backwards from FreeHead via Prev pointers — if we eventually reach
                //mergeIdx, it's fine. Otherwise, set FreeHead to our new block.
                uint32 check = FreeHead;
                bool reachable = false;
                for(uint32 steps = 0; steps < 64; steps++)
                {
                    if(check == mergeIdx)
                    {
                        reachable = true;
                        break;
                    }
                    NodeAllocatorNode cn = ReadNode(check);
                    if(cn.Prev == NIL)
                        break;
                    check = cn.Prev;
                }
                if(!reachable)
                    FreeHead = mergeIdx;
            }

            //Update node data and insert into cache */
            node.Blocks = mergeBlocks;
            WriteNode(mergeIdx, node);

            //Cache the merged result
            CacheInsert(mergeIdx, mergeBlocks);
        }

        void Destroy()
        {
            if(Memory)
            {
                NSTD_ALLOC_FREE(Memory.data);
                Memory = {};
            }
        }

        static void ReserveAhead(void*, uint64) {}

        static void* Malloc(void* c, uint64 byteSize)
        {
            NodeAllocator* context = (NodeAllocator*)c;
            return context->MallocBlocks(byteSize);
        }

        static void Free(void* c, void* ptr)
        {
            NodeAllocator* context = (NodeAllocator*)c;
            context->FreeBlocks(ptr);
        }

        static void* Realloc(void* c, void* p, uint64 byteSize)
        {
            NodeAllocator* context = (NodeAllocator*)c;
            if(!p) 
                return context->MallocBlocks(byteSize);
            
            //Read old size from node
            uint8* mem = (uint8*)p;
            int32 offset = (int32)(mem - context->Memory.data - DATA_OFFSET);
            if(offset < 0 || (offset % (int32)BLOCK_SIZE) != 0)
                return context->MallocBlocks(byteSize);
            
            uint32 blockIdx = (uint32)(offset / BLOCK_SIZE);
            NodeAllocatorNode node = context->ReadNode(blockIdx);
            uint64 oldUsable = context->UsableBytes(context->BlockCountOf(node.Blocks));
            if(oldUsable >= byteSize) //No expansion needed
                return p;
            
            void* newPtr = context->MallocBlocks(byteSize);
            if(newPtr)
            {
                memcpy(newPtr, p, oldUsable);
                context->FreeBlocks(p);
            }
            return newPtr;
        }

        static void FreeAll(void* c)
        {
            NodeAllocator* context = (NodeAllocator*)c;
            if(!context->Memory)
                return;

            //Reset to initial state: one big free region spanning entire pool
            NodeAllocatorNode initNode;
            initNode.Next   = NIL;
            initNode.Prev   = NIL;
            initNode.Blocks = context->BlockCount;
            context->WriteNode(0, initNode);

            context->FreeHead       = 0;
            context->BumpBlock      = 1;
            context->FreeBlockCount = context->BlockCount;

            for(int b = 0; b < BUCKET_COUNT; ++b)
            {
                context->CacheHead[b]  = 0;
                context->CacheCount[b] = 0;
            }

            //Re-seed cache with the initial large free region
            if(context->BlockCount >= 2)
            {
                int bkt = context->BucketForSize(context->BlockCount);
                context->Cache[bkt][0]     = 0;
                context->CacheBlocks[bkt][0] = context->BlockCount;
                context->CacheHead[bkt]    = 0;
                context->CacheCount[bkt]   = 1;
            }
        }

        static void DestroyAlloc(void* c)
        {
            NodeAllocator* context = (NodeAllocator*)c;
            context->Destroy();
        }

        static uint64 GetFreeBytes(const void* c)
        {
            const NodeAllocator* context = (NodeAllocator*)c;
            return context->UsableBytes(context->FreeBlockCount);
        }

        inline AllocatorPool MakeAllocatorPool()
        {
            AllocatorPool retAlloc = {};
            retAlloc.Init(  ReserveAhead, 
                            Malloc, 
                            Free, 
                            Realloc, 
                            FreeAll, 
                            DestroyAlloc, 
                            GetFreeBytes, 
                            this, 
                            true);
            return retAlloc;
        }
    };
}

#endif
