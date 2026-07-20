#ifndef NCPP_NRESULT_N_HPP
#define NCPP_NRESULT_N_HPP

/*
API:
`#define NCCP_NO_PATH 1` to not include file paths in trace
`#define NCPP_NO_DEBUG_BREAK 1` to not automatic break in debugger
```
#define NCPP_ERR_CB(msg, msg_len, msg_cap, traces, traces_cap, trace) \
    your_error_callback(msg, msg_len, msg_cap, traces, traces_cap, trace)
```
to trigger a callback when an error has happened
`#define NCPP_ERR_BUFFER your_error_buffer` to set an alternative error buffer


```c++
struct trace
{
    const char* function;
    const char* file;
    int line;
};

struct error_info
{
    const char* message;
    uint8_t msg_len;
    uint8_t msg_cap;
    trace* traces;
    uint8_t traces_len;
    uint8_t traces_cap;

    inline void append_trace(trace t);
    inline size_t string(char* mem, size_t mem_len);
};

template<uint16_t MSG_CAP = 1024, uint16_t TRACE_CAP = 128, uint8_t ERROR_CAP = 8>
struct error_buffer
{
    uint16_t message_index;
    uint16_t trace_index;
    uint8_t error_index;
    char message[MSG_CAP];
    trace traces[TRACE_CAP];
    error_info errors[ERROR_CAP];
    
    static constexpr uint16_t msg_cap = MSG_CAP;
    static constexpr uint16_t trace_cap = TRACE_CAP;
    static constexpr uint8_t error_cap = ERROR_CAP;
};

template<typename T>
struct nresult
{
    T value;
    error_info* err;
    inline T& value_or(T val);
    inline T& value_or_default();
};
```

`trace ntrace()`: Create a trace at the invoked location
`(printf args) ntrace_fmt([const char* prefix], trace t, [const char* suffix])`: Creates a formated 
    string arguments with `prefix` and `suffix` prepended before and appended to the formatted 
    string which can be used with printf, like so: `printf( ntrace_fmt("    at ", myTrace, "\n") );`

`nresult<T> nerror_msg(fmt, ...)`: Create an error message as nresult
`T nresult<T>.ntry()`: Try to get the result value, otherwise append trace and return error
`T nresult<T>.ntry_act(actions)`: Try to get the result value, otherwise perform `actions` where 
    `error_info& err` is available

`ncheck_true(op)`: Return error if `op == true` evaluates to false
`ncheck_false(op)`: Return error if `op != true` evaluates to false
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

`void nuse_error_defer()`: Indicates the caller function will use `nerror_defer` later
`nerror_defer { <actions> }`: Defer action that only triggers when returning an error

---

Usage:
```c++
nresult<int> TestError(int v)
{
    nuse_error_defer();
    nerror_defer { printf("This should be called by nerror_defer\n"); };
    
    (void)v;
    return nerror_msg("Test Error");
}

nresult<int> TestValue(int v)
{
    return v;
}

nresult<int> TestNested(int v)
{
    nuse_error_defer();
    nerror_defer { printf("This should not be called by nerror_defer\n"); };
    
    int v2 = TestValue(v).ntry();
    return v2;
}

nresult<int> TestNestedError(int v)
{
    nuse_error_defer();
    nerror_defer { printf("This should be called by nerror_defer again\n"); };
    int v2 = TestError(v).ntry();
    return v2;
}

nresult<int> TestCheck(int v)
{
    ncheck_eq(v, 5);
    return v + 5;
}

nresult<int> TestCheckFmt(int v)
{
    ncheck_eq_fmt(v, 5, "v: %d", v);
    return v + 5;
}

{
    #define PRINT_STR_ERROR() err.string(msgMem, 256); printf("%s\n---------------\n", msgMem)
    char* msgMem = (char*)malloc(256);
    ndefer { free(msgMem); };
    
    int r = TestError(5).ntry_act(PRINT_STR_ERROR());
    r = TestNestedError(5).ntry_act(PRINT_STR_ERROR());
    r = TestNested(5).ntry_act(PRINT_STR_ERROR());
    r = TestCheck(r).ntry_act(PRINT_STR_ERROR());
    r = TestCheck(r).ntry_act(PRINT_STR_ERROR());
    r = TestCheckFmt(r).ntry_act(PRINT_STR_ERROR());
    
    //NOTE: Traces can be accessed inside `ntry_act()` with `err.traces`, where printf arguments
    //      for printing a single trace can be obtained with 
    //      `ntrace_fmt(print prefix, trace, print suffix)` and used like so 
    //      `printf(ntrace_fmt(...))`
    
    nresult<int> res = TestValue(3);
    r = res.value;
    bool hasError = res.err;
    if(hasError)
    {
        error_info errInfo = *res.err;
        (void)errInfo;
    }
    r = res.value_or(3);
    r = res.value_or_default();
}
```

Output:
```
This should be called by nerror_defer
Error:
    Test Error
Stack trace:
    at Test.cpp:28 in TestError()
    at Test.cpp:131 in Main()

---------------
This should be called by nerror_defer
This should be called by nerror_defer again
Error:
    Test Error
Stack trace:
    at Test.cpp:28 in TestError()
    at Test.cpp:49 in TestNestedError()
    at Test.cpp:132 in Main()

---------------
Error:
    Expression "v == 5" has failed.
Stack trace:
    at Test.cpp:55 in TestCheck()
    at Test.cpp:135 in Main()

---------------
Error:
    v: 0
Stack trace:
    at Test.cpp:61 in TestCheckFmt()
    at Test.cpp:136 in Main()

---------------
```
*/


#include "./ntype.n.hpp"
#include "./printf.hpp"
#include "./ndefer.n.hpp"

#if NCCP_NO_PATH
    #define NCPP_PATH "(Private File)"
#else
    #define NCPP_PATH __FILE__
#endif

#if !NCPP_NO_DEBUG_BREAK
    #include "./External/debugbreak/debugbreak.h"
#endif

#ifndef NCPP_ERR_BUFFER
    #define NCPP_ERR_BUFFER ncpp::global_error_buffer
    #define INTERN_NCPP_USE_DEFAULT_ERR_BUFFER 1
#else
    #define INTERN_NCPP_USE_DEFAULT_ERR_BUFFER 0
#endif

#ifndef NCPP_ERR_CB
    #define NCPP_ERR_CB(msg, mlen, mcap, ts, tc, t)
#endif

#include <stdint.h>
#include <string.h>
#include <assert.h>

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
    
    namespace
    {
        thread_local volatile bool global_run_error_defer = false;
    }
    
    struct error_info
    {
        const char* message;
        uint16_t msg_len;
        uint16_t msg_cap;
        trace* traces;
        uint16_t traces_len;
        uint16_t traces_cap;
        
        inline void append_trace(trace t)
        {
            if(traces_len >= traces_cap)
                return;
            traces[traces_len++] = t;
        }
        
        template<uint16_t TARGET_MSG_CAP>
        inline static error_info create(const char* msg, 
                                        uint16_t mlen, 
                                        uint16_t max_mcap,
                                        trace* ts, 
                                        uint16_t max_tc, 
                                        trace t)
        {
            #if !defined(NDEBUG) && !NCPP_NO_DEBUG_BREAK
                debug_break();
            #endif
            
            if(max_mcap > TARGET_MSG_CAP && mlen + 1 < TARGET_MSG_CAP)
                max_mcap = TARGET_MSG_CAP;
            else
                max_mcap = mlen + 1;
            
            NCPP_ERR_CB(msg, mlen, max_mcap, ts, max_tc, t);
            
            global_run_error_defer = true;
            
            ts[0] = t;
            return { msg, mlen, max_mcap, ts, 1, max_tc };
        }
        
        inline size_t string(char* mem, size_t mem_len)
        {
            if(!mem || !mem_len)
            {
                size_t ret_len = snprintf_(NULL, 0, "Error: \n    %s\nStack trace:\n", message);
                for(int i = 0; i < traces_len; ++i)
                    ret_len += snprintf_(NULL, 0, ntrace_fmt("    at ", traces[i], "\n"));
                return ret_len;
            }
            else
            {
                size_t wrote_len = snprintf_(mem, 
                                            mem_len, 
                                            "Error: \n    %s\nStack trace:\n", 
                                            message);
                for(int i = 0; i < traces_len; ++i)
                {
                    if(wrote_len >= mem_len - 1)
                        break;
                    wrote_len += snprintf_(  &mem[wrote_len], 
                                            mem_len - wrote_len, 
                                            ntrace_fmt("    at ", traces[i], "\n"));
                }
                return wrote_len;
            }
        }
        
        #define error_info_create(...) \
            ncpp::error_info::create \
            < \
                ntypeof(NCPP_ERR_BUFFER)::msg_cap / ntypeof(NCPP_ERR_BUFFER)::error_cap \
            > \
            ( \
                &NCPP_ERR_BUFFER.message[NCPP_ERR_BUFFER.message_index], \
                (uint16_t)snprintf_(&NCPP_ERR_BUFFER.message[NCPP_ERR_BUFFER.message_index], \
                                    NCPP_ERR_BUFFER.msg_cap - NCPP_ERR_BUFFER.message_index, \
                                    __VA_ARGS__), \
                NCPP_ERR_BUFFER.msg_cap - NCPP_ERR_BUFFER.message_index, \
                &NCPP_ERR_BUFFER.traces[NCPP_ERR_BUFFER.trace_index], \
                NCPP_ERR_BUFFER.trace_cap / NCPP_ERR_BUFFER.error_cap, \
                ntrace() \
            )
    };
    
    template<uint16_t MSG_CAP = 1024, uint16_t TRACE_CAP = 128, uint8_t ERROR_CAP = 8>
    struct error_buffer
    {
        uint16_t message_index;
        uint16_t trace_index;
        uint8_t error_index;
        char message[MSG_CAP];
        trace traces[TRACE_CAP];
        error_info errors[ERROR_CAP];
        
        static constexpr uint16_t msg_cap = MSG_CAP;
        static constexpr uint16_t trace_cap = TRACE_CAP;
        static constexpr uint8_t error_cap = ERROR_CAP;
    };
    
    namespace
    {
        #if INTERN_NCPP_USE_DEFAULT_ERR_BUFFER
            thread_local error_buffer<> global_error_buffer = {};
        #endif
        thread_local error_info* global_error_info = NULL;
    }
    
    template<typename ERROR_BUFFER_T>
    inline error_info* store_error_info(nref ERROR_BUFFER_T& buf, error_info i)
    {
        buf.errors[buf.error_index] = i;
        error_info* ret_i = &buf.errors[buf.error_index];
        
        uint16_t target_msg_cap = buf.msg_cap / buf.error_cap;
        uint16_t target_msg_idx = buf.message_index + i.msg_cap;
        target_msg_idx = (target_msg_idx + (target_msg_cap - 1)) / target_msg_cap * target_msg_cap;
        if(target_msg_idx > buf.msg_cap)
            target_msg_idx = buf.message_index + i.msg_cap;
        
        assert(target_msg_idx - buf.message_index >= i.msg_cap);
        i.msg_cap = target_msg_idx - buf.message_index;
        buf.message_index = target_msg_idx;
        buf.trace_index += i.traces_cap;
        ++buf.error_index;
        
        if(buf.message_index >= buf.msg_cap)
            buf.message_index = 0;
        if(buf.trace_index >= buf.trace_cap)
            buf.trace_index = 0;
        if(buf.error_index >= buf.error_cap)
            buf.error_index = 0;
        
        return ret_i;
    }
    
    template<typename T>
    struct nresult
    {
        T value;
        error_info* err;
        
        inline nresult() = default;
        inline nresult(T val) { value = val; err = NULL; }
        inline nresult(T val, error_info* e) { value = val; err = e; }
        
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
                global_error_info = err;
            return *this;
        }
    };
    
    static_assert(nis_simple(nresult<int>), "");
    
    template<>
    struct nresult<void>
    {
        char c;
        error_info* err;
        
        inline void value_or_default()
        {
            return;
        }
        
        inline nresult& store_error_if_any()
        {
            if(err)
                global_error_info = err;
            return *this;
        }
    };
    
    static_assert(nis_simple(nresult<void>), "");
    
    
    #define nerror_msg(...) { {}, store_error_info(nref NCPP_ERR_BUFFER, error_info_create(__VA_ARGS__)) }
    #define ntry_act(...) \
        store_error_if_any().value_or_default(); \
        do \
        { \
            if(ncpp::global_error_info) \
            { \
                ncpp::global_error_info->append_trace(ntrace()); \
                error_info& err = *ncpp::global_error_info; \
                ncpp::global_error_info = NULL; \
                global_run_error_defer = true; \
                __VA_ARGS__; \
            } \
        } while(false)
    
    #define ntry() ntry_act(return { {}, &err });
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

    #define ncheck_true(op) INTERNAL_NCPP_ASSERT(op, !=, false, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " != false"))
    #define ncheck_false(op) INTERNAL_NCPP_ASSERT(op, ==, false, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " == false"))
    #define ncheck_eq(op, val) INTERNAL_NCPP_ASSERT(op, ==, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " == " INTERN_NCPP_DELAY_STR(val)))
    #define ncheck_neq(op, val) INTERNAL_NCPP_ASSERT(op, !=, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " != " INTERN_NCPP_DELAY_STR(val)))
    #define ncheck_gt(op, val) INTERNAL_NCPP_ASSERT(op, >, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " > " INTERN_NCPP_DELAY_STR(val)))
    #define ncheck_gte(op, val) INTERNAL_NCPP_ASSERT(op, >=, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " >= " INTERN_NCPP_DELAY_STR(val)))
    #define ncheck_lt(op, val) INTERNAL_NCPP_ASSERT(op, <, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " < " INTERN_NCPP_DELAY_STR(val)))
    #define ncheck_lte(op, val) INTERNAL_NCPP_ASSERT(op, <=, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " <= " INTERN_NCPP_DELAY_STR(val)))
    
    
    #define ncheck_true_fmt(op, ...) INTERNAL_NCPP_ASSERT(op, !=, false, __VA_ARGS__)
    #define ncheck_false_fmt(op, ...) INTERNAL_NCPP_ASSERT(op, ==, false, __VA_ARGS__)
    #define ncheck_eq_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, ==, val, __VA_ARGS__)
    #define ncheck_neq_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, !=, val, __VA_ARGS__)
    #define ncheck_gt_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, >, val, __VA_ARGS__)
    #define ncheck_gte_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, >=, val, __VA_ARGS__)
    #define ncheck_lt_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, <, val, __VA_ARGS__)
    #define ncheck_lte_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, <=, val, __VA_ARGS__)


    #define nuse_error_defer() bool missing_nuse_error_defer = false; ncpp::global_run_error_defer = false; (void)missing_nuse_error_defer; ndefer { ncpp::global_run_error_defer = false; }
    
    //NOTE: Improvised from https://stackoverflow.com/a/42060129
    struct ErrorDeferDummy {};
    template <class T> struct ErrorDeferObj { T f; ~ErrorDeferObj() { if(global_run_error_defer) f(); } };
    template <class T> ErrorDeferObj<T> operator*(ErrorDeferDummy, T f) { return {f}; }
    #define nerror_defer missing_nuse_error_defer = true; auto INTERNAL_DEFER__(__COUNTER__) = ncpp::ErrorDeferDummy{} * [&]()
}

#endif
