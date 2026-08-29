#ifndef NSTD_ANY_N_HPP
#define NSTD_ANY_N_HPP

#include "ncpp.n.hpp"

#include <typeinfo>

namespace Nstd
{
    struct Any
    {
        usize Type;
        void* Val;
    
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
