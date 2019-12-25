/*
 * TempBuffer.h
 *
 *  Created on: 2013-7-10
 *      Author: root
 */

#ifndef TEMPBUFFER_H_
#define TEMPBUFFER_H_

#include "TripleBit.h"
#include "ThreadPool.h"
#include "EntityIDBuffer.h"

class TempBuffer {
public:

	int IDCount;
	ID* buffer;
	int pos;
	size_t usedSize;
	size_t totalSize;

	//used to sort
	int sortKey;

public:
	Status insertID(ID idX, ID idY);
	void Print();
	Status sort();
	void uniqe();
	ID& operator[](const size_t index);
	Status clear();
	bool isFull() { return usedSize > totalSize-2; }
	bool isEmpty() { return usedSize == 0; }
	size_t getSize() const{
		return usedSize / IDCount;
	}
	ID* getBuffer() const{
		return buffer;
	}

	ID* getEnd(){
		return getBuffer() + usedSize;
	}

	TempBuffer();
	~TempBuffer();

};

#endif /* TEMPBUFFER_H_ */
