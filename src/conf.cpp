#include <fstream>
#include <cstdlib>
#include <cstring>
#include "conf.h"
#include "utils.h"

static Configuration conf;
const char * const DEFAULT_CONF_FILENAME = "scoreacc.conf";
const int MAX_LINE_LENGTH = 10240;

void Conf_AddIRCChan(const char *ircchan, bool &)
{
	conf.lists.irc_channels.push_back(ircchan);
}

void Conf_RegisterMainCommands(void)
{
	Conf_CmdAdd("addchannel", Conf_AddIRCChan);
}

void Conf_AcceptCommand(const char *line, bool & breakflag)
{
	char cmd[MAX_LINE_LENGTH];
	const char *args;

	const char *space = strchr(line, ' ');

	if (!space) {
		args = "";
	}
	else {
		args = space + 1;
	}

	strlcpy(cmd, line, space-line+1);

	Configuration::strings_t::iterator s;
	Configuration::numbers_t::iterator n;
	Configuration::commands_t::iterator c;
	
	if ((n = conf.numbers.find(cmd)) != conf.numbers.end()) {
		n->second = atoi(args);
	}
	else if ((s = conf.strings.find(cmd)) != conf.strings.end())
	{
		s->second = std::string(args);
	}
	else if ((c = conf.commands.find(cmd)) != conf.commands.end())
	{
		Conf_Cmdcallback_f callbackfnc = c->second;

		callbackfnc(args, breakflag);
	}
	else {
		printf("Unrecognized command \"%s\"\n", cmd);
	}
}

std::string Conf_GetStr(const char *key)
{
	Configuration::strings_t::iterator c = conf.strings.find(key);
	if (c != conf.strings.end()) {
		return c->second;
	}
	else {
		return "";
	}
}

int Conf_GetInt(const char *key)
{
	Configuration::numbers_t::iterator c = conf.numbers.find(key);

	if (c != conf.numbers.end()) {
		return c->second;
	}
	else {
		return 0;
	}
}

bool Conf_Process(std::fstream & f)
{
	char line[MAX_LINE_LENGTH];
	unsigned l = 0;
	bool breakflag = false;
	
	while (!f.eof() && !breakflag) {
		l++;
		f.getline(line, sizeof(line));
		if (line[0] == '\0') {
			// empty line, skip it (yeah we should ignore whitespaces too)
			continue;
		}
		if (line[0] == '/' && line[1] == '/') {
			// comment, skip it
			continue;
		}

		Conf_AcceptCommand(line, breakflag);
	}

	if (breakflag) {
		printf("Line: %u\n", l);
	}

	return true;
}

void Conf_Init(void)
{
	conf.numbers.insert(std::pair<std::string, int> ("lidne", 30));
}

bool Conf_Read(int argc, char **argv)
{
	Conf_Init();

	const char *fname;

	if (argc < 2) {
		fname = DEFAULT_CONF_FILENAME;
	}
	else {
		fname = argv[1];
	}

	std::fstream i(fname, std::ios_base::in);
	if (i.good()) {
		return Conf_Process(i);
	}
	else {
		printf("Couldn't open %s\n", fname);
		return false;
	}
}

void Conf_CmdAdd(const std::string & cmdname, Conf_Cmdcallback_f callback)
{
	conf.commands[cmdname] = callback;
}

bool Conf_CmdExists(const std::string & cmdname)
{
	return conf.commands.find(cmdname) != conf.commands.end();
}

void Conf_ListCommands(void)
{
	Configuration::commands_t::const_iterator c, e;

	printf("Available commands:\n");
	for (c = conf.commands.begin(), e = conf.commands.end(); c != e; c++)
	{
		printf("%s\n", c->first.c_str());
	}
}
