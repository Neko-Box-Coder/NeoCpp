#ifndef NSTD_DEFER_HPP
#define NSTD_DEFER_HPP

/*
Usage:
```c++
{
    defer { <Actions> }
    ...
    if(...)
    {
        //<Actions> called
        return;
    }
    
    //<Actions> called
    return
}
```
*/

#include "./External/Defer.hpp/Defer.hpp"


#endif
