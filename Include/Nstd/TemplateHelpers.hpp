#ifndef NSTD_TEMPLATE_HELPERS_HPP
#define NSTD_TEMPLATE_HELPERS_HPP

#include <stdint.h>

#include <type_traits>

namespace Nstd
{
    #if 0
    template<bool V>
    struct BoolConst
    {
        static constexpr bool Value = V;
    };

    using TrueType = BoolConst<true>;
    using FalseType = BoolConst<false>;
    #endif
    
    template<typename T, typename U>
    struct IsSame
    {
        static constexpr bool Value = false;
    };

    template<typename T>
    struct IsSame<T, T>
    {
        static constexpr bool Value = true;
    };
    
    template<bool B>
    struct EnableIf { };

    template<>
    struct EnableIf<true> { using Type = bool; };
    //TaggedUnion<int, bool, char> test;
    //TaggedUnion<int, bool> test2;
    //TaggedUnion<int, void> test3;
    
    #define typeof(x) decltype(x)
}

#endif
