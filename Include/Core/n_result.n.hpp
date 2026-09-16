#ifndef NCPP_N_RESULT_N_HPP
#define NCPP_N_RESULT_N_HPP

/* API: Error handling with trace stacks + check macros.

Config:
  `#define NCCP_NO_PATH 1`       Hide file paths in traces
  `#define NCPP_NO_DEBUG_BREAK 1` Disable auto-debugger break on error
  `#define NCPP_ERR_CB(msg, mlen, mcap, ts, tc, t) ...`  Custom callback fired when an error occurs
  `#define NCPP_ERR_BUFFER ...`  Alternate global error buffer (n_error_buffer)

```c++
struct n_trace
{
    const char* function;
    const char* file;
    int line;
};

const char* n_trace_fmt_str();      //format string for printf like functions: "%s:%d in %s()"
args... n_trace_fmt_args(trace);    //Arguments for the above format for printf like functions

n_trace n_make_trace();             //Create trace at call site (function, file, line)
void n_trace_printf_track();        //Print current location immediately using printf

struct n_error_info
{
    const char* message;
    uint8 msg_len;
    uint8 msg_cap;
    n_trace* traces;
    uint8 traces_len;
    uint8 traces_cap;

    inline void append_trace(n_trace t);
    inline usize string(char* mem, usize mem_len);
};

template<uint16 MSG_CAP = 128, uint16 TRACE_CAP = 16, uint8 ERROR_CAP = 1>
struct n_error_buffer
{
    //Message/trace/error pools with indices
};

template<typename T>
struct n_result
{
    T value;           //Result value (valid when err == NULL)
    n_error_info* err; //Error info or NULL on success

    inline T& value_or(T val);       //Return value or fallback
    inline T& value_or_default();    //Return value or default-constructed T

    T& n_try();             //Returns value if no error, otherwise append trace and return in caller
    T& n_try_act(actions);  //Same as n_try(), except actions are invoked on error, with access to 
                            //n_error_info& err
};

//Returns error in caller if condition fails
n_check_true(cond);     //Fail if cond == false
n_check_false(cond);    //Fail if cond != false
n_check_eq(val, exp);   //Fail if val != exp
n_check_neq(val, exp);  //Fail if val == exp
n_check_gt(val, min);   //Fail if val <= min
n_check_gte(val, min);  //Fail if val < min
n_check_lt(val, max);   //Fail if val >= max
n_check_lte(val, max);  //Fail if val > max

//Same as above but with printf style arguments as custom error message
n_check_true_fmt(cond, fmt, ...);
n_check_false_fmt(cond, fmt, ...);
n_check_eq_fmt(val, exp, fmt, ...);
n_check_neq_fmt(val, exp, fmt, ...);
n_check_gt_fmt(val, min, fmt, ...);
n_check_gte_fmt(val, min, fmt, ...);
n_check_lt_fmt(val, max, fmt, ...);
n_check_lte_fmt(val, max, fmt, ...);

//Deferred error-only cleanup:
void n_use_error_defer();   //Declare caller uses n_error_defer
n_error_defer { actions };  //Actions run only when an error (n_check_*() / n_try*()) happens
```

Usage:
```c++
n_result<int> Divide(int a, int b)
{
    n_check_gt(b, 0, "b must be positive");
    return a / b;
}

int result = Divide(10, 2).n_try();
```
*/

#include "./n_type.n.hpp"
#include "./n_defer.n.hpp"
#include "./n_lint.n.hpp"
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
        
        #define n_trace_fmt_str() "%s:%d in %s()"
        #define n_trace_fmt_args(trace) trace.file, trace.line, trace.function
        
        #define n_trace_printf_track() printf(n_trace_fmt_str() "\n", n_trace_fmt_args(n_make_trace()))
    };
    
    namespace
    {
        thread_local volatile bool global_run_error_defer = false;
    }
    
    struct n_error_info
    {
        char* message;
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
        inline static n_error_info create(  char* msg, 
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
                {
                    ret_len += snprintf_(   NULL, 
                                            0, 
                                            "    at " n_trace_fmt_str() "\n", 
                                            n_trace_fmt_args(traces[i]));
                    
                }
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
                    wrote_len += snprintf_( &mem[wrote_len], 
                                            mem_len - wrote_len, 
                                            "    at " n_trace_fmt_str() "\n",
                                            n_trace_fmt_args(traces[i]));
                }
                return wrote_len;
            }
        }
        
        //TODO: Change this to use the {} syntax instead
        #define error_info_create(...) \
            ncpp::n_error_info::create \
            < \
                n_typeof(NCPP_ERR_BUFFER)::msg_cap / n_typeof(NCPP_ERR_BUFFER)::error_cap \
            > \
            ( \
                &NCPP_ERR_BUFFER.message[NCPP_ERR_BUFFER.message_index], \
                (uint16)snprintf_(  &NCPP_ERR_BUFFER.message[NCPP_ERR_BUFFER.message_index], \
                                    NCPP_ERR_BUFFER.msg_cap - NCPP_ERR_BUFFER.message_index, \
                                    __VA_ARGS__), \
                NCPP_ERR_BUFFER.msg_cap - NCPP_ERR_BUFFER.message_index, \
                &NCPP_ERR_BUFFER.traces[NCPP_ERR_BUFFER.trace_index], \
                NCPP_ERR_BUFFER.trace_cap / NCPP_ERR_BUFFER.error_cap, \
                n_make_trace() \
            )
    };
    
    template<uint16 MSG_CAP = 128, uint16 TRACE_CAP = 16, uint8 ERROR_CAP = 1>
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
    static_assert(n_is_simple(n_error_buffer<>));

    namespace
    {
        //TODO: Maybe need to have it once in a single n.cpp file?
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
        inline n_result(n_error_info& e) { err = &e; }
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
        
        inline n_result() = default;
        inline n_result(char, n_error_info* e) { err = e; }
        inline n_result(n_error_info& e) { err = &e; }
        
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
                n_error_info& err = *ncpp::global_error_info; (void)err; \
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
