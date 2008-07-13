#ifndef _WIN32
typedef int SOCKET;
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET (-1)
#define closesocket(s) close(s)
#define stricmp(x,y) strcasecmp(x,y)
#include <netinet/in.h>
#endif

SOCKET getsock(const char *host, unsigned short port, int family, int socktype, int protocol);
#define getsockudp(host,port) getsock(host, port, AF_INET, SOCK_DGRAM, IPPROTO_UDP)
#define getsocktcp(host,port) getsock(host, port, AF_INET, SOCK_STREAM, IPPROTO_TCP)

char *getpacket(SOCKET s, size_t *len, long timeout_sec, long timeout_usec);
