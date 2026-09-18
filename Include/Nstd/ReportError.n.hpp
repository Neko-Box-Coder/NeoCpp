#ifndef NSTD_REPORT_ERROR_N_HPP
#define NSTD_REPORT_ERROR_N_HPP

#include <stdio.h>

#define NSTD_REPORT_ERROR() \
    printf("Error: \n    %s\nStack trace:\n", err.message); \
    for(int i = 0; i < err.traces_len; ++i) \
        printf("    at " n_trace_fmt_str() "\n", n_trace_fmt_args(err.traces[i]))

#endif
