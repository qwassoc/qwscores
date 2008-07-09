/*
 * $Id: log.h 17 2004-09-14 18:24:51Z gyunaev $
 */

#ifndef INCLUDE_LOG_H
#define INCLUDE_LOG_H


class CLog
{
	public:
		CLog();
		~CLog();

		enum llev_t
		{
			FATAL,
			ERROR,
			INFO,
			DEBUG
		};

		void Init (unsigned int level, const std::string& path) throw (std::string);
		void Add  (llev_t level, const char * fmt, ...);

	private:
		unsigned int	level;
		std::ofstream	file;
		pthread_mutex_t	mutex;
};


#endif
