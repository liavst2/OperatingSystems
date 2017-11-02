/*
 * CachingFileSystem.cpp
 *
 *  Author: Netanel Zakay, HUJI, 67808  (Operating Systems 2015-2016).
 */

#define FUSE_USE_VERSION 26
#define FUSE_ARG_NUM 3
#define MINIMUM_ARGS_NUM 6
#define SUCCESS 0
#define USAGE "Usage: CachingFileSystem rootdir mountdir\
 numberOfBlocks fOld fNew"
#define DIR_ERROR "System Error: Can't evaluate given directories"
#define ALLOCATION_FAIL "System Error: Allocation Failure"

#include <errno.h>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <dirent.h>

#include "Cache.h"

struct fuse_operations g_caching_oper;
static Cache* cache;
static char rootdir[PATH_MAX];
static LogFile* lf;


bool LogFileAccess(const char* path){
	return !strcmp(path, LOG_FILE_PATH);
}


/**
 * returns the absolute path of the file
 */
char* absolute_path(const char *path){
	char g_abs_path[PATH_MAX];
	strcpy(g_abs_path, rootdir);
	return strncat(g_abs_path, path, PATH_MAX);
}


/**
 * implements getattr
 */
int caching_getattr(const char *path, struct stat *statbuf){

	lf->command("getattr");
	if (LogFileAccess(path)){ // trying to reach our logfile
		return -ENOENT;
	}

	int ret = lstat(absolute_path(path), statbuf);
	return (ret < 0) ? (-errno): ret;
}


/**
 * implements fgetattr
 */
int caching_fgetattr(const char *path, struct stat *statbuf,
                     struct fuse_file_info *fi){
	lf->command("fgetattr");
	if (LogFileAccess(path)){ // trying to reach our logfile
		return -ENOENT;
	}

	if (!strcmp(path, "/")){
		return caching_getattr(path, statbuf);
	}

	int ret = fstat(fi->fh, statbuf);
	return (ret < 0) ? (-errno): ret;
}


/**
 * implements access
 */
int caching_access(const char *path, int mask){

	lf->command("access");
	if (LogFileAccess(path)){ // trying to reach our logfile
		return -ENOENT;
	}

	int ret = access(absolute_path(path), mask);
	return (ret < 0) ? (-errno): ret;
}


/**
 * implements open
 */
int caching_open(const char *path, struct fuse_file_info *fi){

	lf->command("open");
	if (LogFileAccess(path)){ // trying to reach our logfile
		return -ENOENT;
	}

	fi->direct_io = 1;
	if ((fi->flags & 3) != O_RDONLY) {
		return -EACCES;
	}

	int fd = open(absolute_path(path), O_RDONLY|O_DIRECT|O_SYNC);
	if (fd < 0){
		return -ENOENT;
	}

	fi->fh = fd;


	return SUCCESS;
}


/**
 * implements read
 */
int caching_read(const char *path, char *buf, size_t size,
                 off_t offset, struct fuse_file_info *fi){

	lf->command("read");
	if (LogFileAccess(path)){ // trying to reach our logfile
		return -ENOENT;
	}

	int ret = cache->collect(absolute_path(path), buf, size, offset, fi);
	return ret;
}


/**
 * implements flush
 */
int caching_flush(const char *path, struct fuse_file_info*){

	lf->command("flush");
	if (LogFileAccess(path)){ // trying to reach our logfile
		return -ENOENT;
	}

	return SUCCESS;
}


/**
 * implements release
 */
int caching_release(const char *path, struct fuse_file_info *fi){

	lf->command("release");
	if (LogFileAccess(path)){ // trying to reach our logfile
		return -ENOENT;
	}

	int ret = close((int)fi->fh);
	return (ret < 0) ? (-errno): ret;
}


/**
 * implements opendir
 */
int caching_opendir(const char *path, struct fuse_file_info *fi){

	lf->command("opendir");
	if (LogFileAccess(path)){ // trying to reach our logfile
		return -ENOENT;
	}

	DIR* dir = opendir(absolute_path(path));

	if (dir == NULL){
		return -errno;
	}

	fi->fh = (intptr_t) dir;
	return SUCCESS;
}


/**
 * implements readdir
 */
int caching_readdir(const char* path, void *buf,
                    fuse_fill_dir_t filler, off_t ,struct fuse_file_info *fi){

	lf->command("readdir");
	if (LogFileAccess(path)){ // trying to reach our logfile
		return -ENOENT;
	}

	DIR* dp = (DIR*) (uintptr_t) fi->fh;
	struct dirent *de;
	if ((de = readdir(dp)) == 0){
		return -errno;
	}

	do
	{
		if (LogFileAccess(de->d_name)){
			continue;
		}
		if (filler(buf, de->d_name, NULL, 0)){
			return -ENOMEM;
		}
	} while ((de = readdir(dp)) != NULL);

	return SUCCESS;
}

/**
 * implements releasedir
 */
int caching_releasedir(const char* path, struct fuse_file_info *fi){

	lf->command("releasedir");
	if (LogFileAccess(path)){ // trying to reach our logfile
		return -ENOENT;
	}

	int ret = closedir((DIR*) (uintptr_t) fi->fh);
	return (ret < 0) ? (-errno): ret;
}

/**
 * implements rename
 */
int caching_rename(const char *path, const char *newpath){

	lf->command("rename");
	if (LogFileAccess(path)){ // trying to reach our logfile
		return -ENOENT;
	}


	std::string oldPath(absolute_path(path));
	std::string newPath(absolute_path(newpath));

	int ret = rename(oldPath.c_str(), newPath.c_str());
	if (ret < 0){
		return -errno;
	}

	// also change in the cache
	cache->inner_rename(&oldPath, &newPath);
	return ret;
}

/**
 * implements init
 */
void* caching_init(struct fuse_conn_info*){

	lf->command("init");
	return NULL;
}

/**
 * implements destroy
 */
void caching_destroy(void*){

	lf->command("destroy");
	delete(cache);
	delete(lf);
	return;
}

/**
 * implements ioctl
 */
int caching_ioctl (const char*, int, void*,
                   struct fuse_file_info *, unsigned int, void*){
	lf->command("ioctl");
	cache->ioctl_info(&lf, rootdir);
	return SUCCESS;
}



void init_caching_oper() {

	g_caching_oper.getattr = caching_getattr;
	g_caching_oper.access = caching_access;
	g_caching_oper.open = caching_open;
	g_caching_oper.read = caching_read;
	g_caching_oper.flush = caching_flush;
	g_caching_oper.release = caching_release;
	g_caching_oper.opendir = caching_opendir;
	g_caching_oper.readdir = caching_readdir;
	g_caching_oper.releasedir = caching_releasedir;
	g_caching_oper.rename = caching_rename;
	g_caching_oper.init = caching_init;
	g_caching_oper.destroy = caching_destroy;
	g_caching_oper.ioctl = caching_ioctl;
	g_caching_oper.fgetattr = caching_fgetattr;


	g_caching_oper.readlink = NULL;
	g_caching_oper.getdir = NULL;
	g_caching_oper.mknod = NULL;
	g_caching_oper.mkdir = NULL;
	g_caching_oper.unlink = NULL;
	g_caching_oper.rmdir = NULL;
	g_caching_oper.symlink = NULL;
	g_caching_oper.link = NULL;
	g_caching_oper.chmod = NULL;
	g_caching_oper.chown = NULL;
	g_caching_oper.truncate = NULL;
	g_caching_oper.utime = NULL;
	g_caching_oper.write = NULL;
	g_caching_oper.statfs = NULL;
	g_caching_oper.fsync = NULL;
	g_caching_oper.setxattr = NULL;
	g_caching_oper.getxattr = NULL;
	g_caching_oper.listxattr = NULL;
	g_caching_oper.removexattr = NULL;
	g_caching_oper.fsyncdir = NULL;
	g_caching_oper.create = NULL;
	g_caching_oper.ftruncate = NULL;
}


/**
 * checks main args
 */
void check_args(const char* mountdir, const double num_args[]){

	if (*std::min_element(num_args, num_args+2) <= 0 ||
			num_args[1]+num_args[2] > 1){
		std::cout << USAGE << std::endl;
		exit(FALIURE);
	}

	DIR* d1 = opendir(rootdir);
	DIR* d2 = opendir(mountdir);

	if (errno == ENOENT){ // one or both directories don't exist
		std::cout << USAGE << std::endl;
		exit(FALIURE);
	}

	closedir(d1);
	closedir(d2);
	return;
}



int main(int argc, char* argv[]){

	if (argc < MINIMUM_ARGS_NUM) {
		std::cout << USAGE << std::endl;
		exit(FALIURE);
	}

	char mountdir[PATH_MAX];
	if (realpath(argv[1], rootdir) == NULL ||
	    realpath(argv[2], mountdir) == NULL){
		std::cerr << DIR_ERROR << std::endl;
		exit(FALIURE);
	}

	const double num_args[] =
			{(double) atoi(argv[3]), atof(argv[4]), atof(argv[5])};
	check_args(mountdir, num_args);

	try {
		cache = new Cache(num_args);
	} catch (int err){
		std::cout << USAGE << std::endl;
		exit(FALIURE);
	} catch (std::bad_alloc& err){
		std::cerr << ALLOCATION_FAIL << std::endl;
		exit(FALIURE);
	}

	try {
		lf = new LogFile(rootdir);
	} catch (std::bad_alloc& err){
		delete(cache);
		std::cerr << ALLOCATION_FAIL << std::endl;
		exit(FALIURE);
	}

	char* fuse_args[] = {argv[0], mountdir, (char*)"-s"};
	int number_of_args = FUSE_ARG_NUM;

	init_caching_oper();
	int fuse_stat =fuse_main(number_of_args, fuse_args, &g_caching_oper,NULL);
	return fuse_stat;
}

