#ifndef NCPP_N_MOVE_N_HPP
#define NCPP_N_MOVE_N_HPP

/*
API:
```c++
//Zeros out src and returns the value
template<typename T>
inline T n_move(n_ref T& src);
```

Usage:
```c++
{
    int a = 3;
    int b = n_move(n_ref a);
    printf("a: %d, b: %d\n", a, b);
}
```

Output:
```
a: 0, b: 3
```
*/

#include <string.h>

namespace ncpp
{
    template<typename T>
    inline T n_move(n_ref T& src)
    {
        T tmp = src;
        memset(&src, 0, sizeof(T));
        return tmp;
    }
}



#endif
