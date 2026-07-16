#ifndef NCPP_DEFER_HPP
#define NCPP_DEFER_HPP

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

#define ndefer defer

#endif
