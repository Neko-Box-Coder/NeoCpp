#ifndef NSTD_STRING_N_HPP
#define NSTD_STRING_N_HPP

/*
API:
```c++
struct String
{
    List<char> Intern_Chars;
    
    inline String Init(n_ref AllocatorPool& alloc, uint64 reserveSize);
    inline char* Data();
    inline const char* Data() const;
    inline uint64 Len() const;
    inline n_result<void> AppendCString(const char* cs);
    inline n_result<void> AppendStringView(View<char> v);
    inline String InitCString(n_ref AllocatorPool& alloc, const char* cs);
    inline String InitStringView(n_ref AllocatorPool& alloc, View<char> v);
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
        
        inline String Init(Allocator alloc, uint64 reserveSize)
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
        
        inline String InitString(Allocator alloc, n_view<const char> v)
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
        
        /*
        NOTE: startIndex is inclusive, like so:
        ```
        v startIndex
        0 1 2 3 4 5 6 7
        [   ] <-- v.len = 3
        ```
        */
        inline uint64 FindString(n_view<const char> v, uint64 startIndex = 0) const
        {
            if(!Len() || !v || Len() < v.len || startIndex >= Len())
                return Len();

            for(char* p = strchr(&Intern_Chars.Data[startIndex], *v.data); p; p = strchr(++p, *v.data))
            {
                if(strncmp(p, v.data, v.len) == 0)
                    return (uint64)(p - &Intern_Chars.Data[startIndex]);
            }
            return Len();
        }
        
        /*
        NOTE: startIndex is inclusive, like so:
        ```
                      v startIndex
        0 1 2 3 4 5 6 7
                  [   ] <-- v.len = 3
        ```
        */
        inline n_result<uint64> ReverseFindString(n_view<char> v, uint64 startIndex)
        {
            if(!Len() || !v || Len() < v.len || startIndex >= Len() || startIndex < v.len - 1)
                return Len();

            for(int64 i = startIndex + 1 - v.len; i >= 0; --i)
            {
                if(Intern_Chars.At(i) != v.at<false>(0))
                    continue;
                
                if(strncmp(&Intern_Chars.At(i), v.data, v.len) == 0)
                    return i;
            }
            return Len();
        }
        
        #if 1
        inline n_result<void> Intern_AppendBase(n_view<const char> format, 
                                                n_ref int& index, 
                                                const char* arg)
        {
            return {};
        }
        
        inline n_result<void> Intern_AppendBase(n_view<const char> format, 
                                                n_ref int& index, 
                                                n_view<const char> arg)
        {
            return {};
        }
        
        inline n_result<void> Intern_AppendBase(n_view<const char> format, 
                                                n_ref int& index, 
                                                int arg)
        {
            return {};
        }
        
        inline n_result<void> Intern_AppendFormat(n_view<const char> format, n_ref int& index)
        {
            usize i;
            for(i = 0; i < format.len; ++i)
            {
                if(format.at<false>(i) == '{' && i > 0 && format.at<false>(i - 1) != '{')
                    return n_error_msg("Extra format substitution found at %i", index);
                else if(format.at<false>(i) == '}' && i > 0 && format.at<false>(i - 1) != '}')
                    return n_error_msg("Extra format substitution found at %i", index);
            }
            AppendString(format).n_try();
            return {};
        }
        
        template<typename T, typename... Ts>
        inline n_result<void> Intern_AppendFormat(  n_view<const char> format, 
                                                    n_ref int& index, 
                                                    T arg, 
                                                    Ts... args)
        {
            usize s = 0;
            usize i;
            for(i = 0; i < format.len; ++i)
            {
                if(format.at<false>(i) == '{')
                {
                    if(i != format.len - 1 && format.at<false>(i + 1) == '{')
                    {
                        AppendString(format.sub(s, i++ - s)).n_try();
                        s = i + 1;
                    }
                    else
                        break;
                }
                else if(format.at<false>(i) == '}')
                {
                    if(i == format.len - 1 || format.at<false>(i + 1) != '}')
                        return n_error_msg("Unescaped } found at %i", index);
                    
                    AppendString(format.sub(s, i++ - s)).n_try();
                    s = i + 1;
                }
                
            }
            
            if(s < format.len)
            {
                AppendString(format.sub(s, i - s)).n_try();
            }
            if(i >= format.len)
                return {};
            
            s = i;
            for(; i < format.len; ++i)
            {
                if(format.at<false>(i) == '}')
                {
                    if(i != format.len - 1 && format.at<false>(i + 1) == '}')
                        return n_error_msg("Unexpected } found in substitution at %i", index);
                    
                    Intern_AppendBase(format.sub(s, i + 1 - s), index, arg).n_try();
                    s = ++i;
                    break;
                }
                if(i == format.len - 1)
                    return n_error_msg("Unclosed substitution found at %i", index);
            }
            
            if(s < format.len)
            {
                ++index;
                Intern_AppendFormat(format.sub(s, i - s), index, args...).n_try();
            }
            
            return {};
        }
        
        
        template<typename... Ts>
        inline n_result<void> AppendFormat(n_view<const char> format, Ts... args)
        {
            int index = 0;
            Intern_AppendFormat(format, index, args...).n_try();
            return {};
        }
        #endif
        
        inline n_result<uint64> RemoveString(n_view<const char> v)
        {
            uint64 f = FindString(v);
            if(f == Len())
                return f;
            RemoveRange(f, v.len).n_try();
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
        
        inline n_result<void> Free()
        {
            Intern_Chars.Free().n_try();
            return {};
        }
        
        
    };

}



#endif
