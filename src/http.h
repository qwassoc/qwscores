// sending results via http

void HTTP_Send(const serverinfo & s, const char *ip, short port, playerscore *scores, bool teamplay);

void HTTP_Init(void);

void HTTP_AddTarget(const char *url, bool &);

void HTTP_Ping(const char *url);
