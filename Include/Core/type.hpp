#ifndef NCPP_TYPE_HPP
#define NCPP_TYPE_HPP

#include <stdint.h>

namespace ncpp
{
    template<bool V>
    struct bool_const_s
    {
        static constexpr bool value = V;
    };

    #define nbool_const(V) ncpp::bool_const_s<V>::value

    template<typename T, typename U>
    struct is_same_s
    {
        static constexpr bool value = false;
    };

    template<typename T>
    struct is_same_s<T, T>
    {
        static constexpr bool value = true;
    };
    
    #define nis_same(T, U) ncpp::is_same_s<T, U>::value
    
    template<bool B>
    struct enable_if_s { };

    template<>
    struct enable_if_s<true> { using type = bool; };
    
    #define nenable_if(expr) typename ncpp::enable_if_s<expr>::type = true
    
    #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
        #if __cplusplus < 202002L
            #define nis_simple(T) nbool_const(__is_pod(T))
        #else
            #define nis_simple(T)   nbool_const(__is_standard_layout(T)) && \
                                    nbool_const(__is_trivial(T))
        #endif
    #else
        #if __cplusplus < 202002L
            #define nis_simple(T) nbool_const(__is_pod(T))                      //NOTE: We are assuming compiler hook here
        #else
            #define nis_simple(T)   nbool_const(__is_standard_layout(T)) && \  //NOTE: We are assuming compiler hook here
                                    nbool_const(__is_trivial(T))
        #endif
    #endif
    
    #define ntypeof(x) decltype(x)
}

#endif
