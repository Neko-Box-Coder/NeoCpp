/* runcpp2

OverrideCompileFlags:
    Remove: "-std=c++17"
    Append: "-std=c++11 -Wno-sign-compare"
IncludePaths:
-   "../Include"
*/


#include "ncpp.n.hpp"

#include "Nstd/TaggedUnion.n.hpp"
#include "Nstd/Allocator.n.hpp"
#include "Nstd/List.n.hpp"
#include "Nstd/LinkedList.n.hpp"
#include "Nstd/Hashmap.n.hpp"
#include "Nstd/String.n.hpp"

#include <stdint.h>
#include <stdio.h>

nresult<int> TestError(int v)
{
    nuse_error_defer();
    nerror_defer { printf("This should be called by nerror_defer\n"); };
    
    (void)v;
    return nerror_msg("Test Error");
}

nresult<int> TestValue(int v)
{
    return v;
}

nresult<int> TestNested(int v)
{
    nuse_error_defer();
    nerror_defer { printf("This should not be called by nerror_defer\n"); };
    
    int v2 = TestValue(v).ntry();
    return v2;
}

nresult<int> TestNestedError(int v)
{
    nuse_error_defer();
    nerror_defer { printf("This should be called by nerror_defer again\n"); };
    int v2 = TestError(v).ntry();
    return v2;
}

nresult<int> TestCheck(int v)
{
    ncheck_eq(v, 5);
    return v + 5;
}

nresult<int> TestCheckFmt(int v)
{
    ncheck_eq_fmt(v, 5, "v: %d", v);
    return v + 5;
}

nresult<int> Main(int argc, char** argv)
{
    //Nstd/TaggedUnion.n.hpp
    {
        Nstd::TaggedUnion<int, signed char, uint8_t> t = t.Init<uint8_t>(9);
        switch(t.Index)
        {
            case ntypeof(t)::GetIndex<int>():
                printf("int\n");
                break;
            case ntypeof(t)::GetIndex<signed char>():
                printf("char\n");
                break;
            case ntypeof(t)::GetIndex<uint8_t>():
                printf("uint8_t\n");
                break;
        }
        
        t.Get<uint8_t>() = 10;
        printf("t: %d\n", t.Get<uint8_t>());
        printf("t.Is<int>(): %s\n", (t.Is<int>() ? "true" : "false"));
        printf("t.Is<uint8_t>(): %s\n", (t.Is<uint8_t>() ? "true" : "false"));
    }
    
    //Nstd/Allocator.n.hpp
    {
        Nstd::Allocator a = a.Init<int64_t, Nstd::HeapAllocator>(32);   //Reserve 32 int64_t
        ndefer { a.Destroy(); };
        
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
    
    //Core/nmove.n.hpp
    {
        int a = 3;
        int b = nmove(nref a);
        printf("a: %d, b: %d\n", a, b);
    }
    
    //Core/narray.n.hpp
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
    
    //Core/nresult.n.hpp
    {
        #define PRINT_STR_ERROR() err.string(msgMem, 256); printf("%s\n---------------\n", msgMem)
        char* msgMem = (char*)malloc(256);
        ndefer { free(msgMem); };
        
        int r = TestError(5).ntry_act(PRINT_STR_ERROR());
        r = TestNestedError(5).ntry_act(PRINT_STR_ERROR());
        r = TestNested(5).ntry_act(PRINT_STR_ERROR());
        r = TestCheck(r).ntry_act(PRINT_STR_ERROR());
        r = TestCheck(r).ntry_act(PRINT_STR_ERROR());
        r = TestCheckFmt(r).ntry_act(PRINT_STR_ERROR());
        
        //NOTE: Traces can be accessed inside `ntry_act()` with `err.traces`, where printf arguments
        //      for printing a single trace can be obtained with 
        //      `ntrace_fmt(print prefix, trace, print suffix)` and used like so 
        //      `printf(ntrace_fmt(...))`
        
        nresult<int> res = TestValue(3);
        r = res.value;
        bool hasError = res.err;
        if(hasError)
        {
            error_info errInfo = *res.err;
            (void)errInfo;
        }
        r = res.value_or(3);
        r = res.value_or_default();
    }
    
    //Core/noptional.n.hpp
    {
        noptional<int> optionalInt = nnone;
        printf("optionalInt?: %s\n", (optionalInt ? "true" : "false"));
        printf("optionalInt.value_or_default(): %d\n", optionalInt.value_or_default());
        printf("optionalInt.value_or(5): %d\n", optionalInt.value_or(5));
        *optionalInt = 6;
        printf("*optionalInt: %d\n", *optionalInt);
    }
    static_assert(nis_simple(noptional<int>), "");
    
    //Nstd/List.n.hpp
    {
        Nstd::Allocator alloc = alloc.Init<int, Nstd::HeapAllocator>(32);
        ndefer { alloc.Destroy(); };
        
        Nstd::List<int> list = list.Init(nref alloc, 4);    //Reserve 4 ints
        list.Add(1);
        list.Add(2);
        list.Add(3);
        
        for(int i = 0; i < list.Len; ++i)
            printf("list.At(%d): %d\n", i, list.At(i));
        list.Free().ntry();
        
        int nums[] = {7, 8, 9};
        nview<int> numsView = { nums, narray_cap(nums) };
        list = list.InitValues(nref alloc, 0, 1, 2, 3);
        
        list.Add(4).ntry();                     //0 1 2 3 (4)
        list.Reserve(10).ntry();
        list.Insert(4, 5).ntry();               //0 1 2 3 (5) 4
        list.Remove(3).ntry();                  //0 1 2 x 5 4
        list.AddRange(numsView).ntry();         //0 1 2 5 4 (7 8 9)
        list.InsertRange(1, numsView).ntry();   //0 (7 8 9) 1 2 5 4 7 8 9
        list.RemoveRange(2, 4).ntry();          //0 7 x x x x 5 4 7 8 9
        for(int i = 0; i < list.Len; ++i)
            printf("list.At(%d): %d\n", i, list.At(i));
    }
    
    //Nstd/LinkedList.n.hpp
    {
        
    }
    
    //Nstd/Hashmap.n.hpp
    {
        Nstd::Allocator alloc = alloc.Init<int, Nstd::HeapAllocator>(32);
        ndefer { alloc.Destroy(); };
        
        Nstd::Hashmap<int> hmap = 
            hmap.InitValues(alloc, 
                            Nstd::KeyValue<int> { "Test-2", -2 },
                            Nstd::KeyValue<int> { "Test-1", -1 },
                            Nstd::KeyValue<int> { "Test0", 0 });
        
        {
            Nstd::KeyValue<int> keyVals[] = {
                                                Nstd::KeyValue<int> { "Test1", 1 },
                                                Nstd::KeyValue<int> { "Test2", 2 },
                                                Nstd::KeyValue<int> { "Test3", 3 },
                                                Nstd::KeyValue<int> { "Test4", 4 }
                                            };
            hmap.AddRange(narray_to_view(keyVals));
        }
        hmap.Add("Test5", 5).ntry();
        
        Nstd::HashNode<int>* foundNode = hmap.Find("Test3").ntry();
        ncheck_true(foundNode);
        ncheck_eq(foundNode->Key.len, strlen("Test3"));
        ncheck_true(memcmp(foundNode->Key.data, "Test3", strlen("Test3")) == 0);
        printf("hmap[\"Test3\"]: %d\n", foundNode->Value);    //3
        
        hmap.Remove(foundNode);
        size_t l = hmap.Len().ntry();
        printf("hmap.Len(): %lu\n", l);    //4
        
        Nstd::KeyValue<int> entries[3] =    { 
                                                {"Test7", 7}, 
                                                {"Test8", 8}, 
                                                {"Test9", 9}
                                            };
        hmap.AddRange(nview<Nstd::KeyValue<int>> { entries, narray_cap(entries) } ).ntry();
        
        for(Nstd::HashNode<int>* curNode = hmap.First(); curNode != NULL; curNode = hmap.Next(curNode))
            printf("hmap[\"%s\"]: %d\n", curNode->Key.data, curNode->Value);
        
        hmap.Free().ntry();
    }
    
    //Nstd/String.n.hpp
    {
        Nstd::Allocator alloc = alloc.Init<char, Nstd::HeapAllocator>(32);
        ndefer { alloc.Destroy(); };
        Nstd::String s = s.InitString(alloc, "Test");
        printf("String: \"%s\" with len %lu\n", s.Data(), s.Len());
        
        s.Add('s').ntry();
        s.Remove(2).ntry();
        printf("String[2]: %c\n", s.At(2));
        
        s.AppendString("Test2");
        s.AppendString("Test3");
        
        s.InsertString(4, "Test5").ntry();
        printf("String: \"%s\" with len %lu\n", s.Data(), s.Len());
        
        s.RemoveRange(9, 5).ntry();
        printf("String: \"%s\" with len %lu\n", s.Data(), s.Len());
        
        uint64_t f = s.FindString("Test3");
        printf("Test3 is at index %lu\n", f);
        
        f = s.FindString("Test5");
        printf("Test5 is at index %lu\n", f);
        
        f = s.FindString("Test2");
        ncheck_eq(f, s.Len());
        printf("Test2 is not found\n");
        
        s.RemoveString("Test5").ntry();
        printf("String: \"%s\" with len %lu\n", s.Data(), s.Len());
        
        s.RemoveString("Test3").ntry();
        printf("String: \"%s\" with len %lu\n", s.Data(), s.Len());
    }
    
    
    {
        char a[] = "abc";
        nview<char> v = a;
        nview<const char> v2 = v;
        //nview<const char> v = "Abc";
        
        
        //(void)TTTT(v);
    }
    
    
    return 0;
}

int main(int argc, char** argv)
{
    int r = Main(argc, argv).ntry_act(  printf("FAILED.\n");
                                        printf("Error: \n    %s\nStack trace:\n", err.message);
                                        for(int i = 0; i < err.traces_len; ++i)
                                            printf(ntrace_fmt("    at ", err.traces[i], "\n"));
                                        return 1);
    return r;
}
