#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include "conf.h"
#include "common.h"
#include "utils.h"
#ifdef WIN32
#include "windows.h"
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include "qw.h"
#include "sockets.h"

// from MVDSV sources
// STATUS_OLDSTYLE              0
// STATUS_SERVERINFO            1
// STATUS_PLAYERS               2
// STATUS_SPECTATORS            4
// STATUS_SPECTATORS_AS_PLAYERS	8  //for ASE - change only frags: show as "S"
// STATUS_SHOWTEAMS             16
// we don't want spectators, just players, serverinfo and we also want teams
// that's 1+2+16 = 19
#define QW_SERVER_QUERY "\xff\xff\xff\xffstatus 19"
#define QW_SERVER_QUERY_LEN (sizeof(QW_SERVER_QUERY)-1)
#define QW_REPLY_HEADER "\xff\xff\xff\xffn"
#define QW_REPLY_HEADER_LEN (sizeof(QW_REPLY_HEADER)-1)
#define QW_MASTER_QUERY "c\n"
#define QW_MASTER_REPLY_BUFFER 10240


static char readableChars[256] = {	'.', '_' , '_' , '_' , '_' , '.' , '_' , '_' , '_' , '_' , '\n' , '_' , '\n' , '>' , '.' , '.',
						'[', ']', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '_', '_', '_'};

static void CreateReadableChars(void) {
	int i;

	for (i = 32; i < 127; i++)
		readableChars[i] = readableChars[128 + i] = i;
	readableChars[127] = readableChars[128 + 127] = '_';

	for (i = 0; i < 32; i++)
		readableChars[128 + i] = readableChars[i];
	readableChars[128] = '_';
	readableChars[10 + 128] = '_';
	readableChars[12 + 128] = '_';
}

#define readable(x) (readableChars[(unsigned char) x])

void QW_ScanSource(const char *ip, short port, NewServer_fnc report, void *arg)
{
    // get servers from master server
    char request[] = QW_MASTER_QUERY;
	SOCKET newsocket = getsockudp(ip, port);
    int ret, i;
    unsigned char answer[QW_MASTER_REPLY_BUFFER];
    fd_set fd;
	struct timeval tv;

	printf("Scanning master server %s:%d\n",ip,(int) port);
	if (newsocket == INVALID_SOCKET) {
		printf("Couldn't resolve '%s'\n", ip);
#ifndef WIN32
		herror("gethostbyname");
#endif
	}

	ret = send(newsocket, request, sizeof(request), 0);
				  
	if (ret < 0) {
		printf("Couldn't send query to master server\n");
		closesocket(newsocket);
        return;
	}

	FD_ZERO(&fd);
	FD_SET(newsocket, &fd);
    tv.tv_sec = QW_MASTER_TIMEOUT_SECS;
    tv.tv_usec = 0;
    ret = select(newsocket+1, &fd, NULL, NULL, &tv);

    // get answer
	if (ret > 0) {
        ret = recvfrom (newsocket, (char *) answer, QW_MASTER_REPLY_BUFFER, 0, NULL, NULL);

		if (ret > 0  &&  ret < QW_MASTER_REPLY_BUFFER)
		{
			answer[ret] = 0;

			if (memcmp(answer, "\xff\xff\xff\xff\x64\x0a", 6))
			{
				closesocket(newsocket);
				printf("Wrong reply from the master server\n");
				return;
			}

			int c;
			for (c=0, i=6; i+5 < ret; i+=6, c++)
			{
				char ip[64];
				int port = 256 * (int)answer[i+4] + (int)answer[i+5];

				snprintf(ip, sizeof(ip), "%u.%u.%u.%u",
					(int)answer[i+0], (int)answer[i+1],
					(int)answer[i+2], (int)answer[i+3]);
	                
				report(ip, port, arg, c);
				// c->Add(new ServerScan(ip, port, c, (apptime) time));
			}
		}
		else {
			printf("Master server returned %d bytes\n",ret);
		}
	}
	else {
		printf("No reply from master server %s:%d\n",ip,port);
	}

    closesocket(newsocket);
}

char *QW_QueryGetRawReply(const char *ip, short port)
{
	SOCKET s=getsockudp(ip, port);
	if (s==INVALID_SOCKET) {
		return NULL;
	}

	if (send(s, QW_SERVER_QUERY, QW_SERVER_QUERY_LEN, 0) == SOCKET_ERROR) {
		closesocket(s);
		return 0;
	}

	size_t packetlen;
	char *packet = getpacket(s, &packetlen, QW_SERVER_QUERY_TIMEOUT_SEC, QW_SERVER_QUERY_TIMEOUT_USEC);
	if (!packet) {
		closesocket(s);
		return 0;
	}
	closesocket(s);

	return packet;
}

static void SkipNum(char **d)
{
	// numbers can be negative
	if (**d == '-') {
		(*d)++;
	}
	// skip digits
	while (**d && isdigit(**d)) {
		(*d)++;
	}
	// skip trailing whitespace(s)
	while (**d == ' ') {
		(*d)++;
	}
}

static bool MatchString(char **d, std::string & s)
{
	if (**d == '\"') {
		(*d)++;
		while (**d && **d != '\"') {
			s += readable(**d);
			(*d)++;
		}
		if (**d == '\"') {
			(*d)++;
		}
		else {
			return false;
		}
	}

	while (**d == ' ') {
		(*d)++;
	}

	return true;
}

bool QW_ParseQWReply(char *d, serverinfo & svinfo)
{
	if (!strstarts(d, QW_REPLY_HEADER)) {
		return false;
	}
	else {
		d += QW_REPLY_HEADER_LEN;
	}

	// server info
	while (1) {
		if (*d == '\x0A') {
			break;
		}
		else if (*d == '\\') {
			d++;

			std::string key;
			std::string val;
			
			while (*d && *d != '\\') {
				key += *d++;
			}

			if (*d == '\\') {
				d++;
			}
			else {
				return false;
			}

			while (*d && *d != '\\' && *d != '\x0A') {
				val += *d++;
			}

			svinfo.entries.insert(std::pair <std::string, std::string> (key, val));
		}
		else {
			return false;
		}
	}

	// players' info
	while (*d) {
		playerinfo p;

		if (*d == '\0') {
			break;
		}
		else if (*d == '\x0A') {
			d++;

			if (*d == '\0') {
				break;
			}
			
			p.userid = atoi(d);
			SkipNum(&d);

			p.frags = atoi(d);
			SkipNum(&d);

			p.time = atoi(d);
			SkipNum(&d);

			p.ping = atoi(d);
			SkipNum(&d);

			if (!MatchString(&d, p.name)) {
				return false;
			}

			if (!MatchString(&d, p.skin)) {
				return false;
			}

			p.color1 = atoi(d);
			SkipNum(&d);

			p.color2 = atoi(d);
			SkipNum(&d);

			if (*d == '\"') {
				if (!MatchString(&d, p.team)) {
					return false;
				}
			}

			svinfo.players.push_back(p);
		}
		else {
			return false;
		}
	}

	return true;
}

bool QW_QueryGetReply(const char *ip, short port, serverinfo & svinfo)
{
	int i;
	char *f = NULL;

	for (i = 1; i <= QW_SERVER_QUERY_ATTEMPTS; i++)
	{
		f = QW_QueryGetRawReply(ip, port);
		if (f) break;
	}
	
	if (!f) {
		return false;
	}

	bool r = QW_ParseQWReply(f, svinfo);
	free(f);
	
	if (!r) {
		printf("Illegal reply from %s:%d\n", ip, port);
	}

	return r;
}

void QW_ScanSV(const char *ip, short port, server_status & status, unsigned int & timeleft,
			  serverinfo & svinfo)
{
	if (!QW_QueryGetReply(ip, port, svinfo)) {
		status = SVST_dead;
		return;
	}
	
	serverinfo::entries_t::iterator i = svinfo.entries.find("status");

	if (i == svinfo.entries.end()) {
		status = SVST_nostatus;
		return;
	}

	status = SVST_unknownstatus;
	const char *v = i->second.c_str();;
	timeleft = (unsigned int) atoi(v);

	if (strcmp(v, "Standby") == 0) {
		status = SVST_standby;
	}
	else if (strcmp(v, "Countdown") == 0) {
		status = SVST_match;
		serverinfo::entries_t::iterator tl = svinfo.entries.find("timelimit");
		if (tl != svinfo.entries.end()) {
			timeleft = atoi(tl->second.c_str());
		}
		else {
			timeleft = 0;
		}
	}
	else {
		while (*v && isdigit(*v)) v++;
		while (*v && *v == ' ') v++;
	
		if (*v && strstarts(v, "min left")) {
			status = SVST_match;
		}
		else {
			status = SVST_unknownstatus;
			printf("Unsupported status %s\n", i->second.c_str());
		}
	}
}

const char* serverinfo::GetEntryStr(const char *key) const
{
	entries_t::const_iterator i = entries.find(key);
	if (i == entries.end()) {
		return "";
	}
	else {
		return i->second.c_str();
	}
}

int serverinfo::GetEntryInt(const char* key) const
{
	entries_t::const_iterator i = entries.find(key);
	if (i == entries.end()) {
		return 0;
	}
	else {
		return atoi(i->second.c_str());
	}	
}

void QWInit(void)
{
	CreateReadableChars();
}
