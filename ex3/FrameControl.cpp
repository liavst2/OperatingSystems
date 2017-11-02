
#include "FrameControl.h"

////////////////   class LogFile implementation  //////////////////////

LogFile::LogFile(int mThreadLevel){
	__lfile.open(".MapReduceFramework.log", std::ios_base::app);
	__lfile << "runMapReduceFramework started with " <<
	mThreadLevel << " threads" << std::endl;
}

LogFile::~LogFile(){
	__lfile << "runMapReduceFramework finished" << std::endl;
	__lfile.close();
}

void LogFile::tstatus(std::string name, std::string state){
	time_t rawtime;
	char date[DATE_LEN];
	std::time(&rawtime);
	struct tm* timeinfo = localtime(&rawtime);
	strftime(date,DATE_LEN,"[%d.%m.%Y %H:%M:%S]",timeinfo);
	__lfile << "Thread " << name << state << date << std::endl;
}

void LogFile::MapShuffleTime(double time){
	__lfile << "Map and Shuffle took " << time << " ns" << std::endl;
}

void LogFile::ReduceTime(double time){
	__lfile << "Reduce took " << time << " ns" << std::endl;
}

void LogFile::open_log_file(int multiThreadLevel){
	try {
		g_lf = new LogFile(multiThreadLevel);
	} catch (std::bad_alloc& err){
		manager.Abort("new");
	}
}

/**
* start time measurement
*/
void LogFile::tic() {
	if (gettimeofday(&g_timeBegin, NULL)) {
		manager.Abort("gettimeofday");
	}
	return;
}


/**
* end time measurement
*/
void LogFile::toc() {
	if (gettimeofday(&g_timeEnd, NULL)) {
		manager.Abort("gettimeofday");
	}
	return;
}


/**
* calculate the time elapsed from tic to toc
*/
double LogFile::elapsedTime() {
	return (g_timeEnd.tv_sec - g_timeBegin.tv_sec) * NANO_SEC_ADJUST +
	       (g_timeEnd.tv_usec - g_timeBegin.tv_usec) * NANO_MICROSEC_ADJUST;
}


/**
* documents progress to the log file
* @param action - which action
* @param name - the name of the thread
* @param state - created or terminated
*/
void LogFile::document(Action action, std::string name, std::string state) {
	pthread_mutex_lock(&log_lock);
	switch (action) {
		case THREAD_STATUS:
			g_lf->tstatus(name, state);
			break;
		case MAP_SHUFFLE_TIME:
			g_lf->MapShuffleTime(elapsedTime());
			break;
		case REDUCE_TIME:
			g_lf->ReduceTime(elapsedTime());
			break;
		default:
			break;
	}
	pthread_mutex_unlock(&log_lock);
	return;
}

////////////////   class Comparator implementation  //////////////////////


bool Comparator::operator()(const k2Base* key1, const k2Base* key2) const{
	return *key1 < *key2;
}


////////////////   class LogFile implementation  //////////////////////


void Manager::_clearg_ShuffleMap(){
	for (auto& mapItem: g_ShuffleMap){
		if (mapItem.second) {
			delete mapItem.second;
		}
	}
	g_ShuffleMap.clear();
}

void Manager::_clearg_MapMap(){
	for (auto& mapItem: g_MapMap){
		if (mapItem.second) {
			delete mapItem.second;
		}
	}
	g_MapMap.clear();
}

void Manager::_clearg_ReduceMap(){
	for (auto& mapItem: g_ReduceMap){
		if (mapItem.second) {
			delete mapItem.second;
		}
	}
	g_ReduceMap.clear();
}

void Manager::mutex_init() {
	ExecReduce_lock = PTHREAD_MUTEX_INITIALIZER;
	ExecMap_lock = PTHREAD_MUTEX_INITIALIZER;
	index_lock= PTHREAD_MUTEX_INITIALIZER;
	log_lock= PTHREAD_MUTEX_INITIALIZER;
	shuffle_lock= PTHREAD_MUTEX_INITIALIZER;
	shuffle_condition = PTHREAD_MUTEX_INITIALIZER;
	shuffle_wakeup = PTHREAD_COND_INITIALIZER;
}

void Manager::_mutex_destroy_(){
	if (pthread_mutex_destroy(&ExecMap_lock)){
		Abort("pthread_mutex_destroy");
	}
	if (pthread_mutex_destroy(&index_lock)){
		Abort("pthread_mutex_destroy");
	}
	if (pthread_mutex_destroy(&log_lock)){
		Abort("pthread_mutex_destroy");
	}
	if (pthread_mutex_destroy(&shuffle_lock)){
		Abort("pthread_mutex_destroy");
	}
	if (pthread_mutex_destroy(&shuffle_condition)){
		Abort("pthread_mutex_destroy");
	}
	if (pthread_cond_destroy(&shuffle_wakeup)){
		Abort("pthread_mutex_destroy");
	}
}

void Manager::clearStructs(){
	_clearg_ShuffleMap();
	_clearg_MapMap();
	_clearg_ReduceMap();
	g_ShuffleVec.clear();
	g_InputVec.clear();
	_mutex_destroy_();
	if (g_lf){
		delete(g_lf);
	}
	return;
}

void Manager::Abort(std::string who) {
	std::cerr << "MapReduceFramework Failure: " << who
										<< " failed." << std::endl;
	manager.clearStructs();
	exit(FAILURE);
}