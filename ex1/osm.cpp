
#include <sys/time.h>
#include <cmath>
#include <sys/unistd.h>
#include <stdio.h>

#include "osm.h"

#define NANO_ADJUST_TH 1000
#define NANO_ADJUST_BIL 1000000000
#define FAILURE -1
#define SUCCESS 0
#define DEFAULT 1000
#define INSTRUCTION_LOOP_GROUP 6
#define HOST_LEN 1024

FILE* file;

/**
* Initialization function that the user must call
* before running any other library function.
* The function may, for example, allocate memory or
* create/open files.
* Returns 0 uppon success and -1 on failure
*/
int osm_init()
{
	file = fopen("/tmp/liavst2.txt", "w");
	return (file ==  NULL) ? FAILURE: SUCCESS;
}


/**
* finalizer function that the user must call
* after running any other library function.
* The function may, for example, free memory or
* close/delete files.
* Returns 0 uppon success and -1 on failure
*/
int osm_finalizer()
{
	if (fclose(file) == EOF)
	{
		return FAILURE;
	}

	return remove("/tmp/liavst2.txt") ? FAILURE: SUCCESS;
}



/**
* some void function to calculate the function call time
*/
void someFunction()
{}


/**
* returns the elapsed time calculated by gettimeofday
* @param timeBegin - the beginning time
* @param timeEnd - the final time
*/
double elapsedTime(timeval* timeBegin, timeval* timeEnd)
{
	return (timeEnd->tv_sec - timeBegin->tv_sec) * NANO_ADJUST_BIL +
	       (timeEnd->tv_usec - timeBegin->tv_usec) * NANO_ADJUST_TH;
}


/**
* returns the average time it takes to perform a single operation
* @param iterations - the number of times to compute the time
*/
double osm_operation_time(unsigned int iterations)
{
	iterations = (!iterations) ? DEFAULT: iterations;

	timeval timeBegin, timeEnd;
	int loopJumps = std::floor(iterations/INSTRUCTION_LOOP_GROUP);

	if (gettimeofday(&timeBegin, NULL) == FAILURE)
	{
		return FAILURE;
	}

	for (int i = 0; i < loopJumps; i++) // with loop unrolling
	{
		1 + 1;
		1 + 1;
		1 + 1;
		1 + 1;
		1 + 1;
		1 + 1;
	}

	return (gettimeofday(&timeEnd, NULL) == FAILURE) ?
	       FAILURE: (double)(elapsedTime(&timeBegin, &timeEnd) / iterations);
}


/**
* returns the average time it takes to perform a function call operation
* @param iterations - the number of times to compute the time
*/
double osm_function_time(unsigned int iterations)
{
	iterations = (!iterations) ? DEFAULT: iterations;

	timeval timeBegin, timeEnd;
	int loopJumps = std::floor(iterations/INSTRUCTION_LOOP_GROUP);

	if (gettimeofday(&timeBegin, NULL) == FAILURE)
	{
		return FAILURE;
	}

	for (int i = 0; i < loopJumps; i++) // with loop unrolling
	{
		someFunction();
		someFunction();
		someFunction();
		someFunction();
		someFunction();
		someFunction();
	}

	return (gettimeofday(&timeEnd, NULL) == FAILURE) ?
	       FAILURE: (double)(elapsedTime(&timeBegin, &timeEnd) / iterations);
}

/**
* returns the average time it takes to perform a null system call operation
* @param iterations - the number of times to compute the time
*/
double osm_syscall_time(unsigned int iterations)
{
	iterations = (!iterations) ? DEFAULT: iterations;

	timeval timeBegin, timeEnd;
	int loopJumps = std::floor(iterations/INSTRUCTION_LOOP_GROUP);

	if (gettimeofday(&timeBegin, NULL) == FAILURE)
	{
		return FAILURE;
	}

	for (int i = 0; i < loopJumps; i++) // with loop unrolling
	{
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
		OSM_NULLSYSCALL;
	}

	return (gettimeofday(&timeEnd, NULL) == FAILURE) ?
	       FAILURE: (double)(elapsedTime(&timeBegin, &timeEnd) / iterations);
}


/**
* returns the average time it takes to perform a disk access operation
* @param iterations - the number of times to compute the time
*/
double osm_disk_time(unsigned int iterations)
{
	iterations = (!iterations) ? DEFAULT: iterations;

	timeval timeBegin, timeEnd;
	int loopJumps = std::floor(iterations/INSTRUCTION_LOOP_GROUP);

	if (gettimeofday(&timeBegin, NULL) == FAILURE)
	{
		return FAILURE;
	}

	for (int i = 0; i < loopJumps; i++) // with loop unrolling
	{
		if (fclose(file) == EOF) return FAILURE;
		if ((file = fopen("/tmp/liavst2.txt", "w")) == NULL) return FAILURE;
		if (fclose(file) == EOF) return FAILURE;
		if ((file = fopen("/tmp/liavst2.txt", "w")) == NULL) return FAILURE;
		if (fclose(file) == EOF) return FAILURE;
		if ((file = fopen("/tmp/liavst2.txt", "w")) == NULL) return FAILURE;
	}

	return (gettimeofday(&timeEnd, NULL) == FAILURE) ?
	       FAILURE: (double)(elapsedTime(&timeBegin, &timeEnd) / iterations);
}


/**
* returns a struct containing all the information about the
* different timing
* @param operation_iterations - for a simple operation timing
* @param function_iterations - for function call timing
* @param syscall_iterations - for system call timing
* @param disk_iterations - for disk access timing
*/
timeMeasurmentStructure measureTimes (unsigned int operation_iterations,
                                      unsigned int function_iterations,
                                      unsigned int syscall_iterations,
                                      unsigned int disk_iterations)
{
	timeMeasurmentStructure times;
	times.machineName = new char[HOST_LEN];
	if (gethostname(times.machineName, HOST_LEN) == FAILURE)
	{
		times.machineName = NULL;
	}

	times.instructionTimeNanoSecond = osm_operation_time(operation_iterations);
	times.functionTimeNanoSecond = osm_function_time(function_iterations);
	times.trapTimeNanoSecond = osm_syscall_time(syscall_iterations);
	times.diskTimeNanoSecond = osm_disk_time(disk_iterations);

	// checking for errors
	if (times.instructionTimeNanoSecond &&
	    times.instructionTimeNanoSecond != FAILURE)
	{
		if (times.diskTimeNanoSecond != FAILURE)
		{
			times.diskInstructionRatio = (double)(times.diskTimeNanoSecond /
			                                      times.instructionTimeNanoSecond);
		} else {
			times.diskInstructionRatio = FAILURE;
		}

		if (times.functionTimeNanoSecond != FAILURE)
		{
			times.functionInstructionRatio = (double)(times.functionTimeNanoSecond /
			                                times.instructionTimeNanoSecond);
		} else {
			times.functionInstructionRatio = FAILURE;
		}

		if (times.trapTimeNanoSecond != FAILURE)
		{
			times.trapInstructionRatio = (double)(times.trapTimeNanoSecond /
			                                      times.instructionTimeNanoSecond);
		} else {
			times.trapInstructionRatio = FAILURE;
		}

	} else {// there was a problem in the instruction time
		times.diskInstructionRatio = FAILURE;
		times.functionInstructionRatio = FAILURE;
		times.trapInstructionRatio = FAILURE;
	}

	return times;
}










