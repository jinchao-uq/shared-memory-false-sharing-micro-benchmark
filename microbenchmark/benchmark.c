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
//#include <extrae.h>
  
#include "skeleton.h"

#define NUM_THREADS 32
#define VEC_LENGTH 1024
#define REPEAT_TIMES 100000000

//typedef int DataType;
typedef short DataType;

/* working stage*/
enum working_stage{
    ARRAY_INIT,
    ARRAY_RR, //Read only
    ARRAY_WW, //write only
    ARRAY_WR, //write and read 
    ARRAY_TS, //Trure sharing
};
typedef enum working_stage working_stage;

enum thread_op {
	OP_R,
	OP_W,
};
typedef enum thread_op thread_op;

/* worker_info */
typedef struct working_request_t {
	int			tid;            /* thread id */
	int			stage;			/* working_stage*/
	int			num_threads;
	int			vector_length;
	int			pos_w, pos_r;
	thread_op 	op;
	DataType*	array_a;
} working_request_t;

/*return the duration in seconds */
double
cal_elapsed_time(struct timeval* tv_begin, struct timeval* tv_end)
{
	double ela_time;
	long ela_secs, ela_usecs;

	if(tv_end->tv_usec >= tv_begin->tv_usec)
	{
		ela_secs  = tv_end->tv_sec  - tv_begin->tv_sec;
		ela_usecs = tv_end->tv_usec - tv_begin->tv_usec;
	} else {
		ela_secs  = tv_end->tv_sec  - tv_begin->tv_sec - 1;
		ela_usecs = tv_end->tv_usec - tv_begin->tv_usec + 1000000;
	}

	ela_usecs += ela_secs * 1000000;
	ela_time = (double)ela_usecs / 1000000;

	return ela_time;
}

int
check_results(DataType* array, int length, DataType expected)
{
	int flag;
	int i;

	flag = 0;
	for(i = 0; i < length; i++)
		printf("CheckResults index: %d, value: %d\n", i, array[i]);
/*		if(array[i] != expected)
		{
			flag = 1;
			printf("CheckResults index: %d, value: %d\n", i, array[i]);
		}
*/
	//printf("CheckResults flag: %d\n", flag);
	return (flag == 0);
}

int
working_func(void* arg)
{ 
	int i, j, pos_w, pos_r, repeat, index, start, end;
	working_request_t* my_request = (working_request_t*)arg;
	int tid = my_request->tid;
	int length = my_request->vector_length;
	int num_thread = my_request->num_threads;
	DataType *x = my_request->array_a;
	DataType temp;
	pos_w = my_request->pos_w;
	pos_r = my_request->pos_r;

	DataType buffer_w[1000];

	switch(my_request->stage) {
		case ARRAY_INIT:
			start = tid * length / num_thread;
			end = (tid + 1) * length / num_thread;
			//printf("Thread[%d] is running on CPU[%d] with position: %d. Partition [%d %d]\n", tid, sched_getcpu(), pos_w, start, end-1);
			for (j = start; j < end; j++)
			{
				x[j] = 17;
			}
			break;
		case ARRAY_RR:
			for (j = 0; j < REPEAT_TIMES; j++) {
				temp = x[pos_r];
			}
			break;
		case ARRAY_WW:
			for (j = 0; j < REPEAT_TIMES; j++) {
				x[pos_w] += 3;
			}
			break;
		case ARRAY_WR:
			//printf("Thread[%d] is running on CPU[%d] with operation: %d.\n", tid, sched_getcpu(), my_request->op);
			if(my_request->op  == OP_W)
				for (j = 0; j < REPEAT_TIMES; j++) {
					x[pos_w] += 3;
				}
			else
				for (j = 0; j < REPEAT_TIMES; j++) {
					temp = x[pos_r];
				}
			break;
		case ARRAY_TS:
			//printf("Thread[%d] is running on CPU[%d] with operation: %d. w:%d, r:%d\n", tid, sched_getcpu(), my_request->op, pos_w, pos_r);
			for (j = 0; j < REPEAT_TIMES; j++) {
				x[pos_w] += 3;
				temp = x[pos_r];
			}
			break;
		default: 
			printf("ERROR: Thread[%d] is on unknown stage: %d \n", tid, my_request->stage);
	}
	return 0;
} 

int
main(int argc, void** argv)
{
	int vector_length;
	int nthread_input;
	char op_input;
	int affinity_input;
	int num_cores;
	int cores_per_soc;
	int thread_op;
	int affinity;
	int i, j, core, stride;
	int distances[13]={1,2,4,8,16,32,64,128,256,512,1024,2048,4096};
	int distance;
	int num_distances = 14;
	DataType *x;
	int check;
    pthread_t threads[NUM_THREADS]; 
	working_request_t** requests;

	struct timeval begin, end;
	double duration;

	if (argc != 7){
		printf("Usage: %s <# threads> <vector length> <operation: r|w|m|s> <core#> <core# per socket> <affinity: s|c>\n", *argv);
		exit(EXIT_FAILURE);
	}

	nthread_input 	= atoi(*++argv);
	vector_length	= atol(*++argv);
	op_input		= *(char*)(*++argv);
	num_cores	 	= atoi(*++argv);
	cores_per_soc 	= atoi(*++argv);
	affinity_input	= *(char*)(*++argv);

	if ((nthread_input < 1) || (nthread_input > MAX_THREADS)) {
		printf("ERROR: Invalid number of threads: %d\n", nthread_input);
		exit(EXIT_FAILURE);
	}

	if (vector_length < 0) {
		printf("ERROR: Invalid vector length: %ld\n", vector_length);
		exit(EXIT_FAILURE);
	}

	if (num_cores < 0) {
		printf("ERROR: Invalid core count: %d\n", num_cores);
		exit(EXIT_FAILURE);
	}

	if (cores_per_soc < 0) {
		printf("ERROR: Invalid core count per socket: %d\n", cores_per_soc);
		exit(EXIT_FAILURE);
	}

	if( num_cores < cores_per_soc || num_cores % cores_per_soc != 0) {
		printf("ERROR: Invalid core count per socket: %d, and core#: %d\n", cores_per_soc, num_cores);
		exit(EXIT_FAILURE);
	}

	switch(op_input) {
		case 'r':
			thread_op =  ARRAY_RR;
			break;
		case 'w':
			thread_op =  ARRAY_WW;
			break;
		case 'm':
			thread_op =  ARRAY_WR;
			break;
		case 's':
			thread_op =  ARRAY_TS;
			break;
		default:
			thread_op =  ARRAY_RR;
	}

	switch(affinity_input) {
		case 's':
			affinity =  PLACEMENT_SPREAD;
			break;
		case 'c':
			affinity =  PLACEMENT_CROSS;
			break;
		default:
			affinity =  PLACEMENT_SPREAD;
	}

    printf ("nth: %d, vector length: %d, ARRAY_OP[%d]\n", nthread_input, vector_length, thread_op);

	x = (DataType*)malloc(sizeof(DataType)*vector_length);

	requests = (void*)malloc(sizeof(working_request_t*)*nthread_input);
	for(i = 0; i < nthread_input; i++)
	{
		requests[i] = malloc(sizeof(working_request_t));
	}

	//skeleton_initialization(nthread_input, num_cores, cores_per_soc, PLACEMENT_SPREAD, (void*)requests, &working_func);
	skeleton_initialization(nthread_input, num_cores, cores_per_soc, affinity, (void*)requests, &working_func);

	//printf ("Initializing vectors\n");
    for (i = 0; i < nthread_input; i++) {
		requests[i]->tid = i;
		requests[i]->num_threads = nthread_input;	
		requests[i]->stage = ARRAY_INIT;
		requests[i]->pos_w = i;
		requests[i]->vector_length = vector_length;
		requests[i]->array_a = x;	
	}

//	gettimeofday(&begin, NULL);
	skeleton_master_add_working_request_and_wait();
//	gettimeofday(&end, NULL);
//	duration = cal_elapsed_time(&begin, &end);
//	printf("Time: %f for initialization \n", duration);

	printf ("Benchmark is starting...\n");
	distance = 1;	
	for (j = 0; j < num_distances; j++)
	{
	    for (i = 0; i < nthread_input; i++) {
			requests[i]->stage = thread_op;
			requests[i]->pos_w = i*distance;
			requests[i]->pos_r = i*distance;

			if(thread_op == ARRAY_WR)
			{
				requests[i]->op = OP_R;
				if( i%2 == 0 )
					requests[i]->op = OP_W;
			}

			if(thread_op == ARRAY_TS)
				requests[i]->pos_r = ((i + nthread_input/2) % nthread_input)*distance;
		}
		gettimeofday(&begin, NULL);
		skeleton_master_add_working_request_and_wait();
		gettimeofday(&end, NULL);
		duration = cal_elapsed_time(&begin, &end);
		printf("Time: %f by %d threads with distance: %d\n", duration, nthread_input, distance);
		distance = distance << 1;
	}

/*	printf("-=<>=-: Verifying results!!!\n");
	check = check_results(x, vector_length, 1);
	if(check != 1)
	{
		printf("ERROR: Calculation is wrong!!!\n");
	}*/

	skeleton_destroy();

	free(x);
	for(i = 0; i < nthread_input; i++)
		free(requests[i]);
	free(requests);

}
