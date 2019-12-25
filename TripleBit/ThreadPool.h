/*
 * ThreadPool.h
 *
 *  Created on: 2019-09-21
 *      Author: favoniankong
 */

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <vector>
#include <pthread.h>
#include <iostream>
#include <boost/function.hpp>

using namespace std;

class ThreadPool{
private:
	ThreadPool(int threadNum);

public:
	typedef boost::function<void()> Task;

	class NodeTask{
	public:
		Task value;
		NodeTask *next;
		NodeTask():value(0), next(NULL){}
		NodeTask(const Task &val):value(val), next(NULL){}
	};

public:
	NodeTask *head, *tail;

private:
	int threadNum;
	bool shutDown;
	vector<pthread_t> vecIdleThread, vecBusyThread;
	pthread_mutex_t headMutex, tailMutex;
	pthread_mutex_t pthreadIdleMutex, pthreadBusyMutex;
	pthread_cond_t pthreadCond, pthreadEmpty, pthreadBusyEmpty;

	static ThreadPool *workPool, *chunkPool, *partitionPool, *scanjoinPool;

private:
	static void *threadFunc(void *threadData);
	int moveToIdle(pthread_t tid);
	int moveToBusy(pthread_t);
	int create();
	void Enqueue(const Task &task);
	Task Dequeue();
	bool isEmpty(){ return head->next == NULL; }

public:
	static ThreadPool &getWorkPool();
	static ThreadPool &getChunkPool();
	static ThreadPool &getPartitionPool();
	static ThreadPool &getScanjoinPool();
	static void createAllPool();
	static void deleteAllPool();
	static void waitAllPoolComplete();
	~ThreadPool();
	int addTask(const Task &task);
	int stopAll();
	int wait();
};

struct ThreadPoolArg
{
	ThreadPool* pool;
};
#endif /* THREADPOOL_H_ */
