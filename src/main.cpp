#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <queue>
#include <set>
#include <iostream>
#include <fstream>
#include "common.h"
#include "conf.h"
#include "utils.h"
#include "qw.h"
#include "http.h"
#ifndef WITHOUT_IRC
#include "irc.h"
#endif
#include "events.h"
#include "calendar.h"

// disable iso c++
#pragma warning( disable : 4996)

int cmpplayerscore(const void *pa, const void *pb)
{
	playerscore *a = (playerscore *) pa, *b = (playerscore *) pb;

	if (a->frags < b->frags)
		return -1;
	else if (a->frags > b->frags)
		return 1;
	else
		return 0;
}

int cmpplayerscoreteam(const void *pa, const void *pb)
{
	playerscore *a = (playerscore *) pa, *b = (playerscore *) pb;

	int r = strcmp(a->team.c_str(), b->team.c_str());
	if (r)
		return r;
	else
		return cmpplayerscore(pa, pb);
}

void Console_Send(const serverinfo & s, const char *ip, short port, playerscore *scores)
{
	printf("\n-- Match result \"%s\" (%s:%d) --\n", s.GetEntryStr("hostname"), ip, port);
	printf("Map: %s | Players: %u\n", s.GetEntryStr("map"), s.players.size());

	bool tp = s.players.size() && scores[0].team.c_str()[0];

	for (unsigned int i = 0; i < s.players.size(); ++i) {
		if (tp) {
			printf("[%s] %s (%d)\n", scores[i].team.c_str(), scores[i].name.c_str(), scores[i].frags);
		}
		else {
			printf("%s (%d)\n", scores[i].name.c_str(), scores[i].frags);
		}
	}
	printf("--\n");
}

struct DeferredSendData {
	const char *ip;
	short port;
	const serverinfo *svinfo;
	playerscore *scores;
	bool teamplay;
};

DWORD WINAPI DeferredSendThread(void *arg)
{
	DeferredSendData *data = (DeferredSendData *) arg;

#ifndef WITHOUT_IRC
	if (IRC_REPORT) {
		IRC_Send(*data->svinfo, data->ip, data->port, data->scores, data->teamplay);
	}
#endif
	if (HTTP_REPORT) {
		HTTP_Send(*data->svinfo, data->ip, data->port, data->scores, data->teamplay);
	}

	delete data->ip;
	delete data->svinfo;
	delete[] data->scores;
	delete data;

	return 0;
}

void Deferred_Send(const serverinfo *svinfo, const char *ip, short port, playerscore *scores, bool teamplay)
{
	DeferredSendData *data = new DeferredSendData;
	data->ip = ip;
	data->port = port;
	data->svinfo = svinfo;
	data->scores = scores;
	data->teamplay = teamplay;

	Sys_CreateThread(DeferredSendThread, (void *) data);
}

void ReportMatchEnd(const char *ip_, short port, const serverinfo *svinfo, bool & deferred_dealloc)
{
	size_t players = svinfo->players.size();
	if (players == 0) {
		printf("Match missed \"%s\" %s:%d\n", svinfo->GetEntryStr("hostname"), ip_, port);
		return;
	}

	char *ip = strdup(ip_);

	if (!ip) {
		printf("Error: Not enough memory!\n");
		return;
	}

	playerscore *scores = new playerscore[svinfo->players.size()];

	bool teamplay = svinfo->GetEntryInt("teamplay") != 0 && svinfo->players.begin()->team[0] && svinfo->players.size() > 2;
	int i;
	serverinfo::players_t::const_iterator p;
	for (i = 0, p = svinfo->players.begin(); p != svinfo->players.end(); p++, i++) {
		scores[i].frags = p->frags;
		scores[i].name = p->name;
		scores[i].team = p->team;
	}
	qsort(scores, players, sizeof(playerscore), teamplay ? cmpplayerscoreteam : cmpplayerscore);

	Console_Send(*svinfo, ip, port, scores);
	
	Deferred_Send(svinfo, ip, port, scores, teamplay);

	deferred_dealloc = true;
}

void ReportMatchStart(const char *ip, short port, const serverinfo & s)
{
	printf("Match found, %u players @ \"%s\" (%s:%d)\n", s.players.size(),
		s.GetEntryStr("hostname"), ip, port);
}

static void TestIPSET(void)
{
	IP_set a;
	a.Add("1.2.3.4", 1);
	a.Add("1.2.3.4", 2);
	if (a.Has("1.2.3.4", 1)) {
		printf("ok: found 1\n");
	}
	else {
		printf("error: not found 1\n");
	}

	a.Rem("1.2.3.4", 1);
	if (a.Has("1.2.3.4", 1)) {
		printf("error: found 1\n");
	}
	else {
		printf("ok: not found 1\n");
	}
	a.Rem("1.2.3.4", 2);
	a.Rem("1.2.3.4", 1);
}

void NewIP(const char *ip, short port, void *cal_, int c)
{
	Calendar *cal = (Calendar *) cal_;

	if (!cal->Has(ip, port)) {
		cal->Add(new ServerScan(ip, port, cal, 
			(apptime) (Clock::get()->GetAppTime() + c*INITIAL_SCANS_INTERVAL)));
	}
}

bool InitOS(void)
{
#ifdef WIN32
	WSADATA data;
	if(WSAStartup(MAKEWORD(2,2), &data)) {
		return false;
	}
	else {
		return true;
	}
#else
	return true;
#endif
}

static Calendar cal;

void Cmd_Quit(const char*, bool & breakflag)
{
	breakflag = true;
}

void Cmd_Status(const char*, bool &)
{
	cal.PrintStatus();
}

void Cmd_IRC(const char* args, bool &)
{
	if (args[0]) {
#ifndef WITHOUT_IRC
		IRCRaw(args);
#else
		printf("IRC support not compiled\n");
#endif
	}
	else {
		printf("Syntax: irc <command> [arguments]\n");
	}
}

std::string GetIP(const char *str)
{
	const char *colon = strchr(str, ':');
	if (colon) {
		return std::string(str, colon-str);
	}
	else {
		return std::string(str);
	}
}

short GetPort(const char *str, short defaultport)
{
	const char *colon = strchr(str, ':');
	if (colon) {
		return atoi(colon+1);
	}
	else {
		return defaultport;
	}
}

void Cmd_AddIP(const char *args, bool &)
{
	if (args[0]) {
		std::string ip = GetIP(args);
		short port = GetPort(args, QW_DEFAULT_SVPORT);

		NewIP(ip.c_str(), port, &cal, 0);
		printf("Added server %s:%d to scan queue\n", ip.c_str(), port);
	}
	else {
		printf("Syntax: addip <ip> <port>\n");
	}
}

void Cmd_AddMaster(const char* args, bool &)
{
	const char *colon = strchr(args, ':');
	std::string ip = GetIP(args);
	short port = GetPort(args, QW_DEFAULT_MASTER_SERVER_PORT);

	cal.ScheduleMasterScan(Clock::get()->GetAppTime(), ip.c_str(), port);
}

void Cmd_AddFileList(const char* args, bool &)
{
	if (*args) {
		std::fstream i (args, std::ios_base::in);
		if (i.good()) {
			const unsigned int MAX_LINE_LENGTH = 256;
			char buffer[MAX_LINE_LENGTH];

			while (!i.eof()) {
				i.getline(buffer, MAX_LINE_LENGTH);
				if (buffer[0]) {
					bool dummy;

					Cmd_AddIP(buffer, dummy);
				}
			}
		}
		else {
			printf("Couldn't open '%s' for reading\n", args);
		}
	}
	else {
		printf("Usage: addfilelist <filename>\nadds servers from given list\n");
	}
}

void Cmd_AddPingURL(const char* args, bool &)
{
	if (*args && strstarts(args, "http://")) {
		printf("Adding ping url %s\n", args);
		cal.AddPingUrl(args, Clock::get()->GetAppTime());
	}
	else {
		printf("Usage: addpingurl http://...");
	}
}

void Cmd_Help(const char *args, bool &)
{
	Conf_ListCommands();
}

void Main_AddCmds(void)
{
	Conf_CmdAdd("quit", Cmd_Quit);
	Conf_CmdAdd("status", Cmd_Status);
	Conf_CmdAdd("irc", Cmd_IRC);
	Conf_CmdAdd("addip", Cmd_AddIP);
	Conf_CmdAdd("addmaster", Cmd_AddMaster);
	Conf_CmdAdd("addhttp", HTTP_AddTarget);
	Conf_CmdAdd("addfilelist", Cmd_AddFileList);
	Conf_CmdAdd("addpingurl", Cmd_AddPingURL);
	Conf_CmdAdd("help", Cmd_Help);
	Conf_CmdAdd("addchannel", Conf_AddIRCChan);
}

int main(int argc, char **argv)
{
	printf("QW Scores 0.3\nby JohnNy_cz\n---\n\n");

	if (!InitOS()) {
		return -1;
	}

#ifndef WITHOUT_IRC
	printf("Initializing IRC...");
	IRCInit();
	printf("ok\n");
#endif

	QWInit();

	Main_AddCmds();

	printf("Initialization done\n");
	
	printf("Reading configuration file...\n");
	if (!Conf_Read(argc, argv)) {
		printf("Configuration file scoreacc.conf not found\n");
		return -1;
	}
	printf("Configuration loaded\n");

	cal.StartLoop(); // new thread gets created and launched

	bool breakflag = false;

	while (!breakflag) {
		char cmdbuf[256];
		if (std::cin.eof()) {
			Sys_Sleep_ms(1000);
			continue;
		}
		std::cin.getline(cmdbuf, sizeof(cmdbuf));
		Conf_AcceptCommand(cmdbuf, breakflag);
	}

	cal.Break();
	printf("Waiting for calendar to terminate...");
	while (cal.Running()) { Sys_Sleep_ms(100); }
	printf("ok\n");
#ifndef WITHOUT_IRC
	IRCDeinit();
#endif

	return 0;
}
