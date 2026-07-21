#ifndef NCPP_N_OPTIONAL_N_HPP
#define NCPP_N_OPTIONAL_N_HPP

/*
Usage:
```c++
{
    noptional<int> optionalInt = nnone;
    printf("optionalInt?: %s\n", (optionalInt ? "true" : "false"));
    printf("optionalInt.value_or_default(): %d\n", optionalInt.value_or_default());
    printf("optionalInt.value_or(5): %d\n", optionalInt.value_or(5));
    *optionalInt = 6;
    printf("*optionalInt: %d\n", *optionalInt);
}
```

Output:
```
optionalInt?: false
optionalInt.value_or_default(): 0
optionalInt.value_or(5): 5
*optionalInt: 6
```
*/

namespace ncpp
{
    struct optional_nothing {};
    #define n_none ncpp::optional_nothing()
    
    template<typename T>
    struct n_optional
    {
        T value;
        bool exists;
        
        inline n_optional() = default;
        inline n_optional(optional_nothing n) { value = {}; exists = false; }
        inline n_optional(T val) { value = val; exists = true; }
        
        inline operator bool() const { return exists; }
        inline bool operator!() const { return !exists; }
        inline T& operator*() { return value; }
        inline T& operator*() const { return value; }
        inline T* operator->() { return &value; }
        inline T* operator->() const { return &value; }
        
        inline n_optional& operator=(const T& other)
        {
            value = other;
            exists = true;
            return *this;
        }
        
        inline T& value_or(T val)
        {
            if(!exists)
                value = val;
            return value;
        }
        
        inline T& value_or_default()
        {
            if(!exists)
                value = {};
            return value;
        }
    };
}


#endif
