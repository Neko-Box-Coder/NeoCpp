#ifndef NCPP_NMOVE_N_HPP
#define NCPP_NMOVE_N_HPP

/*
Usage:
```c++
{
    int a = 3;
    int b = nmove(nref a);
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
    inline T nmove(nref T& src)
    {
        T tmp = src;
        memset(&src, 0, sizeof(T));
        return tmp;
    }
}



#endif
