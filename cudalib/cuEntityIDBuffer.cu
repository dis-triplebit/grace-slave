#include "cuEntityIDBuffer.h"
#include <stdio.h>
#pragma hd_warning_disable
#define nTPB 256

#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort=true)
{
   if (code != cudaSuccess) 
   {
      fprintf(stderr,"GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
      if (abort) exit(code);
   }
}

__global__ void mykernel(unsigned int* buffer)
{
	int idx = threadIdx.x + (blockDim.x * blockIdx.x);
	buffer[idx]++;
}

cuEntityIDBuffer::cuEntityIDBuffer()
{
	buffersize=1024;
	gpuErrchk(cudaMalloc((void **)&cuBuffer, buffersize * sizeof(unsigned int)));
}

cuEntityIDBuffer::cuEntityIDBuffer(unsigned int *buffer, int size)
{
		buffersize=size;
		gpuErrchk(cudaMalloc((void **)&cuBuffer, buffersize * sizeof(unsigned int)));
		gpuErrchk(cudaMemcpy(cuBuffer,buffer,buffersize*sizeof(unsigned int),cudaMemcpyHostToDevice));
}

cuEntityIDBuffer::cuEntityIDBuffer(cuEntityIDBuffer* cubuffer)
{
	buffersize=cubuffer->buffersize;
	gpuErrchk(cudaMalloc((void **)&cuBuffer, buffersize * sizeof(unsigned int)));
	gpuErrchk(cudaMemcpy(cuBuffer,cubuffer->cuBuffer,buffersize*sizeof(unsigned int),cudaMemcpyDeviceToDevice));
}

void cuEntityIDBuffer::cuCallBackEntityIDBuffer(unsigned int* buffer)
{
	//gpuErrchk(cudaMemcpy(buffer,cuBuffer,buffersize*sizeof(unsigned int),cudaMemcpyDeviceToHost));
}

cuEntityIDBuffer::~cuEntityIDBuffer()
{
	gpuErrchk(cudaFree(cuBuffer));
}

unsigned int* cuEntityIDBuffer::getcuEntityIDBuffer()
{
	return cuBuffer;
}

void cuEntityIDBuffer::cuTest()
{
	mykernel<<<((buffersize+nTPB-1)/nTPB),nTPB>>>(cuBuffer);
	gpuErrchk(cudaPeekAtLastError());
}