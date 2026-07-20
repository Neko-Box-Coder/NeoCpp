#ifndef NSTD_KEY_VALUE_N_HPP
#define NSTD_KEY_VALUE_N_HPP

#include "ncpp.n.hpp"

namespace Nstd
{
    template<typename T>
    struct KeyValue
    {
        nview<const char> Key;
        T Value;
    };
}

#endif
