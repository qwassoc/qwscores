// pseudo-library functions

// only for string constants!
#define strstarts(needle, str) (strncmp(needle,str,sizeof(str)-1)==0)

#define streq(str1,str2) (strcmp((str1),(str2))==0)


// string manipulation

#ifdef _WIN32

#include <cstring>

int snprintf(char *buffer, size_t count, __format_string char const *format, ...);

int vsnprintf(char *buffer, size_t count, const char *format, va_list argptr);

#endif

#if defined(__linux__) || defined(_WIN32)

size_t strlcpy(char *dst, const char *src, size_t siz);

size_t strlcat(char *dst, const char *src, size_t siz);

#endif


// multi-platform thread create

#ifndef _WIN32
#define DWORD void*
#define WINAPI
#else
#include <windows.h>
#endif

int  Sys_CreateThread(DWORD (WINAPI *func)(void *), void *param);
