#include <sys/fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>

// Global Defines
#define TRUE 1
#define FALSE 0

#define DEFAULT_BUSY_WAIT_TIME 5000 // 5 Milliseconds
#define DEFAULT_RR_LOOP_TIME 1000	// 1 Millisecond
#define MICRO_SECOND_MULTIPLIER 1000000

// Global data and structures
static int trace_fd = -1;
static int inputpolicy = 0;

/*<======== Do not change anything in this function =========>*/
static void timespec_add_us(struct timespec *t, long us)
{
	// add microseconds to timespecs nanosecond counter
	t->tv_nsec += us * 1000;
	// if wrapping nanosecond counter, increment second counter
	if (t->tv_nsec > 1000000000)
	{
		t->tv_nsec = t->tv_nsec - 1000000000;
		t->tv_sec += 1;
	}
}

/*<======== Do not change anything in this function =========>*/
static inline int timespec_cmp(struct timespec *now, struct timespec *next)
{
	int diff = now->tv_sec - next->tv_sec;
	return diff ? diff : now->tv_nsec - next->tv_nsec;
}

/*<======== Do not change anything in this function =========>*/
static unsigned long long getTimeStampMicroSeconds(void)
{
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * 1000000 + currentTime.tv_usec;
}

/*<======== Do not change anything in this function =========>*/
static long clock_diff(struct timespec start, struct timespec end)
{
	/* It returns difference in time in nano-seconds */
	struct timespec temp;

	if ((end.tv_nsec - start.tv_nsec) < 0)
	{
		temp.tv_sec = end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
	}
	else
	{
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}
	return temp.tv_nsec;
}

/// Returns the time stored by a timespec struct in the form of microseconds
static long getMicroTime(struct timespec* t)
{
	long micro;
	micro = (t->tv_sec * MICRO_SECOND_MULTIPLIER + t->tv_nsec / 1000);
	return micro;
}

/*<======== Do not change anything in this function =========>*/
static int set_attributes(int *priorities, int num_thr, int *Affinities, int *Policies, pthread_attr_t *attributes)
{
	cpu_set_t cpuset;

	for (int i = 0; i < num_thr; i++)
	{
		if ((pthread_attr_init(&attributes[i]) != 0))
		{
			printf("%d - %s %d\n", __LINE__, "Error Initializing Attributes for Thread:", i + 1);
			return FALSE;
		}

		struct sched_param param;

		param.sched_priority = priorities[i];

		CPU_ZERO(&cpuset);
		// CPU_SET: This macro adds cpu to the CPU set set.
		CPU_SET(Affinities[i], &cpuset);

		if ((pthread_attr_setschedpolicy(&attributes[i], Policies[i]) != 0))
		{
			printf("%s %d\n", "Error Setting Scheduling Policy for Thread:", i);
			return FALSE;
		}

		if ((pthread_attr_setschedparam(&attributes[i], &param) != 0))
		{
			printf("%s %d\n", "Error Setting Scheduling Parameters for Thread:", i);
			return FALSE;
		}

		if ((pthread_attr_setaffinity_np(&attributes[i], sizeof(cpu_set_t), &cpuset) != 0))
		{
			printf("%s %d\n", "Error Setting CPU Affinities for Thread:", i);
			return FALSE;
		}

		if ((pthread_attr_setinheritsched(&attributes[i], PTHREAD_EXPLICIT_SCHED) != 0))
		{
			printf("%s %d\n", "Error Setting CPU Attributes for Thread:", i);
			return FALSE;
		}
	}

	return TRUE;
}

/*<======== Do not change anything in this function =========>*/
static void trace_write(const char *fmt, ...)
{
	va_list ap;
	char buf[256];
	int n;

	if (trace_fd < 0)
	{
		return;
	}

	va_start(ap, fmt);
	n = vsnprintf(buf, 256, fmt, ap);
	va_end(ap);

	write(trace_fd, buf, n);
}

/*<======== Do not change anything in this function =========>*/
static void workload(int tid)
{
	struct timespec ct; // Current time
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ct) != 0)
	{
		fprintf(stderr, "%s\n", "Error Fetching Clock Start Time.");
		return;
	}
	struct timespec workTimer = ct; // Workload timer - default 5ms
	struct timespec slotTimer = ct; // RR Slot timer

	unsigned int counter = 1;

	while (1)
	{
		long micro_ct = getMicroTime(&ct);
		long micro_work = getMicroTime(&workTimer);
		long micro_slot = getMicroTime(&slotTimer);
		
		if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ct) != 0)
		{
			fprintf(stderr, "%s\n", "Error Fetching Clock Start Time.");
			return;
		}

		// Check the total workload time
		if (micro_ct - micro_work >= DEFAULT_BUSY_WAIT_TIME)
			break;

		// Check the RR timeslot. Sa
		if (inputpolicy == SCHED_RR && ((micro_ct - micro_slot) >= DEFAULT_RR_LOOP_TIME))
		{
			//trace_write("RTS_Thread_%d RR Slot: %d", tid, counter++);
			slotTimer.tv_sec = ct.tv_sec;
			slotTimer.tv_nsec = ct.tv_nsec;
		}
	}
}