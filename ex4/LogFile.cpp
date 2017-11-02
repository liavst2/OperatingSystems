

#include "LogFile.h"


LogFile::LogFile(std::string fullpath) {
	fullpath.append(LOG_FILE_PATH);
	__lfile__.open(fullpath, std::ios_base::app);
}


LogFile::~LogFile(){
	__lfile__.close();
}

void LogFile::command(std::string syscall) {
	__lfile__ << time(NULL) << " " << syscall << std::endl;
}

void LogFile::cache_ioctl(std::string path, int index, int ref){
	__lfile__ << path << " " << (index + 1) << " " << ref << std::endl;
}

