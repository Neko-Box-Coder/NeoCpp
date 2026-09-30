#ifndef NSTD_STRING_UTIL_N_HPP
#define NSTD_STRING_UTIL_N_HPP

#include "ncpp.n.hpp"

#include <string.h>
#include <ctype.h>

namespace Nstd
{
    /*
        NOTE: startIndex is inclusive, like so:
        ```
        v startIndex
        0 1 2 3 4 5 6 7
        [   ] <-- v.len = 3
        ```
        */
    inline uint64 FindSubString(n_view<const char> string, 
                                n_view<const char> sub, 
                                uint64 startIndex = 0)
    {
        if(!string.len || !sub || string.len < sub.len || startIndex >= string.len)
            return string.len;

        for(const char* p = strchr(&string.at<false>(startIndex), *sub.data); 
            p; 
            p = strchr(++p, *sub.data))
        {
            if(strncmp(p, sub.data, sub.len) == 0)
                return (uint64)(p - &string.at<false>(startIndex));
        }
        return string.len;
    }
    
    /*
    NOTE: startIndex is inclusive, like so:
    ```
                  v startIndex
    0 1 2 3 4 5 6 7
              [   ] <-- v.len = 3
    ```
    */
    inline uint64 ReverseFindSubString( n_view<const char> string, 
                                        n_view<const char> sub, 
                                        uint64 startIndex)
    {
        if( !string.len || 
            !sub || 
            string.len < sub.len || 
            startIndex >= string.len || 
            startIndex < sub.len - 1)
        {
            return string.len;
        }

        for(int64 i = startIndex + 1 - sub.len; i >= 0; --i)
        {
            if(string.at<false>(i) != sub.at<false>(0))
                continue;
            
            if(strncmp(&string.at<false>(i), sub.data, sub.len) == 0)
                return i;
        }
        return string.len;
    }
    
    inline n_view<char> ToLower(n_view<char> string)
    {
        for(int i = 0; i < string.len; ++i)
            string.at<false>(i) = (char)tolower((int)string.at<false>(i));
        return string;
    }
    
    inline n_view<char> ToUpper(n_view<char> string)
    {
        for(int i = 0; i < string.len; ++i)
            string.at<false>(i) = (char)toupper((int)string.at<false>(i));
        return string;
    }
}

#endif
