// configuration, until someone rewrites it so that it is loaded dynamically

#include <string>
#include <map>
#include <list>

#define HTTP_REPORT true
#define IRC_REPORT true
// uncomment this to disable IRC support
// #define WITHOUT_IRC

typedef void (* Conf_Cmdcallback_f) (const char *args, bool & quitflag);

struct ConfLists {
	std::list<std::string> master_servers;
	std::list<std::string> qw_servers;
	std::list<std::string> irc_channels;
};

struct Configuration {
	typedef std::map<std::string, std::string> strings_t;
	strings_t strings;
	typedef std::map<std::string, int> numbers_t;
	numbers_t numbers;
	typedef std::map<std::string, Conf_Cmdcallback_f> commands_t;
	commands_t commands;
	
	ConfLists lists;
};

void Conf_CmdAdd(const std::string & cmdname, Conf_Cmdcallback_f callback);

bool Conf_Read(int argc, char **argv);

void Conf_ListCommands(void);

void Conf_AcceptCommand(const char *line, bool & breakflag);


// QuakeWorld
#define QW_DEFAULT_MASTER_SERVER_PORT 27000
#define QW_DEFAULT_SVPORT 27500
#define QW_MASTER_RESCAN_TIME (60*60*6)
#define QW_MASTER_TIMEOUT_SECS 5
#define QW_SERVER_QUERY_TIMEOUT_SEC 1
#define QW_SERVER_QUERY_TIMEOUT_USEC 5000

// HTTP reporting settings
#define HTTP_DEFAULT_PORT 80
#define HTTP_REQUEST_BUFFER 10240

// Internet Relay Chat settings
#define IRC_CHANNEL "#qw.results"
#define IRC_KEY 0
#define IRC_SERVER "irc.quakenet.org"
#define IRC_PORT 6667
#define IRC_PASSWORD 0
#define IRC_NICK "qw-results-bot"
#define IRC_REALNAME "made by JohnNy_cz"
#define IRC_USERNAME "qwresults"

// scan algorithm properties
#define LONGINTERVAL (3*60)
#define MICROSCAN 2
#define DEAD_TIME_RESCHEDULE (20*60)
#define MAX_DEAD_TIMES 2
#define MAX_UNKNOWN_COUNT 2
#define UNKNOWN_TIME_RESCHEDULE (10*60)
#define STANDBY_RESCHEDULE (3*60)
#define MIN_TIMELEFT 1
#define SUDDENDEATH_TIMELEFT 0
#define MICROSCANTIME 2
#define INITIAL_SCANS_INTERVAL (0.33)
#define CALENDAR_LOOP_SLEEP_MIN_INTERVAL 100
