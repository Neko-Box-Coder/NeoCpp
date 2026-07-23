#ifndef NCPP_N_ASSERT_N_HPP
#define NCPP_N_ASSERT_N_HPP

#ifndef NCPP_FATAL
    #include <stdio.h>
    #include <stdlib.h>
    #include "./n_result.n.hpp"
    
    #if !defined(NDEBUG) && !NCPP_NO_DEBUG_BREAK
        #define NCPP_FATAL(...) do { printf(__VA_ARGS__); debug_break(); exit(1); } while(0)
    #else
        #define NCPP_FATAL(...) do { printf(__VA_ARGS__); exit(1); } while(0)
    #endif
    
#endif

#if NCPP_FORCE_ASSERT || !NDEBUG
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
#else
    #define n_assert(op) do {} while(0)
#endif


#endif
