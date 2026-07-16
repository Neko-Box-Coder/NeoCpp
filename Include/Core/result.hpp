#ifndef NCPP_RESULT_HPP
#define NCPP_RESULT_HPP

/*
API:
`#define NCCP_NO_PATH 1` to not include file paths in trace
`#define NCPP_NO_DEBUG_BREAK 1` to not automatic break in debugger
`#define NCPP_ERR_CB your_error_callback(const char* msg, uint8_t mlen, uint8_t mcap, trace* ts, uint8_t tc, trace t)` to trigger a callback when an error has happened

```c++
struct error_info
{
    const char* message;
    uint8_t msg_len;
    uint8_t msg_cap;
    trace* traces;
    uint8_t traces_len;
    uint8_t traces_cap;
};

template<uint8_t MSG_CAP = 128, uint8_t TRACE_CAP = 64>
struct error_buffer
{
    char message[MSG_CAP];
    trace traces[TRACE_CAP];
};

template<typename T>
struct nresult
{
    T value;
    noptional<error_info> err;
    inline T& value_or(T val);
    inline T& value_or_default();
};
```

`nerror_msg(fmt, ...)`: Create an error message as nresult
`T nresult<T>.ntry()`: Try to get the result value, otherwise append trace and return error
`T nresult<T>.ntry_act(actions)`: Try to get the result value, otherwise perform `actions` where `error_info err` is available

`ncheck_true(op)`: Return error if `op == true` evaluates to false
`ncheck_false(op)`: Return error if `op == false` evaluates to false
`ncheck_eq(op, val)`: Return error if `op == val` evaluates to false
`ncheck_neq(op, val)`: Return error if `op != val` evaluates to false
`ncheck_gt(op, val)`: Return error if `op > val` evaluates to false
`ncheck_gte(op, val)`: Return error if `op >= val` evaluates to false
`ncheck_lt(op, val)`: Return error if `op < val` evaluates to false
`ncheck_lte(op, val)`: Return error if `op <= val` evaluates to false

`ncheck_true_fmt(op, fmt, ...)`: Same as `ncheck_true` but with custom error message
`ncheck_false_fmt(op, fmt, ...)`: Same as `ncheck_false` but with custom error message
`ncheck_eq_fmt(op, val, fmt, ...)`: Same as `ncheck_eq` but with custom error message
`ncheck_neq_fmt(op, val, fmt, ...)`: Same as `ncheck_neq` but with custom error message
`ncheck_gt_fmt(op, val, fmt, ...)`: Same as `ncheck_gt` but with custom error message
`ncheck_gte_fmt(op, val, fmt, ...)`: Same as `ncheck_gte` but with custom error message
`ncheck_lt_fmt(op, val, fmt, ...)`: Same as `ncheck_lt` but with custom error message
`ncheck_lte_fmt(op, val, fmt, ...)`: Same as `ncheck_lte` but with custom error message

---

Usage:
```c++
nresult<int> TestError(int v)
{
    (void)v;
    return nerror_msg("Test Error");
}

nresult<int> TestValue(int v)
{
    return {v};
}

nresult<int> TestNested(int v)
{
    int v2 = TestValue(v).ntry();
    return {v2};
}

nresult<int> TestCheck(int v)
{
    ncheck_eq(v, 5);
    return {v + 5};
}

nresult<int> TestCheckFmt(int v)
{
    ncheck_eq_fmt(v, 5, "v: %d", v);
    return {v + 5};
}

{
    #define PRINT_ERRORS() \
        do \
        { \
            printf("Error: \n    %s\nStack trace:\n", err.message); \
            for(int i = 0; i < err.traces_len; ++i) \
                printf(ntrace_fmt("    at ", err.traces[i], "\n")); \
            printf("\n"); \
        } while(0)
    
    int r = TestError(5).ntry_act(PRINT_ERRORS());
    r = TestNested(5).ntry_act(PRINT_ERRORS());
    r = TestCheck(r).ntry_act(PRINT_ERRORS());
    r = TestCheck(r).ntry_act(PRINT_ERRORS());
    r = TestCheckFmt(r).ntry_act(PRINT_ERRORS());
}
```

Output:
```
Error:
    Test Error
Stack trace:
    at Test.cpp:24 in TestError()
    at Test.cpp:121 in main()

Error:
    Expression "v == 5" has failed.
Stack trace:
    at Test.cpp:40 in TestCheck()
    at Test.cpp:127 in main()

Error:
    v: 0
Stack trace:
    at Test.cpp:46 in TestCheckFmt()
    at Test.cpp:129 in main()
```
*/


#include "./type.hpp"
#include "./printf.hpp"

#if NCCP_NO_PATH
    #define NCPP_PATH "(Private File)"
#else
    #define NCPP_PATH __FILE__
#endif

#if !NCPP_NO_DEBUG_BREAK
    #include "./External/debugbreak/debugbreak.h"
#endif

#include <stdint.h>
#include <string.h>

namespace
{
#if __cplusplus >= 202002L
    inline consteval const char* GetFileName(const char* path) 
#elif __cplusplus >= 201402L
    inline constexpr const char* GetFileName(const char* path) 
#else
    inline const char* GetFileName(const char* path) 
#endif
    {
        const char* lastSlash = path;
        const char* curr = path;
        while(*curr) 
        {
            if(*curr == '/' || *curr == '\\')
                lastSlash = curr + 1;
            ++curr;
        }
        return lastSlash;
    }
}

namespace ncpp
{
    struct trace
    {
        const char* function;
        const char* file;
        int line;
        
        #define ntrace() ncpp::trace { __func__, GetFileName(NCPP_PATH), __LINE__ }
        #define ntrace_fmt(prefix, trace, suffix) \
            prefix "%s:%d in %s()" suffix, trace.file, trace.line, trace.function
    };
    
    struct error_info
    {
        const char* message;
        uint8_t msg_len;
        uint8_t msg_cap;
        trace* traces;
        uint8_t traces_len;
        uint8_t traces_cap;
        
        void append_trace(trace t)
        {
            if(traces_len >= traces_cap)
                return;
            traces[traces_len++] = t;
        }
        
        inline static error_info create(const char* msg, 
                                        uint8_t mlen, 
                                        uint8_t mcap, 
                                        trace* ts, 
                                        uint8_t tc, 
                                        trace t)
        {
            #if !defined(NDEBUG) && !NCPP_NO_DEBUG_BREAK
                debug_break();
            #endif
            
            #if defined(NCPP_ERR_CB)
                NCPP_ERR_CB(msg, mlen, mcap, ts, tc, t);
            #endif
            
            ts[0] = t;
            return { msg, mlen, mcap, ts, 1, tc };
        }
        
        #ifndef NCPP_ERR_BUFFER
            #define NCPP_ERR_BUFFER ncpp::global_error_buffer
        #endif
        
        #define error_info_create(...) \
            ncpp::error_info::create(   ncpp::global_error_buffer.message, \
                                        (uint8_t)snprintf_( ncpp::global_error_buffer.message, \
                                                            128, \
                                                            __VA_ARGS__), \
                                        128, \
                                        ncpp::global_error_buffer.traces, \
                                        64, \
                                        ntrace())
    };
    
    template<uint8_t MSG_CAP = 128, uint8_t TRACE_CAP = 64>
    struct error_buffer
    {
        char message[MSG_CAP];
        trace traces[TRACE_CAP];
    };
    
    namespace
    {
        thread_local error_buffer<> global_error_buffer = {};
        thread_local noptional<error_info> global_error_info = {};
    }
    
    
    template<typename T>
    struct nresult
    {
        T value;
        noptional<error_info> err;
        
        inline void dummy()
        {
            (void) global_error_buffer;
            (void) &fctprintf;
            (void) &vsnprintf_;
            (void) &snprintf_;
            (void) &sprintf_;
        }
        
        inline T& value_or(T val)
        {
            if(err)
                value = val;
            return value;
        }
        
        inline T& value_or_default()
        {
            if(err)
                value = {};
            return value;
        }
        
        inline nresult& store_error_if_any()
        {
            if(err)
                global_error_info = *err;
            return *this;
        }
    };
    
    static_assert(nis_simple(nresult<int>), "");
    
    template<>
    struct nresult<void>
    {
        char c;
        noptional<error_info> err;
        
        inline void value_or_default()
        {
            return;
        }
        
        inline nresult& store_error_if_any()
        {
            if(err)
                global_error_info = *err;
            return *this;
        }
    };
    
    static_assert(nis_simple(nresult<void>), "");
    
    
    #define nerror_msg(...) { {}, error_info_create(__VA_ARGS__) }
    #define ntry_act(...) \
        store_error_if_any().value_or_default(); \
        do \
        { \
            if(ncpp::global_error_info) \
            { \
                error_info err = *ncpp::global_error_info; \
                err.append_trace(ntrace()); \
                ncpp::global_error_info = noptional<error_info> {}; \
                __VA_ARGS__; \
            } \
        } while(false)
    
    #define ntry() ntry_act(return { {}, err });

    #define INTERN_NCPP_CONCAT(a, b) a ## b
    #define INTERN_NCPP_COMPOSE(a, b) a b
    #define INTERN_NCPP_TEMP_NAME(name) INTERN_NCPP_COMPOSE(INTERN_NCPP_CONCAT, (name, __LINE__))
    
    #define INTERN_NCPP_STR(x) #x
    #define INTERN_NCPP_DELAY_STR(x) INTERN_NCPP_COMPOSE(INTERN_NCPP_STR, (x))
    
    #define INTERNAL_NCPP_ASSERT(left, op, right, ...) \
        do \
        { \
            auto INTERN_NCPP_TEMP_NAME(autoLeft) = left; \
            auto INTERN_NCPP_TEMP_NAME(autoRight) = right; \
            if(!(INTERN_NCPP_TEMP_NAME(autoLeft) op (ntypeof(INTERN_NCPP_TEMP_NAME(autoLeft)))INTERN_NCPP_TEMP_NAME(autoRight))) \
                return nerror_msg(__VA_ARGS__); \
        } \
        while(false)

    #define INTERN_NCPP_EXPR_STR(op_str) "Expression \"" op_str "\" has failed. "

    #define ncheck_true(op) INTERNAL_NCPP_ASSERT(op, ==, true, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " == true"))
    #define ncheck_false(op) INTERNAL_NCPP_ASSERT(op, ==, false, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " == false"))
    #define ncheck_eq(op, val) INTERNAL_NCPP_ASSERT(op, ==, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " == " INTERN_NCPP_DELAY_STR(val)))
    #define ncheck_neq(op, val) INTERNAL_NCPP_ASSERT(op, !=, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " != " INTERN_NCPP_DELAY_STR(val)))
    #define ncheck_gt(op, val) INTERNAL_NCPP_ASSERT(op, >, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " > " INTERN_NCPP_DELAY_STR(val)))
    #define ncheck_gte(op, val) INTERNAL_NCPP_ASSERT(op, >=, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " >= " INTERN_NCPP_DELAY_STR(val)))
    #define ncheck_lt(op, val) INTERNAL_NCPP_ASSERT(op, <, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " < " INTERN_NCPP_DELAY_STR(val)))
    #define ncheck_lte(op, val) INTERNAL_NCPP_ASSERT(op, <=, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " <= " INTERN_NCPP_DELAY_STR(val)))
    
    
    #define ncheck_true_fmt(op, ...) INTERNAL_NCPP_ASSERT(op, ==, true, __VA_ARGS__)
    #define ncheck_false_fmt(op, ...) INTERNAL_NCPP_ASSERT(op, ==, false, __VA_ARGS__)
    #define ncheck_eq_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, ==, val, __VA_ARGS__)
    #define ncheck_neq_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, !=, val, __VA_ARGS__)
    #define ncheck_gt_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, >, val, __VA_ARGS__)
    #define ncheck_gte_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, >=, val, __VA_ARGS__)
    #define ncheck_lt_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, <, val, __VA_ARGS__)
    #define ncheck_lte_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, <=, val, __VA_ARGS__)
}

#endif
