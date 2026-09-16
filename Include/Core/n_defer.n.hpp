#ifndef NCPP_N_DEFER_N_HPP
#define NCPP_N_DEFER_N_HPP

/*
API:
```c++
//Runs at end of scope
n_defer { actions };
```

Usage:
```c++
{
    Nstd::HeapAllocator h = h.Init(32);
    Nstd::Allocator alloc = h.MakeAllocator();
    n_defer { alloc.Destroy(); };

    //alloc.Destroy() is called at end of scope regardless of early returns
}
```
*/

//NOTE: Improvised from https://stackoverflow.com/a/42060129
namespace ncpp
{
    struct DeferDummy {};
    template <class T> struct DeferObj { T f; ~DeferObj() { f(); } };
    template <class T> DeferObj<T> operator*(DeferDummy, T f) { return {f}; }
    #ifndef INTERNAL_DEFER_
        #define INTERNAL_DEFER_(LINE) zz_defer##LINE
    #endif
    
    #ifndef INTERNAL_DEFER__
        #define INTERNAL_DEFER__(LINE) INTERNAL_DEFER_(LINE)
    #endif
    
    #define n_defer auto INTERNAL_DEFER__(__COUNTER__) = ncpp::DeferDummy{} * [&]()
}

#endif
