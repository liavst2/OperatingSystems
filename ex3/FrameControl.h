#ifndef EX3_LOG_H
#define EX3_LOG_H

#include "MapReduceFramework.h"

#include <fstream>
#include <ctime>
#include <sys/time.h>
#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include <pthread.h>
#include <algorithm>


#define DATE_LEN 80
#define NANO_MICROSEC_ADJUST 1000
#define NANO_SEC_ADJUST 1000000000
#define FAILURE 1
#define MAP_SET_SIZE 8
#define REDUCE_SET_SIZE 6
#define SHUFFLE_DELAY 10000000
#define START -1

typedef enum{THREAD_STATUS = 1, MAP_SHUFFLE_TIME = 2, REDUCE_TIME = 3} Action;
typedef std::pair<k2Base*, v2Base*> MID_ITEM;
class LogFile;
class Comparator;
class Manager;


extern LogFile* g_lf;
extern Manager manager;
extern timeval g_timeBegin, g_timeEnd;

extern int g_run_shuffle;
extern int g_protected_index;


extern std::vector<IN_ITEM> g_InputVec;
extern std::vector<std::pair<k2Base*, V2_LIST>> g_ShuffleVec;
extern std::map<pthread_t, std::queue<MID_ITEM>*> g_MapMap;
extern std::map<k2Base*, V2_LIST*, Comparator> g_ShuffleMap;
extern std::map<pthread_t, OUT_ITEMS_LIST*> g_ReduceMap;


extern pthread_mutex_t ExecReduce_lock;
extern pthread_mutex_t ExecMap_lock;
extern pthread_mutex_t index_lock;
extern pthread_mutex_t log_lock;
extern pthread_mutex_t shuffle_lock;
extern pthread_mutex_t shuffle_condition;
extern pthread_cond_t shuffle_wakeup;


/**
 * functor for sorting the shuffle map
 */
class Comparator {
public:
	bool operator()(const k2Base* key1, const k2Base* key2) const;
};


class Manager {
private:

	/**
	 * clear the shuffle output
	 */
	void _clearg_ShuffleMap();

	/**
	 * clear Map output
	 */
	void _clearg_MapMap();

	/**
	 * clear Reduce output
	 */
	void _clearg_ReduceMap();

	/**
	 * destroy all mutexes
	 */
	void _mutex_destroy_();

public:

	/**
	 * clear the global data structures
	 */
	void clearStructs();

	/**
	 * called whenever a framework function fails
	 * @param who - which function failed.
	 */
	void Abort(std::string who);

	/**
	 * init all the framework muitexes
	 */
	void mutex_init();

};


/**
 * implements the log file
 */
class LogFile{
private:
	std::ofstream __lfile;

public:
	/**
	 * constructor
	 */
	LogFile(int mThreadLevel);

	/**
	 * destructor
	 */
	~LogFile();

	/**
	 * documents thread creation and termination
	 */
	void tstatus(std::string name, std::string state);

	/**
	 * documents map and shuffle time
	 */
	void MapShuffleTime(double time);

	/**
	 * documents reduce time
	 */
	void ReduceTime(double time);

	/**
	 * opens the log file for documentation
	 */
	static void open_log_file(int multiThreadLevel);


	/**
	 * start time measurement
	 */
	static void tic();


	/**
	 * end time measurement
	 */
	static void toc();


	/**
	 * calculate the time elapsed from tic to toc
	 */
	static double elapsedTime();


	/**
	 * documents progress to the log file
	 * @param action - which action
	 * @param name - the name of the thread
	 * @param state - created or terminated
	 */
	static void document(Action action, std::string name, std::string state);
};

#endif //EX3_LOG_H