#ifndef NCPP_NSTDINT_N_HPP
#define NCPP_NSTDINT_N_HPP

#include "./ntype.n.hpp"

#include <stdint.h>
#include <stddef.h>

namespace ncpp
{
    using uint8 = uint8_t;
    using int8 = int8_t;
    #define nuint8 ncpp::uint8
    #define nint8 ncpp::int8
    
    using uchar = unsigned char;
    using schar = signed char;
    #define nuchar ncpp::uchar
    #define nschar ncpp::schar
    
    using uint16 = uint16_t;
    using int16 = int16_t;
    #define nuint16 ncpp::uint16
    #define nint16 ncpp::int16
    
    using uint32 = uint32_t;
    using int32 = int32_t;
    #define nuint32 ncpp::uint32
    #define nint32 ncpp::int32
    
    using uint64 = uint64_t;
    using int64 = int64_t;
    #define nuint64 ncpp::uint64
    #define nint64 ncpp::int64
    
    using usize = nto_unsigned(size_t);
    using ssize = nto_signed(size_t);
}



#endif
