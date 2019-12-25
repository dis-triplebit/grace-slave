#include <thrust/reduce.h>
#include <thrust/execution_policy.h>
#include <stdio.h>
#include <assert.h>
#include <cuda_runtime.h>

#ifdef __CUDACC__
#define CUDA_CALLABLE_MEMBER __host__ __device__
#else
#define CUDA_CALLABLE_MEMBER
#endif

class cuEntityIDBuffer
{
public:
	cuEntityIDBuffer();
	cuEntityIDBuffer(unsigned int *buffer, int size);
	cuEntityIDBuffer(cuEntityIDBuffer* buffer);
	void cuCallBackEntityIDBuffer(unsigned int *buffer);
	~cuEntityIDBuffer();
	CUDA_CALLABLE_MEMBER unsigned int* getcuEntityIDBuffer();
	void cuTest();
private:
	int IDCount;
	//int sortKey;
	//size_t usedSize;
	size_t buffersize;
	unsigned int* cuBuffer;
};
