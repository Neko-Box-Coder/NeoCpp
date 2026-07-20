#ifndef NCPP_NVIEW_N_HPP
#define NCPP_NVIEW_N_HPP

#include "./narray.n.hpp"
#include "./ntype.n.hpp"

#include "./nstdint.n.hpp"
#include <string.h>

namespace ncpp
{
    template<typename T>
    struct nview
    {
        T* data;
        uint64 len;
        
        inline nview() = default;
        inline nview(T* d, uint64 l) { data = d; len = l; }
        
        
        //template<typename U = T, nenable_if(nis_same(U, const U))>
        inline nview(const nview<typename nno_const(T)>& other) { data = other.data; len = other.len; }
        
        template<typename U = T, nenable_if(nis_same(U, const char))>
        inline nview(const char* c) { data = c; len = strlen(c); }
        
        inline nview(char* c) { data = c; len = strlen(c); }
        
        inline operator bool() const { return data && len; }
        inline bool operator!() const { return !(data && len); }
    };
    
    #define narray_to_view(arr) ncpp::nview<nno_ref( ntypeof(arr[0]) )> { arr, narray_cap(arr) }
}

#endif
