/*
 * TasksQueueChunk.h
 *
 *  Created on: 2013-8-20
 *      Author: root
 */

#ifndef TASKSQUEUECHUNK_H_
#define TASKSQUEUECHUNK_H_

#include "../TripleBit.h"
#include "../TripleBitQueryGraph.h"
#include "subTaskPackage.h"
#include "IndexForTT.h"
#include "Tools.h"
#include "Tasks.h"

void PrintChunkTask(ChunkTask* chunkTask) {
	cout << "opType:" << chunkTask->operationType << " subject:"
			<< chunkTask->Triple.subject << " object:"
			<< chunkTask->Triple.object << " operation:"
			<< chunkTask->Triple.operation << endl;
}

class NodeChunkQueue: private Uncopyable {
public:
	ChunkTask* chunkTask;
	NodeChunkQueue* next;
	NodeChunkQueue() :
			chunkTask(NULL), next(NULL) {
	}
	~NodeChunkQueue() {
	}
	NodeChunkQueue(ChunkTask *chunk_Task) :
			chunkTask(chunk_Task), next(NULL) {
	}
};

class TasksQueueChunk {
private:
	const uchar* chunkBegin;
	ID chunkID;
	int xyType;
	int soType;

private:
	NodeChunkQueue* head;
	NodeChunkQueue* tail;
	pthread_mutex_t headMutex, tailMutex;

private:
	TasksQueueChunk();
public:
	TasksQueueChunk(const uchar* chunk_Begin, ID& chunk_ID, int xy_Type,
			int so_Type) :
			chunkBegin(chunk_Begin), chunkID(chunk_ID), xyType(xy_Type), soType(
					so_Type) {
		NodeChunkQueue* nodeChunkQueue = new NodeChunkQueue();
		head = tail = nodeChunkQueue;
		pthread_mutex_init(&headMutex, NULL);
		pthread_mutex_init(&tailMutex, NULL);
	}

	~TasksQueueChunk() {
		cout << "index" << endl;
		if (head != NULL) {
			delete head;
			head = NULL;
		}
		if (tail != NULL) {
			delete tail;
			tail = NULL;
		}
		pthread_mutex_destroy(&headMutex);
		pthread_mutex_destroy(&tailMutex);
	}

	bool isEmpty() {
		pthread_mutex_lock(&headMutex);
		if (head->next == NULL) {
			pthread_mutex_unlock(&headMutex);
			return true;
		} else {
			pthread_mutex_unlock(&headMutex);
			return false;
		}
	}
	const ID getChunkID() {
		return chunkID;
	}
	const uchar* getChunkBegin() {
		return chunkBegin;
	}
	const int getXYType() {
		return xyType;
	}
	const int getSOType() {
		return soType;
	}

	void EnQueue(ChunkTask *chunkTask) {
		NodeChunkQueue* nodeChunkQueue = new NodeChunkQueue(chunkTask);
		pthread_mutex_lock(&tailMutex);
		tail->next = nodeChunkQueue;
		tail = nodeChunkQueue;
		pthread_mutex_unlock(&tailMutex);
	}

	ChunkTask* Dequeue() {
		NodeChunkQueue* node = NULL;
		ChunkTask* chunkTask = NULL;

		pthread_mutex_lock(&headMutex);
		node = head;
		NodeChunkQueue* newNode = node->next;
		if (newNode == NULL) {
			pthread_mutex_unlock(&headMutex);
			return chunkTask;
		}
		chunkTask = newNode->chunkTask;
		head = newNode;
		pthread_mutex_unlock(&headMutex);
		delete node;
		return chunkTask;
	}
};

#endif /* TASKSQUEUECHUNK_H_ */
