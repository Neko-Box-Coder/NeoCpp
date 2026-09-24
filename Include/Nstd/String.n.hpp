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
    inline n_result<void> AppendString(n_view<const char> v);
    inline String InitString(n_ref Allocator& alloc, n_view<const char> v);
    inline char& At(uint64 index);
    inline char At(uint64 index) const;
    inline n_result<void> Reserve(uint64 size);
    inline n_result<void> Resize(uint64 size);
    inline n_result<void> Add(char c);
    inline n_result<void> Insert(uint64 index, char c);
    inline n_result<void> Remove(uint64 index);
    inline n_result<void> InsertString(uint64 index, n_view<const char> v);
    inline n_result<void> RemoveRange(uint64 index, uint64 len);
    inline uint64 FindString(n_view<const char> v, uint64 startIndex = 0) const;
    inline n_result<uint64> ReverseFindString(n_view<char> v, uint64 startIndex);
    
    template<typename... Ts>
    inline n_result<void> AppendFormat(n_view<const char> format, Ts... args);
    inline n_result<uint64> RemoveString(n_view<const char> v);
    inline n_view<char> ToView();
    inline n_view<const char> ToView() const;
    inline n_result<void> Free();
};
```

*/

#include "ncpp.n.hpp"
#include "./Allocator.n.hpp"
#include "./List.n.hpp"

#include "../Core/External/printf.hpp"
#include <string.h>

namespace Nstd
{
    struct String
    {
        List<char> Intern_Chars;
        
        inline String Init(n_ref Allocator& alloc, uint64 reserveSize)
        {
            Intern_Chars = Intern_Chars.Init(n_ref alloc, reserveSize + 1);
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
            String s = s.Init(n_ref alloc, v.len);
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
        
        inline n_result<void> Intern_AppendFormatBase(  n_view<const char> format, 
                                                        n_ref int& index, 
                                                        n_view<const char> arg)
        {
            n_check_lt(format.len, 28);
            n_check_lt(arg.len, INT_MAX);
            
            char formatCloneArr[32] = "%";
            n_view<char> formatClone = n_array_to_view(formatCloneArr);
            usize l = 1;
            
            n_check_gte(format.len, 2);
            format.sub(1, format.len - 2).copy_to(formatClone, 1);
            l += format.len - 2;
            
            formatClone[l++] = '.';
            formatClone[l++] = '*';
            formatClone[l++] = 's';
            formatClone[l++] = '\0';
            int lenNeeded = snprintf_(NULL, 0, formatClone.data, (int)arg.len, arg);
            uint64 start = Len();
            Resize(start + lenNeeded).n_try();
            
            n_check_eq_fmt( snprintf_(&At(start), lenNeeded + 1, formatClone.data, (int)arg.len, arg), 
                            lenNeeded,
                            "Format substitution failed at %i", 
                            index);
            return {};
        }
        
        inline n_result<void> Intern_AppendFormatBase(  n_view<const char> format, 
                                                        n_ref int& index, 
                                                        const char* arg)
        {
            Intern_AppendFormatBase(format, index, n_view<const char>(arg)).n_try();
            return {};
        }
        
        inline n_result<void> Intern_AppendFormatBase(  n_view<const char> format, 
                                                        n_ref int& index, 
                                                        const Nstd::String arg)
        {
            Intern_AppendFormatBase(format, index, arg.ToView()).n_try();
            return {};
        }
        
        #define INTERN_FORMAT_BASE(argType, formatStr) \
            inline n_result<void> Intern_AppendFormatBase(  n_view<const char> format, \
                                                            n_ref int& index, \
                                                            argType arg) \
            { \
                const usize formatLen = strlen("%" formatStr); \
                n_check_lt(format.len, 32 - formatLen); \
                \
                char formatCloneArr[32] = "%"; \
                n_view<char> formatClone = n_array_to_view(formatCloneArr); \
                usize l = 1; \
                \
                n_check_gte(format.len, 2); \
                format.sub(1, format.len - 2).copy_to(formatClone, 1); \
                l += format.len - 2; \
                \
                for(int i = 0; i < formatLen - 1; ++i) \
                    formatClone[l++] = formatStr [i]; \
                formatClone[l++] = '\0'; \
                \
                int lenNeeded = snprintf_(NULL, 0, formatClone.data, arg); \
                uint64 start = Len(); \
                Resize(start + lenNeeded).n_try(); \
                \
                n_check_eq_fmt( snprintf_(&At(start), lenNeeded + 1, formatClone.data, arg), \
                                lenNeeded, \
                                "Format substitution failed at %i", \
                                index); \
                return {}; \
            }
        
        INTERN_FORMAT_BASE(uint8, PRIu8)
        INTERN_FORMAT_BASE(uint16, PRIu16)
        INTERN_FORMAT_BASE(uint32, PRIu32)
        INTERN_FORMAT_BASE(uint64, PRIu64)
        
        INTERN_FORMAT_BASE(int8, PRIi8)
        INTERN_FORMAT_BASE(int16, PRIi16)
        INTERN_FORMAT_BASE(int32, PRIi32)
        INTERN_FORMAT_BASE(int64, PRIi64)
        
        INTERN_FORMAT_BASE(double, "f")
        
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
                    
                    Intern_AppendFormatBase(format.sub(s, i + 1 - s), index, arg).n_try();
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
