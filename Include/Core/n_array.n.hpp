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

#ifndef n_typeof
    #define n_typeof(x) decltype(x)
#endif

#define n_array_cap(arr) (sizeof(arr) / sizeof(n_typeof(arr[0])))
#define n_array_at(arr, i) (i < n_array_cap(arr) ? arr[i] : n_typeof(arr[0] + 0)())

#endif
