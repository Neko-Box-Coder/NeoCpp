#ifndef NCPP_N_LINT_N_HPP
#define NCPP_N_LINT_N_HPP

/*
API:
```c++
#define n_defer_with(func)    //Specifies what function needs to be run 
#define n_in                  //Read-only reference parameter annotation
#define n_out                 //Write-only reference parameter annotation
#define n_ref                 //Read/write reference parameter annotation
#define n_out_init            //Write-initialize parameter annotation
#define n_ret_val             //Return value annotation
```

Usage:
```c++
//TODO: Add example showing n_in/n_out/n_ref usage in a function signature
```
*/

#define n_defer_with(func) 
#define n_in                  //Read only reference parameter
#define n_out                 //Write only reference parameter
#define n_ref                 //Read/Write reference parameter
#define n_out_init            //Write initialize parameter
#define n_ret_val             //Return value of this function

#endif
