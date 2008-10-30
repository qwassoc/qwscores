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

Clock* Clock::instance = 0;

Clock::Clock()
{
	clockstart = time(0);
}

Clock* Clock::get(void)
{
	if (!instance) {
		instance = new Clock();
	}

	return instance;
}

apptime Clock::GetAppTime(void) const
{
	return (apptime) difftime(time(0), clockstart);
}

void EventGC::Add(ServerScan* s)
{
	q.push_back(s);
}

void EventGC::FreeAll() {
	while (!q.empty()) {
		ServerScan *s = q.front();
		delete s;
		q.pop_front();
	}
}

bool ltstr::operator()(const char* s1, const char* s2) const {
	return strcmp(s1, s2) < 0;
}

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

bool Calendar::Has(const char *ip, short port) { return ip_set.Has(ip, port); }

void Calendar::PerformQueue(queue_t & q, apptime t)
{
	while (!q.empty() && q.top()->time <= t) {
		Event *e = q.top();
		q.pop();
		e->Perform();
	}
	if (q.empty()) {
		//printf("Warning: Calendar is empty!\n");
	}
}

void Calendar::PerformAllUntil(apptime t)
{
	PerformQueue(hp_q, t);
	PerformQueue(lp_q, t);
}

bool Calendar::queues_empty() const
{
	return hp_q.empty() && lp_q.empty();
}

apptime Calendar::next_event_time() const
{
	if (hp_q.empty() && lp_q.empty()) return 0;
	else if (!hp_q.empty() && lp_q.empty()) return hp_q.top()->time;
	else if (hp_q.empty() && !lp_q.empty()) return lp_q.top()->time;
	else /* both queues not empty */ {
		apptime lpq_t = lp_q.top()->time;
		apptime hpq_t = hp_q.top()->time;
		return (lpq_t < hpq_t) ? lpq_t : hpq_t;
	}
}


void Calendar::Loop(void)
{
	while (!this->breakrun) {
		PerformAllUntil(Clock::get()->GetAppTime());
		event_gc.FreeAll();
		int sleepint = CALENDAR_LOOP_SLEEP_MIN_INTERVAL;

		if (!queues_empty()) {
			apptime diff = next_event_time() - Clock::get()->GetAppTime();
			if (diff > 2) {
				// we have more than 2 seconds, let's get some larger sleep and not waste system resources
				sleepint = 1000 * (diff-2);
				// don't sleep too long, user might want something
				sleepint = min(sleepint, MAX_SLEEP_TIME);
			}
		}
		
		Sys_Sleep_ms(sleepint);
	}
	this->running = false;
}

void Calendar::Break(void) {
	this->breakrun = true;
}

bool Calendar::Running(void) const {
	return this->running;
}

static DWORD WINAPI CALRUN(void *param)
{
	Calendar *cal = (Calendar *) param;
	cal->Loop();
	return 0;
}

void Calendar::StartLoop(void)
{
	this->running = true;
	this->breakrun = false;
	Sys_CreateThread(CALRUN, this);
}

void Calendar::PrintStatus(void) const {
	printf("Active IPs: %d\n", ip_set.size());
}

void Calendar::AddPingUrl(const char* url, apptime t)
{
	this->lp_q.push(new PingURL(url, t, this));
}

void Calendar::ScheduleMasterScan(apptime t, const std::string & ip, short port)
{
	this->lp_q.push(new MasterScan(this, t, ip, port));
}

void Calendar::Add(ServerScan *e, bool high_priority) {
	if (high_priority) {
		this->hp_q.push(e);
	}
	else {
		this->lp_q.push(e);
	}
	this->ip_set.Add(e->GetIP(), e->GetPort());
}

void Calendar::AddToGC(ServerScan *s) {
	ip_set.Rem(s->GetIP(), s->GetPort());
	event_gc.Add(s);
}
