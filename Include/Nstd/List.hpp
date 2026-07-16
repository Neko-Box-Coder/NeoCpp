#ifndef NSTD_LIST_HPP
#define NSTD_LIST_HPP

/*
Usage:
```c++
{
    Nstd::Allocator alloc = alloc.Init<int, Nstd::HeapAllocator>(32);
    Nstd::List<int> list = list.Init(alloc, 4); //Initial size of 4
    for(int i = 0; i < list.Len; ++i)
        list.Data[i] = i;
    list.Add(4);
    list.Reserve(7);
    list.Insert(5, 4);
    list.Remove(4);
    
    for(int i = 0; i < list.Len; ++i)
        printf("list.At(%d): %d\n", i, list.At(i));
    
    list = list.NSTD_INIT_VALUES(alloc, 1, 2, 3);
    for(int i = 0; i < list.Len; ++i)
        printf("list.At(%d): %d\n", i, list.At(i));
}
```

Output:
```
list.At(0): 0
list.At(1): 1
list.At(2): 2
list.At(3): 3
list.At(4): 4
list.At(0): 1
list.At(1): 2
list.At(2): 3
```
*/

#include "ncpp.hpp"

#include "./Allocator.hpp"
#include "./External/MacroPowerToys/ArgsCount.h"
#include "./View.hpp"

#include <stdint.h>
#include <stdarg.h>

namespace Nstd
{
    template<typename T, nenable_if(nis_simple(T))>
    struct List
    {
        Allocator* Alloc;
        T Dummy;
        T* Data;
        uint64_t Len;
        uint64_t Cap;
        
        inline List Init(nref Allocator& alloc, uint64_t initSize)
        {
            List retList;
            retList.Alloc = &alloc;
            retList.Dummy = {};
            retList.Data = alloc.Malloc<T>(initSize);
            retList.Len = initSize;
            retList.Cap = initSize;
            return retList;
        }
        
        inline List InitValues(nref Allocator& alloc, uint8_t count,  ...)
        {
            List<T> l = Init(alloc, count);
            
            va_list args;
            va_start(args, count);
            for (int i = 0; i < count; ++i)
                l.Data[i] = va_arg(args, T);
            va_end(args);
            return l;
        }
        
        #ifndef NSTD_INIT_VALUES
            #define NSTD_INIT_VALUES(alloc, ...) InitValues(alloc, MPT_ARGS_COUNT(__VA_ARGS__), __VA_ARGS__ )
        #endif
        
        inline T& At(uint64_t index)
        {
            if(index >= Len)
                return Dummy;
            else
                return Data[index];
        }
        
        inline const T& At(uint64_t index) const
        {
            if(index >= Len)
                return Dummy;
            else
                return Data[index];
        }
        
        inline nresult<void> Reserve(uint64_t size)
        {
            ncheck_true(Alloc);
            if(size <= Cap)
                return {};
            
            T* tmp = Alloc->Realloc(Data, size);
            if(!tmp)
                return nerror_msg("%s", "Failed to realloc");
            else
            {
                Data = tmp;
                Cap = size;
            }
            return {};
        }
        
        inline nresult<void> ReserveAhead(uint64_t size)
        {
            uint64_t reserveLen;
            if(UINT64_MAX / 2 < Cap)
                reserveLen = UINT64_MAX;
            else
                reserveLen = Cap * 2;
            if(reserveLen < size)
                reserveLen = size;
            Reserve(reserveLen).ntry();
            return {};
        }
        
        inline nresult<void> Resize(uint64_t size)
        {
            if(size <= Len)
                Len = size;
            else
            {
                Reserve(size).ntry();
                Len = size;
            }
            return {};
        }
        
        inline nresult<void> Add(T t)
        {
            ReserveAhead(Len + 1).ntry();
            Data[Len++] = t;
            return {};
        }
        
        inline nresult<void> Insert(T t, uint64_t index)
        {
            ncheck_lte(index, Len);
            ReserveAhead(Len + 1).ntry();
            ++Len;
            
            if(index != Len)
            {
                memmove(&Data[index + 1], &Data[index], sizeof(T) * (Len - index));
                return {};
            }
            else
            {
                Data[index] = t;
                return {};
            }
        }
        
        inline nresult<void> Remove(uint64_t index)
        {
            ncheck_lt(index, Len);
            if(index != Len - 1)
            {
                memmove(&Data[index], &Data[index + 1], sizeof(T) * (Len - index - 1));
                --Len;
                return {};
            }
            else
            {
                --Len;
                return {};
            }
        }
        
        
        inline nresult<void> AddRange(View<T> view)
        {
            ncheck_gte(UINT64_MAX - view.Len, Len);
            ReserveAhead(Len + view.Len).ntry();
            for(uint64_t i = 0; i < view.Len; ++i)
                Data[Len++] = view.Data[i];
            return {};
        }
        
        inline nresult<void> InsertRange(uint64_t index, View<T> view)
        {
            if(!view.Len || !view.Data)
                return {};
            
            ncheck_lte(index, Len);
            ncheck_gte(UINT64_MAX - view.Len, Len);
            Reserve(Len + view.Len).ntry();
            if(index != Len)
                memmove(&Data[index + view.Len], &Data[index], sizeof(T) * (Len - index));
            memcpy(&Data[index], view.Data, sizeof(T) * view.Len);
        }
        
        inline nresult<void> RemoveRange(uint64_t index, uint64_t len)
        {
            ncheck_gte(UINT64_MAX - len, index);
            ncheck_lte(index + len, Len);
            if(index + len != Len)
                memmove(&Data[index], &Data[index + len], sizeof(T) * (Len - index - len));
            Len -= len;
            return {};
        }
    };
}


#endif
