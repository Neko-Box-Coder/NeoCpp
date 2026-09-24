#ifndef NSTD_NODE_ALLOCATOR_N_HPP
#define NSTD_NODE_ALLOCATOR_N_HPP

/*
API:
```c++
template<usize BLOCK_SIZE = 16>
struct NodeAllocator
{
    static inline n_result<NodeAllocator> Init(n_view<uint8> backing);
    
    template<typename T>
    inline n_view<T> Malloc(uint64 count);
    inline bool OwnsPtr(void* ptr);
    inline void Free(void* ptr);
    
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
//TODO: Add example from Test.n.cpp
```
*/

#include "ncpp.n.hpp"
#include "./Allocator.n.hpp"

#include <string.h>
#include <stddef.h>
//#include <stdio.h>


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

        inline int Intern_BucketForSize(uint32 blocks) const
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

        inline bool Intern_IsUsed(uint32 b) const { return b & USED_FLAG; }
        inline uint32 Intern_BlockCountOf(uint32 b) const { return b & ~USED_FLAG; }

        inline uint32 Intern_SetUsed(uint32 n) { return n | USED_FLAG; }
        
        inline NodeAllocatorNode Intern_ReadNode(uint32 i) const 
        {
            return Memory.read<NodeAllocatorNode>(i * BLOCK_SIZE);
        }
        
        inline void Intern_WriteNode(uint32 i, NodeAllocatorNode n) 
        { 
            Memory.write<NodeAllocatorNode>(i * BLOCK_SIZE, n);
        }

        inline uint32 Intern_BlockFromPtr(uint8* p)
        {
            return (uint32)((p - Memory.data - DATA_OFFSET) / BLOCK_SIZE);
        }

        inline uint64 Intern_UsableBytes(uint32 blocks) const
        {
            uint32 total = blocks * BLOCK_SIZE;
            if(total > DATA_OFFSET)
                return (uint64)(total - DATA_OFFSET);
            return 0;
        }

        inline void Intern_CacheInsert(uint32 idx, uint32 blocks)
        {
            int b = Intern_BucketForSize(blocks);
            int slot = (CacheHead[b] + CacheCount[b]) % BUCKET_SIZE;
            Cache[b][slot] = idx;
            CacheBlocks[b][slot] = blocks;
            if(CacheCount[b] >= BUCKET_SIZE)
                CacheHead[b] = (CacheHead[b] + 1) % BUCKET_SIZE;
            else
                CacheCount[b]++;
        }

        inline void Intern_CacheRemoveBucket(int bucket, uint32 removeIdx)
        {
            if(bucket < 0 || bucket >= BUCKET_COUNT) 
                return;
            int cc = CacheCount[bucket];
            if(cc <= 0) 
                return;

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

        inline void Intern_CacheRemove(int bucket, uint32 removeIdx)
        {
            if(bucket < 0 || bucket >= BUCKET_COUNT) 
                return;
            Intern_CacheRemoveBucket(bucket, removeIdx);
        }

        inline n_result<NodeAllocator> Init(n_view<uint8> backing)
        {
            if(!backing)
                return n_error_msg("Invalid backing");
            
            NodeAllocator na = {};
            uint64 totalBlocks = backing.len / BLOCK_SIZE;
            if(totalBlocks < 2 || totalBlocks > UINT32_MAX)
                return n_error_msg("Invalid reserve size: %zu", backing.len);

            na.BlockCount = (uint32)totalBlocks;
            na.FreeBlockCount = na.BlockCount;

            NodeAllocatorNode initNode;
            initNode.Next   = NIL;
            initNode.Prev   = NIL;
            initNode.Blocks = na.BlockCount;
            na.Intern_WriteNode(0, initNode);

            na.FreeHead  = 0;
            na.BumpBlock = 1;
            for(int b = 0; b < BUCKET_COUNT; ++b)
            {
                na.CacheHead[b] = 0;
                na.CacheCount[b] = 0;
            }

            if(na.BlockCount >= 2) //Seed cache with the initial large free region
            {
                int bkt = Intern_BucketForSize(na.BlockCount);
                na.Cache[bkt][0] = 0;
                na.CacheHead[bkt] = 0;
                na.CacheCount[bkt] = 1;
            }

            return na;
        }

        template<typename T>
        inline n_view<T> Malloc(uint64 count)
        {
            uint64 size = sizeof(T) * count;
            if(size == 0)
                size = 1;

            uint32 neededBytes = (uint32)(DATA_OFFSET + size);
            uint32 totalBlocks = (neededBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
            if(totalBlocks < 1)
                totalBlocks = 1;

            if(BumpBlock < BlockCount) //1. Bump pointer fast path
            {
                NodeAllocatorNode node = Intern_ReadNode(BumpBlock);
                uint32 freeSize = Intern_BlockCountOf(node.Blocks);
                if( !Intern_IsUsed(node.Blocks) &&
                    freeSize >= totalBlocks &&
                    freeSize <= (BlockCount - BumpBlock))
                {
                    int bkt = Intern_BucketForSize(freeSize);
                    Intern_CacheRemove(bkt, BumpBlock);
                    return { (T*)Intern_AllocAt(BumpBlock, node, totalBlocks), count };
                }
            }

            //2. Size-class cache: scan matching bucket and larger
            int startBucket = Intern_BucketForSize(totalBlocks);
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

                    NodeAllocatorNode node = Intern_ReadNode(cidx);
                    n_assert_debug(!Intern_IsUsed(node.Blocks));
                    
                    #if 0
                    //Should not happen
                    if(Intern_IsUsed(node.Blocks))
                    {
                        Intern_CacheRemove(b, cidx);
                        cc = CacheCount[b];
                        scanned--;
                        continue;
                    }
                    #endif

                    {
                        uint32 freeSize = Intern_BlockCountOf(node.Blocks);
                        if(freeSize >= totalBlocks && freeSize <= (BlockCount - cidx))
                        {
                            Intern_CacheRemove(b, cidx);
                            return { (T*)Intern_AllocAt(cidx, node, totalBlocks), count };
                        }
                    }
                    ++scanned;
                }
            }

            //3. Final fallback: spatial chain walk
            uint32 cur = FreeHead;
            while(cur != NIL)
            {
                NodeAllocatorNode node = Intern_ReadNode(cur);
                if(!Intern_IsUsed(node.Blocks))
                {
                    uint32 freeSize = Intern_BlockCountOf(node.Blocks);
                    if(freeSize >= totalBlocks && freeSize <= (BlockCount - cur))
                    {
                        int bkt = Intern_BucketForSize(freeSize);
                        Intern_CacheRemove(bkt, cur);
                        return { (T*)Intern_AllocAt(cur, node, totalBlocks), count };
                    }
                }
                cur = node.Next;
            }

            //Exhausted, report stats for diagnostics
            #if 0
            {
                uint32 chainNodes  = 0;
                uint32 chainBlocks = 0;
                cur = FreeHead;
                while(cur != NIL)
                {
                    NodeAllocatorNode cn = Intern_ReadNode(cur);
                    chainNodes++;
                    if(chainNodes > 100000)
                        break;
                    if(!Intern_IsUsed(cn.Blocks))
                        chainBlocks += Intern_BlockCountOf(cn.Blocks);
                    cur = cn.Next;
                }

                uint64 largestFreeChainBlock = 0;
                cur = FreeHead;
                while(cur != NIL)
                {
                    NodeAllocatorNode cn = Intern_ReadNode(cur);
                    if(!Intern_IsUsed(cn.Blocks))
                    {
                        uint32 s = Intern_BlockCountOf(cn.Blocks);
                        if(s > largestFreeChainBlock)
                            largestFreeChainBlock = s;
                    }
                    cur = cn.Next;
                }

                uint64 totalChainBytes     = chainBlocks * BLOCK_SIZE;
                uint64 totalChainUsable    = Intern_UsableBytes(chainBlocks);

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
            #endif

            return {};
        }

        inline void* Intern_AllocAt(uint32 blockIdx, NodeAllocatorNode node, uint32 totalBlocks)
        {
            uint32 freeSize = Intern_BlockCountOf(node.Blocks);
            FreeBlockCount -= totalBlocks;
            if(freeSize == totalBlocks) //Exact fit
            {
                node.Blocks = Intern_SetUsed(totalBlocks);
                Intern_WriteNode(blockIdx, node);
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
                NodeAllocatorNode nextN = Intern_ReadNode(node.Next);
                nextN.Prev = blockIdx + totalBlocks;
                Intern_WriteNode(node.Next, nextN);
            }

            node.Blocks = Intern_SetUsed(totalBlocks);
            node.Next   = blockIdx + totalBlocks;
            Intern_WriteNode(blockIdx, node);
            Intern_WriteNode(blockIdx + totalBlocks, remNode);

            if(BumpBlock == blockIdx) //Update bump pointer to split point
                BumpBlock = blockIdx + totalBlocks;

            //Insert remainder into cache (fresh index from split, no stale entries possible)
            Intern_CacheInsert(blockIdx + totalBlocks, remaining);
            return &Memory[blockIdx * BLOCK_SIZE + DATA_OFFSET];
        }

        inline bool OwnsPtr(void* ptr)
        {
            if(!ptr)
                return false;
            return ptr >= Memory.data && ptr < Memory.data + Memory.len;
        }

        inline void Free(void* ptr)
        {
            if(!ptr)
                return;

            if(!OwnsPtr(ptr))
                return;

            uint8* mem = (uint8*)ptr;
            uint32 idx = Intern_BlockFromPtr(mem);
            NodeAllocatorNode node = Intern_ReadNode(idx);

            if(!Intern_IsUsed(node.Blocks)) //Already free
                return;

            //Mark as free and merge with adjacent free blocks
            uint32 origBlocks  = Intern_BlockCountOf(node.Blocks);
            uint32 mergeIdx    = idx;
            uint32 mergeBlocks = origBlocks;

            FreeBlockCount += origBlocks;
            Intern_CacheRemove(Intern_BucketForSize(mergeBlocks), idx);

            if(node.Prev != NIL) //Merge with predecessor if it's free
            {
                NodeAllocatorNode predN = Intern_ReadNode(node.Prev);
                if(!Intern_IsUsed(predN.Blocks))
                {
                    uint32 predBlocks = Intern_BlockCountOf(predN.Blocks);
                    //Evict absorbed predecessor from its cache bucket BEFORE links change
                    Intern_CacheRemove(Intern_BucketForSize(predBlocks), node.Prev);

                    mergeIdx    = node.Prev;
                    mergeBlocks += predBlocks;

                    if(predN.Prev != NIL) //Wire grandparent to merged block
                    {
                        NodeAllocatorNode gpN = Intern_ReadNode(predN.Prev);
                        gpN.Next = mergeIdx;
                        Intern_WriteNode(predN.Prev, gpN);
                    }
                    else
                        FreeHead = mergeIdx;

                    node.Prev = predN.Prev;

                    //Update downstream successor's Prev pointer, it may still point to the old 
                    //absorbed index
                    if(node.Next != NIL)
                    {
                        NodeAllocatorNode downN = Intern_ReadNode(node.Next);
                        downN.Prev = mergeIdx;
                        Intern_WriteNode(node.Next, downN);
                    }
                }
            }
            else if(FreeHead == idx)
                FreeHead = mergeIdx;

            //Merge with successor
            uint32 succIdx = mergeIdx + mergeBlocks;
            if(succIdx < BlockCount)
            {
                NodeAllocatorNode succN = Intern_ReadNode(succIdx);
                if(!Intern_IsUsed(succN.Blocks))
                {
                    uint32 succBlocks = Intern_BlockCountOf(succN.Blocks);
                    //Evict absorbed successor from its cache bucket BEFORE links change
                    Intern_CacheRemove(Intern_BucketForSize(succBlocks), succIdx);

                    mergeBlocks += succBlocks;
                    node.Next = succN.Next;
                    if(succN.Next != NIL)
                    {
                        NodeAllocatorNode nnN = Intern_ReadNode(succN.Next);
                        nnN.Prev = mergeIdx;
                        Intern_WriteNode(succN.Next, nnN);
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
                    NodeAllocatorNode cn = Intern_ReadNode(check);
                    if(cn.Prev == NIL)
                        break;
                    check = cn.Prev;
                }
                if(!reachable)
                    FreeHead = mergeIdx;
            }

            //Update node data and insert into cache */
            node.Blocks = mergeBlocks;
            Intern_WriteNode(mergeIdx, node);

            //Cache the merged result
            Intern_CacheInsert(mergeIdx, mergeBlocks);
        }
        
        template<typename T>
        inline n_view<T> Realloc(void* data, uint64 count)
        {
            if(!data)
                return Malloc<T>(count);
            
            //Read old size from node
            uint8* mem = (uint8*)data;
            int32 offset = (int32)(mem - Memory.data - DATA_OFFSET);
            if(offset < 0 || (offset % (int32)BLOCK_SIZE) != 0)
                return {};
            
            uint32 blockIdx = (uint32)(offset / BLOCK_SIZE);
            NodeAllocatorNode node = Intern_ReadNode(blockIdx);
            uint64 oldUsable = Intern_UsableBytes(Intern_BlockCountOf(node.Blocks));
            const uint64 byteSize = sizeof(T) * count;
            if(oldUsable >= byteSize) //No expansion needed
                return { (T*)data, count };
            
            n_view<T> newData = Malloc<T>(count).data;
            if(newData)
            {
                n_view<uint8> oldData { (uint8*)data, oldUsable };
                oldData.copy_to(newData.template as<uint8>());
                Free(data);
            }
            return newData;
        }
        
        inline void FreeAll()
        {
            if(!Memory)
                return;

            //Reset to initial state: one big free region spanning entire pool
            NodeAllocatorNode initNode;
            initNode.Next = NIL;
            initNode.Prev = NIL;
            initNode.Blocks = BlockCount;
            Intern_WriteNode(0, initNode);

            FreeHead = 0;
            BumpBlock = 1;
            FreeBlockCount = BlockCount;

            for(int b = 0; b < BUCKET_COUNT; ++b)
            {
                CacheHead[b] = 0;
                CacheCount[b] = 0;
            }

            //Re-seed cache with the initial large free region
            if(BlockCount >= 2)
            {
                int bkt = Intern_BucketForSize(BlockCount);
                Cache[bkt][0] = 0;
                CacheBlocks[bkt][0] = BlockCount;
                CacheHead[bkt] = 0;
                CacheCount[bkt] = 1;
            }
        }
        
        inline void Destroy()
        {
            memset(this, 0, sizeof(NodeAllocator));
        }
        
        inline uint64 GetFreeBytes() const
        {
            return Intern_UsableBytes(FreeBlockCount);
        }
        
        static void* ContextMalloc(void* c, uint64 byteSize)
        {
            NodeAllocator* context = (NodeAllocator*)c;
            return context->Malloc<uint8>(byteSize).data;
        }

        static void ContextFree(void* c, void* ptr)
        {
            NodeAllocator* context = (NodeAllocator*)c;
            context->Free(ptr);
        }

        static void* ContextRealloc(void* c, void* p, uint64 byteSize)
        {
            NodeAllocator* context = (NodeAllocator*)c;
            return context->Realloc<uint8>(p, byteSize).data;
        }

        static void ContextFreeAll(void* c)
        {
            NodeAllocator* context = (NodeAllocator*)c;
            context->FreeAll();
        }

        static void ContextDestroy(void* c)
        {
            NodeAllocator* context = (NodeAllocator*)c;
            context->Destroy();
        }

        static uint64 ContextGetFreeBytes(const void* c)
        {
            const NodeAllocator* context = (NodeAllocator*)c;
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
}

#endif
