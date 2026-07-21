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

#include <stdarg.h>

namespace Nstd
{
    template<typename T, n_enable_if(n_is_simple(T))>
    struct List
    {
        Allocator* Alloc;
        T Dummy;
        T* Data;
        uint64 Len;
        uint64 Cap;
        
        inline List Init(n_ref Allocator& alloc, uint64 reserveSize)
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
        inline n_result<void> AddValues(Ts... values)
        {
            T* arr[] = { &values... };
            ReserveAhead(Len + n_array_cap(arr)).n_try();
            
            uint64 origLen = Len;
            Len += n_array_cap(arr);
            
            for(int i = 0; i < n_array_cap(arr); ++i)
                Data[origLen + i] = *arr[i];
            
            return {};
        }
        
        template<typename... Ts>
        inline List InitValues(n_ref Allocator& alloc, Ts... values)
        {
            List l = Init(alloc, sizeof...(values));
            if(!l.Cap)
                return l;
            
            l.AddValues(values...);
            return l;
        }
        
        inline T& At(uint64 index)
        {
            if(index >= Len)
                return Dummy;
            else
                return Data[index];
        }
        
        inline const T& At(uint64 index) const
        {
            if(index >= Len)
                return Dummy;
            else
                return Data[index];
        }
        
        inline n_result<void> Reserve(uint64 size)
        {
            n_check_true(Alloc);
            if(size <= Cap)
                return {};
            
            T* tmp = Alloc->Realloc(Data, size);
            if(!tmp)
                return n_error_msg("%s", "Failed to realloc");
            else
            {
                Data = tmp;
                Cap = size;
            }
            return {};
        }
        
        inline n_result<void> ReserveAhead(uint64 size)
        {
            uint64 reserveLen;
            if(UINT64_MAX / 2 < Cap)
                reserveLen = UINT64_MAX;
            else
                reserveLen = Cap * 2;
            if(reserveLen < size)
                reserveLen = size;
            Reserve(reserveLen).n_try();
            return {};
        }
        
        inline n_result<void> Resize(uint64 size)
        {
            if(size <= Len)
                Len = size;
            else
            {
                Reserve(size).n_try();
                Len = size;
            }
            return {};
        }
        
        inline n_result<void> Add(T t)
        {
            ReserveAhead(Len + 1).n_try();
            Data[Len++] = t;
            return {};
        }
        
        inline n_result<void> Insert(uint64 index, T t)
        {
            n_check_lte(index, Len);
            ReserveAhead(Len + 1).n_try();
            ++Len;
            
            if(index != Len)
                memmove(&Data[index + 1], &Data[index], sizeof(T) * (Len - index));
            
            Data[index] = t;
            return {};
        }
        
        inline n_result<void> Remove(uint64 index)
        {
            n_check_lt(index, Len);
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
        
        inline n_result<void> AddRange(n_view<const T> v)
        {
            n_check_gte(UINT64_MAX - v.len, Len);
            ReserveAhead(Len + v.len).n_try();
            for(uint64 i = 0; i < v.len; ++i)
                Data[Len++] = v.data[i];
            return {};
        }
        
        inline n_result<void> InsertRange(uint64 index, n_view<const T> v)
        {
            if(!v)
                return {};
            
            n_check_lte(index, Len);
            n_check_gte(UINT64_MAX - v.len, Len);
            Reserve(Len + v.len).n_try();
            if(index != Len)
                memmove(&Data[index + v.len], &Data[index], sizeof(T) * (Len - index));
            memcpy(&Data[index], v.data, sizeof(T) * v.len);
            Len += v.len;
            return {};
        }
        
        inline n_result<void> RemoveRange(uint64 index, uint64 len)
        {
            n_check_gte(UINT64_MAX - len, index);
            n_check_lte(index + len, Len);
            if(index + len != Len)
                memmove(&Data[index], &Data[index + len], sizeof(T) * (Len - index - len));
            Len -= len;
            return {};
        }
        
        inline n_view<T> ToView()
        {
            return n_view<T> { Data, Len };
        }
        
        inline n_view<const T> ToView() const
        {
            return n_view<const T> { Data, Len };
        }
        
        inline n_result<void> Free()
        {
            n_check_true(Alloc);
            Alloc->Free(Data);
            Data = NULL;
            Len = 0;
            Cap = 0;
            return {};
        }
    };
}


#endif
