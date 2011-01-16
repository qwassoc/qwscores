
#ifndef __EVENTS_H__
#define __EVENTS_H__

#include "common.h"

/// One scheduled event in the calendar.
class Event {
public:
	apptime time;
	Event(apptime t_) : time(t_) {}
	virtual void Perform(void) {}
	virtual ~Event() {}
};

typedef Event* EventP;

/// Compares two events, typically by time, used in the priority queue template.
class CompEvent {
public:
	bool operator() (const EventP a, const EventP b);
};

class Calendar;

/// Event which sends given HTTP request periodically.
class PingURL : public Event {
private:
	const char *url;
	Calendar *calendar;

public:
	PingURL(const char* url_, apptime t_, Calendar *calendar_);

	virtual void Perform(void);

	~PingURL();
};

/// Periodical scanning of QuakeWorld master server.
class MasterScan : public Event {
private:
	Calendar *calendar;
	std::string ip;
	short port;

public:
	MasterScan(Calendar *calendar_, apptime t, const std::string & ip_, short port_);

	virtual void Perform(void);
};

/// Periodical scanning of QuakeWorld game server.
class ServerScan : public Event {
private:
	server_status laststatus;
	const char *ip;
	short port;
	unsigned int deadtimes;
	unsigned int unknownstatus_count;
	Calendar *calendar;
	serverinfo *last_good_serverinfo;

	void updateServerinfo(const serverinfo & sinfo);
	void fixServerinfo(const serverinfo & sinfo);
	bool isDroppedPlayers(const serverinfo & sinfo) const;
	bool isScoreboardReset(const serverinfo & sinfo) const;

public:
	ServerScan(const char *ip_, short port_, Calendar *calendar_, apptime t);

	~ServerScan();

	const char *GetIP(void) const;
	short GetPort(void) const;

	void MatchStart(const serverinfo & sinfo);

	void RescheduleIn(unsigned int delay, bool high_priority = false);

	virtual void Perform(void);
};

#endif // __EVENTS_H__
