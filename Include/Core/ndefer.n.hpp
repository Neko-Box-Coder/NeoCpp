#ifndef NCPP_NDEFER_N_HPP
#define NCPP_NDEFER_N_HPP

/*
Usage:
```c++
{
    defer { <Actions> }
    ...
    if(...)
    {
        //<Actions> called
        return;
    }
    
    //<Actions> called
    return
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
    
    #define ndefer auto INTERNAL_DEFER__(__COUNTER__) = ncpp::DeferDummy{} * [&]()
}

#endif
