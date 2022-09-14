#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>

int total_types_rcs;    /* Number of types of resources */
char** resources_name = NULL;   /* List of names of the resources */

int* max_available_rcs = NULL;  /* Stores the maximum number of instances of each resource type */
int* available_rcs = NULL;  /* Stores the available number of instances of each resource type */

int** allocation= NULL; /* Matrix of resources allocated to the threads. */
int** request = NULL;   /* Remaining resources to be further acquired before the thread completes. */
int** cur_request = NULL;   /* Current request to acquire a certain number of instances of the remaining resources required. */

int max_threads;    /* Maximum number of threads. */
int d_check_interval;   /* Deadlock detection interval. */
int heuristic_no;   /* Index of heuristic selected. */
int exec_time;  /* Total execution time after which simulation should terminate. */
unsigned int seed;

int** para = NULL;  /* Parameter passed to each thread */ /* i-th parameter passed to i-th thread. */
unsigned int *thr_seeds = NULL;
pthread_t *worker_thr_ids = NULL;   /* List of thread ids of the worker thrrads. */
pthread_t detector_thr_id;  /* Thread ID of the detector thread. */

pthread_mutex_t mutex;  /* Mutex lock */
pthread_cond_t cond;    /* Conditional Variable */

struct timeval start_time, end_time; /* Variables to store the start time and the time of occurrence of the last deadlock. */
int total_dlocks = 0;   /* Total number of deadlocks */
double total_time_btw_dlocks = 0;

/* Generate a random double from 0 to 1 */
double random_double(unsigned int *seed){
    return ((double)rand_r(seed))/((double)RAND_MAX);
}

/* Generate a random doublefrom a to b */
double random_double_interval(double a, double b, unsigned int *seed){
    return random_double(seed) * (b - a) + a;
}

/**
 * Find maximum between two numbers.
 */
int max(int num1, int num2){
    return (num1 > num2 ) ? num1 : num2;
}

/**
 * Find minimum between two numbers.
 */
int min(int num1, int num2){
    return (num1 > num2 ) ? num2 : num1;
}


void log_msg(const char *msg, bool terminate) {
    printf("%s\n", msg);
    if (terminate) exit(-1); /* failure */
}

/**
 * Function to handle signals SIGALRM and SIGINT.
 * @param signum To differentiate between the type of signal(SIGALRM or SIGINT)
 */
void sig_handler(int signum){
    pthread_mutex_lock(&mutex); /* Acquiring the mutex lock */

    /* Freeing heap memory before program termination */
    free(worker_thr_ids);
    free(max_available_rcs);
    free(available_rcs);
    free(thr_seeds);
    for(int i = 0; i < max_threads; i++){
        free(allocation[i]);
        free(request[i]);
        free(cur_request[i]);
        free(para[i]);
    }
    free(allocation);
    free(request);
    free(cur_request);
    free(para);
    for(int i = 0; i < total_types_rcs; i++){
        free(resources_name[i]);
    }
    free(resources_name);

    /* Calculating the average time between successive deadlocks */

    double avg_dlock_time = total_time_btw_dlocks/total_dlocks;

    if(signum == SIGALRM)
        log_msg("\nLOG: Total allowed execution time has been reached. Program terminating...", false);
    if(signum == SIGINT)
        log_msg("\nLOG: Execution interrupted by the user. Program terminating...", false);

    printf("LOG: Total number of deadlocks = %d\n", total_dlocks);
    printf("LOG: Average time between deadlocks = %lf sec\n", avg_dlock_time);
    log_msg("LOG: Program Terminated.", true);   /* Terminating the program by passing 'true' to the log_msg() function */

    pthread_mutex_unlock(&mutex);   /* Releasing the mutex lock */
}

/**
 * Function to simulate a worker thread.
 * @param idx Contains the thread index of the corresponding thread
 */
void* thread_simulator(void *idx) {
    int my_idx = *((int*) idx);
    while(true){
        bool thr_rcs_acq[total_types_rcs];  /* Boolean array which represents whether all instances of a resource type are
        completely acquired by the thread or not */

        for(int i = 0; i < total_types_rcs; i++){
            thr_rcs_acq[i] = false;
        }

        pthread_mutex_lock(&mutex); /* Acquiring the mutex lock */
        int rcs_acquired = 0;

        /* Generating the request set R_t for the thread */
        for(int i = 0; i < total_types_rcs; i++){
            request[my_idx][i] = (rand_r(&thr_seeds[my_idx]) % (max_available_rcs[i] + 1));
            if(request[my_idx][i] == 0){
                rcs_acquired += 1;
                thr_rcs_acq[i] = true;
            }
        }
        pthread_mutex_unlock(&mutex);   /* Releasing the mutex lock */

        /* The loop continues till the required instances of all the resource types are not acquired */
        while(rcs_acquired < total_types_rcs){

            /* Selecting a random resource type whose required number of instances are yet to be allocated to the thread */
            int ri = (rand_r(&thr_seeds[my_idx]) % (total_types_rcs));
            while (thr_rcs_acq[ri]){
                ri = (rand_r(&thr_seeds[my_idx]) % (total_types_rcs));
            }

            pthread_mutex_lock(&mutex); /* Acquiring the mutex lock */
            /* Generating a random number of instances of the required number of instances of ri-th resource  */
            cur_request[my_idx][ri] = (rand_r(&thr_seeds[my_idx]) % (request[my_idx][ri] + 1));

            /* Checking if the curretly requested number of instances of the ri-th resource are more than the available number*/
            while (cur_request[my_idx][ri] >  available_rcs[ri]){
                /* If true, then we wait on the conditional variable cond */
                pthread_cond_wait(&cond, &mutex);
            }
            
            /* If false, then the current request is allocated to the thread */
            if(cur_request[my_idx][ri] != 0)
                printf("Allocate %d number of instances of resource type %d to thread %d\n", cur_request[my_idx][ri], ri, my_idx);
            available_rcs[ri] -= cur_request[my_idx][ri];
            allocation[my_idx][ri] += cur_request[my_idx][ri];
            request[my_idx][ri] -= cur_request[my_idx][ri];
            cur_request[my_idx][ri] = 0;
            if(request[my_idx][ri] == 0){
                /* If the required number of instances of a resource type are completely acquired, we perform the following */
                rcs_acquired += 1;
                thr_rcs_acq[ri] = true;
            }
            pthread_mutex_unlock(&mutex); /* Releasing the mutex lock */

            /* Generating a random pause between two successive resource requests by the thread */
            double rwait = random_double(&thr_seeds[my_idx]);
            int rwait_nsec = rwait * 1000000000;
            struct timespec wait_time;
            wait_time.tv_sec = 0;
            wait_time.tv_nsec = rwait_nsec;
            nanosleep(&wait_time, NULL); 
        }
        
        /* Holding onto the resources for a random duration between [0.7d, 1.5d] */
        double rhalt = random_double_interval(0.7 * d_check_interval, 1.5 * d_check_interval, &thr_seeds[my_idx]);
        int rhalt_sec = (int)(rhalt);
        int rhalt_nsec = (rhalt - rhalt_sec) * 1000000000;
        struct timespec halt_time;
        halt_time.tv_sec = rhalt_sec;
        halt_time.tv_nsec = rhalt_nsec;
        nanosleep(&halt_time, NULL); 
        pthread_mutex_lock(&mutex); /* Acquiring the mutex lock */

        /* Releasing all the acquired resources before thread termination */
        for(int i = 0; i < total_types_rcs; i++){
            available_rcs[i] += allocation[my_idx][i];
            if(allocation[my_idx][i] != 0)
                printf("Release %d number of instances of resource type %d from thread %d\n", allocation[my_idx][i], i, my_idx);
            allocation[my_idx][i] = 0;
        }
        printf("Restarting thread %d\n", my_idx);
        pthread_cond_broadcast(&cond);  /* Broadcasting a signal to all the threads waiting on the cond variable */
        pthread_mutex_unlock(&mutex);   /* Releasing the mutex lock */ 
    }
    pthread_exit(NULL);
    return NULL;
}

/**
 * Function to find whether the system contains a deadlock
 * @param thr_in_dlock Array to store the indexes of the threads involved in the deadlock
 * @return Return true if deadlock is detected, otherwise false.
 */
bool check_dlock(int thr_in_dlock[]){
    int work[total_types_rcs];
    bool finish[max_threads];
    for(int i = 0; i < total_types_rcs; i++){
        work[i] = available_rcs[i];
    }
    bool chk;
    int count = 0;
    for(int i = 0; i < max_threads; i++){
        chk = true;
        for(int j = 0; j < total_types_rcs; j++){
            if(allocation[i][j] != 0){
                chk = false;
                break;
            }
        }
        if(chk) count += 1;
        finish[i] = chk;
    }

    bool flag1, flag2;
    while (count < max_threads){
        flag1 = false;
        for (int i = 0; i < max_threads; i++){
            if (finish[i] == false){
                flag2 = true;
                for (int j = 0; j < total_types_rcs; j++){        
                    if (request[i][j] > work[j]){
                        flag2 = false;
                        break;
                    }
                }
                if (flag2){
                    finish[i] = true; 
                    count += 1;
                    flag1 = true;                         
                    for (int j = 0; j < total_types_rcs; j++){
                        work[j] = work[j] + allocation[i][j];
                    }
                }
            }
        }
        if (!flag1){
            break;
        }
    }
    int k = 0;
    if (count < max_threads){
        /* Storing the indexes of the threads involved in deadlock, in thr_in_dlock[] */
        for(int i = 0; i < max_threads; i++){
            if(!finish[i])
                thr_in_dlock[k++] = i;
        }
        return true;
    }else{
        return false;
    }
}

/**
 * Function to find the index of the thread involved in a deadlock, such that it has maximum total instances of all resource 
 * types allocated to it.
 * @param thr_in_dlock Array containing the indexes of the threads involved in the deadlock
 * @return Return the index of the thread in thr_in_dlock[] with maximum total instances of all resources allocated to it.
 */
int max_total_rcs_thrIdx(int thr_in_dlock[]){
    int total_rcs_per_thr[max_threads];
    for(int i = 0; i < max_threads; i++){
        total_rcs_per_thr[i] = 0;
    }
    for(int i = 0; i < max_threads; i++){
        for(int j = 0; j < total_types_rcs; j++){
            total_rcs_per_thr[i] += allocation[i][j];
        }
    }
    int max_total_rcs = 0, max_thrIdx;
    for(int i = 0; i < max_threads; i++){
        if(thr_in_dlock[i] == -1){
            break;
        }
        if(total_rcs_per_thr[thr_in_dlock[i]] > max_total_rcs){
            max_total_rcs = total_rcs_per_thr[thr_in_dlock[i]];
            max_thrIdx = thr_in_dlock[i];
        }
    }
    return max_thrIdx;
}

/**
 * Function to find the index of the thread involved in a deadlock, such that it has maximum instances of any resource 
 * type allocated to it.
 * @param thr_in_dlock Array containing the indexes of the threads involved in the deadlock
 * @return Return the index of the thread in thr_in_dlock[] with maximum instances of any resource type allocated to it.
 */
int max_any_rcs_thrIdx(int thr_in_dlock[]){
    int max_rcs_per_thr[max_threads];
    for(int i = 0; i < max_threads; i++){
        max_rcs_per_thr[i] = 0;
    }
    for(int i = 0; i < max_threads; i++){
        for(int j = 0; j < total_types_rcs; j++){
            max_rcs_per_thr[i] = max(max_rcs_per_thr[i], allocation[i][j]);
        }
    }
    int max_any_rcs = 0, max_thrIdx;
    for(int i = 0; i < max_threads; i++){
        if(thr_in_dlock[i] == -1){
            break;
        }
        if(max_rcs_per_thr[thr_in_dlock[i]] > max_any_rcs){
            max_any_rcs = max_rcs_per_thr[thr_in_dlock[i]];
            max_thrIdx = thr_in_dlock[i];
        }
    }
    return max_thrIdx;

}

/**
 * Function to find the index of the thread involved in a deadlock, such that it has minimum total instances of all resource 
 * types allocated to it.
 * @param thr_in_dlock Array containing the indexes of the threads involved in the deadlock
 * @return Return the index of the thread in thr_in_dlock[] with minimum total instances of all resources allocated to it.
 */
int min_total_rcs_thrIdx(int thr_in_dlock[]){
    int total_rcs_per_thr[max_threads];
    for(int i = 0; i < max_threads; i++){
        total_rcs_per_thr[i] = 0;
    }
    for(int i = 0; i < max_threads; i++){
        for(int j = 0; j < total_types_rcs; j++){
            total_rcs_per_thr[i] += allocation[i][j];
        }
    }
    int min_total_rcs = INT_MAX, min_thrIdx;
    for(int i = 0; i < max_threads; i++){
        if(thr_in_dlock[i] == -1){
            break;
        }
        if(total_rcs_per_thr[thr_in_dlock[i]] < min_total_rcs){
            min_total_rcs = total_rcs_per_thr[thr_in_dlock[i]];
            min_thrIdx = thr_in_dlock[i];
        }
    }
    return min_thrIdx;
}

/**
 * Function to find the index of the thread involved in a deadlock, such that it has minimum instances of any resource 
 * type allocated to it.
 * @param thr_in_dlock Array containing the indexes of the threads involved in the deadlock
 * @return Return the index of the thread in thr_in_dlock[] with minimum instances of any resource type allocated to it.
 */

int min_any_rcs_thrIdx(int thr_in_dlock[]){
    int min_rcs_per_thr[max_threads];
    for(int i = 0; i < max_threads; i++){
        min_rcs_per_thr[i] = INT_MAX;
    }
    for(int i = 0; i < max_threads; i++){
        for(int j = 0; j < total_types_rcs; j++){
            min_rcs_per_thr[i] = min(min_rcs_per_thr[i], allocation[i][j]);
        }
    }
    int min_any_rcs = INT_MAX, min_thrIdx;
    for(int i = 0; i < max_threads; i++){
        if(thr_in_dlock[i] == -1){
            break;
        }
        if(min_rcs_per_thr[thr_in_dlock[i]] < min_any_rcs){
            min_any_rcs = min_rcs_per_thr[thr_in_dlock[i]];
            min_thrIdx = thr_in_dlock[i];
        }
    }
    return min_thrIdx;
}

int linear_thrIdx(int thr_in_dlock[]){
    return thr_in_dlock[0];
}
/**
 * Function to return the index of the thread in thr_in_dlock[] to be terminated, based on the heuristic being followed. 
 * @param thr_in_dlock Array containing the indexes of the threads involved in the deadlock
 * @return Return the index of the thread in thr_in_dlock[], which should be terminated in an attempt to resolve the deadlock.
 */
int select_thr_to_cncl(int thr_in_dlock[]){
    int thrIdx;
    switch (heuristic_no){
        case 1: thrIdx = max_total_rcs_thrIdx(thr_in_dlock);
                break;
        case 2: thrIdx = max_any_rcs_thrIdx(thr_in_dlock);
                break;
        case 3: thrIdx = min_total_rcs_thrIdx(thr_in_dlock);
                break;
        case 4: thrIdx = min_any_rcs_thrIdx(thr_in_dlock);
                break;
        case 5: thrIdx = linear_thrIdx(thr_in_dlock);
                break;
    }
   return thrIdx;
}

/**
 * This function accepts the index of the thread to be terminated, thereby making the correspoding request, allocation and cur_request row
 * to be zero. 
 * @param thr_to_cncl Index of the thread to be terminated.
 */
void resolve_dlock(int thrIdx_to_cncl){
    printf("LOG: Terminating thread %d and retrying...\n", thrIdx_to_cncl);

    for(int i = 0; i < total_types_rcs; i++){
        available_rcs[i] += allocation[thrIdx_to_cncl][i];
        allocation[thrIdx_to_cncl][i] = 0;
        request[thrIdx_to_cncl][i] = 0;
        cur_request[thrIdx_to_cncl][i] = 0;
    }
}

/**
 * Function to run the deadlock detection thread.
 * @param dummy This argument is just to ensure the compatability of the defined function with the expected signature.
 */

void* dlock_detection_thr(void * dummy){
    while(true){
        sleep(d_check_interval);    /* To sleep the thread for the input deadlock detection interval */
        pthread_mutex_lock(&mutex); /* Acquiring the mutex lock */
        gettimeofday(&end_time, NULL);
        printf("\nLOG: Deadlock detection started...\n");
        int thr_in_dlock[max_threads];
        int thrIdx_to_cncl;
        for(int i = 0; i < max_threads; i++){
            thr_in_dlock[i] = -1;
        }
        bool is_dlock = check_dlock(thr_in_dlock);  /* Check the presence of deadlock */
        if (is_dlock){
            total_dlocks += 1;
            double time_taken;
            time_taken = (end_time.tv_sec - start_time.tv_sec) * 1e6;
            time_taken = (time_taken + (end_time.tv_usec - start_time.tv_usec)) * 1e-6;
            total_time_btw_dlocks += time_taken;
            gettimeofday(&start_time, NULL);
            while(is_dlock){
                printf("LOG: Deadlock Detected. Threads in deadlock are: ");
                for(int i = 0; i < max_threads; i++){
                    if(thr_in_dlock[i] == -1)
                        break;
                    printf("%d ", thr_in_dlock[i]);
                }
                printf("\n");
                thrIdx_to_cncl = select_thr_to_cncl(thr_in_dlock);
                resolve_dlock(thrIdx_to_cncl);  /* Trying to resolve deadlock*/
                for(int i = 0; i < max_threads; i++){
                    thr_in_dlock[i] = -1;
                }
                is_dlock = check_dlock(thr_in_dlock);
            }
            printf("LOG: Deadlock Resolved.\n");
        }else{
            printf("LOG: No Deadlock.\n");
        }
        printf("\n");
        pthread_cond_broadcast(&cond);  /* Broadcasting signal to threads waiting on the condition variable cond*/
        pthread_mutex_unlock(&mutex);   /* Releasing the mutex lock */
    }
    pthread_exit(NULL);
    return NULL;
}

/**
 * This function creates the worker threads and the deadlock detection thread.
 */
void createAllThreads(){
    gettimeofday(&start_time, NULL);
    int rc;
    worker_thr_ids = (pthread_t *)malloc(sizeof(pthread_t) * max_threads);
    para = (int **)malloc(sizeof(int *) * max_threads);
    for (int i = 0; i < max_threads; i++){
        para[i] = (int*)malloc(sizeof(int));
    }
    for(int i = 0; i < max_threads; i++){
        *(para[i]) = i;
        rc = pthread_create(worker_thr_ids + i, NULL, thread_simulator, (void *) para[i]);
        if (rc) {
            log_msg("Failed to create the worker thread.", true);
        }
    }
    rc = pthread_create(&detector_thr_id, NULL, dlock_detection_thr, NULL);

    if (rc) {
        log_msg("Failed to create the deadlock detector thread.", true);
    }
    pthread_join(detector_thr_id, NULL);
}
