#include "cuEntityIDBuffer.h"
#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#define nTPB 256
#include <stdio.h>
//#pragma once


__global__ void caculateKernel(cuEntityIDBuffer** d_cuEntityIDList, int listSize, unsigned int* res)
{
	int idx=blockDim.x * blockIdx.x + threadIdx.x;
	for(unsigned i = 0; i < listSize; ++i) {
		/* code */
		//(d_cuEntityIDList[3])->cuBuffer[idx]+=(d_cuEntityIDList[i])->cuBuffer[idx];
		//unsigned int* temptr = (d_cuEntityIDList[3])->getcuEntityIDBuffer();
		unsigned int* temptr_i = (d_cuEntityIDList[i])->getcuEntityIDBuffer();
		//temptr[idx] += temptr_i[idx];
		res[idx]+=temptr_i[idx];
	}
}

int main(int argc, char const *argv[])
{
	unsigned int* h_buf, *h_buf1, *h_buf2;
	unsigned int* h_res;

	h_buf=(unsigned int *)(malloc(1024*sizeof(unsigned int)));
	h_buf1=(unsigned int *)(malloc(1024*sizeof(unsigned int)));
	h_buf2=(unsigned int *)(malloc(1024*sizeof(unsigned int)));
	h_res=(unsigned int *)(malloc(1024*sizeof(unsigned int)));
	for(unsigned int i = 0; i < 1024; ++i) {
		/* code */
		h_buf[i]=i;
		h_buf1[i]=i;
		h_buf2[i]=i;
		h_res[i]=0;
	}
	for(unsigned int i = 0; i < 1024; ++i) {
		/* code */
		printf("%u ",h_buf[i]);
	}

	cuEntityIDBuffer d_buf(h_buf,1024);
	cuEntityIDBuffer d_buf1(h_buf1,1024);
	cuEntityIDBuffer d_buf2(h_buf2,1024);
	//cuEntityIDBuffer d_res(h_res,1024);

	unsigned int* d_res;
	cudaMalloc((void **)&d_res, 1024 * sizeof(unsigned int));
	cudaMemcpy(d_res,h_res,1024*sizeof(unsigned int),cudaMemcpyHostToDevice);


	thrust::host_vector<cuEntityIDBuffer*> h_BufferList(4);
	h_BufferList.push_back(&d_buf);
	h_BufferList.push_back(&d_buf1);
	h_BufferList.push_back(&d_buf2);
	//h_BufferList.push_back(&d_res);
	thrust::device_vector<cuEntityIDBuffer*> cuBufferList = h_BufferList;
	
	//CuBufferList.push_back(thrust::raw_pointer_cast(d_buf));
	//cuEntityIDBuffer** d_cuEntityIDList=thrust::raw_pointer_cast<cuEntityIDBuffer**>(cuBufferList.data()); //ask in stackover
	cuEntityIDBuffer** d_cuEntityIDList=thrust::raw_pointer_cast(cuBufferList.data());

	caculateKernel<<<((1024+nTPB-1)/nTPB),nTPB>>>(d_cuEntityIDList,cuBufferList.size(),d_res);
	cudaDeviceSynchronize();
	//d_res.cuTest();
	//d_res.cuCallBackEntityIDBuffer(h_res);
	//d_res.cuCallBackEntityIDBuffer(h_res);

	cudaMemcpy(h_res,d_res,1024*sizeof(unsigned int),cudaMemcpyDeviceToHost);
	for(unsigned int i = 0; i < 1024; ++i) {
		/* code */
		printf("%u ",h_res[i]);
	}
	return 0;
}