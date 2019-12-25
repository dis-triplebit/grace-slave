/*
 * ThreadPool.cpp
 *
 *  Created on: 2019-09-21
 *      Author: favoniankong
 */

#include "ThreadPool.h"
#include "TripleBit.h"
#include <string>
#include <iostream>

using namespace std;

ThreadPool *ThreadPool::workPool = NULL;
ThreadPool *ThreadPool::chunkPool = NULL;
ThreadPool *ThreadPool::partitionPool = NULL;
ThreadPool *ThreadPool::scanjoinPool = NULL;



ThreadPool::ThreadPool(int threadNum) {
	this->threadNum = threadNum;
	shutDown = false;
	NodeTask *nodeTask = new NodeTask;
	head = tail = nodeTask;
	pthread_mutex_init(&headMutex,NULL);
	pthread_mutex_init(&tailMutex, NULL);
	pthread_mutex_init(&pthreadIdleMutex,NULL);
	pthread_mutex_init(&pthreadBusyMutex,NULL);
	pthread_cond_init(&pthreadCond,NULL);
	pthread_cond_init(&pthreadEmpty,NULL);
	pthread_cond_init(&pthreadBusyEmpty,NULL);
	create();
}

ThreadPool::~ThreadPool()
{
	stopAll();
	if(head != NULL){
		delete head;
		head = NULL;
	}
//	if(tail != NULL){
//		delete tail;
//		tail = NULL;
//	}
	pthread_mutex_destroy(&headMutex);
	pthread_mutex_destroy(&tailMutex);
	pthread_mutex_destroy(&pthreadIdleMutex);
	pthread_mutex_destroy(&pthreadBusyMutex);
	pthread_cond_destroy(&pthreadCond);
	pthread_cond_destroy(&pthreadEmpty);
	pthread_cond_destroy(&pthreadBusyEmpty);
}

int ThreadPool::moveToIdle(pthread_t tid) {
	pthread_mutex_lock(&pthreadBusyMutex);
	vector<pthread_t>::iterator busyIter = vecBusyThread.begin();
	while (busyIter != vecBusyThread.end()) {
		if (tid == *busyIter) {
			break;
		}
		busyIter++;
	}
	vecBusyThread.erase(busyIter);

	if ( vecBusyThread.size() == 0) {
		pthread_cond_broadcast(&pthreadBusyEmpty);
	}

	pthread_mutex_unlock(&pthreadBusyMutex);

	pthread_mutex_lock(&pthreadIdleMutex);
	vecIdleThread.push_back(tid);
	pthread_mutex_unlock(&pthreadIdleMutex);
	return 0;
}

int ThreadPool::moveToBusy(pthread_t tid) {
	pthread_mutex_lock(&pthreadIdleMutex);
	vector<pthread_t>::iterator idleIter = vecIdleThread.begin();
	while (idleIter != vecIdleThread.end()) {
		if (tid == *idleIter) {
			break;
		}
		idleIter++;
	}
	vecIdleThread.erase(idleIter);
	pthread_mutex_unlock(&pthreadIdleMutex);

	pthread_mutex_lock(&pthreadBusyMutex);
	vecBusyThread.push_back(tid);
	pthread_mutex_unlock(&pthreadBusyMutex);
	return 0;
}

void* ThreadPool::threadFunc(void * threadData) {
	pthread_t tid = pthread_self();
	int rnt;
	ThreadPoolArg* arg = (ThreadPoolArg*)threadData;
	ThreadPool* pool = arg->pool;
	while (1) {
		rnt = pthread_mutex_lock(&pool->headMutex);
		if ( rnt != 0){
			cout<<"Get mutex error"<<endl;
		}

		while(pool->isEmpty() && pool->shutDown == false){
			pthread_cond_wait(&pool->pthreadCond, &pool->headMutex);
		}

		if ( pool->shutDown == true){
			pthread_mutex_unlock(&pool->headMutex);
			pthread_exit(NULL);
		}

		pool->moveToBusy(tid);
		Task task = pool->Dequeue();

		if(pool->isEmpty()){
			pthread_cond_broadcast(&pool->pthreadEmpty);
		}
		pthread_mutex_unlock(&pool->headMutex);
		task();
		pool->moveToIdle(tid);
	}
	return (void*)0;
}

void ThreadPool::Enqueue(const Task &task){
	NodeTask *nodeTask = new NodeTask(task);
	pthread_mutex_lock(&tailMutex);
	tail->next = nodeTask;
	tail = nodeTask;
	pthread_mutex_unlock(&tailMutex);
}

ThreadPool::Task ThreadPool::Dequeue(){
	NodeTask *node, *newNode;
//	pthread_mutex_lock(&headMutex);
	node = head;
	newNode = head->next;
	Task task = newNode->value;
	head = newNode;
//	pthread_mutex_unlock(&headMutex);
	delete node;
	return task;
}

int ThreadPool::addTask(const Task &task) {
	Enqueue(task);
	pthread_cond_broadcast(&pthreadCond);
	return 0;
}

int ThreadPool::create() {
	struct ThreadPoolArg* arg = new ThreadPoolArg;
	pthread_mutex_lock(&pthreadIdleMutex);
	for (int i = 0; i < threadNum; i++) {
		pthread_t tid = 0;
		arg->pool = this;
		pthread_create(&tid, NULL, threadFunc, arg);
		vecIdleThread.push_back(tid);
	}
	pthread_mutex_unlock(&pthreadIdleMutex);
	return 0;
}

int ThreadPool::stopAll() {
	shutDown = true;
	pthread_mutex_unlock(&headMutex);
	pthread_cond_broadcast(&pthreadCond);
	vector<pthread_t>::iterator iter = vecIdleThread.begin();
	while (iter != vecIdleThread.end()) {
		pthread_join(*iter, NULL);
		iter++;
	}
	
	iter = vecBusyThread.begin();
	while (iter != vecBusyThread.end()) {
		pthread_join(*iter, NULL);
		iter++;
	}

	return 0;
}

int ThreadPool::wait()
{
	pthread_mutex_lock(&headMutex);
	while(!isEmpty()) {
		pthread_cond_wait(&pthreadEmpty, &headMutex);
	}
	pthread_mutex_unlock(&headMutex);
	pthread_mutex_lock(&pthreadBusyMutex);
	while(vecBusyThread.size() != 0) {
		pthread_cond_wait(&pthreadBusyEmpty, &pthreadBusyMutex);
	}
	pthread_mutex_unlock(&pthreadBusyMutex);
	return 0;
}

ThreadPool &ThreadPool::getWorkPool(){
	if(workPool == NULL){
		workPool = new ThreadPool(WORK_THREAD_NUMBER);
	}
	return *workPool;
}

ThreadPool &ThreadPool::getChunkPool(){
	if(chunkPool == NULL){
		chunkPool = new ThreadPool(CHUNK_THREAD_NUMBER);
	}
	return *chunkPool;
}

ThreadPool &ThreadPool::getPartitionPool(){
	if(partitionPool == NULL){
		partitionPool = new ThreadPool(CHUNK_THREAD_NUMBER);
	}
	return *partitionPool;
}

ThreadPool &ThreadPool::getScanjoinPool(){
	if(scanjoinPool == NULL){
		scanjoinPool = new ThreadPool(SCAN_JOIN_THREAD_NUMBER);
	}
	return *scanjoinPool;
}


void ThreadPool::createAllPool(){
	getWorkPool();
	getChunkPool();
	getPartitionPool();
}

void ThreadPool::deleteAllPool(){
	if(workPool != NULL){
		delete workPool;
		workPool = NULL;
	}
	if(chunkPool != NULL){
		delete chunkPool;
		chunkPool = NULL;
	}
	if(partitionPool != NULL){
		delete partitionPool;
		partitionPool = NULL;
	}
	if(scanjoinPool != NULL){
		delete scanjoinPool;
		scanjoinPool = NULL;
	}
}

void ThreadPool::waitAllPoolComplete(){
	getWorkPool().wait();
	getChunkPool().wait();
	getPartitionPool().wait();
	getScanjoinPool().wait();
}
