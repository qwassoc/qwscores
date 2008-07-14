#include <cstdarg>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif
#include "utils.h"

#ifdef _WIN32
int snprintf(char *buffer, size_t count, char const *format, ...)
{
	int ret;
	va_list argptr;
	if (!count) return 0;
	va_start(argptr, format);
	ret = _vsnprintf(buffer, count, format, argptr);
	buffer[count - 1] = 0;
	va_end(argptr);
	return ret;
}
int vsnprintf(char *buffer, size_t count, const char *format, va_list argptr)
{
	int ret;
	if (!count) return 0;
	ret = _vsnprintf(buffer, count, format, argptr);
	buffer[count - 1] = 0;
	return ret;
}
#endif

#if defined(__linux__) || defined(_WIN32)
size_t strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
							*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

size_t strlcat(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));       /* count does not include NUL */
}
#endif

void Sys_Sleep_ms(unsigned int miliseconds)
{
#ifdef WIN32
	Sleep(miliseconds);
#else
	if ((miliseconds % 1000) == 0) {
		sleep(miliseconds/1000); // convert to seconds
	}
	else {
		usleep(miliseconds*1000); // convert to microseconds
	}
#endif
}

#ifdef _WIN32
int Sys_CreateThread(DWORD (WINAPI *func)(void *), void *param)
{
    DWORD threadid;
    HANDLE thread;

    thread = CreateThread (
        NULL,               // pointer to security attributes
        0,                  // initial thread stack size
        func,               // pointer to thread function
        param,              // argument for new thread
        CREATE_SUSPENDED,   // creation flags
        &threadid);         // pointer to receive thread ID

    SetThreadPriority(thread, THREAD_PRIORITY_HIGHEST);
    ResumeThread(thread);

    return 1;
}
#else
int  Sys_CreateThread(DWORD WINAPI (*func)(void *), void *param)
{
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setschedpolicy(&attr, SCHED_OTHER);

    pthread_create(&thread, &attr, func, param);
    return 1;
}
#endif
