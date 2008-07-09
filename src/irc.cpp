#ifndef WITHOUT_IRC
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "memory.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#include "conf.h"
#include "common.h"
#include "utils.h"
#include "libircclient.h"
#include "qw.h"


void IRCConnect(void);


struct IRCSupport {
	irc_callbacks_t callbacks;
	irc_session_t *session;
	bool ready;

	IRCSupport() : ready(false), session(0) {}
};
static IRCSupport IRC;

static void IRC_event_connect (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	irc_cmd_join(IRC.session, IRC_CHANNEL, IRC_KEY);
}

static void IRC_event_join (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	IRC.ready = true;
}

static void IRC_event_general(irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	printf("%s %s", event, origin);
	for (unsigned int i = 0; i < count; ++i) {
		printf(" %s", params[i]);
	}
	printf("\n");
}

static void IRC_event_numeric(irc_session_t * session, unsigned int event, const char * origin, const char ** params, unsigned int count)
{
	printf("%d %s", event, origin);
	for (unsigned int i = 0; i < count; ++i) {
		printf(" %s", params[i]);
	}
	printf("\n");
}

static DWORD WINAPI IRCRUN(void *param)
{
	irc_run(IRC.session);

	return 0;
}

void IRC_Send(const serverinfo & s, const char *ip, short port, playerscore *scores, bool teamplay)
{
	if (!IRC.ready) return;

	typedef std::map<std::string, int> teamfrags_t;
	teamfrags_t teamfrags;

	char nicksline[256] = "[COLOR=TEAL]";

	for (unsigned int i = 0; i < s.players.size(); ++i) {
		char nickentry[64];

		if (teamplay) {
			snprintf(nickentry, sizeof(nickentry), "%s %s (%d)", scores[i].team.c_str(), scores[i].name.c_str(), scores[i].frags);
			if (teamfrags.find(scores[i].team) == teamfrags.end()) {
				teamfrags.insert(std::pair <std::string, int> (scores[i].team.c_str(), scores[i].frags));
			}
			else {
				teamfrags[scores[i].team] += scores[i].frags;
			}
		}
		else {
			snprintf(nickentry, sizeof(nickentry), "%s (%d)", scores[i].name.c_str(), scores[i].frags);
		}
		strlcat(nicksline, nickentry, sizeof(nicksline));
		if (i != s.players.size()-1) {
			strlcat(nicksline, " | ", sizeof(nicksline));
		}
	}

	char teamsline[256] = "[COLOR=TEAL]";
	if (teamplay) {
		bool first = true;

		for (teamfrags_t::const_iterator i = teamfrags.begin(); i != teamfrags.end(); i++) {
			if (!first) {
				strlcat(teamsline, " | ", sizeof(teamsline));
			}
			first = false;
			
			char teamentry[64];
			snprintf(teamentry, sizeof(teamentry), "%s [%d]", i->first.c_str(), i->second);

			strlcat(teamsline, teamentry, sizeof(teamsline));
		}
	}

	char msg1[256];
	char *colored;
	
	snprintf(msg1, sizeof(msg1), "[COLOR=OLIVE]##[/COLOR] [B]%s[B] (%s:%d) | [B]%s[/B]", s.GetEntryStr("hostname"), ip, port, s.GetEntryStr("map"));
	colored = irc_color_convert_to_mirc(msg1);
	irc_cmd_msg(IRC.session, IRC_CHANNEL, colored);
	free(colored);

	if (teamplay) {
		colored = irc_color_convert_to_mirc(teamsline);
		irc_cmd_msg(IRC.session, IRC_CHANNEL, colored);
		free(colored);
	}

	colored = irc_color_convert_to_mirc(nicksline);
	irc_cmd_msg(IRC.session, IRC_CHANNEL, colored);
	free(colored);
}

void IRCRaw(const char *cmd)
{
	if (streq(cmd, "connect")) {
		IRCConnect();
	}
	else {
		irc_send_raw(IRC.session, "%s", cmd);
	}
}

void IRCInit(void)
{
	IRC.callbacks.event_connect = IRC_event_connect;
	IRC.callbacks.event_join = IRC_event_join;

	IRC.callbacks.event_numeric = IRC_event_numeric;
	IRC.callbacks.event_ctcp_req = IRC_event_general;
	IRC.callbacks.event_privmsg = IRC_event_general;
	IRC.callbacks.event_channel = IRC_event_general;
	IRC.callbacks.event_nick = IRC_event_general;
	IRC.callbacks.event_part = IRC_event_general;
	IRC.callbacks.event_ctcp_action = IRC_event_general;
	IRC.callbacks.event_ctcp_rep = IRC_event_general;
	IRC.callbacks.event_invite = IRC_event_general;
	IRC.callbacks.event_kick = IRC_event_general;
	IRC.callbacks.event_mode = IRC_event_general;
	IRC.callbacks.event_notice = IRC_event_general;
	IRC.callbacks.event_quit = IRC_event_general;
	IRC.callbacks.event_umode = IRC_event_general;
	IRC.callbacks.event_unknown = IRC_event_general;
	IRC.callbacks.event_topic = IRC_event_general;

	IRC.session = irc_create_session(&IRC.callbacks);
}

void IRCConnect(void)
{
	if (IRC.session) {
		if (irc_connect(IRC.session, IRC_SERVER, IRC_PORT, IRC_PASSWORD, IRC_NICK, IRC_USERNAME, IRC_REALNAME) == 0) {
			Sys_CreateThread(IRCRUN, 0);
		}
		else {
			printf("Error: Couldn't start IRC connection\n");
		}
	}
	else {
		printf("Internal error: IRC Session not initialized\n");
	}
}

void IRCDeinit(void)
{
	irc_destroy_session(IRC.session);
}

#endif
