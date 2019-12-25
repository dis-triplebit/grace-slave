/*
 * ResultBuffer.h
 *
 *  Created on: 2013-7-1
 *      Author: root
 */

#ifndef RESULTBUFFER_H_
#define RESULTBUFFER_H_

#include <iostream>
#include "../TripleBit.h"
#include "../ResultIDBuffer.h"

using namespace std;
using namespace boost;

class ResultBuffer{
private:
	int tail;
	int head;
	int length;
	ResultIDBuffer *resultBuffer[MAXRESULTNUM + 1];

	boost::mutex mu;
	condition_variable_any cond_enqueue;
	condition_variable_any cond_dequeue;

public:
	bool Queue_Empty(){ return head == tail; }

	bool Queue_Full(){ return (tail % length + 1) == head;	}

	ResultBuffer():tail(1),head(1),length(MAXRESULTNUM){}
//	~ResultBuffer(){
//		delete[] resultBuffer;
//	}

	void EnQueue(ResultIDBuffer*& result){
		{
			boost::mutex::scoped_lock lock(mu);
			while(Queue_Full()){
				cond_enqueue.wait(mu);
			}
			resultBuffer[tail] = result;
			tail = (tail == length)? 1 : (tail + 1);
		}
		cond_dequeue.notify_one();
	}

	ResultIDBuffer* DeQueue(){
		boost::mutex::scoped_lock lock(mu);
		while(Queue_Empty()){
			cond_dequeue.wait(mu);
		}
		ResultIDBuffer* result = resultBuffer[head];
		resultBuffer[head] = NULL;
		head = (head == length)? 1 : (head + 1);
		cond_enqueue.notify_one();

		return result;
	}
};



#endif /* RESULTBUFFER_H_ */
