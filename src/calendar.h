#ifndef __CALENDAR_H__
#define __CALENDAR_H__

#include <queue>
#include <set>
#include "events.h"

/// Global application clock, singleton.
class Clock {
private:
	time_t clockstart;
	static Clock* instance;

public:
	Clock();

	static Clock* get(void);

	apptime GetAppTime(void) const;
};

class ServerScan;

/// Garbage collector of removed events.
class EventGC {
public:
	std::deque<ServerScan*> q;
	void Add(ServerScan* s);
	void FreeAll();
};

/// Comparator of two strings.
struct ltstr
{
	bool operator()(const char* s1, const char* s2) const;
};

/// Set of IP Addresses with 'quick' (not as good) insert/remove/find functions.
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

class Event;

/// Priotitized queue of events with ability to perform events scheduled for current time
class Calendar {
private:
	typedef std::priority_queue<EventP, std::vector<EventP>, CompEvent> queue_t;
	queue_t lp_q;
	queue_t hp_q;
	EventGC event_gc;
	bool running;
	bool breakrun;
	IP_set ip_set;

public:
	Calendar() : running(false), breakrun(false) {}

	void Add(ServerScan *e, bool high_priority = false);

	bool Has(const char *ip, short port);

	void PerformQueue(queue_t & q, apptime t);

	void PerformAllUntil(apptime t);

	bool queues_empty() const;

	apptime next_event_time() const;

	void AddToGC(ServerScan *s);

	void Loop(void);

	void Break(void);

	bool Running(void) const;

	void StartLoop(void);

	void PrintStatus(void) const;

	void AddPingUrl(const char* url, apptime t);

	void ScheduleMasterScan(apptime t, const std::string & ip, short port);
};

#endif // __CALENDAR_H__
