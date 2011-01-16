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
	last_good_serverinfo = NULL;
}

ServerScan::~ServerScan() {
	delete ip;
}

const char* ServerScan::GetIP(void) const { return ip; }
short ServerScan::GetPort(void) const { return port; }

void ServerScan::RescheduleIn(unsigned int delay, bool high_priority) {
	// printf("Next scan of %s:%d in %u secs\n", ip, port, delay);
	if (delay > STANDBY_RESCHEDULE) {
		delay = STANDBY_RESCHEDULE;
	}
	time = Clock::get()->GetAppTime() + delay;
	calendar->Add(this, high_priority);
}

void ReportMatchEnd(const char *ip_, short port, const serverinfo *svinfo, bool & deferred_dealloc);
void ReportMatchStart(const char *ip, short port, const serverinfo & s);

bool ServerScan::isDroppedPlayers(const serverinfo & sinfo) const
{
	if (last_good_serverinfo == NULL) {
		return false;
	}

	// new < last -> somebody dropped
	return (sinfo.players.size() < last_good_serverinfo->players.size());
}

bool ServerScan::isScoreboardReset(const serverinfo & sinfo) const
{
	if (last_good_serverinfo == NULL) {
		return false;
	}

	serverinfo::players_t::const_iterator pl_it;
	for (pl_it = sinfo.players.begin(); pl_it != sinfo.players.end(); pl_it++) {
		if (pl_it->frags != 0) {
			return false;
		}
	}

	return true;
}

/**
 * Fixes the serverinfo in <code>sinfo</code> with entries from serverinfo kept
 * in the last_good_server info.
 * 
 * The fixing process basically fills in players that left the server too early.
 *
 * @param sinfo Serverinfo to be fixed.
 */
void ServerScan::fixServerinfo(const serverinfo & sinfo)
{
	serverinfo *newinfo = new serverinfo(sinfo);
	serverinfo::players_t::const_iterator pl_it;
	std::map<int, serverinfo::players_t::const_iterator> userid_map;
	serverinfo::players_t missing_players;

	printf("Fixing report for %s, players dropped from %u to %u\n",
		sinfo.GetEntryStr("hostname"), last_good_serverinfo->players.size(),
		sinfo.players.size());

	for (pl_it = sinfo.players.begin(); pl_it != sinfo.players.end(); pl_it++) {
		userid_map.insert(std::pair<int, serverinfo::players_t::const_iterator>(pl_it->userid, pl_it));
	}

	for (pl_it = last_good_serverinfo->players.begin(); pl_it != last_good_serverinfo->players.end(); pl_it++) {
		if (userid_map.find(pl_it->userid) == userid_map.end()) {
			newinfo->players.push_back(*pl_it);
		}
	}

	delete last_good_serverinfo;
	last_good_serverinfo = newinfo;
}

void ServerScan::updateServerinfo(const serverinfo & sinfo)
{
	if (last_good_serverinfo == NULL) {
		last_good_serverinfo = new serverinfo(sinfo);
	}
	else {
		bool droppedPlayers = isDroppedPlayers(sinfo);
		bool scoreboardReset = isScoreboardReset(sinfo);

		if (scoreboardReset) {
			// keep last_good_serverinfo, this one is unfixable
		}
		else if (droppedPlayers) {
			fixServerinfo(sinfo);
		}
		else {
			// this scores are ok, use them
			delete last_good_serverinfo;
			last_good_serverinfo = new serverinfo(sinfo);
		}
	}
}

void ServerScan::MatchStart(const serverinfo & sinfo)
{
	ReportMatchStart(ip, port, sinfo);
	last_good_serverinfo = new serverinfo(sinfo);
}

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
			updateServerinfo(*sinfo);
			serverinfo *reported_serverinfo = new serverinfo(*last_good_serverinfo);
			ReportMatchEnd(ip, port, reported_serverinfo, deferred_dealloc);
			delete last_good_serverinfo;
			last_good_serverinfo = NULL;
			if (!deferred_dealloc) {
				delete reported_serverinfo;
			}
		}
		RescheduleIn(STANDBY_RESCHEDULE);
		break;

	case SVST_match:
		updateServerinfo(*sinfo);

		if (tl == MIN_TIMELEFT || tl == SUDDENDEATH_TIMELEFT) {
			RescheduleIn(MICROSCANTIME, true);
		}
		else if (tl > MIN_TIMELEFT) {
			RescheduleIn((tl-1) * 60, true);
		}
		else {
			// practically does not happen, but constants may change
			RescheduleIn(STANDBY_RESCHEDULE);
		}
		break;
	}

	laststatus = newstatus;
	delete sinfo;
}
