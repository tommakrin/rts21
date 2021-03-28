/* Created by: Muhammad Junaid Aslam
*  Teaching Assistant, Real-Time Systems Course IN4343, 2020
*  PhD Candidate Technical University of Delft, Netherlands.
*/

// Header File inclusion
#define _GNU_SOURCE

#include "helpers.h"
#include <stdbool.h>

#define NUM_THREADS 3
#define HIGHEST_PRIORITY 99

#define handle_error_en(en, msg) \
	do                           \
	{                            \
		errno = en;              \
		perror(msg);             \
		exit(EXIT_FAILURE);      \
	} while (0)

#define POL_TO_STR(x)                                   \
	x == SCHED_FIFO ? "FIFO" : x == SCHED_RR  ? "RR"    \
						   : x == SCHED_OTHER ? "OTHER" \
											  : "INVALID"

#define DEFAULT_BUSY_WAIT_TIME 5000 // 5 Milliseconds
#define DEFAULT_RR_LOOP_TIME 1000	// 1 Millisecond
#define MICRO_SECOND_MULTIPLIER 1000000

static struct thread_args results[NUM_THREADS];
typedef void *(*thread_func_prototype)(void *);

/*<======== This is where you have to work =========>*/
static void *Thread(void *inArgs)
{
	long long thread_response_time = 0;

	/*This fetches the thread control block (thread_args) passed to the thread at the time of creation*/
	struct thread_args *args = (struct thread_args *)inArgs;

	/* tid is just the number of the executing thread; Note that, pthread_t specified thread id is no the same as this thread id as this depicts only the sequence number of this thread.*/
	int tid = args->thread_number;
	int period = args->thread_period;

	struct timespec now;
	struct timespec next;
	struct timespec prev;
	struct timespec dead;
	char line[256];

	trace_write("RTS_Thread_%d Policy:%s Priority:%d\n", /*This is an example trace message which appears at the start of the thread in KernelShark */
				args->thread_number, POL_TO_STR(args->thread_policy), args->thread_priority);

	clock_gettime(CLOCK_REALTIME, &results[tid].thread_start_time); // This fetches the timespec structure through which can get current time.
	// Initialize next release
	next = results[tid].thread_start_time;
	timespec_add_us(&next, period * 1000);

	while (1)
	{
		// Define next arrival.
		clock_gettime(CLOCK_REALTIME, &now);
		prev = next; // Save previous value of next, in order to calculate deadline miss
		timespec_add_us(&next, period * 1000);

		// In case of deadline, abort, otherwise run
		if (timespec_cmp(&now, &prev) > 0)
		{
			trace_write("Thread %d MISSED the deadline!", tid);
			// break;
		}
		else
		{
			clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &prev, NULL);
			trace_write("WAKE: Thread_%d", tid);
			workload(args->thread_number); // This produces a busy wait loop of ~5+/-100us milliseconds
		}

		results[tid].thread_number = args->thread_number;
		results[tid].thread_start_time = prev;
		clock_gettime(CLOCK_REALTIME, &results[tid].thread_end_time);
		results[tid].thread_deadline = prev;
		timespec_add_us(&results[tid].thread_deadline, period * 1000);

		logger(&results[tid]);
	}

	/* In order to change the execution time (busy wait loop) of this thread
	*  from ~5+/-100us milliseconds to XX milliseconds, you have to change the value of
	*  DEFAULT_BUSY_WAIT_TIME macro at the top of this file. 
	*/
	clock_gettime(CLOCK_REALTIME, &results[tid].thread_end_time);

	/* Following sequence of commented instructions should be filled at the end of each periodic iteration*/

	/* Do not change the below sequence of instructions.*/
	results[tid].thread_number = args->thread_number;
	results[tid].thread_policy = args->thread_policy;
	results[tid].thread_affinity = sched_getcpu();
	results[tid].thread_priority = args->thread_priority;
	results[tid].thread_end_timestamp = getTimeStampMicroSeconds();

	trace_write("RTS_Thread_%d Terminated ... ResponseTime:%lld Deadline:%lld", args->thread_number, args->thread_response_time, args->thread_deadline);

	/* <==================== ADD CODE ABOVE =======================>*/
}

/*<======== Do not change anything in this function =========>*/
static void help(void)
{
	printf("Error... Provide One of the Following Scheduling Policies\n");
	printf("1 - FIFO\n");
	printf("2 - RR\n");
	printf("4 - OTHER\n");
}

/*<======== Do not change anything in this function =========>*/
static int comparator(const void *p, const void *q)
{
	if (((struct thread_args *)p)->thread_end_timestamp < ((struct thread_args *)q)->thread_end_timestamp)
	{
		return -1;
	}
	else if (((struct thread_args *)p)->thread_end_timestamp > ((struct thread_args *)q)->thread_end_timestamp)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int main(int argc, char **argv)
{
	/*<======== You have to change the following two data structures whose values will be assigned to threads in sequence =========>*/
	/* NOTE: Do not forget to change the value of macro NUM_THREADS when you want to change the number of threads, 
	 * and then fill the following arrays accordingly. Default value of NUM_THREADS is 4. 
	 * -------------------------------------------------------------------------------------------------------------------------------
	 * Warning: If you run the code without filling in priorities, you will get runtime stack error.
	 * If you do not have a multicore computer, then change the value of affinities[i] from 1 to 0.
	 */

	// Priorities are 0 in SCHED_OTHER and 1-99 for other policies

	// int priorities[NUM_THREADS] = {95, 50, 25}; 
	int priorities[NUM_THREADS] = {0, 0, 0}; 

	int periods[NUM_THREADS] = {10, 20, 30}; // Used in calculation of next period value in a periodic task / thread.

	/*<======== Do not change anything below unless you have to change value of affinities[i] below =========>*/

	thread_func_prototype thread_functions[NUM_THREADS] = {Thread};
	pthread_t thread_id[NUM_THREADS];
	pthread_attr_t attributes[NUM_THREADS];
	int affinities[NUM_THREADS];
	int policies[NUM_THREADS];
	struct thread_args lvargs[NUM_THREADS] = {0};
	srand((unsigned)time(NULL));

	if (argc <= 1)
	{
		help();
		return FALSE;
	}
	else
	{
		if (strcmp(argv[1], "RR") == 0)
		{
			printf("Round Robin Scheduling\n");
			for (int i = 0; i < NUM_THREADS; i++)
			{
				policies[i] = SCHED_RR;
				inputpolicy = SCHED_RR;
				affinities[i] = 1;
			}
		}
		else if (strcmp(argv[1], "FIFO") == 0)
		{
			printf("First in First out Scheduling\n");
			for (int i = 0; i < NUM_THREADS; i++)
			{
				policies[i] = SCHED_FIFO;
				inputpolicy = SCHED_FIFO;
				affinities[i] = 1;
			}
		}
		else if (strcmp(argv[1], "OTHER") == 0)
		{
			printf("Completely Fair Scheduling\n");
			for (int i = 0; i < NUM_THREADS; i++)
			{
				policies[i] = SCHED_OTHER;
				inputpolicy = SCHED_OTHER;
				affinities[i] = 1;
			}
		}
		else
		{
			help();
			return FALSE;
		}
	}

	printf("Assignment_2 Threads:%d Scheduling Policy:%s\n", NUM_THREADS, POL_TO_STR(inputpolicy));

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	// For Main Thread Affinity is set to CPU 0
	if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0)
	{
		printf("%d Main Error Setting CPU Affinity\n", __LINE__);
	}

	trace_fd = open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY);
	if (trace_fd < 0)
	{
		printf("Error Opening trace marker, try running with sudo\n");
	}
	else
	{
		printf("Successfully opened trace_marker\n");
	}

	//trace_write("JATW_1");

	if ((set_attributes(priorities, NUM_THREADS, affinities, policies, attributes) == FALSE))
	{
		printf("Error Setting Attributes of all threads...\n");
	}

	for (int i = 0; i < NUM_THREADS; i++)
	{
		lvargs[i].thread_number = i;
		lvargs[i].thread_policy = policies[i];
		lvargs[i].thread_affinity = affinities[i];
		lvargs[i].thread_period = periods[i];
		lvargs[i].thread_priority = priorities[i];

		int pthread_err = pthread_create(&thread_id[i], &attributes[i], Thread, (void *)&lvargs[i]);

		if (pthread_err != 0)
			printf("%d - %s %d\n", pthread_err, "Error Spawning Thread:", i);
		else
			trace_write("RTS_Spawned Thread_%d\n", i);
	}

	for (int i = 0; i < NUM_THREADS; i++)
	{
		pthread_join(thread_id[i], NULL);
	}

	//trace_write("JATW_2");

	qsort(results, NUM_THREADS, sizeof(struct thread_args), comparator);

	for (int i = 0; i < NUM_THREADS; i++)
	{
		results[i].thread_exectime = (clock_diff(results[i].thread_start_time, results[i].thread_end_time) / 1.0e6);
		printf(
			"--- >>> TimeStamp:%llu Thread:%d Policy:%s Affinity:%d Priority:%d  ExecutionTime:%f Responsetime:%lldms Deadline:%lld\n",
			results[i].thread_end_timestamp, results[i].thread_number, POL_TO_STR(results[i].thread_policy),
			results[i].thread_affinity, results[i].thread_priority, results[i].thread_exectime,
			results[i].thread_response_time, results[i].thread_deadline);
	}

	if (close(trace_fd) == 0)
	{
		printf("Successfully closed the trace file\n");
	}
	return 0;
}