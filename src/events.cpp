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

bool CompEvent::operator() (const EventP a, const EventP b)
{
	return a->time > b->time;
}

PingURL::PingURL(const char* url_, apptime t_, Calendar *calendar_) : Event(t_), calendar(calendar_)
{
	url = strdup(url_);
}

void PingURL::Perform(void)
{
	HTTP_Ping(url);
	calendar->AddPingUrl(url, Clock::get()->GetAppTime() + PING_INTERVAL);
}

PingURL::~PingURL()
{
	if (url) {
		delete url;
	}
}

MasterScan::MasterScan(Calendar *calendar_, apptime t, const std::string & ip_, short port_) :
calendar(calendar_),
Event(t),
ip(ip_),
port(port_)
{
}

void NewIP(const char *ip, short port, void *cal_, int c);

void MasterScan::Perform(void)
{
	calendar->ScheduleMasterScan(Clock::get()->GetAppTime() + QW_MASTER_RESCAN_TIME, ip, port);
	QW_ScanSource(ip.c_str(), port, NewIP, (void *) calendar);	
	
	// dangerous, but works so far
	delete this;
}

ServerScan::ServerScan(const char *ip_, short port_, Calendar *calendar_, apptime t)
	: Event(t), port(port_), laststatus(SVST_standby), deadtimes(0), calendar(calendar_),
	unknownstatus_count(0)
{
	ip = strdup(ip_);
	printf("Adding %s:%d\n", ip, port);
}

ServerScan::~ServerScan() {
	delete ip;
}

const char* ServerScan::GetIP(void) const { return ip; }
short ServerScan::GetPort(void) const { return port; }

void ServerScan::RescheduleIn(unsigned int delay, bool high_priority) {
	// printf("Next scan of %s:%d in %u secs\n", ip, port, delay);
	time = Clock::get()->GetAppTime() + delay;
	calendar->Add(this, high_priority);
}

void ReportMatchEnd(const char *ip_, short port, const serverinfo *svinfo, bool & deferred_dealloc);
void ReportMatchStart(const char *ip, short port, const serverinfo & s);

void ServerScan::Perform(void)
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
			RescheduleIn(DEAD_TIME_RESCHEDULE+(rand()%180));
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
			RescheduleIn(DEAD_TIME_RESCHEDULE+(rand()%180));
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
			RescheduleIn(MICROSCANTIME, true);
		}
		else if (tl > MIN_TIMELEFT) {
			RescheduleIn((tl-1) * 60, true);
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
