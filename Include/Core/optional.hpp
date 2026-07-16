#ifndef NCPP_OPTIONAL_HPP
#define NCPP_OPTIONAL_HPP

/*
Usage:
```c++
{
    noptional<int> optionalInt = {};
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
    template<typename T>
    struct noptional
    {
        T value;
        bool exists;
        
        inline noptional() = default;
        inline noptional(T val)
        {
            value = val;
            exists = true;
        }
        
        inline operator bool() const
        {
            return exists;
        }
        
        inline bool operator!() const
        {
            return !exists;
        }
        
        inline T& operator*()
        {
            return value;
        }
        
        inline T& operator*() const
        {
            return value;
        }
        
        inline noptional& operator=(const T& other)
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
