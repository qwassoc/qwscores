#include <cstdio>
#include <cstring>
#include <ctime>
#include <queue>
#include <set>
#include <iostream>
#include "common.h"
#include "conf.h"
#ifdef WIN32
#include "windows.h"
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#define Sleep(x) usleep((x)*1000)
#define closesocket close
#endif
#include "utils.h"
#include "qw.h"
#include "http.h"
#ifndef WITHOUT_IRC
#include "irc.h"
#endif


#pragma warning( disable : 4996)

typedef int apptime;

class Clock {
public:
	Clock() {
		clockstart = time(0);
	}

	apptime GetAppTime(void) const {
		return (apptime) difftime(time(0), clockstart);
	}

private:
	time_t clockstart;
};
static Clock AppClock;

class Event {
public:
	apptime time;
	Event(apptime t_) : time(t_) {}
	virtual void Perform(void) {}
	virtual ~Event() {}
};

int cmpplayerscore(const void *pa, const void *pb)
{
	playerscore *a = (playerscore *) pa, *b = (playerscore *) pb;

	return a->frags < b->frags;
}

int cmpplayerscoreteam(const void *pa, const void *pb)
{
	playerscore *a = (playerscore *) pa, *b = (playerscore *) pb;

	int r = strcmp(a->team.c_str(), b->team.c_str());
	if (!r) {
		return a->frags < b->frags;
	}
	else return r;
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

	playerscore *scores = new playerscore[svinfo->players.size()];
	char *ip = strdup(ip_);

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

typedef Event* EventP;

class CompEvent {
public:
	bool operator() (const EventP a, const EventP b) {
		return a->time > b->time;
	}
};

class ServerScan;
class EventGC {
public:
	std::deque<ServerScan*> q;
	void Add(ServerScan* s) {
		q.push_back(s);
	}

	void FreeAll();
};

static DWORD WINAPI CALRUN(void *param);

struct ltstr
{
	bool operator()(const char* s1, const char* s2) const {
		return strcmp(s1, s2) < 0;
	}
};

class IP_set {
public:
	void Add(const char* ip, short port);
	bool Has(const char* ip, short port);
	void Rem(const char* ip, short port);
	size_t size(void) const { return inset.size(); }	
private:
	typedef std::set<const char *, ltstr> inset_t;
	inset_t inset;
};

void IP_set::Add(const char *ip, short port)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%s:%d", ip, port);
	
	char *ipstr = strdup(buf);
	if (ipstr) {
		inset.insert(ipstr);
	}
}

bool IP_set::Has(const char *ip, short port)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%s:%d", ip, port);

	if (inset.empty()) return false;

	return inset.find(buf) != inset.end();
}

void IP_set::Rem(const char *ip, short port)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%s:%d", ip, port);

	if (Has(ip, port)) {
		inset_t::iterator f = inset.find(buf);
		if (f != inset.end()) {
			free((void *) *f);
			inset.erase(f);
		}
	}
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

class Calendar {
private:
	std::priority_queue<EventP, std::vector<EventP>, CompEvent> q;
	EventGC event_gc;
	bool running;
	bool breakrun;
	IP_set ip_set;

public:
	Calendar() : running(false), breakrun(false) {}

	void Add(ServerScan *e);

	bool Has(const char *ip, short port) { return ip_set.Has(ip, port); }

	void PerformAllUntil(apptime t) {
		while (!q.empty() && q.top()->time <= t) {
			Event *e = q.top();
			q.pop();
			e->Perform();
		}
		if (q.empty()) {
			//printf("Warning: Calendar is empty!\n");
		}
	}

	void AddToGC(ServerScan *s);

	void Loop(void) {
		while (!this->breakrun) {
			PerformAllUntil(AppClock.GetAppTime());
			event_gc.FreeAll();
			Sleep(100);
		}
		this->running = false;
	}

	void Break(void) {
		this->breakrun = true;
	}

	bool Running(void) const {
		return this->running;
	}

	void StartLoop(void)
	{
		this->running = true;
		this->breakrun = false;
		Sys_CreateThread(CALRUN, this);
	}

	void PrintStatus(void) const {
		printf("Active IPs: %d\n", ip_set.size());
	}

	void ScheduleMasterScan(apptime t, const std::string & ip, short port);
};

static DWORD WINAPI CALRUN(void *param)
{
	Calendar *cal = (Calendar *) param;
	cal->Loop();
	return 0;
}

void NewIP(const char *ip, short port, void *cal_, int c);

class MasterScan : public Event {
private:
	Calendar *calendar;
	std::string ip;
	short port;

public:
	MasterScan(Calendar *calendar_, apptime t, const std::string & ip_, short port_)
		: calendar(calendar_), Event(t), ip(ip_), port(port_)
	{}

	virtual void Perform(void) {
		calendar->ScheduleMasterScan(AppClock.GetAppTime() + QW_MASTER_RESCAN_TIME, ip, port);
		QW_ScanSource(ip.c_str(), port, NewIP, (void *) calendar);	
		
		// dangerous, but works so far
		delete this;
	}
};

void Calendar::ScheduleMasterScan(apptime t, const std::string & ip, short port)
{
	this->q.push(new MasterScan(this, t, ip, port));
}

class ServerScan : public Event {
private:
	server_status laststatus;
	const char *ip;
	short port;
	unsigned int deadtimes;
	unsigned int unknownstatus_count;
	Calendar *calendar;

public:
	ServerScan(const char *ip_, short port_, Calendar *calendar_, apptime t)
		: Event(t), port(port_), laststatus(SVST_standby), deadtimes(0), calendar(calendar_),
		unknownstatus_count(0)
	{
		ip = strdup(ip_);
		printf("Adding %s:%d\n", ip, port);
	}

	~ServerScan() {
		delete ip;
	}

	const char *GetIP(void) const { return ip; }
	short GetPort(void) const { return port; }

	void RescheduleIn(unsigned int delay) {
		// printf("Next scan of %s:%d in %u secs\n", ip, port, delay);
		time = AppClock.GetAppTime() + delay;
		calendar->Add(this);
	}

	virtual void Perform(void)
	{
		server_status newstatus;
		unsigned int tl;
		serverinfo *sinfo = new serverinfo;
		bool deferred_dealloc = false;

		QW_ScanSV(ip, port, newstatus, tl, *sinfo);
		switch (newstatus) {
		case SVST_dead:
			printf("Dead: %s:%d\n", ip, port);
			if (++deadtimes >= MAX_DEAD_TIMES) {
				printf("Server %s:%d was found dead for %d scans and was removed\n",
					ip, port, deadtimes);

				this->calendar->AddToGC(this);
			}
			else {
				RescheduleIn(deadtimes*DEAD_TIME_RESCHEDULE);
			}
			break;

		case SVST_nostatus:
			unknownstatus_count++;
			printf("No status: \"%s\" %s:%d\n", sinfo->GetEntryStr("hostname"), ip, port);
			if (unknownstatus_count >= MAX_UNKNOWN_COUNT) {
				printf("Removed: \"%s\" %s:%d (no status)\n", sinfo->GetEntryStr("hostname"), ip, port);
				this->calendar->AddToGC(this);
			}
			else {
				RescheduleIn(unknownstatus_count*DEAD_TIME_RESCHEDULE);
			}
			break;

		case SVST_unknownstatus:
			printf("  %s:%d \"%s\"\n", ip, port, sinfo->GetEntryStr("hostname"));
			break;

		case SVST_standby:
			if (laststatus == SVST_match) {
				if (laststatus == SVST_match) {
					ReportMatchEnd(ip, port, sinfo, deferred_dealloc);
				}
			}
			RescheduleIn(STANDBY_RESCHEDULE);
			break;

		case SVST_match:
			if (laststatus != SVST_match) {
				ReportMatchStart(ip, port, *sinfo);
			}
			if (tl == MIN_TIMELEFT || tl == SUDDENDEATH_TIMELEFT) {
				RescheduleIn(MICROSCANTIME);
			}
			else if (tl > MIN_TIMELEFT) {
				RescheduleIn((tl-1) * 60);
			}
			else {
				RescheduleIn(STANDBY_RESCHEDULE);
			}
			break;
		}

		laststatus = newstatus;
		if (!deferred_dealloc) {
			delete sinfo;
		}
	}
};

void Calendar::Add(ServerScan *e) {
	this->q.push(e);
	this->ip_set.Add(e->GetIP(), e->GetPort());
}

void Calendar::AddToGC(ServerScan *s) {
	ip_set.Rem(s->GetIP(), s->GetPort());
	event_gc.Add(s);
}

void EventGC::FreeAll() {
	while (!q.empty()) {
		ServerScan *s = q.front();
		delete s;
		q.pop_front();
	}
}

void NewIP(const char *ip, short port, void *cal_, int c)
{
	Calendar *cal = (Calendar *) cal_;

	if (!cal->Has(ip, port)) {
		cal->Add(new ServerScan(ip, port, cal, 
			(apptime) (AppClock.GetAppTime() + c*INITIAL_SCANS_INTERVAL)));
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

	cal.ScheduleMasterScan(AppClock.GetAppTime(), ip.c_str(), port);
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
	Conf_CmdAdd("help", Cmd_Help);
}

int main(int argc, char **argv)
{
	printf("QW Scores 0.2\nby JohnNy_cz\n---\n\n");

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
		std::cin.getline(cmdbuf, sizeof(cmdbuf));
		Conf_AcceptCommand(cmdbuf, breakflag);
	}

	cal.Break();
	printf("Waiting for calendar to terminate...");
	while (cal.Running()) { Sleep(100); }
	printf("ok\n");
#ifndef WITHOUT_IRC
	IRCDeinit();
#endif

	return 0;
}
