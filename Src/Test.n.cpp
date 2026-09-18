/* runcpp2

RequiredProfiles:
    DefaultPlatform: ["g++"]

OverrideCompileFlags:
    DefaultPlatform:
        "g++":
        # "g++14":
            Remove: "-std=c++17"
            # Append: "-std=c++11 -Wno-sign-compare -E -P"
            # Append: "-std=c++11 -Wno-sign-compare -pg -g"
            Append: "-std=c++11 -Wno-sign-compare -Wno-narrowing -O0"

# OverrideLinkFlags:
#     DefaultPlatform:
#         "g++":
#         # "g++14":
#             Append: "-pg"

# Defines: ["NDEBUG=1"]

IncludePaths:
-   "../Include"
*/


#include "ncpp.n.hpp"

#include "Nstd/AllocatorTest.n.hpp"


#include "Nstd/TaggedUnion.n.hpp"
#include "Nstd/HeapAllocator.n.hpp"
#include "Nstd/FastAllocator.n.hpp"
#include "Nstd/Allocator.n.hpp"
#include "Nstd/AllocatorPool.n.hpp"
#include "Nstd/List.n.hpp"
#include "Nstd/LinkedList.n.hpp"
#include "Nstd/Hashmap.n.hpp"
#include "Nstd/String.n.hpp"
#include "Nstd/Atomic.n.hpp"
#include "Nstd/Any.n.hpp"
#include "Nstd/Threads.n.hpp"
#include "Nstd/Filesystem.n.hpp"
#include "Nstd/ReportError.n.hpp"

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

n_result<int> Main(int, char**)
{
    #if 0
        Nstd::BenchmarkAllocatorsMain().n_try();
        if(true)
            return 0;
    #endif
    
    Nstd::HeapAllocator h = h.Init(32);
    Nstd::Allocator alloc = h.MakeAllocator();
    n_defer { alloc.Destroy(); };
    
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
        n_view<int64_t> ints = alloc.Malloc<int64_t>(16); //Allocate 16 int64_t
        (void)ints;
        //...
        ints = alloc.Realloc<int64_t>(ints, 64); //Expands to 64 int64_t
        n_view<char> chars = alloc.Malloc<char>(16);
        (void)chars;
        alloc.Free(ints);
        alloc.FreeAll();
        chars = alloc.Malloc<char>(4);
    }
    
    {
        //TODO
        Nstd::AllocatorPool<Nstd::FastAllocator<>> AllocPool = AllocPool.Init(n_ref alloc, 64).n_try();
        
        
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
        
        n_array<int, 3> d = {1, 2, 3};
        n_view<const int> dv = d.to_view();
        (void)dv;
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
        //TODO
    }
    
    //Nstd/Hashmap.n.hpp
    {
        Nstd::Hashmap<int> hmap = 
            hmap.InitValues(n_ref alloc, 
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
        Nstd::String s = s.InitString(n_ref alloc, "Test");
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
    
        s.AppendFormat("Hello {-10}", "World").n_try();
        printf("String: \"%s\" with len %" PRIu64 "\n", s.Data(), s.Len());
        
        
        s.AppendFormat(" Int32: {}", (int32)95535).n_try();
        printf("String: \"%s\" with len %" PRIu64 "\n", s.Data(), s.Len());
        
        s.AppendFormat(" UInt16: {}", (uint16)55335).n_try();
        printf("String: \"%s\" with len %" PRIu64 "\n", s.Data(), s.Len());
        
        s.AppendFormat(" Float: {}", 5.5f).n_try();
        printf("String: \"%s\" with len %" PRIu64 "\n", s.Data(), s.Len());
        
        s.AppendFormat(" Double: {}", 10.10).n_try();
        printf("String: \"%s\" with len %" PRIu64 "\n", s.Data(), s.Len());
    }
    
    //Nstd/Atomic.n.hpp
    {
        Nstd::Atomic<int8> a;
        a.Store(6); //6
        int8 b = a.Load();
        b = a.Exchange(8); //8
        (void)a.StoreIfEqual(8, b); //6
        
        a.Add(5); //11
        a.Sub(3); //8
        a.Or(3); //11
        a.Xor(5); //14
        a.And(1); //15
        n_check_eq(a.Load(), 15);
    }
    
    
    //Nstd/Filesystem.n.hpp
    {
        //Write a test file
        n_view<const char> testData = "Hello from Nstd Filesystem!\n";
        Nstd::FileWriteAll("../fs_test.txt", testData.as<const uint8>()).n_try();

        //Read it back
        Nstd::List<uint8> content = Nstd::FileReadAll(alloc, "../fs_test.txt").n_try();
        n_check_true(content.ToView().as<const char>() == testData);
        
        //Check path exists
        bool exists = Nstd::PathExists("../fs_test.txt").n_try();
        n_check_true(exists);

        //Open file manually
        Nstd::File f = Nstd::FileOpen("../fs_test.txt", "rb").n_try();
        n_defer { (void)f.Close(); };

        int64_t pos = f.Tell().n_try();
        n_check_eq(pos, 0);

        n_array<uint8, 10> buf;
        buf.zero();
        usize bytesRead;
        f.Read(buf.to_view(), n_ref bytesRead).n_try();
        n_check_true(testData.sub(0, 10) == buf.to_view().as<const char>());
        
        pos = f.Tell().n_try();
        n_check_eq(pos, 10);

        //Seek back to start
        f.Seek(0).n_try();
        pos = f.Tell().n_try();
        n_check_eq(pos, 0);

        //List directory
        Nstd::DirIterator dir = Nstd::OpenDirectory("..").n_try();
        n_defer { dir.Close(); };

        while(true)
        {
            bool hasMore = dir.Next().n_try();
            if(!hasMore) break;

            Nstd::DirEntry entry = dir.Current().n_try();
            printf("  %s%s\n", entry.Name.data, entry.IsDirectory ? "/" : "");
        }

        //Create & delete directory test
        Nstd::CreateDirectory("../fs_test_dir").n_try();
        bool isDir = Nstd::IsDirectory("../fs_test_dir").n_try();
        n_check_true(isDir);

        //Create nested structure for DeleteDirectory test
        n_view<const char> delDirData = "test content\n";
        
        Nstd::CreateDirectory("../fs_test_dir/sub1").n_try();
        Nstd::CreateDirectory("../fs_test_dir/sub2").n_try();
        Nstd::CreateDirectory("../fs_test_dir/sub1/deep").n_try();
        
        //Write files at various levels
        Nstd::FileWriteAll("../fs_test_dir/file1.txt", delDirData.as<const uint8>()).n_try();
        Nstd::FileWriteAll("../fs_test_dir/sub1/file2.txt", delDirData.as<const uint8>()).n_try();
        Nstd::FileWriteAll("../fs_test_dir/sub1/deep/file3.txt", delDirData.as<const uint8>()).n_try();
        Nstd::FileWriteAll("../fs_test_dir/sub2/file4.txt", delDirData.as<const uint8>()).n_try();
        
        //Verify structure exists before deletion
        {
            bool pathExists = Nstd::PathExists("../fs_test_dir").n_try();
            n_check_true(pathExists);
            
            bool deepExists = Nstd::PathExists("../fs_test_dir/sub1/deep/file3.txt").n_try();
            n_check_true(deepExists);
        }
        
        //Delete entire tree iteratively
        Nstd::DeleteDirectory(n_ref alloc, "../fs_test_dir").n_try();
        
        //Verify everything is gone
        {
            bool stillExists = Nstd::PathExists("../fs_test_dir").n_try();
            n_check_false(stillExists);
        }

        //Rename test - rename a file
        {
            Nstd::FileWriteAll("../fs_rename_src.txt", delDirData.as<const uint8>()).n_try();
            Nstd::Rename("../fs_rename_src.txt", "../fs_rename_dst.txt").n_try();
            
            bool srcGone = Nstd::PathExists("../fs_rename_src.txt").n_try();
            n_check_false(srcGone);
            
            bool dstExists = Nstd::PathExists("../fs_rename_dst.txt").n_try();
            n_check_true(dstExists);
            
            Nstd::DeleteFile("../fs_rename_dst.txt").n_try();
        }
        
        //Cleanup
        Nstd::DeleteFile("../fs_test.txt").n_try();
        exists = Nstd::PathExists("../fs_test.txt").n_try();
        n_check_false(exists);
    }
    
    return 0;
}

int main(int argc, char** argv)
{
    int r = Main(argc, argv).n_try_act( printf("FAILED.\n");
                                        NSTD_REPORT_ERROR();
                                        return 1);
    return r;
}
