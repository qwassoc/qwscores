// common structures for more modules

#include <string>

struct playerscore {
	std::string name;
	std::string team;
	int frags;
};

typedef enum {
	SVST_dead,
	SVST_nostatus,
	SVST_unknownstatus,
	SVST_standby,
	SVST_match
} server_status;
