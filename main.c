#include "all_functions.h" 

int main(int argc, char *argv[]) {
    if (argc <= 6) {
        printf("Usage: %s  max_num_threads  deadlock_detection_interval  heuristic_selected  total_simulation_time seed total_types_resources  resource_1_name  resource_1_max_instances  resource_2_name  resource_2_max_instances ....\n",argv[0]);
        printf("where, \n");
        printf("max_num_threads = The maximum number of threads to be used in the simulation\n");
        printf("deadlock_detection_interval = The time interval in seconds between two successive deadlock detection checks\n");
        printf("heuristic_selected = A number from 1 to 6 denoting one of the following heuristics to be adopted for resolving deadlocks\n");
        printf("\t1. Terminate thread with maximum number of total resources allocated.\n");
        printf("\t2. Terminate thread with maximum instances of any resource allocated.\n");
        printf("\t3. Terminate thread with minimum number of total resources allocated.\n");
        printf("\t4. Terminate thread with minimum instances of any resource allocated.\n");
        printf("\t5. Terminate thread in a linear order of deadlocked threads.\n");
        printf("total_simulation_time = The time(in seconds) after which the simulation should end\n");
        printf("seed = The value of seed to be supplied to the random generator function\n");
        printf("total_types_resources = The total number of types of resources\n");
        printf("These arguments are followed by the resource names and their corresponding maximum available instances. Count of the resource types is determined by total_types_resources.\n");
        exit(-1);
    }
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    max_threads = atoi(argv[1]);
    d_check_interval = atoi(argv[2]);
    heuristic_no = atoi(argv[3]);
    exec_time = atoi(argv[4]);
    seed = atoi(argv[5]);
    total_types_rcs = atoi(argv[6]);

    resources_name = (char**)malloc(total_types_rcs * sizeof(char*));
    max_available_rcs = (int *) malloc(sizeof(int) * total_types_rcs);
    available_rcs = (int *) malloc(sizeof(int) * total_types_rcs);
    int len;
    for(int i = 0; i < total_types_rcs; i++){
        len = strlen(argv[7 + 2 * i]) + 1;
        resources_name[i] = (char*)malloc(len * sizeof(char));
        strcpy(resources_name[i], argv[7 + 2 * i]);
        max_available_rcs[i] = atoi(argv[8 + 2 * i]);
        available_rcs[i] = max_available_rcs[i];
    }


    printf("==========================Simulation==========================\n");
    printf("Parameters for simulation :: \n");
    printf("Number of types of resources = %d\n", total_types_rcs);
    for(int i = 0; i < total_types_rcs; i++){
        printf("\t%s = %d\n", resources_name[i], max_available_rcs[i]);
    }
    printf("Number of threads = %d\n", max_threads);
    printf("Deadlock detection interval = %d sec\n", d_check_interval);
    printf("Heuristic number = %d\n", heuristic_no);
    printf("Execution Time = %d sec\n", exec_time);
    printf("\n");


    allocation = (int **)malloc(max_threads * sizeof(int *));
    request = (int **)malloc(max_threads * sizeof(int *));
    cur_request = (int **)malloc(max_threads * sizeof(int *));
    thr_seeds = (int *)malloc(max_threads * sizeof(int));

    srand(seed);
    for (int i = 0; i < max_threads; i++){
        allocation[i] = (int*)malloc(total_types_rcs * sizeof(int));
        request[i] = (int*)malloc(total_types_rcs * sizeof(int));
        cur_request[i] = (int*)malloc(total_types_rcs * sizeof(int));
        thr_seeds[i] = rand();
    }

    for(int i = 0; i < max_threads; i++){
        for(int j = 0; j < total_types_rcs; j++){
            allocation[i][j] = 0;
            request[i][j] = 0;
            cur_request[i][j] = 0;
        }
    }
    
    signal(SIGALRM, sig_handler); // Register signal handler for SIGALRM
    signal(SIGINT,sig_handler); // Register signal handler for SIGINT
    alarm(exec_time);

    createAllThreads();
    pthread_mutex_destroy(&mutex);  // Destroying the mutex
    pthread_cond_destroy(&cond);    // Destroying the conditional variable
    return 0;
}