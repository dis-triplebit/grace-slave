#include <stdio.h>
#include "test1.h"

#define DSIZE 1024
#define DVAL 10
#define nTPB 256

#define cudaCheckErrors(msg) \
    do { \
        cudaError_t __err = cudaGetLastError(); \
        if (__err != cudaSuccess) { \
            fprintf(stderr, "Fatal error: %s (%s at %s:%d)\n", \
                msg, cudaGetErrorString(__err), \
                __FILE__, __LINE__); \
            fprintf(stderr, "*** FAILED - ABORTING\n"); \
            exit(1); \
        } \
    } while (0)

__global__ void my_kernel1(int *data){
  int idx = threadIdx.x + (blockDim.x *blockIdx.x);
  if (idx < DSIZE) data[idx] =+ DVAL;
}

int my_test_func1(){

  int *d_data, *h_data;
  h_data = (int *) malloc(DSIZE * sizeof(int));
  if (h_data == 0) {printf("malloc fail\n"); exit(1);}
  cudaMalloc((void **)&d_data, DSIZE * sizeof(int));
  cudaCheckErrors("cudaMalloc fail");
  for (int i = 0; i < DSIZE; i++) h_data[i] = 0;
  cudaMemcpy(d_data, h_data, DSIZE * sizeof(int), cudaMemcpyHostToDevice);
  cudaCheckErrors("cudaMemcpy fail");
  my_kernel1<<<((DSIZE+nTPB-1)/nTPB), nTPB>>>(d_data);
  cudaDeviceSynchronize();
  cudaCheckErrors("kernel");
  cudaMemcpy(h_data, d_data, DSIZE * sizeof(int), cudaMemcpyDeviceToHost);
  cudaCheckErrors("cudaMemcpy 2");
  for (int i = 0; i < DSIZE; i++)
    if (h_data[i] != DVAL) {printf("Results check failed at offset %d, data was: %d, should be %d\n", i, h_data[i], DVAL); exit(1);}
  printf("Results check 1 passed!\n");
  return 0;
}
