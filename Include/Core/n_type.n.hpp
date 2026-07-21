#ifndef NCPP_N_TYPE_N_HPP
#define NCPP_N_TYPE_N_HPP

#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

namespace ncpp
{
    template<bool V>
    struct bool_const_s
    {
        static constexpr bool value = V;
    };
    #define n_bool_const(V) ncpp::bool_const_s<V>::value

    template<typename T, typename U>
    struct is_same_s { static constexpr bool value = false; };
    template<typename T>
    struct is_same_s<T, T> { static constexpr bool value = true; };
    #define n_is_same(T, U) ncpp::is_same_s<T, U>::value
    
    template<bool B> struct enable_if_s { };
    template<> struct enable_if_s<true> { using type = bool; };
    #define n_enable_if(expr) typename ncpp::enable_if_s<expr>::type = true
    
    #if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
        #if __cplusplus < 202002L
            #define n_is_simple(T) n_bool_const(__is_pod(T))
        #else
            #define n_is_simple(T)  n_bool_const(__is_standard_layout(T)) && \
                                    n_bool_const(__is_trivial(T))
        #endif
    #else
        #if __cplusplus < 202002L
            #define n_is_simple(T) n_bool_const(__is_pod(T))                      //NOTE: We are assuming compiler hook here
        #else
            #define n_is_simple(T)  n_bool_const(__is_standard_layout(T)) && \  //NOTE: We are assuming compiler hook here
                                    n_bool_const(__is_trivial(T))
        #endif
    #endif
    
    #define n_typeof(x) decltype(x)
    
    template<typename T> struct no_ref_s { typedef T type; };
    template<typename T> struct no_ref_s<T&> { typedef T type; };
    template<typename T> struct no_ref_s<T&&> { typedef T type; };
    #define n_no_ref(t) typename ncpp::no_ref_s<t>::type

    template<typename T> struct no_const_s { typedef T type; };
    template<typename T> struct no_const_s<const T> { typedef T type; };
    #define n_no_const(t) typename ncpp::no_const_s<t>::type
    
    template<typename T> struct no_volatile_s { typedef T type; };
    template<typename T> struct no_volatile_s<volatile T> { typedef T type; };
    #define n_no_volatie(t) typename ncpp::no_volatile_s<t>::type
    
    #define n_no_cv(t) n_no_volatie(n_no_const(t))
    #define n_no_cvr(t) n_no_ref(n_no_volatie(n_no_const(t)))

    template<typename T> struct is_int_type_s 
    { 
        static constexpr bool value =   n_is_same(n_no_cvr(T), uint8_t) ||
                                        n_is_same(n_no_cvr(T), int8_t) ||
                                        n_is_same(n_no_cvr(T), uint16_t) ||
                                        n_is_same(n_no_cvr(T), int16_t) ||
                                        n_is_same(n_no_cvr(T), uint32_t) ||
                                        n_is_same(n_no_cvr(T), int32_t) ||
                                        n_is_same(n_no_cvr(T), uint64_t) ||
                                        n_is_same(n_no_cvr(T), int64_t);
    };
    #define n_is_int_type(t) ncpp::is_int_type_s<t>::value
    
    template<typename T, size_t> struct to_signed_s_3 {};
    template<typename T> struct to_signed_s_3<T, 1> { using type = int8_t; };
    template<typename T> struct to_signed_s_3<T, 2> { using type = int16_t; };
    template<typename T> struct to_signed_s_3<T, 4> { using type = int32_t; };
    template<typename T> struct to_signed_s_3<T, 8> { using type = int64_t; };
    template<typename T, n_enable_if(n_is_int_type(n_no_cvr(T)))> 
    struct to_signed_s_2 { using type = typename to_signed_s_3<n_no_cvr(T), sizeof(T)>::type; };
    #define n_to_signed(x) typename ncpp::to_signed_s_2<x>::type


    template<typename T, size_t> struct to_unsigned_s_3 {};
    template<typename T> struct to_unsigned_s_3<T, 1> { using type = uint8_t; };
    template<typename T> struct to_unsigned_s_3<T, 2> { using type = uint16_t; };
    template<typename T> struct to_unsigned_s_3<T, 4> { using type = uint32_t; };
    template<typename T> struct to_unsigned_s_3<T, 8> { using type = uint64_t; };
    template<typename T, n_enable_if(n_is_int_type(n_no_cvr(T)))> 
    struct to_unsigned_s_2 { using type = typename to_unsigned_s_3<n_no_cvr(T), sizeof(T)>::type; };
    #define n_to_unsigned(x) typename ncpp::to_unsigned_s_2<x>::type


    using uint8 = uint8_t;
    using int8 = int8_t;
    #define n_uint8 ncpp::uint8
    #define n_int8 ncpp::int8
    
    using uchar = unsigned char;
    using schar = signed char;
    #define n_uchar ncpp::uchar
    #define n_schar ncpp::schar
    
    using uint16 = uint16_t;
    using int16 = int16_t;
    #define n_uint16 ncpp::uint16
    #define n_int16 ncpp::int16
    
    using uint32 = uint32_t;
    using int32 = int32_t;
    #define n_uint32 ncpp::uint32
    #define n_int32 ncpp::int32
    
    using uint64 = uint64_t;
    using int64 = int64_t;
    #define n_uint64 ncpp::uint64
    #define n_int64 ncpp::int64
    
    using usize = n_to_unsigned(size_t);
    using ssize = n_to_signed(size_t);
    #define n_usize ncpp::usize
    #define n_ssize ncpp::ssize
}

#endif
