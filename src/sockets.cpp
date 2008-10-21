/*
	Copyright (c) 2004 Cory Nelson

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files (the
	"Software"), to deal in the Software without restriction, including
	without limitation the rights to use, copy, modify, merge, publish,
	distribute, sublicense, and/or sell copies of the Software, and to
	permit persons to whom the Software is furnished to do so, subject to
	the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdio>
#include <cstddef>
#include <cstdlib>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
typedef int SOCKET;
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET (-1)
#define closesocket(s) close(s)
#define stricmp(x,y) strcasecmp(x,y)
#endif


SOCKET getsock(const char *host, unsigned short port, int family, int socktype, int protocol) {
	struct addrinfo hints={0};
	struct addrinfo *info, *iter;
	char portstr[32];

	sprintf(portstr, "%d", (int)port);

	hints.ai_family=family;
	hints.ai_socktype=socktype;
	hints.ai_protocol=protocol;

	if(getaddrinfo(host, portstr, &hints, &info)) {
		return INVALID_SOCKET;
	}

	for(iter=info; iter!=NULL; iter=iter->ai_next) {
		SOCKET sock=socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);
		if(sock==INVALID_SOCKET) continue;

		if(connect(sock, iter->ai_addr, (int)iter->ai_addrlen)!=SOCKET_ERROR) {
			freeaddrinfo(info);

			return sock;
		}
		else closesocket(sock);
	}

	freeaddrinfo(info);

	return INVALID_SOCKET;
}

char *getpacket(SOCKET s, size_t *len, long timeout_sec, long timeout_usec)
{
	fd_set set;
	struct timeval socktimeout={timeout_sec, timeout_usec};

	FD_ZERO(&set);
	FD_SET(s, &set);

	if(select(FD_SETSIZE, &set, NULL, NULL, &socktimeout)>0) {
		char *buf=(char*)calloc(10241, sizeof(char));
		int ret=recv(s, buf, 10240, 0);

		if(ret!=SOCKET_ERROR && ret!=0) {
			*len=ret;
			return (char*)realloc(buf, ret+1);
		}
		else {
#if defined(_DEBUG) && defined(WIN32)
			fprintf(stderr, "%d\n", WSAGetLastError());
#endif
			free(buf);
			return NULL;
		}
	}
	else return NULL;
}
