// Internet Relay Chat

void IRC_Send(const serverinfo & s, const char *ip, short port, playerscore *scores, bool teamplay);

void IRCInit(void);

void IRCConnect(void);

void IRCDeinit(void);

void IRCRaw(const char *cmd);
