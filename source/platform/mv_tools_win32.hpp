#pragma once

#include "../mv_tools.hpp"
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace mv_tools
{
    mv_handle map(size_t size)
    {
        auto ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        auto mapped = static_cast<mv_handle>(ptr);
        return mapped;
    }

    bool unMap(mv_handle mapped)
    {
        const bool result = VirtualFree(mapped, 0, MEM_RELEASE);
        return result;
    }
}

namespace io
{
    bool close(file_t *file)
    {
        const bool result = mv_tools::unMap(file->handle);
        file->handle = nullptr;
        file->size = 0;
        return result;
    }

    file_t read(const char *filename)
    {
        HANDLE fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        LARGE_INTEGER fileSize;
        file_t result{};

        if((fileHandle != INVALID_HANDLE_VALUE) && GetFileSizeEx(fileHandle, &fileSize)){
            uint32_t fileSize32 = static_cast<uint32_t>(fileSize.QuadPart);
            result.handle = mv_tools::map(fileSize.QuadPart);
            if(result.handle){
                DWORD bytesRead;
                if(ReadFile(fileHandle, result.handle, fileSize32, &bytesRead, 0) && fileSize32 == bytesRead){
                    result.size = fileSize32;
                }
                else{
                    mv_tools::unMap(result.handle);
                    result.handle = nullptr;
                }
            }
            CloseHandle(fileHandle);
        }
        return result;
    }

    bool write(const char *filename, file_t *file)
    {
        bool result = false;
        HANDLE fileHandle = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if(fileHandle != INVALID_HANDLE_VALUE){
            DWORD bytesWritten;
            if(WriteFile(fileHandle, file->handle, static_cast<DWORD>(file->size), &bytesWritten, 0))
                result = (file->size == bytesWritten);

            CloseHandle(fileHandle);
        }
        return result;
    }

    bool fileExists(const char *filename)
    {
        WIN32_FIND_DATA findData{};
        HANDLE hFindFile = FindFirstFileA(filename, &findData);
        bool result = false;
        if(hFindFile != INVALID_HANDLE_VALUE){
            result = true;
            FindClose(hFindFile);
        }
        return result;
    }
}