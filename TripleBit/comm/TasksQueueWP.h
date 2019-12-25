/*
 * TasksQueueWP.h
 *
 *  Created on: 2019-08-09
 *      Author: favoniankong
 */

#ifndef TASKSQUEUEWP_H_
#define TASKSQUEUEWP_H_

#include "../TripleBit.h"
#include "../TripleBitQueryGraph.h"
#include "Tools.h"
#include "IndexForTT.h"
#include "Tasks.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>

using namespace std;
using namespace boost;

class NodeTasksQueueWP: private Uncopyable {
public:
	SubTrans *tasksWP;
	NodeTasksQueueWP *next;
	NodeTasksQueueWP() :
			tasksWP(NULL), next(NULL) {
	}
	~NodeTasksQueueWP() {
	}
	NodeTasksQueueWP(SubTrans *subTrans) :
			tasksWP(subTrans), next(NULL) {
	}
};

class TasksQueueWP {
private:
	NodeTasksQueueWP *head, *tail;
	pthread_mutex_t headMutex, tailMutex;

public:
	TasksQueueWP() {
		NodeTasksQueueWP *node = new NodeTasksQueueWP;
		head = tail = node;
		pthread_mutex_init(&headMutex, NULL);
		pthread_mutex_init(&tailMutex, NULL);
	}
	~TasksQueueWP() {
		if (head) {
			delete head;
			head = NULL;
		}
		if (tail) {
			delete tail;
			tail = NULL;
		}
		pthread_mutex_destroy(&headMutex);
		pthread_mutex_destroy(&tailMutex);
	}

	void EnQueue(SubTrans *subTrans) {
		NodeTasksQueueWP *node = new NodeTasksQueueWP(subTrans);
		pthread_mutex_lock(&tailMutex);
		tail->next = node;
		tail = node;
		pthread_mutex_unlock(&tailMutex);
	}

	SubTrans *Dequeue() {
		NodeTasksQueueWP *node = NULL;
		SubTrans *trans = NULL;

		pthread_mutex_lock(&headMutex);
		node = head;
		NodeTasksQueueWP *newNode = node->next;
		if (newNode == NULL) {
			pthread_mutex_unlock(&headMutex);
			return trans;
		}
		trans = newNode->tasksWP;
		head = newNode;
		pthread_mutex_unlock(&headMutex);
		delete node;
		return trans;
	}

	bool Queue_Empty() {
		pthread_mutex_lock(&headMutex);
		if (head->next == NULL) {
			pthread_mutex_unlock(&headMutex);
			return true;
		} else {
			pthread_mutex_unlock(&headMutex);
			return false;
		}
	}
};

#endif /* TASKSQUEUEWP_H_ */
