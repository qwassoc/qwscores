// common structures for more modules

#ifndef __COMMON_H__
#define __COMMON_H__

#include <string>

typedef int apptime;

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

#endif // __COMMON_H__
