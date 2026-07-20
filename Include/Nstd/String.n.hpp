#ifndef NSTD_STRING_N_HPP
#define NSTD_STRING_N_HPP

/*
API:
```c++
struct String
{
    List<char> Intern_Chars;
    
    inline String Init(nref Allocator& alloc, uint64 reserveSize);
    inline char* Data();
    inline const char* Data() const;
    inline uint64 Len() const;
    inline nresult<void> AppendCString(const char* cs);
    inline nresult<void> AppendStringView(View<char> v);
    inline String InitCString(nref Allocator& alloc, const char* cs);
    inline String InitStringView(nref Allocator& alloc, View<char> v);
    inline char& At(uint64 index);
    inline char At(uint64 index) const;
    inline nresult<void> Reserve(uint64 size);
    inline nresult<void> Resize(uint64 size);
    inline nresult<void> Add(char c);
    inline nresult<void> Insert(uint64 index, char c);
    inline nresult<void> Remove(uint64 index);
    inline nresult<void> InsertStringView(uint64 index, View<char> view);
    inline nresult<void> InsertCString(const char* cs);
    inline nresult<void> RemoveRange(uint64 index, uint64 len);
    inline nresult<uint64> FindStringView(View<char> view);
    inline nresult<uint64> FindCString(const char* cs);
    inline nresult<void> Free();
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
        
        inline String Init(nref Allocator& alloc, uint64 reserveSize)
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
        
        inline nresult<void> Intern_PruneNull()
        {
            return  Intern_Chars.Len > 0 && Intern_Chars.Data[Intern_Chars.Len - 1] == '\0' ? 
                    Intern_Chars.Remove(Intern_Chars.Len - 1) :
                    nresult<void> {};
        }
        
        inline nresult<void> Intern_RestoreNull()
        {
            return Intern_Chars.Add('\0');
        }
        
        inline nresult<void> AppendString(nview<const char> v)
        {
            Intern_PruneNull().ntry();
            ndefer { Intern_RestoreNull(); };
            Intern_Chars.AddRange(v).ntry();
            return {};
        }
        
        inline String InitString(nref Allocator& alloc, nview<const char> v)
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
        
        inline nresult<void> Reserve(uint64 size)
        {
            Intern_Chars.Reserve(size + 1).ntry();
            return {};
        }
        
        inline nresult<void> Resize(uint64 size)
        {
            Intern_Chars.Resize(size + 1).ntry();
            Intern_Chars.At(Intern_Chars.Len - 1) = '\0';
            return {};
        }
        
        inline nresult<void> Add(char c)
        {
            Intern_Chars.At(Intern_Chars.Len - 1) = c;
            Intern_RestoreNull().ntry();
            return {};
        }
        
        inline nresult<void> Insert(uint64 index, char c)
        {
            if(index < Len())
            {
                Intern_Chars.Insert(index, c).ntry();
                return {};
            }
            
            ncheck_eq(index, Len());
            Add(c).ntry();
            return {};
        }
        
        inline nresult<void> Remove(uint64 index)
        {
            Intern_Chars.Remove(index).ntry();
            return {};
        }
        
        inline nresult<void> InsertString(uint64 index, nview<const char> v)
        {
            if(index < Len())
            {
                Intern_Chars.InsertRange(index, v).ntry();
                return {};
            }
            
            ncheck_eq(index, Len());
            AppendString(v).ntry();
            return {};
        }
        
        inline nresult<void> RemoveRange(uint64 index, uint64 len)
        {
            Intern_PruneNull().ntry();
            ndefer { Intern_RestoreNull(); };
            Intern_Chars.RemoveRange(index, len);
            return {};
        }
        
        inline uint64 FindString(nview<const char> v) const
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
        
        inline nresult<uint64> RemoveString(nview<const char> v)
        {
            uint64 f = FindString(v);
            if(f == Len())
                return f;
            RemoveRange(f, v.len).ntry();
            return f;
        }
        
        inline nresult<uint64> RemoveCString(const char* cs)
        {
            uint64 f = FindCString(cs);
            if(f == Len())
                return f;
            RemoveRange(f, strlen(cs)).ntry();
            return f;
        }
        
        inline nview<char> ToView()
        {
            nview<char> v = Intern_Chars.ToView();
            if(v.len > 0)
                v.len -= 1;
            return v;
        }
        
        inline nview<const char> ToView() const
        {
            nview<const char> v = Intern_Chars.ToView();
            if(v.len > 0)
                v.len -= 1;
            return v;
        }
        
        //TODO
        #if 0
        inline nresult<uint64> ReverseFindStringView(View<char> view)
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
        
        inline nresult<uint64> ReverseFindCString(const char* cs)
        {
            //const char* f = strstr(Intern_Chars.Data, cs);
            //return f ? (uint64)(f - Intern_Chars.Data) : Len();
        }
        #endif
        
        inline nresult<void> Free()
        {
            Intern_Chars.Free().ntry();
            return {};
        }
        
        
    };

}



#endif
