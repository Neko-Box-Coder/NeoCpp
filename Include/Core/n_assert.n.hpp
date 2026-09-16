#ifndef NCPP_N_ASSERT_N_HPP
#define NCPP_N_ASSERT_N_HPP

/*
API:
```c++
#define n_assert(op)          //Assert with fatal error on failure (always-on)
#define n_assert_debug(op)    //Assert only in debug builds (no-op if NDEBUG defined or NCPP_NO_ASSERT is set)
```

Usage:
```c++
{
    int x = 5;
    n_assert(x > 0);            //Always checked
    n_assert_debug(x < 100);    //Only checked in debug builds
}
```
*/

#ifndef NCPP_FATAL
    #include <stdio.h>
    #include <stdlib.h>
    #include "./n_result.n.hpp"
    
    #if defined(NDEBUG) || NCPP_NO_DEBUG_BREAK
        #define NCPP_FATAL(...) do { printf(__VA_ARGS__); exit(1); } while(0)
    #else
        #define NCPP_FATAL(...) do { printf(__VA_ARGS__); debug_break(); exit(1); } while(0)
    #endif
    
#endif

#define n_assert(op) \
    do \
    { \
        if(!(op)) \
        { \
            NCPP_FATAL( "\"%s\" failed. " n_trace_fmt_str() "\n", \
                        INTERN_NCPP_DELAY_STR(op), \
                        n_trace_fmt_args(n_make_trace())); \
        } \
    } while(0)

#if NCPP_NO_ASSERT || defined(NDEBUG)
    #define n_assert_debug(op) do {} while(0)
#else
    #define n_assert_debug(op) n_assert(op)
#endif


#endif
