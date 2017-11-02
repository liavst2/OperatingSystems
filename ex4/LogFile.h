

#ifndef EX4_LOGFILE_H
#define EX4_LOGFILE_H


#include <iosfwd>
#include <fstream>
#include <fuse.h>

#define LOG_FILE_PATH "/.filesystem.log"


class LogFile{
private:
	std::ofstream __lfile__;

public:
	LogFile(std::string);

	~LogFile();

	void command(std::string);

	void cache_ioctl(std::string, int, int);
};


#endif //EX4_LOGFILE_H
