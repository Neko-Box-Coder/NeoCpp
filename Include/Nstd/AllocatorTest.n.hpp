#ifndef NSTD_ALLOCATOR_TEST_N_HPP
#define NSTD_ALLOCATOR_TEST_N_HPP

#include <stdlib.h>

namespace Nstd
{
    static uint64 MemUsed = 0;
    
    inline void* BenchMalloc(uint64 sz)
    {
        MemUsed += sz;
        char* p = (char*)malloc(sz + sizeof(uint64));
        memcpy(p, &sz, sizeof(uint64));
        return p + sizeof(uint64);
    }
    
    inline void BenchFree(void* ptr)
    {
        uint64 sz;
        memcpy(&sz, (char*)ptr - sizeof(uint64), sizeof(uint64));
        MemUsed -= sz;
        free((char*) ptr - sizeof(uint64));
    }
    
    inline void* BenchRealloc(void* ptr, uint64 sz)
    {
        char* p = (char*)ptr - sizeof(uint64);
        uint64 oriSize;
        memcpy(&oriSize, p, sizeof(uint64));
        void* p2 = realloc(p, sz + sizeof(uint64));
        if(!p2)
            return NULL;
        if(sz > oriSize)
            MemUsed += sz - oriSize;
        else
            MemUsed -= oriSize - sz;
        return p2;
    }
}

#define BENCH_BASE_LINE     0
#define BENCH_SAMPLE        1000000
#define BENCH_FREE_N        500000
#define BENCH_ALLOC_2       1
#define BENCH_ALLOC_PROB    80,90,95,99 //int, BVec3, BVec3D, BFatNode, 1KB


#define NSTD_ALLOC_MALLOC(sz) BenchMalloc(sz)
#define NSTD_ALLOC_FREE(p) BenchFree(p)
#define NSTD_ALLOC_REALLOC(p, sz) BenchRealloc(p, sz)
#include "./Allocator.n.hpp"

#include "./External/msutimer/msutimer.h"
#include "./External/msutimer/msutimer.c"

//TODO: Move this test to src instead
namespace Nstd
{
    struct BVec3
    {
        float x;
        float y;
        float z;
    };
    static_assert(sizeof(BVec3) == 12, "");
    
    struct BVec3D
    {
        double x;
        double y;
        double z;
    };
    static_assert(sizeof(BVec3D) == 24, "");
    
    struct BFatNode
    {
        BFatNode* N;
        BFatNode* P;
        bool F;
        void* Table;
        void* Key;
        uint64 KeyL;
        uint64 ChildrenCount;
    };
    
    
    inline void PerformAllocations( n_ref Nstd::Allocator& alloc, 
                                    n_view<void*> sv, 
                                    n_ref uint64& minMem,
                                    n_view<uint32> szv,
                                    uint32 from,
                                    uint32 to)
    {
        int allocProbs[] = { BENCH_ALLOC_PROB };
        for(int i = from; i < to; ++i)
        {
            int r = rand() % 100;
            if(r < allocProbs[0])
            {
                #if BENCH_BASE_LINE
                    sv.data[i] = BenchMalloc(sizeof(int));
                #else
                    sv.data[i] = alloc.Malloc<int>(1);
                #endif
                minMem += sizeof(int);
                szv.data[i] = sizeof(int);
            }
            else if(r < allocProbs[1])
            {
                #if BENCH_BASE_LINE
                    sv.data[i] = BenchMalloc(sizeof(BVec3));
                #else
                    sv.data[i] = alloc.Malloc<BVec3>(1);
                #endif
                minMem += sizeof(BVec3);
                szv.data[i] = sizeof(BVec3);
            }
            else if(r < allocProbs[2])
            {
                #if BENCH_BASE_LINE
                    sv.data[i] = BenchMalloc(sizeof(BVec3D));
                #else
                    sv.data[i] = alloc.Malloc<BVec3D>(1);
                #endif
                minMem += sizeof(BVec3D);
                szv.data[i] = sizeof(BVec3D);
            }
            else if(r < allocProbs[3])
            {
                #if BENCH_BASE_LINE
                    sv.data[i] = BenchMalloc(sizeof(BFatNode));
                #else
                    sv.data[i] = alloc.Malloc<BFatNode>(1);
                #endif
                minMem += sizeof(BFatNode);
                szv.data[i] = sizeof(BFatNode);
            }
            else
            {
                #if BENCH_BASE_LINE
                    sv.data[i] = BenchMalloc(1024);
                #else
                    sv.data[i] = alloc.Malloc<char>(1024); //1KB
                #endif
                minMem += 1024;
                szv.data[i] = 1024;
            }
        }
    }
    
    inline n_result<void> BenchmarkAllocators()
    {
        MemUsed = 0;
        
        void** stores = (void**)malloc(sizeof(void*) * BENCH_SAMPLE);
        n_check_true(stores);
        n_defer { free(stores); };
        n_view<void*> sv = {stores, BENCH_SAMPLE};
        
        uint32* sizes = (uint32*)malloc(sizeof(uint32) * BENCH_SAMPLE);
        n_check_true(sizes);
        n_defer { free(sizes); };
        n_view<uint32> szv = {sizes, BENCH_SAMPLE};
        
        MSUTimer* timer = msutimer_new();
        n_check_true(timer);
        n_defer { msutimer_free( timer ); };
        
        uint64 minMem = 0;
        
        //Initialization
        double initReserveStart = msutimer_gettime(timer);
        #if BENCH_BASE_LINE
            Nstd::Allocator alloc = {};
        #else
            Nstd::Allocator alloc = alloc.Init<int, Nstd::HeapAllocator>(BENCH_SAMPLE);
        #endif
        double initReserveEnd = msutimer_gettime(timer);
        
        
        //First allocations
        uint32 allocFrom = 0;
        uint32 allocTo = BENCH_SAMPLE;
        #if BENCH_ALLOC_2
            allocTo /= 2;
        #endif
        double allocStart = msutimer_gettime(timer);
        PerformAllocations(n_ref alloc, sv, minMem, szv, allocFrom, allocTo);
        double allocEnd = msutimer_gettime(timer);
        
        printf("Allocations done\n");
        printf("Used %" PRIu64 " bytes total\n", MemUsed);
        printf("Data %" PRIu64 " bytes\n", minMem);
        printf("\n");
        
        //Free
        int range = allocTo - allocFrom;
        double freeStart = msutimer_gettime(timer);
        for(int i = 0; i < BENCH_FREE_N; ++i)
        {
            static_assert(RAND_MAX > BENCH_SAMPLE, "");
            int f = allocFrom + rand() % range;
            if(sv.data[f])
            {
                #if BENCH_BASE_LINE
                    BenchFree(sv.data[f]);
                #else
                    alloc.Free(sv.data[f]);
                #endif
                sv.data[f] = NULL;
                minMem -= szv.data[f];
            }
        }
        double freeEnd = msutimer_gettime(timer);
        
        printf("Free done\n");
        printf("Used %" PRIu64 " bytes of memory\n", MemUsed);
        printf("Minimum memory %" PRIu64 " bytes\n", minMem);
        printf("\n");
        
        
        
        //Second allocations
        #if BENCH_ALLOC_2
            allocFrom = allocTo;
            allocTo = BENCH_SAMPLE;
            
            double alloc2Start = msutimer_gettime(timer);
            PerformAllocations(n_ref alloc, sv, minMem, szv, allocFrom, allocTo);
            double alloc2End = msutimer_gettime(timer);
        
            printf("Allocations done\n");
            printf("Used %" PRIu64 " bytes total\n", MemUsed);
            printf("Data %" PRIu64 " bytes\n", minMem);
            printf("\n");
        #endif
        
        
        //Free all
        double freeAllStart = msutimer_gettime(timer);
        #if !BENCH_BASE_LINE
            alloc.FreeAll();
        #endif
        double freeAllEnd = msutimer_gettime(timer);
        
        
        
        //Destroy
        double destroyStart = msutimer_gettime(timer);
        #if !BENCH_BASE_LINE
            alloc.Destroy();
        #endif
        double destroyEnd = msutimer_gettime(timer);
        
        
        
        //Final
        printf("Init Reserve:   %.3lf usecs\n", initReserveEnd - initReserveStart);
        printf("Allocations:    %.3lf usecs\n", allocEnd - allocStart);
        
        #if BENCH_ALLOC_2
        printf("Allocations2:   %.3lf usecs\n", alloc2End - alloc2Start);
        #endif
        
        printf("Frees:          %.3lf usecs\n", freeEnd - freeStart);
        printf("Free All:       %.3lf usecs\n", freeAllEnd - freeAllStart);
        printf("Destroy:        %.3lf usecs\n", destroyEnd - destroyStart);
        return {};
    }
    
    inline n_result<void> BenchmarkAllocatorsMain()
    {
        for(int i = 0; i < 5; ++i)
        {
            BenchmarkAllocators().n_try();
            printf("\n--------------------------\n");
        }
        
        return {};
    }
}



#endif
