#ifndef NCPP_N_STDINT_N_HPP
#define NCPP_N_STDINT_N_HPP

#include "./n_type.n.hpp"

#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

namespace ncpp
{
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
