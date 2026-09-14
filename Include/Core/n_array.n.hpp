#ifndef NCPP_N_ARRAY_N_HPP
#define NCPP_N_ARRAY_N_HPP

/*
Usage:
```c++
{
    int a[] = { 1, 2, 3 };
    int b[] = { };
    char c[] = "Hello";
    printf("narray_cap(a): %zu\n", narray_cap(a));
    printf("narray_cap(b): %zu\n", narray_cap(b));
    printf("narray_cap(c): %zu\n", narray_cap(c));
    
    printf("narray_at(a, 0): %d\n", narray_at(a, 0));
    printf("narray_at(a, 5): %d\n", narray_at(a, 5));
}
```

Output:
```
narray_cap(a): 3
narray_cap(b): 0
narray_cap(c): 6
narray_at(a, 0): 1
narray_at(a, 5): 0
```
*/

#include "./n_view.n.hpp"
#include "./n_assert.n.hpp"
#include <string.h>

#ifndef n_typeof
    #define n_typeof(x) decltype(x)
#endif

#define n_array_cap(arr) (sizeof(arr) / sizeof(n_typeof(arr[0])))
#define n_array_at(arr, i) (i < n_array_cap(arr) ? arr[i] : n_typeof(arr[0] + 0)())
#define n_array_to_view(arr) ncpp::n_view<n_no_ref( n_typeof(arr[0]) )> { arr, n_array_cap(arr) }

namespace ncpp
{
    template<typename T, usize Len>
    struct n_array
    {
        T data[Len];
        static constexpr usize len = Len;
    
        inline void zero()
        {
            memset(data, 0, sizeof(T) * len);
        }
        
        template<bool ASSERT = true>
        inline T& at(usize index) 
        {
            if(ASSERT)
                n_assert(index < len); 
            else
                n_assert_debug(index < len); 
            
            return data[index]; 
        }
        
        template<bool ASSERT = true>
        inline const T& at(usize index) const 
        {
            if(ASSERT)
                n_assert(index < len); 
            else
                n_assert_debug(index < len);
            
            return data[index]; 
        }
        
        inline T& operator[](usize index) { return at<true>(index); }
        inline const T& operator[](usize index) const { return at<true>(index); }
        
        inline n_view<T> to_view()
        {
            return n_view<T>(data, len);
        }
        
        inline n_view<const T> to_view() const
        {
            return n_view<const T>(data, len);
        }
    };
}

#endif
