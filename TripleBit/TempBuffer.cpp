/*
 * TempBuffer.cpp
 *
 *  Created on: 2013-7-10
 *      Author: root
 */

#include "TempBuffer.h"
#include "MemoryBuffer.h"
#include <math.h>
#include <pthread.h>

TempBuffer::TempBuffer() {
	// TODO Auto-generated constructor stub
	buffer = (ID*)malloc(TEMPBUFFER_INIT_PAGE_COUNT * getpagesize());
	usedSize = 0;
	totalSize = TEMPBUFFER_INIT_PAGE_COUNT * getpagesize() / sizeof(ID);
	pos = 0;

	IDCount = 2;
	sortKey = 0;
}

TempBuffer::~TempBuffer() {
	// TODO Auto-generated destructor stub
	if(buffer != NULL)
		free(buffer);
	buffer = NULL;
}

ID& TempBuffer::operator [](const size_t index)
{
	if(index > (usedSize / IDCount))
		return buffer[0];
	return buffer[index * IDCount + sortKey];
}

Status TempBuffer::insertID(ID idX, ID idY)
{
	buffer[pos++] = idX;
	buffer[pos++] = idY;
	usedSize++;
	usedSize++;
	return OK;
}

Status TempBuffer::clear()
{
	pos = 0;
	usedSize = 0;
	return OK;
}

void TempBuffer::Print()
{
	for(size_t i = 0; i < usedSize; ++++i)
	{
		cout << "x:" << buffer[i];
		cout << " y:" << buffer[i + 1] << " ";
	}
	cout << endl;
}

int cmp(const void *lhs, const void *rhs)
{
	ID xlhs = *(ID*)lhs;
	ID ylhs = *(ID*)(lhs + sizeof(ID));
	ID xrhs = *(ID*)rhs;
	ID yrhs = *(ID*)(rhs + sizeof(ID));
	if(xlhs != xrhs) return xlhs - xrhs;
	else return ylhs - yrhs;
}

Status TempBuffer::sort()
{
	qsort(buffer, getSize(), sizeof(ID) * IDCount, cmp);
	return OK;
}

void TempBuffer::uniqe()
{
	if(usedSize <= 2) return;
	ID *lastPtr, *currentPtr, *endPtr;
	lastPtr = currentPtr = buffer;
	endPtr = getEnd();
	currentPtr += IDCount;
	while(currentPtr < endPtr)
	{
		if(*lastPtr == *currentPtr && *(lastPtr + 1) == *(currentPtr + 1))
		{
			currentPtr += IDCount;
		}
		else
		{
			lastPtr += IDCount;
			*lastPtr = *currentPtr;
			*(lastPtr + 1) = *(currentPtr + 1);
			currentPtr += 2;
		}
	}
	usedSize = lastPtr + IDCount - buffer;
}

