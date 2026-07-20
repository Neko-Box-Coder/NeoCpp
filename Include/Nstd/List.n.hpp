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

#include "ncpp.n.hpp"

#include "./Allocator.n.hpp"
#include "./External/MacroPowerToys/ArgsCount.h"

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
        
        inline List Init(nref Allocator& alloc, uint64_t reserveSize)
        {
            List retList;
            retList.Alloc = &alloc;
            retList.Dummy = {};
            retList.Data = alloc.Malloc<T>(reserveSize);
            retList.Len = 0;
            retList.Cap = retList.Data ? reserveSize : 0;
            return retList;
        }
        
        
        template<typename... Ts>
        inline nresult<void> AddValues(Ts... values)
        {
            T* arr[] = { &values... };
            ReserveAhead(Len + narray_cap(arr)).ntry();
            
            uint64_t origLen = Len;
            Len += narray_cap(arr);
            
            for(int i = 0; i < narray_cap(arr); ++i)
                Data[origLen + i] = *arr[i];
            
            return {};
        }
        
        template<typename... Ts>
        inline List InitValues(nref Allocator& alloc, Ts... values)
        {
            List l = Init(alloc, sizeof...(values));
            if(!l.Cap)
                return l;
            
            l.AddValues(values...);
            return l;
        }
        
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
        
        inline nresult<void> Insert(uint64_t index, T t)
        {
            ncheck_lte(index, Len);
            ReserveAhead(Len + 1).ntry();
            ++Len;
            
            if(index != Len)
                memmove(&Data[index + 1], &Data[index], sizeof(T) * (Len - index));
            
            Data[index] = t;
            return {};
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
        
        inline nresult<void> AddRange(nview<const T> v)
        {
            ncheck_gte(UINT64_MAX - v.len, Len);
            ReserveAhead(Len + v.len).ntry();
            for(uint64_t i = 0; i < v.len; ++i)
                Data[Len++] = v.data[i];
            return {};
        }
        
        inline nresult<void> InsertRange(uint64_t index, nview<const T> v)
        {
            if(!v)
                return {};
            
            ncheck_lte(index, Len);
            ncheck_gte(UINT64_MAX - v.len, Len);
            Reserve(Len + v.len).ntry();
            if(index != Len)
                memmove(&Data[index + v.len], &Data[index], sizeof(T) * (Len - index));
            memcpy(&Data[index], v.data, sizeof(T) * v.len);
            Len += v.len;
            return {};
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
        
        inline nview<T> ToView()
        {
            return nview<T> { Data, Len };
        }
        
        inline nview<const T> ToView() const
        {
            return nview<const T> { Data, Len };
        }
        
        inline nresult<void> Free()
        {
            ncheck_true(Alloc);
            Alloc->Free(Data);
            Data = NULL;
            Len = 0;
            Cap = 0;
            return {};
        }
    };
}


#endif
