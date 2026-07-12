/* runcpp2

OverrideCompileFlags:
    Remove: "-std=c++17"
    Append: "-std=c++11"
IncludePaths:
-   "../Include"
*/


#include <stdint.h>
#include <stdio.h>

//#define NSTD_TU_NAME MyTU
//#define NSTD_VALUE_TYPES int8_t,int16_t,char
#include "Nstd/TaggedUnion.hpp"

#include "Nstd/Allocator.hpp"

//#include "Nstd/TemplateHelpers.hpp"


int main(int argc, char** argv)
{
    {
        Nstd::TaggedUnion<int, signed char, uint8_t> t = t.Init<uint8_t>(9);
        switch(t.Index)
        {
            case typeof(t)::GetIndex<int>():
                printf("int\n");
                break;
            case typeof(t)::GetIndex<signed char>():
                printf("char\n");
                break;
            case typeof(t)::GetIndex<uint8_t>():
                printf("uint8_t\n");
                break;
        }
        
        t.Get<uint8_t>() = 10;
        printf("t: %d\n", t.Get<uint8_t>());
    }
    
    {
        Nstd::Allocator a = a.Init<int64_t, Nstd::HeapAllocator>(32);   //Reserve 32 int64_t
        defer { a.Destroy(); };
        int64_t* ints = a.Malloc<int64_t>(16);                          //Allocate 16 int64_t
        (void)ints;
        //...
        ints = a.Realloc<int64_t>(ints, 64);                            //Expands to 64 int64_t
        char* chars = a.Malloc<char>(16);
        (void)chars;
        a.Free(ints);
        a.FreeAll();
        chars = a.Malloc<char>(4);
    }
    
    return 0;
    
    
    #if 0
    {
        MyTU tu;
        NSTD_TU_INIT(tu, char, 'H');
        MyTU::Index i = tu.Type;
        switch(i)
        {
            case tu.NSTD_TU_INDEX(int8_t):
                tu.NSTD_TU_FIELD(int8_t) = 5;
                break;
            case tu.NSTD_TU_INDEX(int16_t):
                tu.NSTD_TU_FIELD(int16_t) = 5;
                break;
            case tu.NSTD_TU_INDEX(char):
                tu.NSTD_TU_FIELD(char) = 'B';
                break;
            case tu.COUNT_INDEX:
                break;
        }
    }
    #endif
    
    return 0;
}
