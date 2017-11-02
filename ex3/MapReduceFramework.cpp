
#include "MapReduceFramework.h"
#include "FrameControl.h"


using namespace std;


LogFile* g_lf;
timeval g_timeBegin, g_timeEnd;
Manager manager;

int g_run_shuffle;
int g_protected_index;


vector<IN_ITEM> g_InputVec;
vector<pair<k2Base*, V2_LIST>> g_ShuffleVec;
map<pthread_t, queue<MID_ITEM>*> g_MapMap;
map<k2Base*, V2_LIST*, Comparator> g_ShuffleMap;
map<pthread_t, OUT_ITEMS_LIST*> g_ReduceMap;


pthread_mutex_t ExecReduce_lock;
pthread_mutex_t ExecMap_lock;
pthread_mutex_t index_lock;
pthread_mutex_t log_lock;
pthread_mutex_t shuffle_lock;
pthread_mutex_t shuffle_condition;
pthread_cond_t shuffle_wakeup;


/**
 * multithreaded function to execute Map function in parallel
 * @param mapReduceVoid - the struct holding the Map function
 */
void* ExecMap(void* mapReduceVoid){

	LogFile::document(THREAD_STATUS, "ExecMap", " created ");
	MapReduceBase* mapReduce = (MapReduceBase*) mapReduceVoid;
	int InputVecSize = (int)g_InputVec.size();

	int i = 0;
	do
	{
		pthread_mutex_lock(&index_lock);
		g_protected_index++;
		i = g_protected_index * MAP_SET_SIZE;
		pthread_mutex_unlock(&index_lock);

		// prepare the correct number of iterations
		int iterations = min(MAP_SET_SIZE, InputVecSize - i);

		for(int v = i; iterations > 0 ; iterations--, ++v){
			mapReduce->Map(g_InputVec[v].first, g_InputVec[v].second);
		}

		pthread_cond_signal(&shuffle_wakeup);

	} while (i < InputVecSize);

	pthread_mutex_lock(&shuffle_condition);
	g_run_shuffle--; // when it gets to zero, shuffle stop its routine
	pthread_mutex_unlock(&shuffle_condition);

	LogFile::document(THREAD_STATUS, "ExecMap", " terminated ");

	pthread_exit(NULL);
	return NULL;
}

/**
 * shuffling routine for shuffle thread
 */
void sortMapItems(){

	// <--- shuffling routine ---> //

	for (map<pthread_t, queue<MID_ITEM>*>::iterator it = g_MapMap.begin();
	     it != g_MapMap.end(); it++){
		while(!it->second->empty()) {
			MID_ITEM next_item = it->second->front();
			if (g_ShuffleMap.find(next_item.first) == g_ShuffleMap.end()){
				try {
					g_ShuffleMap[next_item.first] = new V2_LIST();
				} catch (bad_alloc& err){
					manager.Abort("new");
				}
			}
			g_ShuffleMap[next_item.first]->push_back(next_item.second);

			it->second->pop();
		}
	}
	return;
}


/**
 * execute the shuffle routine
 */
void* Shuffle(void*){

	LogFile::document(THREAD_STATUS, "Shuffle", " created ");
	struct timespec waitTime;
	waitTime.tv_sec = 0;
	waitTime.tv_nsec = SHUFFLE_DELAY;

	do
	{
		pthread_mutex_lock(&shuffle_lock);
		pthread_cond_timedwait(&shuffle_wakeup, &shuffle_lock, &waitTime);
		pthread_mutex_unlock(&shuffle_lock);

		sortMapItems();

	} while (g_run_shuffle);

	sortMapItems(); // if the loop ended too soon

	LogFile::document(THREAD_STATUS, "Shuffle", " terminated ");
	pthread_exit(NULL);
	return NULL;
}

/**
 * multithreaded function to execute Reduce function in parallel
 * @param mapReduceVoid - the struct holding the Reduce function
 */
void* ExecReduce(void* mapReduceVoid){

	LogFile::document(THREAD_STATUS, "ExecReduce", " created ");
	MapReduceBase* mapReduce = (MapReduceBase*) mapReduceVoid;
	int ShuffleVecSize = (int)g_ShuffleVec.size();

	int i = 0;
	do
	{
		pthread_mutex_lock(&index_lock);
		g_protected_index++;
		i = g_protected_index * REDUCE_SET_SIZE;
		pthread_mutex_unlock(&index_lock);

		// prepare the correct number of iterations
		int iterations = min(REDUCE_SET_SIZE, ShuffleVecSize - i);

		for(int v = i; iterations > 0 ; iterations--, ++v){
			mapReduce->Reduce(g_ShuffleVec[v].first, g_ShuffleVec[v].second);
		}

	} while (i < ShuffleVecSize);

	LogFile::document(THREAD_STATUS, "ExecReduce", " terminated ");
	pthread_exit(NULL);
	return NULL;
}

/**
 * execute the Map and Shuffle procedures
 * @param mapReduce - a struct holding the Map function
 * @param itemsList - the input of the framework
 * @param multiThreadLevel - the thread amount threshold
 */
void runMapShuffle(MapReduceBase& mapReduce,
                   IN_ITEMS_LIST& itemsList, int multiThreadLevel){
	g_run_shuffle = multiThreadLevel;
	g_protected_index = START;
	manager.mutex_init();
	pthread_t shuffleThread;
	pthread_t MapThreads[multiThreadLevel];

	// converting from list to vector
	for (auto& item: itemsList) {
		g_InputVec.push_back(item);
	}

	// ensuring that Emit2 would not raise segmentation fault
	pthread_mutex_lock(&ExecMap_lock);

	// create the ExecMap thread pool
	for (int i = 0; i < multiThreadLevel; i++) {
		if (pthread_create(&MapThreads[i], NULL, ExecMap, (void*)&mapReduce)){
			manager.Abort("pthread_create");
		}
	}

	try {
		for (int i = 0; i < multiThreadLevel; i++) {
			g_MapMap[MapThreads[i]] = new queue<MID_ITEM>();
		}
	} catch (bad_alloc &err) {
		manager.Abort("new");
	}

	// now ExecMap threads can write to their structs
	pthread_mutex_unlock(&ExecMap_lock);


	if (pthread_create(&shuffleThread, NULL, Shuffle, nullptr)){
		manager.Abort("pthread_create");
	}

	// joining the ExecMap and Shuffle threads
	for (int i = 0; i < multiThreadLevel; i++){
		if (pthread_join(MapThreads[i], NULL)){
			manager.Abort("pthread_join");
		}
	}

	if (pthread_join(shuffleThread, NULL)){
		manager.Abort("pthread_join");
	}

	return;
}

/**
 * Execute Reduce procedure
 * @param mapReduce - the struct holding the Reduce function
 * @param multiThreadLevel - the thread amount threshold
 */
void runReduce(MapReduceBase& mapReduce, int multiThreadLevel){

	g_protected_index = START;
	pthread_t ReduceThreads[multiThreadLevel];

	// converting from map to vector
	for (auto& item: g_ShuffleMap) {
		g_ShuffleVec.push_back
				(pair<k2Base*,V2_LIST>(item.first,*(item.second)));
	}

	// ensuring that writing in Emit3 would not raise segmentation fault
	pthread_mutex_lock(&ExecReduce_lock);

	// create the ExecReduce thread pool
	for (int i = 0; i < multiThreadLevel; i++) {
		if (pthread_create(&ReduceThreads[i], NULL, ExecReduce,
		                                                (void*)&mapReduce)){
			manager.Abort("pthread_create");
		}
	}

	try {
		for (int i = 0; i < multiThreadLevel; i++) {
			g_ReduceMap[ReduceThreads[i]] = new OUT_ITEMS_LIST();
		}
	} catch (bad_alloc &err) {
		manager.Abort("new");
	}

	// now ExecReduce threads can write to their structs
	pthread_mutex_unlock(&ExecReduce_lock);

	// joining the ExecReduce threads
	for (int i = 0; i < multiThreadLevel; i++){
		if (pthread_join(ReduceThreads[i], NULL)){
			manager.Abort("pthread_join");
		}
	}

	return;
}


/**
 * producing the final output
 */
OUT_ITEMS_LIST produceFinalOutput(){
	OUT_ITEMS_LIST finalOutput;
	auto comparator = [](const OUT_ITEM& o1, const OUT_ITEM& o2){
		return *o1.first < *o2.first;
	};

	// merging and sorting
	for(map<pthread_t, OUT_ITEMS_LIST*>::iterator it = g_ReduceMap.begin();
	    it != g_ReduceMap.end(); it++){
		it->second->sort(comparator);
		finalOutput.merge(*(it->second));
	}
	finalOutput.sort(comparator);
	return finalOutput;
}

/**
 * the main MapReduce framework of this project
 * @param mapReduce - a struct holding the Map function
 * @param itemsList - the input of the framework
 * @param multiThreadLevel - the thread amount threshold
 */
OUT_ITEMS_LIST runMapReduceFramework(MapReduceBase& mapReduce,
                    IN_ITEMS_LIST& itemsList, int multiThreadLevel){

	LogFile::tic();
	LogFile::open_log_file(multiThreadLevel);

	////////////////////// starting map and shuffle ////////////////////////

	runMapShuffle(mapReduce, itemsList, multiThreadLevel);
	LogFile::toc();

	LogFile::document(MAP_SHUFFLE_TIME, "", "");

	//////////////// ended map and shuffle, starting reduce ////////////////

	LogFile::tic();
	runReduce(mapReduce, multiThreadLevel);
	OUT_ITEMS_LIST finalOutput = produceFinalOutput();
	LogFile::toc();

	LogFile::document(REDUCE_TIME, "", "");

	///////////////////////// end of framework /////////////////////////////

	manager.clearStructs();
	return finalOutput;
}

/**
 * add a pair to g_MapMap
 */
void Emit2 (k2Base* k2, v2Base* v2){
	pthread_mutex_lock(&ExecMap_lock);
	g_MapMap[pthread_self()]->push(MID_ITEM(k2, v2));
	pthread_mutex_unlock(&ExecMap_lock);
}

/**
 * add a pair to g_ReduceMap
 */
void Emit3 (k3Base* k3, v3Base* v3){
	pthread_mutex_lock(&ExecReduce_lock);
	g_ReduceMap[pthread_self()]->push_back(OUT_ITEM(k3, v3));
	pthread_mutex_unlock(&ExecReduce_lock);
}
