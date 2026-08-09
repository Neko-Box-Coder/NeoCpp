#ifndef NCPP_N_VIEW_N_HPP
#define NCPP_N_VIEW_N_HPP

#include "./n_array.n.hpp"
#include "./n_type.n.hpp"
#include "./n_assert.n.hpp"

#include <string.h>

namespace ncpp
{
    template<typename T>
    struct n_view
    {
        T* data;
        usize len;
        
        inline n_view() = default;
        inline n_view(T* d, usize l) { data = d; len = l; }
        
        template<typename T2 = T, typename T3 = n_no_const(T)> //NOTE: Hack to be trivial, same as below
        inline n_view(const n_view<T3>& other) { data = other.data; len = other.len; }
        //inline n_view(const n_view<n_no_const(T)>& other) { data = other.data; len = other.len; }
        
        template<typename U = T, n_enable_if(n_is_same(U, const char))>
        inline n_view(const char* c) { data = c; len = strlen(c); }
        inline n_view(char* c) { data = c; len = strlen(c); }
        
        inline n_view<T> sub(usize index, usize l)
        {
            if(!data || USIZE_MAX - l < index || index + l > len)
            {
                n_assert(false);
                return {};
            }
            return { &data[index], l };
        }
        
        inline n_view<const T> sub(usize index, usize l) const
        {
            if(!data || USIZE_MAX - l < index || index + l > len)
            {
                n_assert(false);
                return {};
            }
            return { &data[index], l };
        }
        
        inline void zero() 
        { 
            if(!data || !len)
            {
                n_assert(false);
                return; 
            }
            memset(data, 0, sizeof(T) * len);
        }
        
        inline void copy(n_view<T> dst, usize offset) const
        {
            if( !data || 
                !len || 
                !dst.data || 
                !dst.len || 
                USIZE_MAX - len < offset || 
                offset + len > dst.len)
            {
                n_assert(false);
                return;
            }
            memcpy(dst.data, data, sizeof(T) * len);
        }
        
        template<bool ASSERT = true>
        inline T& at(usize index) { if(ASSERT) n_assert(index < len); return data[index]; }
        
        template<bool ASSERT = true>
        inline const T& at(usize index) const { if(ASSERT) n_assert(index < len); return data[index]; }
        
        template<   typename T2, 
                    n_enable_if(sizeof(T) >= sizeof(T2) ? 
                                !(sizeof(T) % sizeof(T2)) : 
                                !(sizeof(T2) % sizeof(T)))>
        inline n_view<T2> as() 
        { 
            return  { 
                        (T2*)data, 
                        sizeof(T) >= sizeof(T2) ? 
                            len * (sizeof(T) / sizeof(T2)) : 
                            len / (sizeof(T2) / sizeof(T))
                    };
        }
        
        template<   typename T2, 
                    n_enable_if(sizeof(T) >= sizeof(T2) ? 
                                !(sizeof(T) % sizeof(T2)) : 
                                !(sizeof(T2) % sizeof(T)))>
        inline n_view<const T2> as() const
        { 
            return  { 
                        (const T2*)data, 
                        sizeof(T) >= sizeof(T2) ? 
                            len * (sizeof(T) / sizeof(T2)) : 
                            len / (sizeof(T2) / sizeof(T))
                    };
        }
        
        template<typename T2>
        inline T2 read(usize index) const
        {
            if(!data || !len || index * sizeof(T) + sizeof(T2) > len * sizeof(T))
            {
                n_assert(false);
                return {};
            }
            T2 temp;
            memcpy(&temp, &data[index], sizeof(T2));
            return temp;
        }
        
        template<typename T2>
        inline void write(usize index, const T2& var) const
        {
            if(!data || !len || index * sizeof(T) + sizeof(T2) > len * sizeof(T))
            {
                n_assert(false);
                return;
            }
            memcpy(&data[index], &var, sizeof(T2));
        }
        
        inline bool operator==(const n_in n_view<T>& other) 
        { 
            return  data && 
                    other.data && 
                    len == other.len && 
                    memcmp(data, other.data, sizeof(T) * len) == 0;
        }
        
        inline operator bool() const { return data && len; }
        inline bool operator!() const { return !(data && len); }
        
        inline T& operator[](usize index) { return at<true>(index); }
        inline const T& operator[](usize index) const { return at<true>(index); }
    };
    
    #define n_array_to_view(arr) ncpp::n_view<n_no_ref( n_typeof(arr[0]) )> { arr, n_array_cap(arr) }
    
    static_assert(n_is_simple(n_view<char>), "");
}

#endif
