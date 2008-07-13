#include <cstdio>
#include <cctype>
#include "memory.h"
#include "common.h"

#if !defined (WIN32)
	#include <sys/select.h>	/* fd_set */
	#include <netinet/in.h>	/* sockaddr_in */
	#include <netdb.h>
	#include <unistd.h>
#else
	#include <winsock.h>
#endif
#include "utils.h"
#include "conf.h"
#include "qw.h"
#include "sockets.h"


struct HttpTarget {
	std::string host;
	short port;
	std::string script;
};

typedef std::list<HttpTarget> http_targets_t;
static http_targets_t http_targets;

void HTTP_Init(void)
{
}

bool ParseURL(const char *url, std::string & server, short & port, std::string & script)
{
	if (!strstarts(url, "http://")) {
		return false;
	}

	const char* p = url + sizeof("http://") - 1;

	const char* slash = strchr(p, '/');
	const char* colon = strchr(p, ':');
	if (!slash) {
		return false;
	}

	if (colon && colon < slash) {
		server = std::string(p,colon-p);
		port = atoi(colon+1);
	}
	else {
		server = std::string(p,slash-p);
		port = HTTP_DEFAULT_PORT;
	}

	script = std::string(slash);

	return true;
}

static char *url_encode(const char *s)
{
	static char innerbuf[256] = "";
	int bp = 0;

	innerbuf[0] = '\0';

	for (; *s && bp < 256; *s++)
	{
		if (isalnum(*s)) {
			innerbuf[bp++] = *s;
			innerbuf[bp] = '\0';
		}
		else {
			char enc[8];

			snprintf(enc, 8, "%%%x", *s);

			strlcat(innerbuf, enc, 256);
			bp += strlen(enc);
		}
	}

	return innerbuf;
}

static void add_http_query(char *s, size_t bufsize, const char *key, const char *value)
{
	if (!bufsize) return;

	if (s[0]) {
		strlcat(s, "&", bufsize);
	}

	strlcat(s, url_encode(key), bufsize);
	strlcat(s, "=", bufsize);
	strlcat(s, url_encode(value), bufsize);
}

void HTTP_SendOne(const serverinfo & s, const char *ip, short port, playerscore *scores, bool teamplay, const char *http_host, short http_port, const char *http_script)
{
	size_t numscores = s.players.size();
	SOCKET http_socket = getsocktcp(ip, port);

	if (http_socket == INVALID_SOCKET) {
		printf("Couldn't connect to %s on HTTP port (%d)\n", ip, (int) port);
		return;
	}

	char query[HTTP_REQUEST_BUFFER] = "";
	
	char ports[16];
	snprintf(ports, 16, "%d", port);
	
	add_http_query(query, HTTP_REQUEST_BUFFER, "ip", ip);
	add_http_query(query, HTTP_REQUEST_BUFFER, "port", ports);
	add_http_query(query, HTTP_REQUEST_BUFFER, "host", s.GetEntryStr("hostname"));
	add_http_query(query, HTTP_REQUEST_BUFFER, "map", s.GetEntryStr("map"));
	add_http_query(query, HTTP_REQUEST_BUFFER, "teamplay", teamplay ? "1" : "0");
	
	char players_str[16];
	snprintf(players_str, 16, "%u", s.players.size());
	add_http_query(query, HTTP_REQUEST_BUFFER, "players", players_str);

	for (unsigned int i = 0; i < numscores; ++i) {
		// nick
		char keybuf[32];
		snprintf(keybuf, 32, "nick[%d]", i);
		add_http_query(query, HTTP_REQUEST_BUFFER, keybuf, scores[i].name.c_str());
		
		// frags
		snprintf(keybuf, 32, "frags[%d]", i);
		char valbuf[16];
		snprintf(valbuf, 16, "%d", scores[i].frags);
		add_http_query(query, HTTP_REQUEST_BUFFER, keybuf, valbuf);

		if (teamplay) {
			// team
			snprintf(keybuf, 32, "team[%d]", i);
			add_http_query(query, HTTP_REQUEST_BUFFER, keybuf, scores[i].team.c_str());
		}
	}

	char request[HTTP_REQUEST_BUFFER];
	snprintf(request, HTTP_REQUEST_BUFFER,
		"POST %s HTTP/1.1\r\n"
		"Host: %s \r\n"
		"Connection: close\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: %d\r\n\r\n%s\r\n",
		http_script, http_host, strlen(query), query);	

	// printf("%s",request);
	if (send(http_socket, request, strlen(request) + 1, 0) < 0) {
		printf("Couldn't send to HTTP server\n");
		return;
	}

	// just forget the result anyway
	int l = recv(http_socket, request, HTTP_REQUEST_BUFFER, 0);
	if (l > 0) {
		request[l] = '\0';
	}
	if (!strstarts(request, "HTTP/1.1 200 OK")) {
		printf("HTTP server '%s' replied with an error:\n", ip);
		printf("%s\n---\n", request);
	}

	closesocket(http_socket);
}

void HTTP_AddTarget(const char *url, bool &)
{
	HttpTarget t;

	if (ParseURL(url, t.host, t.port, t.script)) {
		http_targets.push_back(t);
	}
	else {
		printf("Error: Couldn't parse URL '%s'\n", url);
	}
}

void HTTP_Send(const serverinfo & s, const char *ip, short port, playerscore *scores, bool teamplay)
{
	http_targets_t::const_iterator c, e;

	for (c = http_targets.begin(), e = http_targets.end(); c != e; c++) {
		HTTP_SendOne(s, ip, port, scores, teamplay,	c->host.c_str(), c->port, c->script.c_str());
	}
}

