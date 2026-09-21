#ifndef NSTD_FILESYSTEM_N_HPP
#define NSTD_FILESYSTEM_N_HPP

/*
API:
```c++
struct File
{
    FILE* Handle;

    inline n_result<void> Close();
    inline n_result<void> Read(n_view<uint8> buffer, n_ref usize& bytesRead);
    inline n_result<void> Write(n_view<const uint8> buffer, n_ref usize& bytesWrote);
    inline n_result<void> Seek(int64 offset);
    inline n_result<void> SeekEnd();
    inline n_result<int64> Tell();
    inline n_result<void> Flush();
};

struct DirEntry
{
    n_view<const char> Name;
    bool IsDirectory;
    bool IsRegularFile;
};

struct DirIterator
{
    inline n_result<bool> Next();
    inline n_result<DirEntry> Current();
    inline void Close();
};

inline n_result<File> FileOpen( n_view<const char> path, 
                                n_view<const char> mode, 
                                bool binaryMode = true) n_defer_with(File.Close);
inline n_result<uint64> GetFileSize(n_view<const char> path);
inline n_result<List<uint8>> FileReadAll(n_ref Allocator& alloc, n_view<const char> path);
inline n_result<void> FileWriteAll(n_view<const char> path, n_view<const uint8> data);
inline n_result<DirIterator> 
OpenDirectory(n_view<const char> path, bool sorted = false) n_defer_with(DirIterator.Close);
inline n_result<bool> IsDirectory(n_view<const char> path);
inline n_result<bool> PathExists(n_view<const char> path);
inline n_result<void> CreateDirectory(n_view<const char> path);
inline n_result<void> DeleteFile(n_view<const char> path);
inline n_result<void> DeleteDirectory(n_ref Allocator& alloc, n_view<const char> path);
inline n_result<void> DeletePath(n_ref Allocator& alloc, n_view<const char> path);
inline n_result<void> Rename(n_view<const char> oldPath, n_view<const char> newPath);
```

Usage:
```c++
//Rename a file and verify source is gone, destination exists
Nstd::FileWriteAll("../fs_rename_src.txt", data.as<const uint8>()).n_try();
Nstd::Rename("../fs_rename_src.txt", "../fs_rename_dst.txt").n_try();

bool srcGone = Nstd::PathExists("../fs_rename_src.txt").n_try();
//srcGone is false - source was moved, not copied

bool dstExists = Nstd::PathExists("../fs_rename_dst.txt").n_try();
//dstExists is true - destination now exists
```

*/

#include "ncpp.n.hpp"
#include "./Allocator.n.hpp"
#include "./List.n.hpp"
#include "./String.n.hpp"

//TODO: Use allocator
#include "../Nstd/External/TinyDir/tinydir.h"
#include <stdio.h>
#include <sys/stat.h>
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
#else
    #include <unistd.h>
#endif

namespace Nstd
{
    struct File
    {
        FILE* Handle;

        inline n_result<void> Close()
        {
            if(!Handle)
                return {};

            n_check_eq(fflush(Handle), 0);
            n_check_eq(fclose(Handle), 0);
            Handle = NULL;
            return {};
        }

        //TODO: Use this between read/write
        inline n_result<void> Intern_Seek(int64_t offset, int whence)
        {
            #if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
                if(fseeko(Handle, offset, whence) != 0)
            #elif defined(_WIN32)
                if(_fseeki64(Handle, offset, whence) != 0)
            #else
                if(fseek(Handle, offset, whence) != 0)
            #endif
                {
                    return n_error_msg("Failed to seek in file: %s", strerror(errno));
                }
            return {};
        }

        inline n_result<void> Read(n_view<uint8> buffer, n_ref usize& bytesRead)
        {
            n_check_true(Handle);
            bytesRead = fread(buffer.data, 1, buffer.len, Handle);
            if(feof(Handle) == 0 && ferror(Handle) != 0)
                return n_error_msg("Failed to read from file: %s", strerror(errno));
            return {};
        }

        inline n_result<void> Write(n_view<const uint8> buffer, n_ref usize& bytesWrote)
        {
            n_check_true(Handle);
            bytesWrote = fwrite(buffer.data, 1, buffer.len, Handle);
            if(ferror(Handle) != 0)
                return n_error_msg("Failed to write to file: %s", strerror(errno));
            
            n_check_eq(bytesWrote, buffer.len);
            return {};
        }

        inline n_result<void> Seek(int64 offset)
        {
            n_check_true(Handle);
            Intern_Seek(offset, SEEK_SET).n_try();
            return {};
        }

        inline n_result<void> SeekEnd()
        {
            n_check_true(Handle);
            Intern_Seek(0, SEEK_END).n_try();
            return {};
        }

        inline n_result<int64> Tell()
        {
            n_check_true(Handle);

            #if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
                int64 pos = ftello(Handle);
            #elif defined(_WIN32)
                int64 pos = _ftelli64(Handle);
            #else
                int64 pos = ftell(Handle);
            #endif
            
            if(pos < 0)
                return n_error_msg("Failed to get file position: %s", strerror(errno));
            return pos;
        }

        inline n_result<void> Flush()
        {
            n_check_true(Handle);
            if(fflush(Handle) != 0)
                return n_error_msg("Failed to flush file: %s", strerror(errno));
            return {};
        }
    };

    struct DirEntry
    {
        n_view<const char> Name;
        bool IsDirectory;
        bool IsRegularFile;
    };

    struct DirIterator
    {
        tinydir_dir Dir;
        tinydir_file CurrentFile;

        inline n_result<bool> Next()
        {
            int ret = tinydir_next(&Dir);
            if(ret < 0)
                return n_error_msg("Failed to advance directory iterator");

            if(!Dir.has_next)
                return false;

            if(tinydir_readfile(&Dir, &CurrentFile) < 0)
                return n_error_msg("Failed to read directory entry data");

            return true;
        }

        inline n_result<DirEntry> Current()
        {
            DirEntry entry;
            entry.Name = (const char*)CurrentFile.name;
            entry.IsDirectory = CurrentFile.is_dir != 0;
            entry.IsRegularFile = CurrentFile.is_reg != 0;
            return entry;
        }

        inline void Close()
        {
            tinydir_close(&Dir);
        }
    };

    inline n_result<File> FileOpen( n_view<const char> path, 
                                    n_view<const char> mode, 
                                    bool binaryMode = true) n_defer_with(File.Close)
    {
        n_array<char, 512> pathBuf;
        n_array<char, 16> modeStr;
        
        if(path.len >= pathBuf.len)
            return n_error_msg("Path too long (%zu)", path.len);
        if(mode.len >= modeStr.len)
            return n_error_msg("Mode too long (%zu)", mode.len);
        
        n_view<char> pathBufView = pathBuf.to_view();
        path.copy_to(pathBufView);
        pathBufView[path.len] = '\0';
        
        n_view<char> modeStrView = modeStr.to_view();
        mode.copy_to(modeStrView);
        usize modeStrLen = mode.len;
        if(binaryMode)
            modeStrView[modeStrLen++] = 'b';
        modeStrView[modeStrLen++] = '\0';
        
        //TODO: Deal with windows for Unicode path
        FILE* f = fopen(pathBufView.data, modeStrView.data);
        if(!f)
            return n_error_msg("Failed to open file '%s': %s", pathBuf, strerror(errno));

        return File{f};
    }

    inline n_result<uint64> GetFileSize(n_view<const char> path)
    {
        n_use_error_defer();
        
        File f = FileOpen(path, "r").n_try();
        n_defer { f.Close(); };
        
        f.SeekEnd().n_try();
        int64 s = f.Tell().n_try();
        return s;
    }

    inline n_result<List<uint8>> FileReadAll(n_ref Allocator& alloc, n_view<const char> path)
    {
        n_use_error_defer();
        
        File f = FileOpen(path, "r").n_try();
        n_defer { f.Close(); };
        
        f.SeekEnd().n_try();
        int64 fileSize = f.Tell().n_try();
        f.Seek(0).n_try();
        
        List<uint8> bytes = bytes.Init(n_ref alloc, fileSize);
        bytes.Resize(fileSize).n_try();
        usize bytesRead;
        f.Read(bytes.ToView(), bytesRead).n_try();
        n_check_eq(bytesRead, bytes.Len);
        return bytes;
    }

    inline n_result<void> FileWriteAll(n_view<const char> path, n_view<const uint8> data)
    {
        n_use_error_defer();
        
        File f = FileOpen(path, "w").n_try();
        n_defer { f.Close(); };
        
        f.Seek(0).n_try();
        usize bytesWrote;
        f.Write(data, bytesWrote).n_try();
        n_check_eq(bytesWrote, data.len);
        return {};
    }

    inline n_result<DirIterator> 
    OpenDirectory(n_view<const char> path, bool sorted = false) n_defer_with(DirIterator.Close)
    {
        n_array<char, 512> pathBuf;
        if(path.len >= pathBuf.len)
            return n_error_msg("Path too long (%zu): %.*s", path.len, (int)path.len, path.data);
        
        path.copy_to(pathBuf.to_view());
        pathBuf[path.len] = '\0';
        
        DirIterator iter = {};
        int ret;
        if(!sorted)
            ret = tinydir_open(&iter.Dir, pathBuf.data);
        else
            ret = tinydir_open_sorted(&iter.Dir, pathBuf.data);
        if(ret != 0)
            return n_error_msg("Failed to open directory '%s'", pathBuf.data);

        return iter;
    }

    inline n_result<bool> IsDirectory(n_view<const char> path)
    {
        n_array<char, 512> pathBuf;
        if(path.len >= pathBuf.len)
            return n_error_msg("Path too long (%zu)", path.len);
        
        path.copy_to(pathBuf.to_view());
        pathBuf[path.len] = '\0';

        #if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
            struct stat st;
            if(stat(pathBuf.data, &st) != 0)
                return n_error_msg("Failed to stat path: %s", strerror(errno));

            return S_ISDIR(st.st_mode);
        #elif defined(_WIN32)
            struct _stat st;
            if(_stat(pathBuf.data, &st) != 0)
                return n_error_msg("Failed to stat path: %s", strerror(errno));

            return (_S_IFDIR & st.st_mode) != 0;
        #else
            #error "Unsupported platform"
        #endif
    }

    inline n_result<bool> PathExists(n_view<const char> path)
    {
        n_array<char, 512> pathBuf;
        if(path.len >= pathBuf.len)
            return n_error_msg("Path too long (%zu)", path.len);
        
        path.copy_to(pathBuf.to_view());
        pathBuf[path.len] = '\0';
        
        #if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
            struct stat st;
            int ret = stat(pathBuf.data, &st);
        #elif defined(_WIN32)
            int ret = _access(pathBuf.data, 0);
        #else //NOTE: Fallback. Try file first, then try dir.
            FILE* f = fopen(pathBuf.data, modeStrView.data);
            int ret = f == NULL ? 0 : 1;
            if(!f)
                fclose(f);
            
            if(ret == 1)
            {
                n_result<bool> isDir = IsDirectory(path);
                if(!isDir.err && isDir.value)
                    ret = 0;
            }
        #endif

        return ret == 0;
    }

#ifdef _WIN32
    inline n_result<n_view<char>> GetWin32ErrorString(DWORD err, n_view<char> outBuf)
    {
        LPSTR msg = NULL;
        DWORD len = FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | //dwFlags
                                    FORMAT_MESSAGE_IGNORE_INSERTS | 
                                    FORMAT_MESSAGE_ALLOCATE_BUFFER, 
                                    NULL, //lpSource
                                    err, //dwMessageId
                                    0, //dwLanguageId
                                    (LPSTR)&msg, //lpBuffer
                                    0, //nSize
                                    NULL); //Arguments
        n_defer { LocalFree(msg); };
        n_check_lt(len, 0);
        while(len > 0 && (msg[len-1] == '\n' || msg[len-1] == '\r' || msg[len-1] == ' '))
            len--;
        
        n_view<const char> msgView = { msg, len };
        n_check_lte(len, outBuf.len);
        msgView.copy_to(outBuf);
        
        return outBuf.sub(0, len);
    }
#endif

    inline n_result<void> CreateDirectory(n_view<const char> path)
    {
        n_array<char, 512> pathBuf;
        if(path.len >= pathBuf.len)
            return n_error_msg("Path too long (%zu)", path.len);
        
        path.copy_to(pathBuf.to_view());
        pathBuf[path.len] = '\0';

        #if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
            if(mkdir(pathBuf.data, 0755) != 0)
            {
                if(errno != EEXIST)
                    return n_error_msg("Failed to create directory: %s", strerror(errno));
            }
        #elif defined(_WIN32)
            if(::CreateDirectoryA(pathBuf.data, NULL) == 0)
            {
                DWORD err = GetLastError();
                if(err != ERROR_ALREADY_EXISTS)
                {
                    n_array<char, 1024> msgBuf;
                    n_result<n_view<char>> msgResult = GetWin32ErrorString(err, msgBuf.to_view());
                    if(msgResult.err)
                        return n_error_msg("Failed to create directory: Win32 error %lu", err);
                    
                    return n_error_msg( "Failed to create directory: %.*s", 
                                        (int)msgResult.value.len, 
                                        msgResult.value.data);
                }
            }
        #else
            #error "Unsupported platform"
        #endif

        return {};
    }

    inline n_result<void> DeleteFile(n_view<const char> path)
    {
        n_array<char, 512> pathBuf;
        if(path.len >= pathBuf.len)
            return n_error_msg("Path too long (%zu)", path.len);
        
        path.copy_to(pathBuf.to_view());
        pathBuf[path.len] = '\0';

        if(remove(pathBuf.data) != 0)
            return n_error_msg("Failed to delete file '%s': %s", pathBuf.data, strerror(errno));

        return {};
    }
    
    inline n_result<void> DeleteDirectory(n_ref Allocator& alloc, n_view<const char> path)
    {
        n_use_error_defer();
        
        List<String> stack;
        stack = stack.Init(n_ref alloc, 32);
        n_defer 
        {
            for(int i = 0; i < stack.Len; ++i)
                stack.At(i).Free();
            stack.Free();
        };
        
        {
            String s = s.Init(n_ref alloc, 256);
            s.AppendString(path).n_try();
            stack.Add(s).n_try();
        }

        while(stack.Len > 0)
        {
            uint64 currentDirIndex = stack.Len - 1;
            String currentDir = stack.At(currentDirIndex);

            DirIterator iter = OpenDirectory(currentDir.ToView()).n_try();
            n_defer { iter.Close(); };
            
            while(true) //Remove all the files in the directory
            {
                bool hasNext = iter.Next().n_try();
                if(!hasNext)
                    break;

                DirEntry entry = iter.Current().n_try();
                
                if(entry.Name == ".." || entry.Name == ".")
                    continue;
                
                String entryPath = entryPath.Init(n_ref alloc, currentDir.Len() + 1 + entry.Name.len);
                n_error_defer { entryPath.Free(); };
                entryPath.AppendString(currentDir.ToView()).n_try();
                entryPath.Add('/').n_try();
                entryPath.AppendString(entry.Name).n_try();

                if(entry.IsDirectory)
                {
                    stack.Add(entryPath).n_try();
                }
                else
                {
                    DeleteFile(entryPath.ToView()).n_try();
                    entryPath.Free().n_try();
                }
            }
            
            //Still have sub-directories, can't remove current directory yet
            if(stack.Len != currentDirIndex + 1)
                continue;

            #if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
                if(rmdir(currentDir.Data()) != 0)
                {
                    return n_error_msg( "Failed to remove directory '%s': %s", 
                                        currentDir.Data(), 
                                        strerror(errno));
                }
            #elif defined(_WIN32)
                if(RemoveDirectoryA(currentDir.Data()) == 0)
                {
                    unsigned long err = GetLastError();
                    n_array<char, 1024> msgBuf;
                    n_result<n_view<char>> msgResult = GetWin32ErrorString(err, msgBuf.to_view());
                    if(msgResult.err)
                    {
                        return n_error_msg( "Failed to remove directory '%s': Win32 error %lu", 
                                            currentDir.Data(), 
                                            err);
                    }
                    
                    return n_error_msg( "Failed to remove directory '%s': %.*s", 
                                        currentDir.Data(), 
                                        (int)msgResult.value.len, 
                                        msgResult.value.data);
                }
            #else
                #error "Unsupported platform"
            #endif
            
            stack.Remove(stack.Len - 1).n_try();
            currentDir.Free().n_try();
        } //while(stack.Len > 0)

        return {};
    }
    
    inline n_result<void> DeletePath(n_ref Allocator& alloc, n_view<const char> path)
    {
        bool isDir = IsDirectory(path).n_try();
        if(isDir)
        {
            DeleteDirectory(n_ref alloc, path).n_try();
        }
        else
        {
            DeleteFile(path).n_try();
        }
        
        return {};
    }

    inline n_result<void> Rename(n_view<const char> oldPath, n_view<const char> newPath)
    {
        n_array<char, 512> oldBuf;
        n_array<char, 512> newBuf;
        if(oldPath.len >= oldBuf.len)
            return n_error_msg("Old path too long (%zu)", oldPath.len);
        if(newPath.len >= newBuf.len)
            return n_error_msg("New path too long (%zu)", newPath.len);

        oldPath.copy_to(oldBuf.to_view());
        oldBuf[oldPath.len] = '\0';
        newPath.copy_to(newBuf.to_view());
        newBuf[newPath.len] = '\0';

        #if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
            if(rename(oldBuf.data, newBuf.data) != 0)
            {
                return n_error_msg("Failed to rename '%.*s' to '%.*s': %s",
                                    (int)oldPath.len, oldPath.data,
                                    (int)newPath.len, newPath.data,
                                    strerror(errno));
            }
        #elif defined(_WIN32)
            if(MoveFileA(oldBuf.data, newBuf.data) == 0)
            {
                DWORD err = GetLastError();
                n_array<char, 1024> msgBuf;
                n_result<n_view<char>> msgResult = GetWin32ErrorString(err, msgBuf.to_view());
                if(msgResult.err)
                {
                    return n_error_msg("Failed to rename '%.*s' to '%.*s': Win32 error %lu",
                                        (int)oldPath.len, oldPath.data,
                                        (int)newPath.len, newPath.data,
                                        err);
                }

                return n_error_msg("Failed to rename '%.*s' to '%.*s': %.*s",
                                    (int)oldPath.len, oldPath.data,
                                    (int)newPath.len, newPath.data,
                                    (int)msgResult.value.len, msgResult.value.data);
            }
        #else
            #error "Unsupported platform"
        #endif

        return {};
    }
}

#endif
