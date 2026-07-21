#ifndef NCPP_N_RESULT_N_HPP
#define NCPP_N_RESULT_N_HPP

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

struct n_error_info
{
    const char* message;
    uint8 msg_len;
    uint8 msg_cap;
    trace* traces;
    uint8 traces_len;
    uint8 traces_cap;

    inline void append_trace(trace t);
    inline usize string(char* mem, usize mem_len);
};

template<uint16 MSG_CAP = 1024, uint16 TRACE_CAP = 128, uint8 ERROR_CAP = 8>
struct error_buffer
{
    uint16 message_index;
    uint16 trace_index;
    uint8 error_index;
    char message[MSG_CAP];
    trace traces[TRACE_CAP];
    n_error_info errors[ERROR_CAP];
    
    static constexpr uint16 msg_cap = MSG_CAP;
    static constexpr uint16 trace_cap = TRACE_CAP;
    static constexpr uint8 error_cap = ERROR_CAP;
};

template<typename T>
struct n_result
{
    T value;
    n_error_info* err;
    inline T& value_or(T val);
    inline T& value_or_default();
};
```

`trace n_trace()`: Create a trace at the invoked location
`(printf args) n_trace_fmt([const char* prefix], trace t, [const char* suffix])`: Creates a formated 
    string arguments with `prefix` and `suffix` prepended before and appended to the formatted 
    string which can be used with printf, like so: `printf( n_trace_fmt("    at ", myTrace, "\n") );`

`n_result<T> nerror_msg(fmt, ...)`: Create an error message as n_result
`T n_result<T>.ntry()`: Try to get the result value, otherwise append trace and return error
`T n_result<T>.ntry_act(actions)`: Try to get the result value, otherwise perform `actions` where 
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
n_result<int> TestError(int v)
{
    nuse_error_defer();
    nerror_defer { printf("This should be called by nerror_defer\n"); };
    
    (void)v;
    return nerror_msg("Test Error");
}

n_result<int> TestValue(int v)
{
    return v;
}

n_result<int> TestNested(int v)
{
    nuse_error_defer();
    nerror_defer { printf("This should not be called by nerror_defer\n"); };
    
    int v2 = TestValue(v).ntry();
    return v2;
}

n_result<int> TestNestedError(int v)
{
    nuse_error_defer();
    nerror_defer { printf("This should be called by nerror_defer again\n"); };
    int v2 = TestError(v).ntry();
    return v2;
}

n_result<int> TestCheck(int v)
{
    ncheck_eq(v, 5);
    return v + 5;
}

n_result<int> TestCheckFmt(int v)
{
    ncheck_eq_fmt(v, 5, "v: %d", v);
    return v + 5;
}

{
    #define PRINT_STR_ERROR() err.string(msgMem, 256); printf("%s\n---------------\n", msgMem)
    char* msgMem = (char*)malloc(256);
    n_defer { free(msgMem); };
    
    int r = TestError(5).ntry_act(PRINT_STR_ERROR());
    r = TestNestedError(5).ntry_act(PRINT_STR_ERROR());
    r = TestNested(5).ntry_act(PRINT_STR_ERROR());
    r = TestCheck(r).ntry_act(PRINT_STR_ERROR());
    r = TestCheck(r).ntry_act(PRINT_STR_ERROR());
    r = TestCheckFmt(r).ntry_act(PRINT_STR_ERROR());
    
    //NOTE: Traces can be accessed inside `ntry_act()` with `err.traces`, where printf arguments
    //      for printing a single trace can be obtained with 
    //      `n_trace_fmt(print prefix, trace, print suffix)` and used like so 
    //      `printf(n_trace_fmt(...))`
    
    n_result<int> res = TestValue(3);
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


#include "./n_type.n.hpp"
#include "./n_defer.n.hpp"
#include "./External/printf.hpp"

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
    struct n_trace
    {
        const char* function;
        const char* file;
        int line;
        
        #define n_make_trace() ncpp::n_trace { __func__, GetFileName(NCPP_PATH), __LINE__ }
        #define n_trace_fmt(prefix, trace, suffix) \
            prefix "%s:%d in %s()" suffix, trace.file, trace.line, trace.function
    };
    
    namespace
    {
        thread_local volatile bool global_run_error_defer = false;
    }
    
    struct n_error_info
    {
        const char* message;
        uint16 msg_len;
        uint16 msg_cap;
        n_trace* traces;
        uint16 traces_len;
        uint16 traces_cap;
        
        inline void append_trace(n_trace t)
        {
            if(traces_len >= traces_cap)
                return;
            traces[traces_len++] = t;
        }
        
        template<uint16 TARGET_MSG_CAP>
        inline static n_error_info create(  const char* msg, 
                                            uint16 mlen, 
                                            uint16 max_mcap,
                                            n_trace* ts, 
                                            uint16 max_tc, 
                                            n_trace t)
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
        
        inline usize string(char* mem, usize mem_len)
        {
            if(!mem || !mem_len)
            {
                usize ret_len = snprintf_(NULL, 0, "Error: \n    %s\nStack trace:\n", message);
                for(int i = 0; i < traces_len; ++i)
                    ret_len += snprintf_(NULL, 0, n_trace_fmt("    at ", traces[i], "\n"));
                return ret_len;
            }
            else
            {
                usize wrote_len = snprintf_(mem, 
                                            mem_len, 
                                            "Error: \n    %s\nStack trace:\n", 
                                            message);
                for(int i = 0; i < traces_len; ++i)
                {
                    if(wrote_len >= mem_len - 1)
                        break;
                    wrote_len += snprintf_(  &mem[wrote_len], 
                                            mem_len - wrote_len, 
                                            n_trace_fmt("    at ", traces[i], "\n"));
                }
                return wrote_len;
            }
        }
        
        #define error_info_create(...) \
            ncpp::n_error_info::create \
            < \
                n_typeof(NCPP_ERR_BUFFER)::msg_cap / n_typeof(NCPP_ERR_BUFFER)::error_cap \
            > \
            ( \
                &NCPP_ERR_BUFFER.message[NCPP_ERR_BUFFER.message_index], \
                (uint16)snprintf_(&NCPP_ERR_BUFFER.message[NCPP_ERR_BUFFER.message_index], \
                                    NCPP_ERR_BUFFER.msg_cap - NCPP_ERR_BUFFER.message_index, \
                                    __VA_ARGS__), \
                NCPP_ERR_BUFFER.msg_cap - NCPP_ERR_BUFFER.message_index, \
                &NCPP_ERR_BUFFER.traces[NCPP_ERR_BUFFER.trace_index], \
                NCPP_ERR_BUFFER.trace_cap / NCPP_ERR_BUFFER.error_cap, \
                n_make_trace() \
            )
    };
    
    template<uint16 MSG_CAP = 1024, uint16 TRACE_CAP = 128, uint8 ERROR_CAP = 8>
    struct n_error_buffer
    {
        uint16 message_index;
        uint16 trace_index;
        uint8 error_index;
        char message[MSG_CAP];
        n_trace traces[TRACE_CAP];
        n_error_info errors[ERROR_CAP];
        
        static constexpr uint16 msg_cap = MSG_CAP;
        static constexpr uint16 trace_cap = TRACE_CAP;
        static constexpr uint8 error_cap = ERROR_CAP;
    };

    namespace
    {
        #if INTERN_NCPP_USE_DEFAULT_ERR_BUFFER
            thread_local n_error_buffer<> global_error_buffer = {};
        #endif
        thread_local n_error_info* global_error_info = NULL;
    }

    template<typename ERROR_BUFFER_T>
    inline n_error_info* store_error_info(n_ref ERROR_BUFFER_T& buf, n_error_info i)
    {
        buf.errors[buf.error_index] = i;
        n_error_info* ret_i = &buf.errors[buf.error_index];

        uint16 target_msg_cap = buf.msg_cap / buf.error_cap;
        uint16 target_msg_idx = buf.message_index + i.msg_cap;
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
    struct n_result
    {
        T value;
        n_error_info* err;
        
        inline n_result() = default;
        inline n_result(T val) { value = val; err = NULL; }
        inline n_result(T val, n_error_info* e) { value = val; err = e; }
        
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
        
        inline n_result& store_error_if_any()
        {
            if(err)
                global_error_info = err;
            return *this;
        }
    };
    
    static_assert(n_is_simple(n_result<int>), "");
    
    template<>
    struct n_result<void>
    {
        char c;
        n_error_info* err;
        
        inline void value_or_default()
        {
            return;
        }
        
        inline n_result& store_error_if_any()
        {
            if(err)
                global_error_info = err;
            return *this;
        }
    };
    
    static_assert(n_is_simple(n_result<void>), "");
    
    
    #define n_error_msg(...) { {}, store_error_info(n_ref NCPP_ERR_BUFFER, error_info_create(__VA_ARGS__)) }
    #define n_try_act(...) \
        store_error_if_any().value_or_default(); \
        do \
        { \
            if(ncpp::global_error_info) \
            { \
                ncpp::global_error_info->append_trace(n_make_trace()); \
                n_error_info& err = *ncpp::global_error_info; \
                ncpp::global_error_info = NULL; \
                global_run_error_defer = true; \
                __VA_ARGS__; \
            } \
        } while(false)
    
    #define n_try() n_try_act(return { {}, &err });
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
            if(!(INTERN_NCPP_TEMP_NAME(autoLeft) op (n_typeof(INTERN_NCPP_TEMP_NAME(autoLeft)))INTERN_NCPP_TEMP_NAME(autoRight))) \
                return n_error_msg(__VA_ARGS__); \
        } \
        while(false)

    #define INTERN_NCPP_EXPR_STR(op_str) "Expression \"" op_str "\" has failed. "

    #define n_check_true(op) INTERNAL_NCPP_ASSERT(op, !=, false, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " != false"))
    #define n_check_false(op) INTERNAL_NCPP_ASSERT(op, ==, false, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " == false"))
    #define n_check_eq(op, val) INTERNAL_NCPP_ASSERT(op, ==, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " == " INTERN_NCPP_DELAY_STR(val)))
    #define n_check_neq(op, val) INTERNAL_NCPP_ASSERT(op, !=, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " != " INTERN_NCPP_DELAY_STR(val)))
    #define n_check_gt(op, val) INTERNAL_NCPP_ASSERT(op, >, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " > " INTERN_NCPP_DELAY_STR(val)))
    #define n_check_gte(op, val) INTERNAL_NCPP_ASSERT(op, >=, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " >= " INTERN_NCPP_DELAY_STR(val)))
    #define n_check_lt(op, val) INTERNAL_NCPP_ASSERT(op, <, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " < " INTERN_NCPP_DELAY_STR(val)))
    #define n_check_lte(op, val) INTERNAL_NCPP_ASSERT(op, <=, val, INTERN_NCPP_EXPR_STR(INTERN_NCPP_DELAY_STR(op) " <= " INTERN_NCPP_DELAY_STR(val)))
    
    
    #define n_check_true_fmt(op, ...) INTERNAL_NCPP_ASSERT(op, !=, false, __VA_ARGS__)
    #define n_check_false_fmt(op, ...) INTERNAL_NCPP_ASSERT(op, ==, false, __VA_ARGS__)
    #define n_check_eq_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, ==, val, __VA_ARGS__)
    #define n_check_neq_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, !=, val, __VA_ARGS__)
    #define n_check_gt_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, >, val, __VA_ARGS__)
    #define n_check_gte_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, >=, val, __VA_ARGS__)
    #define n_check_lt_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, <, val, __VA_ARGS__)
    #define n_check_lte_fmt(op, val, ...) INTERNAL_NCPP_ASSERT(op, <=, val, __VA_ARGS__)


    #define n_use_error_defer() bool missing_nuse_error_defer = false; ncpp::global_run_error_defer = false; (void)missing_nuse_error_defer; n_defer { ncpp::global_run_error_defer = false; }
    
    //NOTE: Improvised from https://stackoverflow.com/a/42060129
    struct ErrorDeferDummy {};
    template <class T> struct ErrorDeferObj { T f; ~ErrorDeferObj() { if(global_run_error_defer) f(); } };
    template <class T> ErrorDeferObj<T> operator*(ErrorDeferDummy, T f) { return {f}; }
    #define n_error_defer missing_nuse_error_defer = true; auto INTERNAL_DEFER__(__COUNTER__) = ncpp::ErrorDeferDummy{} * [&]()
}

#endif
