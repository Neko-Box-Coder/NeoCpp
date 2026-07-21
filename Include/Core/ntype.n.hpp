#ifndef NCPP_NTYPE_N_HPP
#define NCPP_NTYPE_N_HPP

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
    
    
    template<typename T> struct no_ref_s { typedef T type; };
    template<typename T> struct no_ref_s<T&> { typedef T type; };
    template<typename T> struct no_ref_s<T&&> { typedef T type; };
    
    #define nno_ref(t) ncpp::no_ref_s<t>::type


    template<typename T> struct no_const_s { typedef T type; };
    template<typename T> struct no_const_s<const T> { typedef T type; };
    
    #define nno_const(t) ncpp::no_const_s<t>::type
    
    
    template<typename T, size_t>
    struct to_signed_s {};
    
    template<typename T> struct to_signed_s<T, 1> { using type = int8_t; };
    template<typename T> struct to_signed_s<T, 2> { using type = int16_t; };
    template<typename T> struct to_signed_s<T, 4> { using type = int32_t; };
    template<typename T> struct to_signed_s<T, 8> { using type = int64_t; };
    
    #define nto_signed(x) ncpp::to_signed_s<x, sizeof(x)>::type

    
    template<typename T, size_t>
    struct to_unsigned_s {};
    
    template<typename T> struct to_unsigned_s<T, 1> { using type = uint8_t; };
    template<typename T> struct to_unsigned_s<T, 2> { using type = uint16_t; };
    template<typename T> struct to_unsigned_s<T, 4> { using type = uint32_t; };
    template<typename T> struct to_unsigned_s<T, 8> { using type = uint64_t; };
    
    #define nto_unsigned(x) ncpp::to_unsigned_s<x, sizeof(x)>::type
}

#endif
