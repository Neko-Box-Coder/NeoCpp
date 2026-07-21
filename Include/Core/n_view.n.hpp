#ifndef NCPP_N_VIEW_N_HPP
#define NCPP_N_VIEW_N_HPP

#include "./n_array.n.hpp"
#include "./n_type.n.hpp"

#include <string.h>

namespace ncpp
{
    template<typename T>
    struct n_view
    {
        T* data;
        uint64 len;
        
        inline n_view() = default;
        inline n_view(T* d, uint64 l) { data = d; len = l; }
        
        
        //template<typename U = T, n_enable_if(n_is_same(U, const U))>
        inline n_view(const n_view<n_no_const(T)>& other) { data = other.data; len = other.len; }
        
        template<typename U = T, n_enable_if(n_is_same(U, const char))>
        inline n_view(const char* c) { data = c; len = strlen(c); }
        
        inline n_view(char* c) { data = c; len = strlen(c); }
        
        inline operator bool() const { return data && len; }
        inline bool operator!() const { return !(data && len); }
    };
    
    #define n_array_to_view(arr) ncpp::n_view<n_no_ref( n_typeof(arr[0]) )> { arr, n_array_cap(arr) }
}

#endif
