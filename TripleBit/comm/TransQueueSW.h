/*
 * TransQueueSW.h
 *
 *  Created on: 2013-6-28
 *      Author: root
 */

#ifndef TRANSQUEUESW_H_
#define TRANSQUEUESW_H_

#include <iostream>
#include "../TripleBit.h"
#include "Tools.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>

using namespace std;
using namespace boost;

//#define DEBUG

class transQueueSW
{
private:
	//something about the Queue
	int tail, head, length;
	uint64_t timeBase;
	transaction *transQueue[MAXTRANSNUM + 1];
	//something about thread control
	boost::mutex mu;
	condition_variable_any cond_enqueue;
	condition_variable_any cond_dequeue;

public:
	bool Queue_Empty(){	return head == tail; }
	bool Queue_Full(){	return (tail % length + 1) == head; }
	void Print(transaction* trans)
	{
		cout << "transTime(sec): " << trans->transTime.tv_sec << endl;
		cout << "transTime(usec): " << trans->transTime.tv_usec << endl;
		cout << "transInfo: " << trans->transInfo << endl;
	}

	transQueueSW():tail(1),head(1),length(MAXTRANSNUM), timeBase(0){}
//	~transQueueSW() { delete []transQueue; }

	void setTimeBase(uint64_t time) { timeBase = time; }
	uint64_t getTimeBase() { return timeBase; }

	void EnQueue(const string& transString)
	{
		{
			boost::mutex::scoped_lock lock(mu);
			while(Queue_Full())
			{
				cond_enqueue.wait(mu);
			}
			transQueue[tail] = new transaction(transString);
#ifdef DEBUG
			Print(transQueue[tail]);
#endif
			tail = (tail == length)? 1 : (tail + 1);
		}
		cond_dequeue.notify_one();
	}

	transaction* DeQueue()
	{
		boost::mutex::scoped_lock lock(mu);
		while(Queue_Empty())
		{
			cond_dequeue.wait(mu);
		}
		transaction  *trans = transQueue[head];
#ifdef DEBUG
		Print(transQueue[head]);
#endif
		head = (head == length)? 1: (head + 1);
		cond_enqueue.notify_one();
		return trans;
	}
};


#endif /* TRANSQUEUESW_H_ */
