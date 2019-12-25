/*
 * PartitionBufferManager.h
 *
 *  Created on: 2019å¹´08æœˆ16æ—¥
 *      Author: favoniankong
 */


#include "PartitionBufferManager.h"
#include "../EntityIDBuffer.h"

//Éè¼Æ³É»·ÐÎÊÇ²»ÊÇ¸üºÃÄØ

PartitionBufferManager::PartitionBufferManager() {
	// TODO Auto-generated constructor stub
	bufferCnt = INIT_PARTITION_BUFFERS;
	for(int i = 0; i < bufferCnt; i++)
	 {
		 EntityIDBuffer* buffer = new EntityIDBuffer;
		 bufferPool.push_back(buffer);
		 cleanBuffer.push_back(buffer);
	 }
	 usedBuffer.clear();
}

PartitionBufferManager::PartitionBufferManager(int initBufferNum)
{
	bufferCnt = (initBufferNum > MAX_BUFFERS ? MAX_BUFFERS: initBufferNum);
	for(int i = 0; i < bufferCnt; i++)
	{
		EntityIDBuffer* buffer = new EntityIDBuffer;
		bufferPool.push_back(buffer);
		cleanBuffer.push_back(buffer);
	}
	usedBuffer.clear();
}

PartitionBufferManager::~PartitionBufferManager() {
	// TODO Auto-generated destructor stub
}

EntityIDBuffer* PartitionBufferManager::getNewBuffer()
{
	if(usedBuffer.size() == bufferPool.size())
	{
		return NULL;
	}
	EntityIDBuffer* buffer = cleanBuffer.front();
	{
		boost::mutex::scoped_lock lock(bufferMutex);
		cleanBuffer.erase(cleanBuffer.begin());
		usedBuffer.push_back(buffer);
	}
	return buffer;
}

void PartitionBufferManager::destroyBuffers()
{
	for(unsigned i = 0; i < bufferPool.size(); i++)
	{
		delete bufferPool[i];
	}

	usedBuffer.clear();
	cleanBuffer.clear();
}

Status PartitionBufferManager::freeBuffer(EntityIDBuffer* buffer)
{
	vector<EntityIDBuffer*>::iterator iter = find(usedBuffer.begin(), usedBuffer.end(), buffer);
	if(iter != usedBuffer.end())
	{
		boost::mutex::scoped_lock lock(bufferMutex);
		usedBuffer.erase(iter);
		cleanBuffer.push_back(*iter);
		(*iter)->empty();
		return OK;
	}
	else
	{
		return NOT_FOUND;
	}
}

Status PartitionBufferManager::reserveBuffer()
{
	usedBuffer.clear();
	cleanBuffer.clear();
	for(int i = 0; i < bufferCnt; i++)
	{
		bufferPool[i]->empty();
		cleanBuffer.push_back(bufferPool[i]);
	}
	return OK;
}
