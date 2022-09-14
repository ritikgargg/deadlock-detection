#include "../all_functions.h"
#include <stdio.h>
#include <stdlib.h>

int main(){
    int matrix[4][5] = {{0,1,3,1,1}, 
                        {4,5,6,7,8}, 
                        {1,1,3,4,1},
                        {2,1,1,1,1}};
    allocation = (int **)malloc(4 * sizeof(int *));
    max_threads = 4;
    total_types_rcs = 5;
    for (int i = 0; i < 4; i++){
        allocation[i] = (int*)malloc(5 * sizeof(int));
        for(int j = 0; j < 5; j++)
            allocation[i][j] = matrix[i][j];
    }
    int thr_in_dlock[5] = {0,1,2,-1};
    if(linear_thrIdx(thr_in_dlock) == 0){
        printf("Test #5 passed\n");
    }else{
        printf("Test #5 failed\n");
    }
}
