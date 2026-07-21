#ifndef NSTD_STRING_N_HPP
#define NSTD_STRING_N_HPP

/*
API:
```c++
struct String
{
    List<char> Intern_Chars;
    
    inline String Init(n_ref Allocator& alloc, uint64 reserveSize);
    inline char* Data();
    inline const char* Data() const;
    inline uint64 Len() const;
    inline n_result<void> AppendCString(const char* cs);
    inline n_result<void> AppendStringView(View<char> v);
    inline String InitCString(n_ref Allocator& alloc, const char* cs);
    inline String InitStringView(n_ref Allocator& alloc, View<char> v);
    inline char& At(uint64 index);
    inline char At(uint64 index) const;
    inline n_result<void> Reserve(uint64 size);
    inline n_result<void> Resize(uint64 size);
    inline n_result<void> Add(char c);
    inline n_result<void> Insert(uint64 index, char c);
    inline n_result<void> Remove(uint64 index);
    inline n_result<void> InsertStringView(uint64 index, View<char> view);
    inline n_result<void> InsertCString(const char* cs);
    inline n_result<void> RemoveRange(uint64 index, uint64 len);
    inline n_result<uint64> FindStringView(View<char> view);
    inline n_result<uint64> FindCString(const char* cs);
    inline n_result<void> Free();
};
```

*/

#include "ncpp.n.hpp"

#include "./List.n.hpp"

#include <string.h>

namespace Nstd
{
    struct String
    {
        List<char> Intern_Chars;
        
        inline String Init(n_ref Allocator& alloc, uint64 reserveSize)
        {
            Intern_Chars = Intern_Chars.Init(alloc, reserveSize + 1);
            Intern_Chars.Add('\0');
            return { Intern_Chars };
        }
        
        inline char* Data()
        {
            return Intern_Chars.Data;
        }
        
        inline const char* Data() const
        {
            return Intern_Chars.Data;
        }
        
        inline uint64 Len() const
        {
            return Intern_Chars.Len > 0 ? Intern_Chars.Len - 1 : 0;
        }
        
        inline n_result<void> Intern_PruneNull()
        {
            return  Intern_Chars.Len > 0 && Intern_Chars.Data[Intern_Chars.Len - 1] == '\0' ? 
                    Intern_Chars.Remove(Intern_Chars.Len - 1) :
                    n_result<void> {};
        }
        
        inline n_result<void> Intern_RestoreNull()
        {
            return Intern_Chars.Add('\0');
        }
        
        inline n_result<void> AppendString(n_view<const char> v)
        {
            Intern_PruneNull().n_try();
            n_defer { Intern_RestoreNull(); };
            Intern_Chars.AddRange(v).n_try();
            return {};
        }
        
        inline String InitString(n_ref Allocator& alloc, n_view<const char> v)
        {
            String s = s.Init(alloc, v.len);
            s.AppendString(v);
            return s;
        }
        
        inline char& At(uint64 index)
        {
            return Intern_Chars.At(index);
        }
        
        inline char At(uint64 index) const
        {
            return Intern_Chars.At(index);
        }
        
        inline n_result<void> Reserve(uint64 size)
        {
            Intern_Chars.Reserve(size + 1).n_try();
            return {};
        }
        
        inline n_result<void> Resize(uint64 size)
        {
            Intern_Chars.Resize(size + 1).n_try();
            Intern_Chars.At(Intern_Chars.Len - 1) = '\0';
            return {};
        }
        
        inline n_result<void> Add(char c)
        {
            Intern_Chars.At(Intern_Chars.Len - 1) = c;
            Intern_RestoreNull().n_try();
            return {};
        }
        
        inline n_result<void> Insert(uint64 index, char c)
        {
            if(index < Len())
            {
                Intern_Chars.Insert(index, c).n_try();
                return {};
            }
            
            n_check_eq(index, Len());
            Add(c).n_try();
            return {};
        }
        
        inline n_result<void> Remove(uint64 index)
        {
            Intern_Chars.Remove(index).n_try();
            return {};
        }
        
        inline n_result<void> InsertString(uint64 index, n_view<const char> v)
        {
            if(index < Len())
            {
                Intern_Chars.InsertRange(index, v).n_try();
                return {};
            }
            
            n_check_eq(index, Len());
            AppendString(v).n_try();
            return {};
        }
        
        inline n_result<void> RemoveRange(uint64 index, uint64 len)
        {
            Intern_PruneNull().n_try();
            n_defer { Intern_RestoreNull(); };
            Intern_Chars.RemoveRange(index, len);
            return {};
        }
        
        inline uint64 FindString(n_view<const char> v) const
        {
            if(!Len() || !v)
                return Len();

            for(char* p = strchr(Intern_Chars.Data, *v.data); 
                *p != '\0'; 
                p = strchr(++p, *v.data))
            {
                if(strncmp(p, v.data, v.len) == 0)
                    return (uint64)(p - Intern_Chars.Data);
            }
            return Len();
        }
        
        inline uint64 FindCString(const char* cs) const
        {
            const char* f = strstr(Intern_Chars.Data, cs);
            return f ? (uint64)(f - Intern_Chars.Data) : Len();
        }
        
        inline n_result<uint64> RemoveString(n_view<const char> v)
        {
            uint64 f = FindString(v);
            if(f == Len())
                return f;
            RemoveRange(f, v.len).n_try();
            return f;
        }
        
        inline n_result<uint64> RemoveCString(const char* cs)
        {
            uint64 f = FindCString(cs);
            if(f == Len())
                return f;
            RemoveRange(f, strlen(cs)).n_try();
            return f;
        }
        
        inline n_view<char> ToView()
        {
            n_view<char> v = Intern_Chars.ToView();
            if(v.len > 0)
                v.len -= 1;
            return v;
        }
        
        inline n_view<const char> ToView() const
        {
            n_view<const char> v = Intern_Chars.ToView();
            if(v.len > 0)
                v.len -= 1;
            return v;
        }
        
        //TODO
        #if 0
        inline n_result<uint64> ReverseFindStringView(View<char> view)
        {
            if(!Len() || !view.Data || !view.Len)
                return Len();

            for(char* p = strchr(Intern_Chars.Data, view.Data); p != '\0'; strchr(p, view.Data))
            {
                if(strncmp(p, view.Data, view.Len) == 0)
                    return (uint64)(p - Intern_Chars.Data);
            }
            return Len();
        }
        
        inline n_result<uint64> ReverseFindCString(const char* cs)
        {
            //const char* f = strstr(Intern_Chars.Data, cs);
            //return f ? (uint64)(f - Intern_Chars.Data) : Len();
        }
        #endif
        
        inline n_result<void> Free()
        {
            Intern_Chars.Free().n_try();
            return {};
        }
        
        
    };

}



#endif
