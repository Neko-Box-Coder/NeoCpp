#ifndef NCPP_N_ASSERT_N_HPP
#define NCPP_N_ASSERT_N_HPP

#ifndef NCPP_FATAL
    #include <stdio.h>
    #include <stdlib.h>
    
    #define NCPP_FATAL(...) do { printf(__VA_ARGS__); exit(1); } while(0)
#endif

#if NCPP_FORCE_ASSERT || !NDEBUG
    #include "./n_result.n.hpp"
    #define n_assert(op) \
        do \
        { \
            if(!(op)) \
                NCPP_FATAL( n_trace_fmt(#op " failed. ", n_trace(), /* no suffix */)); \
        } while(0)
#else
    #define n_assert(op)
#endif


#endif
