/*
 * PartitionBufferManager.h
 *
 *  Created on: 2019年08月15日
 *      Author: favoniankong
 */

#ifndef PARTITIONBUFFERMANAGER_H_
#define PARTITIONBUFFERMANAGER_H_

#define INIT_PARTITION_BUFFERS 20
#define MAX_BUFFERS 100

class EntityIDBuffer;

#include "../TripleBit.h"

class PartitionBufferManager {
private:
	vector<EntityIDBuffer*> bufferPool;
	vector<EntityIDBuffer*> usedBuffer;
	vector<EntityIDBuffer*> cleanBuffer;
	int bufferCnt;

	boost::mutex bufferMutex;

public:
	PartitionBufferManager();
	PartitionBufferManager(int initBufferNum);
	virtual ~PartitionBufferManager();
	EntityIDBuffer* getNewBuffer();
	Status freeBuffer(EntityIDBuffer* buffer);
	Status reserveBuffer();
	void destroyBuffers();
};

#endif /* PARTITIONBUFFERMANAGER_H_ */
