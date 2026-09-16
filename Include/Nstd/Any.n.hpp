#ifndef NSTD_ANY_N_HPP
#define NSTD_ANY_N_HPP

/*
API:
```c++
struct Any
{
    void* Val;
    uint64 Type;

    template<typename T>
    inline bool Is();
    
    template<typename T>
    inline T* Get();
    
    template<typename T>
    static inline Any Init(n_ref T& val);
};
```

Usage:
```c++
//TODO: Add example from Test.n.cpp
```
*/

#include "ncpp.n.hpp"

#include <typeinfo>

namespace Nstd
{
    struct Any
    {
        void* Val;
        uint64 Type;
    
        template<typename T>
        inline bool Is() { return typeid(T).hash_code() == Type; }
        
        template<typename T>
        inline T* Get() { return (T*)Val; }
        
        template<typename T>
        static inline Any Init(n_ref T& val)
        {
            return { typeid(T).hash_code(), &val };
        }
    };
}

#endif
