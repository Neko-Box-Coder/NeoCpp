#ifndef NSTD_VIEW_HPP
#define NSTD_VIEW_HPP

#include "ncpp.hpp"
#include <stdint.h>

namespace Nstd
{
    template<typename T>
    struct View
    {
        T* Data;
        uint64_t Len;
    };
}

#endif
