#if defined(_WIN32)
#include <Windows.h>
#endif
#include "common.h"

bool alloc_commit(size_t requested_size, u8 **out) {
#if defined(_WIN32)
    *out = (u8 *)VirtualAlloc(NULL, requested_size, MEM_COMMIT | MEM_RESERVE,
                              PAGE_READWRITE);
    return *out != NULL;
#else
    todo("memory allocation on non-win32 platforms");
#endif
}

// NOTE(yousef): assumes pathname is valid
bool read_file(char const *pathname, byte_slice *buffer) {
#if defined(_WIN32)
    HANDLE file_handle =
        CreateFileA(pathname, GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        panic("unable to open \"%s\" for reading. Last Error: %lu", pathname,
              GetLastError());
    }
    DWORD _file_size_hi = 0;
    DWORD _file_size_lo = GetFileSize(file_handle, &_file_size_hi);
    DWORD file_size = _file_size_lo;
    if (_file_size_hi != 0 || file_size > buffer->len) {
        panic("file too big");
    }

    DWORD bytes_read = 0;
    bool result =
        ReadFile(file_handle, (void *)buffer->at, file_size, &bytes_read, NULL);

    if (file_size == bytes_read) {
        dbg("read: %lu bytes from %s", file_size, pathname);
    } else {
        DWORD last_error = GetLastError();
        panic("unable to read file. file_size: %lu, bytes_read: "
              "%lu, last_error: %lu",
              file_size, bytes_read, last_error);
    }

    buffer->len = file_size;
#else

#endif
}
