

#ifndef EX4_CACHE_H
#define EX4_CACHE_H

#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <map>
#include <queue>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>

#include "LogFile.h"
#include "Block.h"

#define FALIURE 1
#define SUCCESS 0


class Cache{
public:

	Cache(const double num_args[]);

	~Cache();

	int collect(const char *path, char *buf, size_t size,
	            off_t offset, struct fuse_file_info *fi);

	int ioctl_info(LogFile** lf, std::string rootdir);

	void inner_rename(std::string* path, std::string* newpath);

private:
	blksize_t _blksize;
	size_t _TotalSize;
	size_t _OldSize;
	size_t _MidSize;
	size_t _NewSize;

	std::deque<Block*> _Cache;

	void _cache_clean();

	char* _cache_replace(const char* path, int index);

	char* _cache_add(const char* path, struct fuse_file_info *fi, int index);

	void _cache_evict();

	void _sort_old(void);

	bool _find_existing_block(const char *path, int index);

	int _fsize(const char *path);

};

#endif //EX4_CACHE_H