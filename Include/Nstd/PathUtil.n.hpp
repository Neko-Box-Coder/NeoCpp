#ifndef NSTD_PATH_UTIL_N_HPP
#define NSTD_PATH_UTIL_N_HPP

/*
API:
```c++
namespace Nstd
{
    inline bool IsSeparator(char c);
    inline uint64 GetExtensionIndex(n_view<const char> path);
    inline n_view<const char> GetExtension(n_view<const char> path);
    inline n_view<const char> GetFileName(n_view<const char> path);
    inline n_view<const char> GetStem(n_view<const char> path);
    inline n_view<const char> GetParentPath(n_view<const char> path);
    inline bool EndsWithSeparator(n_view<const char> path);
    inline bool StartsWithSeparator(n_view<const char> path);
    inline bool IsAbsolutePath(n_view<const char> path);
    inline bool HasExtension(n_view<const char> path);
    inline n_result<String> JoinPaths(n_ref Allocator& alloc, n_view<const char> a, n_view<const char> b);
    inline n_result<String> ReplaceExtension(n_ref Allocator& alloc, n_view<const char> path, n_view<const char> newExt);
    inline n_result<String> NormalizePath(n_ref Allocator& alloc, n_view<const char> path);
}
```

Usage:
```c++
#ifdef _WIN32
    n_view<const char> path = "C:\\home\\user\\docs\\report.txt";
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
    n_view<const char> path = "/home/user/docs/report.txt";
#else
    #error "Unsupported platform"
#endif

n_view<const char> ext = Nstd::GetExtension(path);
printf("Extension: \"%.*s\"\n", (int)ext.len, ext.data);

n_view<const char> name = Nstd::GetFileName(path);
printf("FileName: \"%.*s\"\n", (int)name.len, name.data);

n_view<const char> nameNoExt = Nstd::GetStem(path);
printf("FileNameWithoutExt: \"%.*s\"\n", (int)nameNoExt.len, nameNoExt.data);

n_view<const char> parent = Nstd::GetParentPath(path);
printf("ParentDir: \"%.*s\"\n", (int)parent.len, parent.data);

n_check_true(Nstd::IsAbsolutePath(path));
n_check_false(Nstd::EndsWithSeparator(path));

#ifdef _WIN32
    n_check_true(Nstd::IsAbsolutePath("\\\\home\\user\\docs\\report.txt"));
#endif

Nstd::String joinedPath = Nstd::JoinPaths(n_ref alloc, parent, "./docs2/./report2.txt").n_try();
printf("JoinPaths: \"%.*s\"\n", (int)joinedPath.Len(), joinedPath.Data());

Nstd::String normalized = Nstd::NormalizePath(n_ref alloc, joinedPath.ToConstView()).n_try();
printf("Normalized: \"%.*s\"\n", (int)normalized.Len(), normalized.Data());
```
*/

#include "ncpp.n.hpp"
#include "Nstd/String.n.hpp"

namespace Nstd
{
    inline bool IsSeparator(char c)
    {
        return c == '/' || c == '\\';
    }

    inline uint64 GetExtensionIndex(n_view<const char> path)
    {
        if(!path)
            return 0;
        for(int64 i = path.len - 1; i >= 0; --i)
        {
            if(IsSeparator(path.at<false>(i)))
                return path.len;

            if(path.at<false>(i) == '.')
                return i + 1;
        }

        return path.len;
    }
    
    inline n_view<const char> GetExtension(n_view<const char> path)
    {
        uint64 extIndex = GetExtensionIndex(path);
        if(extIndex == path.len)
            return {};
        return path.sub(extIndex, path.len - extIndex);
    }

    inline n_view<const char> GetFileName(n_view<const char> path)
    {
        if(!path)
            return {};

        for(int64 i = path.len - 1; i >= 0; --i)
        {
            if(IsSeparator(path.at<false>(i)))
                return path.sub(i + 1, path.len - (i + 1));
        }

        return path;
    }

    inline n_view<const char> GetStem(n_view<const char> path)
    {
        n_view<const char> name = GetFileName(path);
        for(int64 i = name.len - 1; i >= 0; --i)
        {
            if(name.at<false>(i) == '.')
                return name.sub(0, (uint64)i);
        }

        return name;
    }

    inline n_view<const char> GetParentPath(n_view<const char> path)
    {
        if(path.len <= 1)
            return {};

        int64 lastSep = IsSeparator(path.at<false>(path.len - 1)) ? path.len - 1 : path.len;
        for(int64 i = path.len - 2; i >= 0; --i)
        {
            if(IsSeparator(path.at<false>(i)))
            {
                if(lastSep == i + 1)
                    lastSep = i;
                else
                {
                    lastSep = i;
                    break;
                }
            }
        }

        if(lastSep >= path.len - 1)
            return {};

        return path.sub(0, lastSep);
    }

    inline bool EndsWithSeparator(n_view<const char> path)
    {
        if(!path)
            return false;
        return IsSeparator(path.at<false>(path.len - 1));
    }

    inline bool StartsWithSeparator(n_view<const char> path)
    {
        if(!path)
            return false;
        return IsSeparator(path.at<false>(0));
    }

    inline bool IsAbsolutePath(n_view<const char> path)
    {
        #ifdef _WIN32
            if(path.len > 2)
            {
                // unc
                if(path.at<false>(0) == '/' && path.at<false>(1) == '/')
                    return true;
                else if(path.at<false>(0) == '\\' && path.at<false>(1) == '\\')
                    return true;
                //<name>:
                else
                {
                    uint64 f = FindSubString(path, "/");
                    f = f == path.len ? FindSubString(path, "\\") : f;
                    return FindSubString(path.sub(0, f), ":") != f;
                }
            }
            return false;
        #elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
            return path.len > 0 && path.at<false>(0) == '/';
        #else
            #error "Unsupported platform"
        #endif
    }
    
    inline bool IsRelativePath(n_view<const char> path)
    {
        return !IsAbsolutePath(path);
    }

    inline bool HasExtension(n_view<const char> path)
    {
        n_view<const char> name = GetFileName(path);
        if(!name)
            return false;

        for(uint64 i = 0; i < name.len; ++i)
        {
            if(name.at<false>(i) == '.')
                return true;
        }

        return false;
    }

    inline n_result<String> JoinPaths(  n_ref Allocator& alloc, 
                                        n_view<const char> a, 
                                        n_view<const char> b)
    {
        if(IsAbsolutePath(b))
            return n_error_msg("Cannot join absolute path");
        
        String result = result.Init(n_ref alloc, a.len + b.len + 2);
        n_use_error_defer();
        n_error_defer { result.Free(); };
        
        result.AppendString(a).n_try();
        
        if(!a || !b)
            return result;
        
        bool aEndsSep = EndsWithSeparator(a);
        if(!aEndsSep)
        {
            result.Add('/').n_try();
        }

        result.AppendString(b).n_try();
        return result;
    }

    inline n_result<String> ReplaceExtension(   n_ref Allocator& alloc, 
                                                n_view<const char> path, 
                                                n_view<const char> newExt)
    {
        uint64 extIndex = GetExtensionIndex(path);
        if(extIndex == path.len)
            return n_error_msg("Failed to get path extension");

        String result = result.Init(alloc, extIndex + newExt.len);
        n_use_error_defer();
        n_error_defer { result.Free(); };
        
        result.AppendString(path.sub(0, extIndex)).n_try();
        result.AppendString(newExt).n_try();
        return result;
    }


    inline n_result<String> NormalizePath(n_ref Allocator& alloc, n_view<const char> path)
    {
        if(!path)
            return String {};

        List<n_view<const char>> segments = segments.Init(n_ref alloc, path.len / 2);
        n_defer { segments.Free(); };
        
        uint64 startIndex = 0;
        for(uint64 i = 0; i < path.len; ++i)
        {
            if(IsSeparator(path.at<false>(i)) || i == path.len - 1)
            {
                uint64 segLen = i == path.len - 1 ? path.len - startIndex : i - startIndex;
                if(path.sub(startIndex, segLen) == "..")
                {
                    segments.Remove(segments.Len - 1).n_try();
                }
                else
                {
                    segments.Add(path.sub(startIndex, segLen)).n_try();
                }
                startIndex = i + 1;
            }
        }
        
        if(!segments.Len)
            return String {};
        
        n_use_error_defer();
        String result = result.Init(n_ref alloc, path.len);
        
        uint64 i = 0;
        #ifdef _WIN32
            if(segments.Len > 2)
            {
                if(!segments.Data[0].len && !segments.Data[1].len) //unc
                {
                    result.AppendString("\\\\").n_try();
                    i = 2;
                }
                else if(!segments.Data[0].len && segments.Data[1].len) //Invalid path
                    return n_error_msg("Invalid path");
            }
        #elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
            if(!segments.Data[0].len)
            {
                result.AppendString("/").n_try();
                i = 1;
            }
        #else
            #error "Unsupported platform"
        #endif
        
        for(; i < segments.Len; ++i)
        {
            if(!segments.Data[i].len)
            {
                n_assert_debug(i > 0);
                continue;
            }
            
            if(segments.Data[i] == ".")
                continue;
            
            result.AppendString(segments.Data[i]).n_try();
            
            if(i == segments.Len - 1 && !EndsWithSeparator(path))
                break;
            
            #ifdef _WIN32
                result.Add('\\').n_try();
            #elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
                result.Add('/').n_try();
            #else
                #error "Unsupported platform"
            #endif
        }
        
        return result;
    }
}

#endif
