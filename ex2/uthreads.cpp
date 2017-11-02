

using namespace std;

#include <cstdlib>
#include "uthreads.h"
#include <signal.h>
#include <sys/time.h>
#include <list>
#include <map>
#include <vector>
#include <queue>
#include <setjmp.h>
#include <iostream>
#include <memory>

#define FAILURE -1
#define SUCCESS 0
#define SYSTEM_FAILURE 1

typedef enum {READY = 1, SLEEP = 2, RUNNING = 3, BLOCKED = 4} State;

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
	address_t ret;
	asm volatile("xor    %%fs:0x30,%0\n"
			"rol    $0x11,%0\n"
	: "=g" (ret)
	: "0" (addr));
	return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
		"rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif

class Thread;

// segment for the timer signal
struct sigaction g_sa;

// timer for the slot
struct itimerval g_timer;

int g_current_running_thread;

// total quantums run in the current process
int g_total_quantums = 0;


// env buffer
vector<sigjmp_buf> g_thread_env(MAX_THREAD_NUM);

// stack for each thread
vector<char[STACK_SIZE]> g_stack(MAX_THREAD_NUM);

// for blocking and unblocking the timer signal
sigset_t g_mask_set;

// (tid:thread) map
map<int, unique_ptr<Thread> > g_thread_map;

// queue of unused tids. implemented as a min_heap
priority_queue<int, vector<int>, greater<int> >g_unused_indexes;

// a list of all the thread tids waiting to run
list<int> g_ready_threads;

// sleeping map. each key is the wake up time,
// each value is a vector containing the all the thread
// tids with that wake up time.
map<int, vector<int> > g_sleeping_thread_map;



/**
 * prints an error when a library function fails.
 */
void terror(string msg);

/**
 * prints an error when a system call fails, and exits.
 */
void serror(string msg);

/**
 * blocks the SIGVTALRM signal
 */
inline void mask_block(){
	if (sigprocmask(SIG_BLOCK, &g_mask_set, NULL)){
		serror("sigprocmask failed");
	};
}

/**
 * unblocks the SIGVTALRM signal
 */
inline void mask_unblock(){
	if (sigprocmask(SIG_UNBLOCK, &g_mask_set, NULL)){
		serror("sigprocmask failed");
	};
}

/**
 * flushes all data structures used, and exits the process.
 */
void general_exit(int exit_type){

	g_sleeping_thread_map.clear();
	g_thread_map.clear();
	g_sleeping_thread_map.clear();
	g_ready_threads.clear();
	g_thread_env.clear();
	g_stack.clear();
	mask_unblock();
	if (sigemptyset(&g_mask_set)){
		serror("sigemptyset failed");
	}
	exit(exit_type);
}

/**
 * prints an error when a library function fails.
 */
void terror(string msg){
	cerr << "thread library error: " << msg << endl;
}

/**
 * prints an error when a system call fails, and exits.
 */
void serror(string msg){
	cerr << "system error: " << msg << endl;
	general_exit(SYSTEM_FAILURE);
}



class Thread {

private:
	Thread() = delete;
	State _state;
	int _tid;
	int _wakeup_time;
	int _quantum_count;

public:

	/**
	 * thread constructor.
	 */
	Thread(int tid):
		_state(READY),
		_tid(tid),
		_wakeup_time(0),
		_quantum_count(0)
	{}

	/**
	 * thread destructor
	 */
	~Thread(){}

	/**
	 * blocks a specific thread.
	 */
	int block() {
		if (_state == READY) {
			_state = BLOCKED;
			g_ready_threads.remove(_tid);
			mask_unblock();
			return SUCCESS;
		}
		else if(_state == RUNNING){
			_state = BLOCKED;

			int jump = sigsetjmp(g_thread_env[_tid], 0);
			if(!jump){
				check_sleepers();
				next_run();
				serror("siglongjmp failed");
			}
			return SUCCESS;
		}
		mask_unblock();
		return SUCCESS;
	}

	/**
	 * resumes a specific thread from the
	 * blocked state.
	 */
	void resume() {
		if (_state == BLOCKED){
			_state = READY;
			g_ready_threads.push_back(_tid);
		}
		return;
	}

	/**
	 * a thread switcher. called every time the running thread
	 * is blocked / goes to sleep / the quanta ended.
	 */
	static int next_run() {

		try {

			int front = g_ready_threads.front();
			g_ready_threads.pop_front();

			// next thread
			g_current_running_thread = front;
			if (g_thread_map.find(front) != g_thread_map.end()) {
				g_thread_map[front]->_quantum_count++;
				g_thread_map[front]->_state = RUNNING;
			}
			g_total_quantums++;


			if (setitimer(ITIMER_VIRTUAL, &g_timer, NULL)) {
				serror("timer failed");
			}

			mask_unblock();
			siglongjmp(g_thread_env[front], 1);
		}
		catch (const out_of_range& eer) {
			terror(eer.what());
			general_exit(FAILURE);
			return FAILURE;
		}
	}

	/**
	 * puts a specific thread to sleep.
	 */
	void sleep(int sleep_quantums) {
		// not including the current quantum
		if (_state == RUNNING) {

			// for example went to sleep at in between slots
			// 100-101 then wake up in slot 108 (100 + 7 + 1)
			_wakeup_time = g_total_quantums + sleep_quantums + 1;
			_state = SLEEP;
			g_sleeping_thread_map[_wakeup_time].push_back(_tid);

			int jump = sigsetjmp(g_thread_env[_tid], 0);
			if(!jump){
				check_sleepers();
				next_run();
				serror("siglongjmp failed");
			}
		}
		mask_unblock();
		return;
	}

	/**
	 * returns the wake up time of a specific thread.
	 */
	int time_to_wake() {
		if (_state == SLEEP) {
			return (_wakeup_time - g_total_quantums);
		}
		return FAILURE;
	}

	/**
	 * returns the quantum count for a specific thread.
	 */
	inline int get_quantum_count() {
		return _quantum_count;
	}

	/**
	 * the handler for the signal.
	 */
	static void scheduler(int){

		mask_block();

		check_sleepers();

		g_thread_map[g_current_running_thread]->_state = READY;
		g_ready_threads.push_back(g_current_running_thread);

		int jump = sigsetjmp(g_thread_env[g_current_running_thread], 0);
		if(!jump){
			next_run();
			serror("siglongjmp failed");
		}
	}

	/**
	 * goes over the sleeping map and checks who need to wake up.
	 */
	static void check_sleepers(){

		if (g_sleeping_thread_map.find(g_total_quantums+1) ==
		    g_sleeping_thread_map.end()){
			return;
		}

		for(vector<int>::iterator it =
			g_sleeping_thread_map[g_total_quantums+1].begin(); it !=
			g_sleeping_thread_map[g_total_quantums+1].end(); it++){

			g_thread_map[*it]->_state = READY;
			g_ready_threads.push_back(*it);
		}
		g_sleeping_thread_map.erase(g_total_quantums+1);
	}

};



// ===================================================================== //



/*
 * Description: This function initializes the Thread library.
 * You may assume that this function is called before any other Thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs){

	if(quantum_usecs > 0){

		// init the unused indexes queue
		for (int i = 0; i < MAX_THREAD_NUM; ++i) {
			g_unused_indexes.push(i);
		}

		int tid = g_unused_indexes.top();
		g_unused_indexes.pop();

		g_thread_map[tid] = unique_ptr<Thread> (new Thread(tid));
		g_ready_threads.push_back(tid);

		// setting function where SIGVTALRM signal goes
		g_sa.sa_handler = &Thread::scheduler;
		g_sa.sa_flags = 0;

		if (sigaction(SIGVTALRM, &g_sa, NULL)) {
			serror("sigaction failed");
		}

		// initiate the time. set to one quantum
		g_timer.it_value.tv_sec = 0;
		g_timer.it_value.tv_usec = quantum_usecs;

		g_timer.it_interval.tv_sec = 0;
		g_timer.it_interval.tv_usec = quantum_usecs;

		if (sigemptyset(&g_mask_set)){
			serror("sigemptyset failed");
		}
		if (sigaddset(&g_mask_set, SIGVTALRM)){
			serror("sigaddset failed");
		}
		if (sigprocmask(SIG_SETMASK, &g_mask_set, NULL)) {
			serror("sigprocmask failed");
		}

		int jump = sigsetjmp(g_thread_env[tid], 0);
		if(!jump){
			Thread::next_run();
			serror("siglongjmp failed");
		}
		return SUCCESS;
	}

	terror("non-positive quantum time");
	return FAILURE;
}

/*
 * Description: This function creates a new Thread, whose entry point is the
 * function f with the signature void f(void). The Thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each Thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created Thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)(void)){

	mask_block();

	if (g_unused_indexes.empty()){
		// no more threads can be made (by the given limit)
		terror("thread limit exceeded");
		mask_unblock();
		return FAILURE;
	}

	int tid = g_unused_indexes.top();
	g_unused_indexes.pop();

	g_thread_map[tid] = unique_ptr<Thread> (new Thread(tid));

	sigsetjmp(g_thread_env[tid], 0);
	(g_thread_env[tid]->__jmpbuf)[JB_SP] = translate_address
			((address_t)(g_stack[tid]) + STACK_SIZE - sizeof(address_t));
	(g_thread_env[tid]->__jmpbuf)[JB_PC] = translate_address((address_t)f);

	g_ready_threads.push_back(tid); // now it is ready to run

	mask_unblock();
	return tid;
}

/*
 * Description: This function terminates the Thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this Thread should be released. If no Thread with ID tid
 * exists it is considered as an error. Terminating the main Thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the Thread was successfully
 * terminated and -1 otherwise. If a Thread terminates itself or the main
 * Thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){

	mask_block();

	if(tid == 0){
		general_exit(SUCCESS);
	}

	if (g_thread_map.find(tid) == g_thread_map.end()){
		terror("terminate: thread not found");
		mask_unblock();
		return FAILURE;
	}


	g_thread_map.erase(tid);
	g_unused_indexes.push(tid);
	if (tid == g_current_running_thread){ // the thread terminated itself
		Thread::check_sleepers();
		Thread::next_run();
		serror("siglongjmp failed");
	}

	g_ready_threads.remove(tid);
	mask_unblock();
	return SUCCESS;

}

/*
 * Description: This function blocks the Thread with ID tid. The Thread may
 * be resumed later using uthread_resume. If no Thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main Thread (tid == 0). If a Thread blocks itself, a scheduling decision
 * should be made. Blocking a Thread in BLOCKED or SLEEPING states has no
 * effect and is not considered as an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid){

	mask_block();

	if(tid == 0){
		terror("main thread cannot be blocked ");
		mask_unblock();
		return FAILURE;
	}

	if (g_thread_map.find(tid) == g_thread_map.end()) {
		terror("block: thread not found");
		mask_unblock();
		return FAILURE;
	}

	return g_thread_map[tid]->block();

}

/*
 * Description: This function resumes a blocked Thread with ID tid and moves
 * it to the READY state. Resuming a Thread in the RUNNING, READY or SLEEPING
 * state has no effect and is not considered as an error. If no Thread with
 * ID tid exists it is considered as an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid){

	mask_block();

	if (g_thread_map.find(tid) == g_thread_map.end()) {
		terror("resume: thread not found");
		mask_unblock();
		return FAILURE;
	}

	g_thread_map[tid]->resume();
	mask_unblock();
	return SUCCESS;

}

/*
 * Description: This function puts the RUNNING Thread to sleep for a period
 * of num_quantums (not including the current quantum) after which it is moved
 * to the READY state. num_quantums must be a positive number. It is an error
 * to try to put the main Thread (tid==0) to sleep. Immediately after a Thread
 * transitions to the SLEEPING state a scheduling decision should be made.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums) {

	mask_block();

	if (num_quantums <= 0){
		terror("cannot sleep for negative time");
		mask_unblock();
		return FAILURE;
	}

	if (g_current_running_thread == 0) {
		terror("main thread cannot sleep");
		mask_unblock();
		return FAILURE;
	}
	else{
		g_thread_map[g_current_running_thread]->sleep(num_quantums);
		return SUCCESS;
	}

	mask_unblock();
	return SUCCESS;
}

/*
 * Description: This function returns the number of quantums until the Thread
 * with id tid wakes up including the current quantum. If no Thread with ID
 * tid exists it is considered as an error. If the Thread is not sleeping,
 * the function should return 0.
 * Return value: Number of quantums (including current quantum) until wakeup.
*/
int uthread_get_time_until_wakeup(int tid){

	mask_block();

	if (g_thread_map.find(tid) == g_thread_map.end()){
		terror("wake up: thread not found");
		mask_unblock();
		return FAILURE;
	}

	int return_val  = g_thread_map[tid]->time_to_wake();
	if(return_val < 0) {
		mask_unblock();
		return SUCCESS;
	}
	mask_unblock();
	return return_val;

}

/*
 * Description: This function returns the Thread ID of the calling Thread.
 * Return value: The ID of the calling Thread.
*/
int uthread_get_tid(){
	return g_current_running_thread;
}

/*
 * Description: This function returns the total number of quantums that were
 * started since the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums(){
	return g_total_quantums;
}

/*
 * Description: This function returns the number of quantums the Thread with
 * ID tid was in RUNNING state. On the first time a Thread runs, the function
 * should return 1. Every additional quantum that the Thread starts should
 * increase this value by 1 (so if the Thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * Thread with ID tid exists it is considered as an error.
 * Return value: On success, return the number of quantums of the Thread with
 * ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid){

	mask_block();

	if (g_thread_map.find(tid) == g_thread_map.end()){
		terror("get quantums: thread not found");
		mask_unblock();
		return FAILURE;
	}

	mask_unblock();
	return g_thread_map[tid]->get_quantum_count();

}



