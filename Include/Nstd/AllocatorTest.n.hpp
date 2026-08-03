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
#define BENCH_RAW_MEM       0
#define BENCH_SAMPLE_N      100000
#define BENCH_ACCESS_N      50000
#define BENCH_FREE_N        30000
#define BENCH_ALLOC_IT      3
#define BENCH_ALLOC_PROB    10,50,80,98 //int, BVec3, BVec3D, BFatNode, 1KB


#define NSTD_ALLOC_MALLOC(sz) BenchMalloc(sz)
#define NSTD_ALLOC_FREE(p) BenchFree(p)
#define NSTD_ALLOC_REALLOC(p, sz) BenchRealloc(p, sz)
#include "./HeapAllocatorPool.n.hpp"
#include "./PageAllocator.n.hpp"
#include "./AllocatorPool.n.hpp"

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
    
    
    inline uint32 PerformAllocations( n_ref Nstd::AllocatorPool& alloc, 
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
                    #if BENCH_RAW_MEM
                        sv.data[i] = malloc(sizeof(int));
                    #else
                        sv.data[i] = BenchMalloc(sizeof(int));
                    #endif
                #else
                    sv.data[i] = alloc.Malloc<int>(1);
                #endif
                
                if(!sv.data[i])
                    return i;
                
                minMem += sizeof(int);
                szv.data[i] = sizeof(int);
            }
            else if(r < allocProbs[1])
            {
                #if BENCH_BASE_LINE
                    #if BENCH_RAW_MEM
                        sv.data[i] = malloc(sizeof(BVec3));
                    #else
                        sv.data[i] = BenchMalloc(sizeof(BVec3));
                    #endif
                #else
                    sv.data[i] = alloc.Malloc<BVec3>(1);
                #endif
                
                if(!sv.data[i])
                    return i;
                
                minMem += sizeof(BVec3);
                szv.data[i] = sizeof(BVec3);
            }
            else if(r < allocProbs[2])
            {
                #if BENCH_BASE_LINE
                    #if BENCH_RAW_MEM
                        sv.data[i] = malloc(sizeof(BVec3D));
                    #else
                        sv.data[i] = BenchMalloc(sizeof(BVec3D));
                    #endif
                #else
                    sv.data[i] = alloc.Malloc<BVec3D>(1);
                #endif
                
                if(!sv.data[i])
                    return i;
                
                minMem += sizeof(BVec3D);
                szv.data[i] = sizeof(BVec3D);
            }
            else if(r < allocProbs[3])
            {
                #if BENCH_BASE_LINE
                    #if BENCH_RAW_MEM
                        sv.data[i] = malloc(sizeof(BFatNode));
                    #else
                        sv.data[i] = BenchMalloc(sizeof(BFatNode));
                    #endif
                #else
                    sv.data[i] = alloc.Malloc<BFatNode>(1);
                #endif
                
                if(!sv.data[i])
                    return i;
                
                minMem += sizeof(BFatNode);
                szv.data[i] = sizeof(BFatNode);
            }
            else
            {
                #if BENCH_BASE_LINE
                    #if BENCH_RAW_MEM
                        sv.data[i] = malloc(1024);
                    #else
                        sv.data[i] = BenchMalloc(1024);
                    #endif
                #else
                    sv.data[i] = alloc.Malloc<char>(1024); //1KB
                #endif
                
                if(!sv.data[i])
                    return i;
                
                minMem += 1024;
                szv.data[i] = 1024;
            }
        }
    
        return to;
    }
    
    inline void Access(uint32 allocFrom, uint32 allocTo, n_view<void*> sv)
    {
        int range = allocTo - allocFrom;
        for(int i = 0; i < BENCH_ACCESS_N; ++i)
        {
            static_assert(RAND_MAX > BENCH_SAMPLE_N, "");
            int f = allocFrom + rand() % range;
            if(sv.data[f])
            {
                if(f > 0 && sv.data[f - 1])
                {
                    int* cur = (int*)sv.data[f];
                    int* prev = (int*)sv.data[f - 1];
                    *prev += *cur;
                }
            }
        }
    }
    
    inline void Free(   n_ref Nstd::AllocatorPool& alloc, 
                        uint32 allocFrom, 
                        uint32 allocTo, 
                        n_view<void*> sv, 
                        n_view<uint32> szv,
                        n_ref uint64& minMem)
    {
        int range = allocTo - allocFrom;
        for(int i = 0; i < BENCH_ACCESS_N; ++i)
        {
            static_assert(RAND_MAX > BENCH_SAMPLE_N, "");
            int f = allocFrom + rand() % range;
            if(sv.data[f])
            {
                #if BENCH_BASE_LINE
                    #if BENCH_RAW_MEM
                        free(sv.data[f]);
                    #else
                        BenchFree(sv.data[f]);
                    #endif
                #else
                    alloc.Free(sv.data[f]);
                #endif
                sv.data[f] = NULL;
                minMem -= szv.data[f];
            }
        }
    }
    
    inline n_result<void> BenchmarkAllocators()
    {
        MemUsed = 0;
        
        void** stores = (void**)calloc(BENCH_SAMPLE_N * BENCH_ALLOC_IT, sizeof(void*));
        n_check_true(stores);
        n_defer { free(stores); };
        n_view<void*> sv = {stores, BENCH_SAMPLE_N * BENCH_ALLOC_IT};
        
        uint32* sizes = (uint32*)malloc(sizeof(uint32) * BENCH_SAMPLE_N * BENCH_ALLOC_IT);
        n_check_true(sizes);
        n_defer { free(sizes); };
        n_view<uint32> szv = {sizes, BENCH_SAMPLE_N * BENCH_ALLOC_IT};
        
        MSUTimer* timer = msutimer_new();
        n_check_true(timer);
        n_defer { msutimer_free( timer ); };
        
        uint64 minMem = 0;
        
        //Initialization
        double initReserveStart = msutimer_gettime(timer);
        #if BENCH_BASE_LINE
            Nstd::AllocatorPool alloc = {};
        #else
            #if 0
                Nstd::HeapAllocatorPool h = {};
                h.Init(BENCH_SAMPLE_N);
                Nstd::AllocatorPool alloc = h.MakeAllocatorPool();
            #endif
            
            #if 1
                Nstd::PageAllocator<16> p = {};
                p.Init(3 MB).n_try();
                Nstd::AllocatorPool alloc = p.MakeAllocatorPool();
            #endif
        #endif
        double initReserveEnd = msutimer_gettime(timer);
        
        
        for(int it = 0; it < BENCH_ALLOC_IT; ++it)
        {
            uint32 allocFrom = 0;
            uint32 allocTo = BENCH_SAMPLE_N / BENCH_ALLOC_IT * (it + 1);
            double allocStart = msutimer_gettime(timer);
            uint32 oom = PerformAllocations(n_ref alloc, sv, n_ref minMem, szv, allocFrom, allocTo);
            double allocEnd = msutimer_gettime(timer);
            
            printf("Allocations done\n");
            if(oom != allocTo)
            {
                printf( "OOM at %" PRIu32 ", from %" PRIu32 " to %" PRIu32 "\n", 
                        oom, 
                        allocFrom, 
                        allocTo);
            }
            printf("Used %" PRIu64 " bytes total\n", MemUsed);
            printf("Data %" PRIu64 " bytes\n", minMem);
            printf("\n");
            
            double accessStart = msutimer_gettime(timer);
            Access(allocFrom,  allocTo, sv);
            double accessEnd = msutimer_gettime(timer);
            
            //TODO: Realloc
            
            //Free
            double freeStart = msutimer_gettime(timer);
            Free(n_ref alloc, allocFrom, allocTo, sv, szv, n_ref minMem);
            double freeEnd = msutimer_gettime(timer);
            
            printf("Free done\n");
            printf("Used %" PRIu64 " bytes total\n", MemUsed);
            printf("Data %" PRIu64 " bytes\n", minMem);
            printf("\n");
            
            printf("Allocations:    %.3lf usecs\n", allocEnd - allocStart);
            printf("Access:         %.3lf usecs\n", accessEnd - accessStart);
            printf("Frees:          %.3lf usecs\n", freeEnd - freeStart);
            
            if(oom != allocTo)
                break;
        }
        
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
        printf("Free All:       %.3lf usecs\n", freeAllEnd - freeAllStart);
        printf("Destroy:        %.3lf usecs\n", destroyEnd - destroyStart);
        return {};
    }
    
    inline n_result<void> BenchmarkAllocatorsMain()
    {
        printf("sizeof(int): %zu, ", sizeof(int));
        printf("sizeof(BVec3): %zu, ", sizeof(BVec3));
        printf("sizeof(BVec3D): %zu, ", sizeof(BVec3));
        printf("1KB \n");
        
        for(int i = 0; i < 3; ++i)
        {
            BenchmarkAllocators().n_try();
            printf("\n--------------------------\n");
        }
        
        return {};
    }
}



#endif
