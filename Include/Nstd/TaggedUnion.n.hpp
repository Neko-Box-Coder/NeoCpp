#ifndef NSTD_TAGGED_UNION_N_HPP
#define NSTD_TAGGED_UNION_N_HPP

/*
Usage:
```c++
{
    Nstd::TaggedUnion<int, signed char, uint8> t = t.Init<uint8>(9);
    switch(t.Index)
    {
        case ntyepof(t)::GetIndex<int>():
            printf("int\n");
            break;
        case ntyepof(t)::GetIndex<signed char>():
            printf("char\n");
            break;
        case ntyepof(t)::GetIndex<uint8>():
            printf("uint8\n");
            break;
    }
    
    t.Get<uint8>() = 10;
    printf("t: %d\n", t.Get<uint8>());
    printf("t.Is<int>(): %s\n", (t.Is<int>() ? "true" : "false"));
    printf("t.Is<uint8>(): %s\n", (t.Is<uint8>() ? "true" : "false"));
}
```

Output:
```
uint8
t: 10
t.Is<int>(): false
t.Is<uint8>(): true
```
*/

#include "ncpp.n.hpp"

namespace Nstd
{
    template<   typename T1 = char, typename T2 = char, typename T3 = char, typename T4 = char,
                typename T5 = char, typename T6 = char, typename T7 = char, typename T8 = char>
    struct TaggedUnion
    {
        union
        {
            T1 TT1;
            T2 TT2;
            T3 TT3;
            T4 TT4;
            T5 TT5;
            T6 TT6;
            T7 TT7;
            T8 TT8;
        } Ts;
        
        uint8 Index;
    
        template<typename T, n_enable_if(n_is_same(T, T1))>
        static TaggedUnion Init(const T& val)
        {
            TaggedUnion returnT;
            returnT.Ts.TT1 = val;
            returnT.Index = 1;
            return returnT;
        }
        
        template<typename T, n_enable_if(n_is_same(T, T1))>
        static constexpr uint8 GetIndex()
        {
            return 1;
        }
        
        template<typename T, n_enable_if(n_is_same(T, T1))>
        T1& Get()
        {
            return Ts.TT1;
        }
        
        template<typename T, n_enable_if(n_is_same(T, T1))>
        const T1& Get() const
        {
            return Ts.TT1;
        }
        
        #define INTERN_MATCHED(currentT) n_is_same(T, currentT)
        #define INTERN_DEFINED(previousT, currentT, i) !n_is_same(previousT, currentT) && i
        
        
        #define INTERN_DECLARE_FUNCS(previousT, currentT, dataField, i) \
            template<   typename T, \
                        n_enable_if(INTERN_MATCHED(currentT) && INTERN_DEFINED(previousT, currentT, i))> \
            static TaggedUnion Init(const currentT& val) \
            { \
                TaggedUnion returnT; \
                returnT.Ts.dataField = val; \
                returnT.Index = i; \
                return returnT; \
            } \
            \
            template<   typename T, \
                        n_enable_if(INTERN_MATCHED(currentT) && INTERN_DEFINED(previousT, currentT, i))> \
            static constexpr uint8 GetIndex() \
            { \
                return i; \
            } \
            \
            template<   typename T, \
                        n_enable_if(INTERN_MATCHED(currentT) && INTERN_DEFINED(previousT, currentT, i))> \
            currentT& Get() \
            { \
                return Ts.dataField; \
            } \
            \
            template<   typename T, \
                        n_enable_if(INTERN_MATCHED(currentT) && INTERN_DEFINED(previousT, currentT, i))> \
            const currentT& Get() const \
            { \
                return Ts.dataField; \
            }
        
        INTERN_DECLARE_FUNCS(T1, T2, TT2, 2)
        INTERN_DECLARE_FUNCS(T2, T3, TT3, 3)
        INTERN_DECLARE_FUNCS(T3, T4, TT4, 4)
        INTERN_DECLARE_FUNCS(T4, T5, TT5, 5)
        INTERN_DECLARE_FUNCS(T5, T6, TT6, 6)
        INTERN_DECLARE_FUNCS(T6, T7, TT7, 7)
        INTERN_DECLARE_FUNCS(T7, T8, TT8, 8)
        
        template<typename T>
        bool Is() const 
        {
            return Index == GetIndex<T>();
        }
    };
}

#endif
