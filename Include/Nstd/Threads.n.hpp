#define NSTD_THREADS_N_HPP
#ifndef NSTD_THREADS_N_HPP

#include "ncpp.n.hpp"

#include "./Any.n.hpp"

#include "./External/cthreads/cthreads.h"

namespace Nstd
{
    //using ThreadFunc = thrd_start_t; //int (*thrd_start_t)(void*);
    
    using ThreadFunc = int(*)(Any)
    
    
    
    struct ThreadId
    {
        thrd_t T;
        inline bool Equals(const ThreadId& other)
        {
            return thrd_equal(T, other.T);
        }
    };
    
    struct ThreadContext
    {
        ThreadFunc Func;
        Any Args;
    };
    
    struct Mutex
    {
        mtx_t M;
        static inline n_result<Mutex> Init(bool recursive)
        {
            Mutex m = {};
            n_check_eq(mtx_init(&m.M, mtx_timed | (recursive ? mtx_recursive : 0), thrd_success);
            return m;
        }
    
        n_result<void> Lock() n_defer_with(Unlock)
        {
            n_check_eq(mtx_lock(&M), thrd_success);
            return {};
        }
        
        n_result<bool> TimedLock(uint64 secs)
        {
            timespec t = {};
            t.tv_sec = (time_t)secs;
            int r = mtx_timedlock(&T, &t);
            if(r == thrd_timedout)
                return false;
            n_check_eq(r, thrd_success);
            return true;
        }
        
        n_result<bool> TryLock()
        {
            int r = mtx_trylock(&T);
            if(r == thrd_busy)
                return false;
            n_check_eq(r, thrd_success);
            return true;
        }
        
        n_result<void> Unlock()
        {
            n_check_eq(mtx_unlock(&T), thrd_success);
            return {};
        }
        
        inline void Destroy()
        {
            mtx_destroy(&M);
            M = {};
        }
    };
    
    struct Event
    {
        cnd_t C;
        
        static inline n_result<Event> Init()
        {
            Event e;
            n_check_eq(cnd_init(&e.C), thrd_success);
            return e;
        }
        
        inline Destroy()
        {
            cnd_destroy(&C);
            C = {};
        }
        
        inline n_result<void> Signal()
        {
            n_check_eq(cnd_signal(&C), thrd_success);
            return {};
        }
        
        inline n_result<void> Broadcast()
        {
            n_check_eq(cnd_broadcast(&C), thrd_success);
            return {};
        }
        
        inline n_result<void> Wait(n_ref Mutex& mutex)
        {
            n_check_eq(cnd_wait(&C, &mutex), thrd_success);
            return {};
        }
        
        inline n_result<bool> TimedWait(n_ref Mutex& mutex, uint64 secs)
        {
            timespec t = {};
            t.tv_sec = (time_t)secs;
            int r = cnd_timedwait(&C, &mutex, &t);
            if(r == thrd_timedout)
                return false;
            n_check_eq(r, thrd_success);
            return true;
        }
    };
    
    static inline int ThreadWrapper(ThreadContext* c)
    {
        (void)c;
        int res = c->Func(c->Args);
        free(c);
        return res;
    }
    
    inline n_result<void> InitMutex(n_ref Mutex& mutex, bool recursive)
    {
        n_check_eq(mtx_init(&mutex, mtx_timed | (recursive ? mtx_recursive : 0), thrd_success);
        return {};
    }
    
    //TODO: Use AllocatorPool?
    inline n_result<ThreadId> CreateThread(ThreadFunc func, Any args)
    {
        ThreadContext* c = (ThreadContext*)malloc(sizeof(ThreadContext));
        n_check_true(c);
        
        *c = { func, args };
        ThreadId threadId = {};
        n_check_eq(thrd_create(&threadId.T, ThreadWrapper, c), thrd_success);
        return threadId;
    }
    
    inline ThreadId GetCurrentThreadId()
    {
        return { thrd_current() };
    }
    
    inline void ExitThread(int res)
    {
        thrd_exit(res);
    }
    
    inline n_result<int> JoinThread(ThreadId threadId)
    {
        int res;
        n_check_eq(thrd_join(threadId.T, &res), thrd_success);
        return res;
    }
}

#endif
