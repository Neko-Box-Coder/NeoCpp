/* runcpp2

OverrideCompileFlags:
    DefaultPlatform:
        "g++":
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

n_result<int> TestError(int v)
{
    n_use_error_defer();
    n_error_defer { printf("This should be called by n_error_defer\n"); };
    
    (void)v;
    return n_error_msg("Test Error");
}

n_result<int> TestValue(int v)
{
    return v;
}

n_result<int> TestNested(int v)
{
    n_use_error_defer();
    n_error_defer { printf("This should not be called by n_error_defer\n"); };
    
    int v2 = TestValue(v).n_try();
    return v2;
}

n_result<int> TestNestedError(int v)
{
    n_use_error_defer();
    n_error_defer { printf("This should be called by n_error_defer again\n"); };
    int v2 = TestError(v).n_try();
    return v2;
}

n_result<int> TestCheck(int v)
{
    n_check_eq(v, 5);
    return v + 5;
}

n_result<int> TestCheckFmt(int v)
{
    n_check_eq_fmt(v, 5, "v: %d", v);
    return v + 5;
}

n_result<int> Main(int argc, char** argv)
{
    //Nstd/TaggedUnion.n.hpp
    {
        Nstd::TaggedUnion<int, signed char, uint8> t = t.Init<uint8>(9);
        switch(t.Index)
        {
            case n_typeof(t)::GetIndex<int>():
                printf("int\n");
                break;
            case n_typeof(t)::GetIndex<signed char>():
                printf("char\n");
                break;
            case n_typeof(t)::GetIndex<uint8>():
                printf("uint8\n");
                break;
        }
        
        t.Get<uint8>() = 10;
        printf("t: %d\n", t.Get<uint8>());
        printf("t.Is<int>(): %s\n", (t.Is<int>() ? "true" : "false"));
        printf("t.Is<uint8>(): %s\n", (t.Is<uint8>() ? "true" : "false"));
    }
    
    //Nstd/Allocator.n.hpp
    {
        Nstd::Allocator a = a.Init<int64_t, Nstd::HeapAllocator>(32);   //Reserve 32 int64_t
        n_defer { a.Destroy(); };
        
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
    
    //Core/n_move.n.hpp
    {
        int a = 3;
        int b = n_move(n_ref a);
        printf("a: %d, b: %d\n", a, b);
    }
    
    //Core/n_array.n.hpp
    {
        int a[] = { 1, 2, 3 };
        int b[1] = { };
        char c[] = "Hello";
        printf("n_array_cap(a): %zu\n", n_array_cap(a));
        printf("n_array_cap(b): %zu\n", n_array_cap(b));
        printf("n_array_cap(c): %zu\n", n_array_cap(c));
        
        printf("n_array_at(a, 0): %d\n", n_array_at(a, 0));
        printf("n_array_at(a, 5): %d\n", n_array_at(a, 5));
    }
    
    //Core/n_result.n.hpp
    {
        #define PRINT_STR_ERROR() err.string(msgMem, 256); printf("%s\n---------------\n", msgMem)
        char* msgMem = (char*)malloc(256);
        n_defer { free(msgMem); };
        
        int r = TestError(5).n_try_act(PRINT_STR_ERROR());
        r = TestNestedError(5).n_try_act(PRINT_STR_ERROR());
        r = TestNested(5).n_try_act(PRINT_STR_ERROR());
        r = TestCheck(r).n_try_act(PRINT_STR_ERROR());
        r = TestCheck(r).n_try_act(PRINT_STR_ERROR());
        r = TestCheckFmt(r).n_try_act(PRINT_STR_ERROR());
        
        //NOTE: Traces can be accessed inside `n_try_act()` with `err.traces`, where printf arguments
        //      for printing a single trace can be obtained with 
        //      `ntrace_fmt(print prefix, trace, print suffix)` and used like so 
        //      `printf(ntrace_fmt(...))`
        
        n_result<int> res = TestValue(3);
        r = res.value;
        bool hasError = res.err;
        if(hasError)
        {
            n_error_info errInfo = *res.err;
            (void)errInfo;
        }
        r = res.value_or(3);
        r = res.value_or_default();
    }
    
    //Core/n_optional.n.hpp
    {
        n_optional<int> optionalInt = n_none;
        printf("optionalInt?: %s\n", (optionalInt ? "true" : "false"));
        printf("optionalInt.value_or_default(): %d\n", optionalInt.value_or_default());
        printf("optionalInt.value_or(5): %d\n", optionalInt.value_or(5));
        *optionalInt = 6;
        printf("*optionalInt: %d\n", *optionalInt);
    }
    static_assert(n_is_simple(n_optional<int>), "");
    
    //Nstd/List.n.hpp
    {
        Nstd::Allocator alloc = alloc.Init<int, Nstd::HeapAllocator>(32);
        n_defer { alloc.Destroy(); };
        
        Nstd::List<int> list = list.Init(n_ref alloc, 4);    //Reserve 4 ints
        list.Add(1);
        list.Add(2);
        list.Add(3);
        
        for(int i = 0; i < list.Len; ++i)
            printf("list.At(%d): %d\n", i, list.At(i));
        list.Free().n_try();
        
        int nums[] = {7, 8, 9};
        n_view<int> numsView = { nums, n_array_cap(nums) };
        list = list.InitValues(n_ref alloc, 0, 1, 2, 3);
        
        list.Add(4).n_try();                     //0 1 2 3 (4)
        list.Reserve(10).n_try();
        list.Insert(4, 5).n_try();               //0 1 2 3 (5) 4
        list.Remove(3).n_try();                  //0 1 2 x 5 4
        list.AddRange(numsView).n_try();         //0 1 2 5 4 (7 8 9)
        list.InsertRange(1, numsView).n_try();   //0 (7 8 9) 1 2 5 4 7 8 9
        list.RemoveRange(2, 4).n_try();          //0 7 x x x x 5 4 7 8 9
        for(int i = 0; i < list.Len; ++i)
            printf("list.At(%d): %d\n", i, list.At(i));
    }
    
    //Nstd/LinkedList.n.hpp
    {
        
    }
    
    //Nstd/Hashmap.n.hpp
    {
        Nstd::Allocator alloc = alloc.Init<int, Nstd::HeapAllocator>(32);
        n_defer { alloc.Destroy(); };
        
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
            hmap.AddRange(n_array_to_view(keyVals));
        }
        hmap.Add("Test5", 5).n_try();
        
        Nstd::HashNode<int>* foundNode = hmap.Find("Test3").n_try();
        n_check_true(foundNode);
        n_check_eq(foundNode->Key.len, strlen("Test3"));
        n_check_true(memcmp(foundNode->Key.data, "Test3", strlen("Test3")) == 0);
        printf("hmap[\"Test3\"]: %d\n", foundNode->Value);    //3
        
        hmap.Remove(foundNode);
        size_t l = hmap.Len().n_try();
        printf("hmap.Len(): %zu\n", l);    //4
        
        Nstd::KeyValue<int> entries[3] =    { 
                                                {"Test7", 7}, 
                                                {"Test8", 8}, 
                                                {"Test9", 9}
                                            };
        hmap.AddRange(n_view<Nstd::KeyValue<int>> { entries, n_array_cap(entries) } ).n_try();
        
        for(Nstd::HashNode<int>* curNode = hmap.First(); curNode != NULL; curNode = hmap.Next(curNode))
            printf("hmap[\"%s\"]: %d\n", curNode->Key.data, curNode->Value);
        
        hmap.Free().n_try();
    }
    
    //Nstd/String.n.hpp
    {
        Nstd::Allocator alloc = alloc.Init<char, Nstd::HeapAllocator>(32);
        n_defer { alloc.Destroy(); };
        Nstd::String s = s.InitString(alloc, "Test");
        printf("String: \"%s\" with len %" PRIu64 "\n", s.Data(), s.Len());
        
        s.Add('s').n_try();
        s.Remove(2).n_try();
        printf("String[2]: %c\n", s.At(2));
        
        s.AppendString("Test2");
        s.AppendString("Test3");
        
        s.InsertString(4, "Test5").n_try();
        printf("String: \"%s\" with len %" PRIu64 "\n", s.Data(), s.Len());
        
        s.RemoveRange(9, 5).n_try();
        printf("String: \"%s\" with len %" PRIu64 "\n", s.Data(), s.Len());
        
        uint64 f = s.FindString("Test3");
        printf("Test3 is at index %" PRIu64 "\n", f);
        
        f = s.FindString("Test5");
        printf("Test5 is at index %" PRIu64 "\n", f);
        
        f = s.FindString("Test2");
        n_check_eq(f, s.Len());
        printf("Test2 is not found\n");
        
        s.RemoveString("Test5").n_try();
        printf("String: \"%s\" with len %" PRIu64 "\n", s.Data(), s.Len());
        
        s.RemoveString("Test3").n_try();
        printf("String: \"%s\" with len %" PRIu64 "\n", s.Data(), s.Len());
    }
    
    
    {
        char a[] = "abc";
        n_view<char> v = a;
        n_view<const char> v2 = v;
        //n_view<const char> v = "Abc";
        
        
        //(void)TTTT(v);
    }
    
    
    return 0;
}

int main(int argc, char** argv)
{
    int r = Main(argc, argv).n_try_act( printf("FAILED.\n");
                                        printf("Error: \n    %s\nStack trace:\n", err.message);
                                        for(int i = 0; i < err.traces_len; ++i)
                                            printf(n_trace_fmt("    at ", err.traces[i], "\n"));
                                        return 1);
    return r;
}
