#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <pthread.h> 
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <stdlib.h>
#include <math.h> 
  
#define MAX_THREADS 48

/* Thread placement policy*/
enum placement_policy{
    PLACEMENT_SPREAD,
    PLACEMENT_COMPACT,
    PLACEMENT_CROSS,
};
typedef enum placement_policy placement_policy;

/* Request given by master*/
enum master_request{
    REQUEST_WORK,
    REQUEST_QUIT,
};
typedef enum master_request master_request;

typedef enum placement_policy placement_policy;
/* worker_info */
typedef struct worker_info_t {
	int		tid;            /* thread id */
	int		cid;			/* core id */
	int		invoked;
	int		request;
	int		new_request;
	void*	input;
	int 	(*func)(void*);
} worker_info_t;

extern int skeleton_initialization(int num_threads, int num_cores, int cores_per_soc, int policy, void* input[], void *func);
extern int skeleton_destroy();
extern int skeleton_master_add_working_request_and_wait();
