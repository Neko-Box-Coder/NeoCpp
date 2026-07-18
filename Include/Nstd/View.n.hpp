#ifndef NSTD_VIEW_N_HPP
#define NSTD_VIEW_N_HPP

#include "ncpp.n.hpp"
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
