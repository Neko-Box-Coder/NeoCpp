#ifndef NSTD_TAGGED_UNION_HPP
#define NSTD_TAGGED_UNION_HPP

/*
Usage:
```c++
{
    Nstd::TaggedUnion<int, signed char, uint8_t> t = t.Init<uint8_t>(9);
    switch(t.Index)
    {
        case tyepof(t)::GetIndex<int>():
            printf("int\n");
            break;
        case tyepof(t)::GetIndex<signed char>():
            printf("char\n");
            break;
        case tyepof(t)::GetIndex<uint8_t>():
            printf("uint8_t\n");
            break;
    }
    
    t.Get<uint8_t>() = 10;
    printf("t: %d\n", t.Get<uint8_t>());
}
```
*/

#include "./TemplateHelpers.hpp"


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
        
        uint8_t Index;
    
        template<typename T, typename EnableIf< IsSame<T, T1>::Value >::Type = true >
        static TaggedUnion Init(const T& val)
        {
            TaggedUnion returnT;
            returnT.Ts.TT1 = val;
            returnT.Index = 1;
            return returnT;
        }
        
        template<typename T, typename EnableIf< IsSame<T, T1>::Value >::Type = true >
        static constexpr uint8_t GetIndex()
        {
            return 1;
        }
        
        template<typename T, typename EnableIf< IsSame<T, T1>::Value >::Type = true >
        T1& Get()
        {
            return Ts.TT1;
        }
        
        #define INTERN_MATCHED(currentT) IsSame<T, currentT>::Value
        #define INTERN_DEFINED(previousT, currentT, i) !IsSame<previousT, currentT>::Value && i
        
        
        #define INTERN_DECLARE_FUNCS(previousT, currentT, dataField, i) \
            template<   typename T, \
                        typename EnableIf<  INTERN_MATCHED(currentT) && \
                                            INTERN_DEFINED(previousT, currentT, i) >::Type = true> \
            static TaggedUnion Init(const currentT& val) \
            { \
                TaggedUnion returnT; \
                returnT.Ts.dataField = val; \
                returnT.Index = i; \
                return returnT; \
            } \
            \
            template<   typename T, \
                        typename EnableIf<  INTERN_MATCHED(currentT) && \
                                            INTERN_DEFINED(previousT, currentT, i) >::Type = true> \
            static constexpr uint8_t GetIndex() \
            { \
                return i; \
            } \
            \
            template<   typename T, \
                        typename EnableIf<  INTERN_MATCHED(currentT) && \
                                            INTERN_DEFINED(previousT, currentT, i) >::Type = true> \
            currentT& Get() \
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
    };
}

#endif
