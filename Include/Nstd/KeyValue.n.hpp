#ifndef NSTD_KEY_VALUE_N_HPP
#define NSTD_KEY_VALUE_N_HPP

/*
API:
```c++
template<typename T>
struct KeyValue
{
    n_view<const char> Key;
    T Value;
};
```

Usage:
```c++
{
    Nstd::KeyValue<int> kv = { "Test", 42 };
}
```
*/

#include "ncpp.n.hpp"

namespace Nstd
{
    template<typename T>
    struct KeyValue
    {
        n_view<const char> Key;
        T Value;
    };
}

#endif
