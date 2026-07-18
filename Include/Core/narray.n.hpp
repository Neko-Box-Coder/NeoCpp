#ifndef NCPP_NARRAY_N_HPP
#define NCPP_NARRAY_N_HPP

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

#ifndef ntypeof
    #define ntypeof(x) decltype(x)
#endif

#define narray_cap(arr) (sizeof(arr) / sizeof(ntypeof(arr[0])))
#define narray_at(arr, i) (i < narray_cap(arr) ? arr[i] : ntypeof(arr[0] + 0)())

#endif
