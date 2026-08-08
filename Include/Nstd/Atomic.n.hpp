#ifndef NSTD_ATOMIC_N_HPP
#define NSTD_ATOMIC_N_HPP

#include "ncpp.n.hpp"

#include "./External/c89atomic/c89atomic.h"

namespace Nstd
{
    enum MEMORY_ORDER
    {
        //(memory_order_relaxed)
        MEMORY_ORDER_NONE = 1,      //Only atomicity is guaranteed
        //(memory_order_acquire)
        MEMORY_ORDER_LOAD = 3,      //Use for loading, pair with MEMORY_ORDER_STORE. 
                                    //Values are synchronized before atomic is written on the thread 
                                    //that writes to the atomic, to this thread that reads from the 
                                    //atomic
        //(memory_order_release)
        MEMORY_ORDER_STORE = 4,     //Use for storing, pair with MEMORY_ORDER_LOAD.
                                    //Values are synchronized before atomic is written on this thread 
                                    //to threads that read from this atomic
        //(memory_order_acq_rel)
        MEMORY_ORDER_LOAD_STORE = 5,//Use for Read-Modify-Write, such as fetch-add
                                    //Values are synchronized before atomic is written on the thread 
                                    //that writes to the atomic. 
                                    //Values on this thread is also synchronized before atomic is 
                                    //written on this thread, to threads that read from this atomic
        //(memory_order_seq_cst)
        MEMORY_ORDER_GLOBAL = 6     //Global synchronizations.
                                    //Values are synchronized to all threads before atomic is written 
                                    //on this thread.
                                    //Suitable for values that depend on multi-atomic variables
    };
    
    template<typename T> 
    struct Atomic;
    
    #define INTERN_DECL_ATOMIC(type, typewidth, lockFree) \
        template<> \
        struct Atomic<type> \
        { \
            c89atomic_##type Value; \
            \
            static constexpr bool LockFree = lockFree; \
            \
            inline type Exchange(type replace, MEMORY_ORDER order = MEMORY_ORDER_GLOBAL) \
            { \
                return c89atomic_exchange_explicit_##typewidth( (c89atomic_##type*)&Value, \
                                                                (c89atomic_##type)replace, \
                                                                order); \
            } \
            \
            inline void Store(type store, MEMORY_ORDER order = MEMORY_ORDER_GLOBAL) \
            { \
                (void)Exchange(store, order); \
            } \
            \
            inline type Load(MEMORY_ORDER order = MEMORY_ORDER_GLOBAL) \
            { \
                return c89atomic_load_explicit_##typewidth((c89atomic_##type*)&Value, order); \
            } \
            \
            template<bool STRONG = true> \
            inline type StoreIfEqual(   type test,  \
                                        type replace,  \
                                        MEMORY_ORDER successOrder = MEMORY_ORDER_GLOBAL) \
                                        /*MEMORY_ORDER failOrder = MEMORY_ORDER_GLOBAL)*/ \
            { \
                if(STRONG) \
                { \
                    (void)  c89atomic_compare_exchange_strong_explicit_##typewidth \
                            ( \
                                (c89atomic_##type*)&Value,  \
                                (c89atomic_##type*)&test,  \
                                (c89atomic_##type)replace,  \
                                successOrder,  \
                                successOrder == MEMORY_ORDER_NONE ?  \
                                    MEMORY_ORDER_NONE : \
                                    MEMORY_ORDER_LOAD \
                            ); \
                } \
                else \
                { \
                    (void)  c89atomic_compare_exchange_weak_explicit_##typewidth \
                            ( \
                                (c89atomic_##type*)&Value,  \
                                (c89atomic_##type*)&test,  \
                                (c89atomic_##type)replace,  \
                                successOrder,  \
                                successOrder == MEMORY_ORDER_NONE ? \
                                    MEMORY_ORDER_NONE : \
                                    MEMORY_ORDER_LOAD \
                            ); \
                } \
                return test; \
            } \
            \
            template<bool STRONG = true> \
            inline type Add(type addVal, MEMORY_ORDER order = MEMORY_ORDER_GLOBAL) \
            { \
                return c89atomic_fetch_add_explicit_##typewidth((c89atomic_##type*)&Value, \
                                                                (c89atomic_##type)addVal, order) + \
                                                                addVal; \
            } \
            \
            template<bool STRONG = true> \
            inline type Sub(type subVal, MEMORY_ORDER order = MEMORY_ORDER_GLOBAL) \
            { \
                return c89atomic_fetch_sub_explicit_##typewidth((c89atomic_##type*)&Value, \
                                                                (c89atomic_##type)subVal, order) - \
                                                                subVal; \
            } \
            \
            template<bool STRONG = true> \
            inline type Or(type orVal, MEMORY_ORDER order = MEMORY_ORDER_GLOBAL) \
            { \
                return c89atomic_fetch_or_explicit_##typewidth( (c89atomic_##type*)&Value, \
                                                                (c89atomic_##type)orVal, order) | \
                                                                orVal; \
            } \
            \
            template<bool STRONG = true> \
            inline type Xor(type xorVal, MEMORY_ORDER order = MEMORY_ORDER_GLOBAL) \
            { \
                return c89atomic_fetch_xor_explicit_##typewidth((c89atomic_##type*)&Value, \
                                                                (c89atomic_##type)xorVal, order) ^ \
                                                                xorVal; \
            } \
            \
            template<bool STRONG = true> \
            inline type And(type andVal, MEMORY_ORDER order = MEMORY_ORDER_GLOBAL) \
            { \
                return c89atomic_fetch_xor_explicit_##typewidth((c89atomic_##type*)&Value, \
                                                                (c89atomic_##type)andVal, order) & \
                                                                andVal; \
            } \
        }
    
    #ifdef C89ATOMIC_IS_LOCK_FREE_8
        #define INTERN_LOCK_FREE_8 true
    #else
        #define INTERN_LOCK_FREE_8 false
    #endif
    
    #ifdef C89ATOMIC_IS_LOCK_FREE_16
        #define INTERN_LOCK_FREE_16 true
    #else
        #define INTERN_LOCK_FREE_16 false
    #endif
    
    #ifdef C89ATOMIC_IS_LOCK_FREE_32
        #define INTERN_LOCK_FREE_32 true
    #else
        #define INTERN_LOCK_FREE_32 false
    #endif
    
    #ifdef C89ATOMIC_IS_LOCK_FREE_64
        #define INTERN_LOCK_FREE_64 true
    #else
        #define INTERN_LOCK_FREE_64 false
    #endif
    
    INTERN_DECL_ATOMIC(uint8, 8, INTERN_LOCK_FREE_8);
    INTERN_DECL_ATOMIC(int8, 8, INTERN_LOCK_FREE_8);
    INTERN_DECL_ATOMIC(uint16, 16, INTERN_LOCK_FREE_16);
    INTERN_DECL_ATOMIC(int16, 16, INTERN_LOCK_FREE_16);
    INTERN_DECL_ATOMIC(uint32, 32, INTERN_LOCK_FREE_32);
    INTERN_DECL_ATOMIC(int32, 32, INTERN_LOCK_FREE_32);
    INTERN_DECL_ATOMIC(uint64, 64, INTERN_LOCK_FREE_64);
    INTERN_DECL_ATOMIC(int64, 64, INTERN_LOCK_FREE_64);
    
    #undef INTERN_LOCK_FREE_8
    #undef INTERN_LOCK_FREE_16
    #undef INTERN_LOCK_FREE_32
    #undef INTERN_LOCK_FREE_64
    #undef INTERN_DECL_ATOMIC
    
    struct AtomicSignal
    {
        c89atomic_flag Value;
        
        inline bool GetPreviousAndSignal(MEMORY_ORDER order = MEMORY_ORDER_GLOBAL)
        {
            return c89atomic_flag_test_and_set_explicit(&Value, order);
        }
        
        inline void Clear(MEMORY_ORDER order = MEMORY_ORDER_GLOBAL)
        {
            c89atomic_flag_clear_explicit(&Value, order);
        }
    };
}

#endif
