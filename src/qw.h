
#include <map>
#include <string>
#include <list>

struct playerinfo {
	std::string name;
	std::string team;
	std::string skin;

	int userid;
	int frags;
	int time;
	int ping;
	int color1;
	int color2;
};

struct serverinfo {
	typedef std::map<std::string, std::string> entries_t;
	typedef std::list<playerinfo> players_t;
	entries_t entries;
	players_t players;
	const char* GetEntryStr(const char *key) const;
	int GetEntryInt(const char* key) const;
};

// initialize this module
void QWInit(void);

// prototype of a function that accepts reports of new servers
typedef void (* NewServer_fnc) (const char *ip, short port, void *arg, int c);

void QW_ScanSource(const char *ip, short port, NewServer_fnc report, void *arg);

void QW_ScanSV(const char *ip, short port, server_status & status, unsigned int & timeleft,
			  serverinfo & svinfo);
