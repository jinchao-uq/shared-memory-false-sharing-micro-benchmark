#include "skeleton.h"

int g_num_threads = 0;
int g_num_cores = 0;
int g_num_sockets = 0;
int g_cores_per_soc = 0;
int g_threads_per_soc = 0;
int g_init_flag = 0;
pthread_t g_threads[MAX_THREADS]; 
worker_info_t worker_info[MAX_THREADS];

pthread_mutex_t barrier_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t request_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t worker_idle = PTHREAD_COND_INITIALIZER;
pthread_cond_t master_wait = PTHREAD_COND_INITIALIZER;
int	g_ready_threads = 0;

int
skeleton_get_core_id(int tid, int policy)
{
	int cid;
	int socket_id;
	if(tid >= MAX_THREADS || tid < 0) {
		return 0;
	}

	switch(policy) {
		case PLACEMENT_SPREAD:
			socket_id = tid / g_threads_per_soc;
			cid = socket_id * g_cores_per_soc + tid - socket_id * g_threads_per_soc;
			break;
		case PLACEMENT_COMPACT:
			cid = tid;
			break;
		case PLACEMENT_CROSS:
			socket_id = tid % g_num_sockets;
			cid = socket_id * g_cores_per_soc + tid / g_num_sockets;
			break;
		default:
			cid = 0;
	}
	printf("generating core id. tid: %d, cid: %d, socket id: %d\n", tid, cid, socket_id);

	return cid;
}

void*
skeleton_worker(void* arg) 
{
	int i, j, quit;
	worker_info_t* my_info = (worker_info_t*)arg;
	int tid = my_info->tid;
	int cid = my_info->cid;

	/* set thread affinity */
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cid, &cpuset);
 
	pthread_t pid = pthread_self();
	int set_result = pthread_setaffinity_np(pid, sizeof(cpu_set_t), &cpuset);
	if (set_result != 0) {
		printf("pthread_setaffinity_np return error[%d]\n", set_result);
	}

	//printf("INFO: Thread[%d] is running on CPU[%d]\n", tid, sched_getcpu());
	//fflush(stdout);

	quit = 0;
	while( !quit )
	{
		//barrier
		pthread_mutex_lock(&barrier_mutex);
		g_ready_threads++;	
		pthread_cond_signal(&master_wait);
		pthread_mutex_unlock(&barrier_mutex);

		//wait_for_request
		pthread_mutex_lock(&request_mutex);
		while( !my_info->new_request)
		{
			//printf("INFO: Thread[%d] is wating on master request with g_ready_threads=[%d]\n", tid, g_ready_threads);
			pthread_cond_wait(&worker_idle, &request_mutex);
		}
		pthread_mutex_unlock(&request_mutex);
		my_info->new_request=0;

		switch(my_info->request)
		{
			case REQUEST_WORK:
				break;
			case REQUEST_QUIT:
				quit = 1;
				break;
			default:
				quit = 1;
		}

		if(!quit)
		{
			//printf("INFO: Thread[%d] is invoking worker function for the %d(nth) time!\n", tid, g_ready_threads, ++my_info->invoked);
			(*my_info->func)(my_info->input);
		}
	}
}

int
skeleton_master_wait_workers()
{
	if(! g_init_flag)
		return 0;

	pthread_mutex_lock(&barrier_mutex);
	while( g_ready_threads != g_num_threads)
	{
		//printf("INFO: skeleton_master_wait_workers g_ready_threads = [%d]!\n", g_ready_threads);
		pthread_cond_wait(&master_wait, &barrier_mutex);
	}
	g_ready_threads = 0;
	pthread_mutex_unlock(&barrier_mutex);

	return 0;
}

int
skeleton_master_add_working_request()
{
	int i;
	if(! g_init_flag)
		return 0;

    for (i = 0; i < g_num_threads; i++) 
	{
		worker_info[i].request = REQUEST_WORK;
		worker_info[i].new_request = 1;
	}

	pthread_mutex_lock(&request_mutex);
	pthread_cond_broadcast(&worker_idle);
	pthread_mutex_unlock(&request_mutex);

	return 0;
}

int
skeleton_master_add_working_request_and_wait()
{
	skeleton_master_add_working_request();
	skeleton_master_wait_workers();
}

int
skeleton_initialization(int num_threads, int num_cores, int cores_per_soc, int policy, void* input[], void *func)
{
	int i;
	if(num_threads < 1 || num_threads > MAX_THREADS)
		return -1;

	g_num_threads = num_threads;
	g_num_cores = num_cores;
	g_cores_per_soc = cores_per_soc;
	g_num_sockets = g_num_cores / g_cores_per_soc;
	g_threads_per_soc = g_num_threads / g_num_sockets;
	if(g_threads_per_soc == 0)
		g_threads_per_soc = 1;

	//printf("INFO: skeleton_initialization num_threads: %d, placement: %d, cores_per_soc: %d, num_sockets: %d, threads_per_soc: %d!\n", num_threads, policy, g_cores_per_soc, g_num_sockets, g_threads_per_soc);

	g_ready_threads = 0;
    for (i = 0; i < g_num_threads; i++) {
		worker_info[i].tid = i;	
		worker_info[i].cid = skeleton_get_core_id(i, policy);
		worker_info[i].invoked = 0;
		worker_info[i].new_request = 0;
		if(input != NULL)
			worker_info[i].input = input[i];
		worker_info[i].func = func;	

        pthread_create(&g_threads[i], NULL, skeleton_worker, (void*)&(worker_info[i])); 
	}

	g_init_flag = 1;
	skeleton_master_wait_workers();
	return 0;
}

int
skeleton_destroy()
{
	int i;

	if(! g_init_flag)
		return 0;

    for (i = 0; i < g_num_threads; i++) 
	{
		worker_info[i].request = REQUEST_QUIT;
		worker_info[i].new_request = 1;
	}

	pthread_mutex_lock(&request_mutex);
	pthread_cond_broadcast(&worker_idle);
	pthread_mutex_unlock(&request_mutex);

	// waiting for all threads to complete 
	for (i = 0; i < g_num_threads; i++) 
   		pthread_join(g_threads[i], NULL); 

	printf("skeleton_destroy is invoked succesfully!\n");
	return 0;
}
