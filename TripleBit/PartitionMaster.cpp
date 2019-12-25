/*
 * PartitionMaster.cpp
 *
 *  Created on: 2013-8-19
 *      Author: root
 */

#include "MemoryBuffer.h"
#include "BitmapBuffer.h"
#include "TripleBitRepository.h"
#include "EntityIDBuffer.h"
#include "TripleBitQueryGraph.h"
#include "util/BufferManager.h"
#include "util/PartitionBufferManager.h"
#include "comm/TasksQueueWP.h"
#include "comm/ResultBuffer.h"
#include "comm/TasksQueueChunk.h"
#include "comm/subTaskPackage.h"
#include "comm/Tasks.h"
#include "TempBuffer.h"
#include "MMapBuffer.h"
#include "PartitionMaster.h"
#include "ThreadPool.h"
#include "TempMMapBuffer.h"
#include "util/Timestamp.h"
#include "TripleBit.h"
#include "CBitMap.h"

#define QUERY_TIME

extern ID xMin;
extern ID xMax;
extern ID zMin;
extern ID zMax;

extern vector<vector<CBitMap *> > patternBitmap;
extern ID preVar;
extern ID postVar;
extern int preLevel;
extern int postLevel;
extern bool isSingle;
extern bool isPreUnknown;
PartitionMaster::PartitionMaster(TripleBitRepository*& repo, const ID parID) {
	tripleBitRepo = repo;
	bitmap = repo->getBitmapBuffer();

	workerNum = repo->getWorkerNum();
	partitionNum = repo->getPartitionNum();
	vector<TasksQueueWP*> tasksQueueWP = repo->getTasksQueueWP();
	vector<ResultBuffer*> resultWP = repo->getResultWP();

	partitionID = parID;
	partitionChunkManager[0] = bitmap->getChunkManager(partitionID, 0);
	partitionChunkManager[1] = bitmap->getChunkManager(partitionID, 1);

	tasksQueue = tasksQueueWP[partitionID - 1];
	for (int workerID = 1; workerID <= workerNum; ++workerID) {
		resultBuffer[workerID] = resultWP[(workerID - 1) * partitionNum + partitionID - 1];
	}

	unsigned chunkSizeAll = 0;
	for (int type = 0; type < 2; type++) {
		int xyType = 1;
		const uchar* startPtr = partitionChunkManager[type]->getStartPtr(xyType);
		xChunkNumber[type] = partitionChunkManager[type]->getChunkNumber(xyType);
		chunkSizeAll += xChunkNumber[type];
		ID chunkID = 0;
		xChunkQueue[type][0] = new TasksQueueChunk(startPtr, chunkID, xyType, type);
		xChunkTempBuffer[type][0] = new TempBuffer;
		for (chunkID = 1; chunkID < xChunkNumber[type]; chunkID++) {
			xChunkQueue[type][chunkID] = new TasksQueueChunk(startPtr + chunkID * MemoryBuffer::pagesize - sizeof(ChunkManagerMeta), chunkID, xyType,
					type);
			xChunkTempBuffer[type][chunkID] = new TempBuffer;
		}

		xyType = 2;
		startPtr = partitionChunkManager[type]->getStartPtr(xyType);
		xyChunkNumber[type] = partitionChunkManager[type]->getChunkNumber(xyType);
		chunkSizeAll += xyChunkNumber[type];
		for (chunkID = 0; chunkID < xyChunkNumber[type]; chunkID++) {
			xyChunkQueue[type][chunkID] = new TasksQueueChunk(startPtr + chunkID * MemoryBuffer::pagesize, chunkID, xyType, type);
			xyChunkTempBuffer[type][chunkID] = new TempBuffer;
		}
	}

	partitionBufferManager = new PartitionBufferManager(chunkSizeAll);
}

void PartitionMaster::endupdate() {
	for (int soType = 0; soType < 2; ++soType) {
		int xyType = 1;
		const uchar *startPtr = partitionChunkManager[soType]->getStartPtr(xyType);
		ID chunkID = 0;
		combineTempBufferToSource(xChunkTempBuffer[soType][chunkID], startPtr, chunkID, xyType, soType);
		for (chunkID = 1; chunkID < xChunkNumber[soType]; ++chunkID) {
			if (!xChunkTempBuffer[soType][chunkID]->isEmpty()) {
				combineTempBufferToSource(xChunkTempBuffer[soType][chunkID], startPtr - sizeof(ChunkManagerMeta) + chunkID * MemoryBuffer::pagesize,
						chunkID, xyType, soType);
			}
		}

		xyType = 2;
		startPtr = partitionChunkManager[soType]->getStartPtr(xyType);
		for (chunkID = 0; chunkID < xyChunkNumber[soType]; ++chunkID) {
			if (!xyChunkTempBuffer[soType][chunkID]->isEmpty()) {
				combineTempBufferToSource(xyChunkTempBuffer[soType][chunkID], startPtr + chunkID * MemoryBuffer::pagesize, chunkID, xyType, soType);
			}
		}
	}
}

PartitionMaster::~PartitionMaster() {
	for (int type = 0; type < 2; type++) {
		for (unsigned chunkID = 0; chunkID < xChunkNumber[type]; chunkID++) {
			if (xChunkQueue[type][chunkID]) {
				delete xChunkQueue[type][chunkID];
				xChunkQueue[type][chunkID] = NULL;
			}
			if (xChunkTempBuffer[type][chunkID]) {
				delete xChunkTempBuffer[type][chunkID];
				xChunkTempBuffer[type][chunkID] = NULL;
			}
		}
		for (unsigned chunkID = 0; chunkID < xyChunkNumber[type]; chunkID++) {
			if (xyChunkQueue[type][chunkID]) {
				delete xyChunkQueue[type][chunkID];
				xChunkQueue[type][chunkID] = NULL;
			}
			if (xyChunkTempBuffer[type][chunkID]) {
				delete xyChunkTempBuffer[type][chunkID];
				xChunkTempBuffer[type][chunkID] = NULL;
			}
		}
	}

	if (partitionBufferManager) {
		delete partitionBufferManager;
		partitionBufferManager = NULL;
	}
}

void PartitionMaster::Work() {
#ifdef MYDEBUG
	cout << __FUNCTION__ << " partitionID: " << partitionID<< endl;
#endif

	while (1) {
		SubTrans* subTransaction = tasksQueue->Dequeue();

		if (subTransaction == NULL)
			break;

		switch (subTransaction->operationType) {
		case TripleBitQueryGraph::QUERY:
			executeQuery(subTransaction);
			delete subTransaction;
			break;
		case TripleBitQueryGraph::INSERT_DATA:
			executeInsertData(subTransaction);
			delete subTransaction;
			break;
		case TripleBitQueryGraph::DELETE_DATA:
			executeDeleteData(subTransaction);
			delete subTransaction;
			break;
		case TripleBitQueryGraph::DELETE_CLAUSE:
			executeDeleteClause(subTransaction);
			delete subTransaction;
			break;
		case TripleBitQueryGraph::UPDATE:
			SubTrans *subTransaction2 = tasksQueue->Dequeue();
			executeUpdate(subTransaction, subTransaction2);
			delete subTransaction;
			delete subTransaction2;
			break;
		}
	}
}

void PartitionMaster::doChunkGroupQuery(vector<pair<ChunkTask*,TasksQueueChunk*> > & tasks,int begin,int end){
	    //cout << " in this group" << endl;
		//printf("%p to %p\n",tasks[begin].second->getChunkBegin(),tasks[end].second->getChunkBegin());
		struct timeval start,e;
		//gettimeofday(&start,NULL);
		
		for(int i = begin;i <= end; i++){
			doChunkQuery(tasks[i]);
		}		
		//gettimeofday(&e,NULL);
		//cout << "this group time elapsed: " << ((e.tv_sec - start.tv_sec) * 1000000 + e.tv_usec - start.tv_usec) << " us" << endl;
}

void PartitionMaster::doChunkQuery(pair < ChunkTask *, TasksQueueChunk * > & task){
	ChunkTask *chunkTask = task.first;
	TasksQueueChunk* tasksQueue = task.second;
	ID chunkID = tasksQueue->getChunkID();
	int xyType = tasksQueue->getXYType();
	int soType = tasksQueue->getSOType();
	const uchar* chunkBegin = tasksQueue->getChunkBegin();
	executeChunkTaskQuery(chunkTask, chunkID, chunkBegin, xyType);
}

void PartitionMaster::taskEnQueue(ChunkTask *chunkTask, TasksQueueChunk *tasksQueue) {
	if (tasksQueue->isEmpty()) {
		tasksQueue->EnQueue(chunkTask);
		ThreadPool::getChunkPool().addTask(boost::bind(&PartitionMaster::handleTasksQueueChunk, this, tasksQueue));
	} else {
		tasksQueue->EnQueue(chunkTask);
	}
}

void PartitionMaster::executeQuery(SubTrans *subTransaction){
#ifdef QUERY_TIME
	Timestamp t1;
#endif

	ID minID = subTransaction->minID;
	ID maxID = subTransaction->maxID;
	TripleNode *triple = &(subTransaction->triple);
	size_t chunkCount, xChunkCount, xyChunkCount;
	size_t xChunkIDMin, xChunkIDMax, xyChunkIDMin, xyChunkIDMax;
	int soType, xyType;
	xChunkIDMin = xChunkIDMax = xyChunkIDMin = xyChunkIDMax = 0;
	chunkCount = xChunkCount = xyChunkCount = 0;

	switch (triple->scanOperation) {
	case TripleNode::FINDOSBYP: {
		soType = 1;
		xyType = 1;
		xChunkIDMin = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(minID, minID + 1);
		xChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(maxID, UINT_MAX);
		assert(xChunkIDMin <= xChunkIDMax);
		xChunkCount = xChunkIDMax - xChunkIDMin + 1;

		xyType = 2;
		minID = subTransaction->minID;
		maxID = subTransaction->maxID;
		xyChunkIDMin = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(minID, 1);
		xyChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(maxID, maxID - 1);
		assert(xyChunkIDMin <= xyChunkIDMax);
		xyChunkCount = xyChunkIDMax - xyChunkIDMin + 1;

		break;
	}
	case TripleNode::FINDSOBYP: {
		soType = 0;
		xyType = 1;
		xChunkIDMin = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(minID, minID + 1);
		xChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(maxID, UINT_MAX);
		assert(xChunkIDMin <= xChunkIDMax);
		xChunkCount = xChunkIDMax - xChunkIDMin + 1;

		xyType = 2;
		minID = subTransaction->minID;
		maxID = subTransaction->maxID;
		xyChunkIDMin = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(minID, 1);
		xyChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(maxID, maxID - 1);
		assert(xyChunkIDMin <= xyChunkIDMax);
		xyChunkCount = xyChunkIDMax - xyChunkIDMin + 1;

		break;
	}
	case TripleNode::FINDSBYPO: {
		soType = 1;
		if (minID > triple->object) {
			xyType = 1;
			if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->object, minID, xChunkIDMin)) {
				if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->object, maxID, xChunkIDMax)) {
					assert(xChunkIDMax >= xChunkIDMin);
					xChunkCount = xChunkIDMax - xChunkIDMin + 1;
				}
			} else
				xChunkCount = 0;
		} else if (maxID < triple->object) {
			xyType = 2;
			if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->object, minID, xyChunkIDMin)) {
				if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->object, maxID, xyChunkIDMax)) {
					assert(xyChunkIDMax >= xyChunkIDMin);
					xyChunkCount = xyChunkIDMax - xyChunkIDMin + 1;
				}
			} else
				xyChunkCount = 0;
		} else if (minID < triple->object && maxID > triple->object) {
			xyType = 1;
			if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->object, triple->object + 1, xChunkIDMin)) {
				if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->object, maxID, xChunkIDMax)) {
					assert(xChunkIDMax >= xChunkIDMin);
					xChunkCount = xChunkIDMax - xChunkIDMin + 1;
				}
			} else
				xChunkCount = 0;

			xyType = 2;
			if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->object, minID, xyChunkIDMin)) {
				if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->object, triple->object - 1, xyChunkIDMax)) {
					assert(xyChunkIDMax >= xyChunkIDMin);
					xyChunkCount = xyChunkIDMax - xyChunkIDMin + 1;
				}
			} else
				xyChunkCount = 0;
		}
		break;
	}
	case TripleNode::FINDOBYSP: {
		soType = 0;
		if (minID > triple->subject) {
			xyType = 1;
			if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->subject, minID, xChunkIDMin)) {
				xChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->subject, maxID);
				assert(xChunkIDMax >= xChunkIDMin);
				xChunkCount = xChunkIDMax - xChunkIDMin + 1;
			} else
				xChunkCount = 0;
		} else if (maxID < triple->subject) {
			xyType = 2;
			if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->subject, minID, xyChunkIDMin)) {
				xyChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->subject, maxID);
				assert(xyChunkIDMax >= xyChunkIDMin);
				xyChunkCount = xyChunkIDMax - xyChunkIDMin + 1;
			} else
				xyChunkCount = 0;
		} else if (minID < triple->subject && maxID > triple->subject) {
			xyType = 1;
			if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->subject, triple->subject + 1, xChunkIDMin)) {
				xChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->subject, maxID);
				assert(xChunkIDMax >= xChunkIDMin);
				xChunkCount = xChunkIDMax - xChunkIDMin + 1;
			} else
				xChunkCount = 0;

			xyType = 2;
			if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->subject, minID, xyChunkIDMin)) {
				xyChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(triple->subject, triple->subject - 1);
				assert(xyChunkIDMax >= xyChunkIDMin);
				xyChunkCount = xyChunkIDMax - xyChunkIDMin + 1;
			} else
				xyChunkCount = 0;
		}
		break;
	}
	case TripleNode::FINDSBYP: {
		soType = 0;
		xyType = 1;
		xChunkIDMin = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(minID, minID + 1);
		xChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(maxID, UINT_MAX);
		assert(xChunkIDMax >= xChunkIDMin);
		xChunkCount = xChunkIDMax - xChunkIDMin + 1;

		xyType = 2;
		minID = subTransaction->minID;
		maxID = subTransaction->maxID;
		xyChunkIDMin = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(minID, 1);
		xyChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(maxID, maxID - 1);
		assert(xyChunkIDMax >= xyChunkIDMin);
		xyChunkCount = xyChunkIDMax - xyChunkIDMin + 1;

		break;
	}
	case TripleNode::FINDOBYP: {
		soType = 1;
		xyType = 1;
		xChunkIDMin = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(minID, minID + 1);
		xChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(maxID, UINT_MAX);
		assert(xChunkIDMax >= xChunkIDMin);
		xChunkCount = xChunkIDMax - xChunkIDMin + 1;

		xyType = 2;
		minID = subTransaction->minID;
		maxID = subTransaction->maxID;
		xyChunkIDMin = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(minID, 1);
		xyChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(maxID, maxID - 1);
		assert(xyChunkIDMax >= xyChunkIDMin);
		xyChunkCount = xyChunkIDMax - xyChunkIDMin + 1;

		break;
	}
	}

	if (xChunkCount + xyChunkCount == 0) {
		//tripleBitRepo->tripleBitWorker[1]->workerQuery->resultPtr->push_back("Empty Result");
		cout << "Empty Result" << endl;
		return;
	}

	ID sourceWorkerID = subTransaction->sourceWorkerID;
	chunkCount = xChunkCount + xyChunkCount;
	boost::shared_ptr<subTaskPackage> taskPackage(
			new subTaskPackage(chunkCount, subTransaction->operationType, sourceWorkerID, subTransaction->minID, subTransaction->maxID, 0, 0,
					partitionBufferManager));

	vector<pair<ChunkTask*,TasksQueueChunk*> > tasks;
	ChunkTask *chunkTask = new ChunkTask(subTransaction->operationType, triple->subject, triple->object, triple->scanOperation, taskPackage,
						subTransaction->indexForTT);
	
	if (xChunkCount != 0) {
		for (size_t offsetID = xChunkIDMin; offsetID <= xChunkIDMax; offsetID++) {
			tasks.push_back(make_pair(chunkTask,xChunkQueue[soType][offsetID]));				
		}
	}

	if (xyChunkCount != 0) {
		for (size_t offsetID = xyChunkIDMin; offsetID <= xyChunkIDMax; offsetID++) {
			tasks.push_back(make_pair(chunkTask,xyChunkQueue[soType][offsetID]));							
		}
	}

	int cbegin = tasks.size()*5/8;   //parallel tasks
	vector<pair<ChunkTask*,TasksQueueChunk*> > ctasks;
		
	for(int i = cbegin;i < tasks.size(); i++){
		ctasks.push_back(tasks.back());
		tasks.pop_back();
	}

	ThreadPool::getScanjoinPool().addTask(boost::bind(&PartitionMaster::doChunkGroupQuery,this,ctasks,0,ctasks.size()-1));			

	if(tasks.size() < SCAN_JOIN_THREAD_NUMBER){
		for(int i = 0;i < tasks.size();i++){
			ThreadPool::getScanjoinPool().addTask(boost::bind(&PartitionMaster::doChunkGroupQuery,this,tasks,i,i));			
		}
	}else{
		int _chunkCount = SCAN_JOIN_THREAD_NUMBER;
		size_t chunkSize =  tasks.size() / _chunkCount;
		int i = 0;
		for( i = 0; i < _chunkCount; i++ )
		{
			if( i == _chunkCount -1 ){
				ThreadPool::getScanjoinPool().addTask(boost::bind(&PartitionMaster::doChunkGroupQuery,this,tasks,i*chunkSize,tasks.size()-1));
			}
			else{
				ThreadPool::getScanjoinPool().addTask(boost::bind(&PartitionMaster::doChunkGroupQuery,this,tasks,i*chunkSize,i * chunkSize + chunkSize - 1));
			}
		}
	}
	ThreadPool::getScanjoinPool().wait();

#ifdef QUERY_TIME
	Timestamp t3;
#endif

#ifdef QUERY_TIME
	Timestamp t4;
	cout << "ChunkCount:" << chunkCount << " taskEnqueue time elapsed: " << (static_cast<double> (t4-t3)/1000.0) << " s" << endl;
#endif
}

void PartitionMaster::executeInsertData(SubTrans* subTransaction) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << endl;
#endif
	ID subjectID = subTransaction->triple.subject;
	ID objectID = subTransaction->triple.object;
	size_t chunkID;
	int soType, xyType;
	boost::shared_ptr<subTaskPackage> taskPackage(new subTaskPackage);
	if (subjectID < objectID) {
		soType = 0;
		xyType = 1;
		chunkID = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(subjectID, objectID);
		ChunkTask *chunkTask1 = new ChunkTask(subTransaction->operationType, subjectID, objectID, subTransaction->triple.scanOperation, taskPackage,
				subTransaction->indexForTT);
		taskEnQueue(chunkTask1, xChunkQueue[soType][chunkID]);

		soType = 1;
		xyType = 2;
		chunkID = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(objectID, subjectID);
		ChunkTask *chunkTask2 = new ChunkTask(subTransaction->operationType, subjectID, objectID, subTransaction->triple.scanOperation, taskPackage,
				subTransaction->indexForTT);
		taskEnQueue(chunkTask2, xyChunkQueue[soType][chunkID]);
	} else {
		soType = 0;
		xyType = 2;
		chunkID = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(subjectID, objectID);
		ChunkTask *chunkTask1 = new ChunkTask(subTransaction->operationType, subjectID, objectID, subTransaction->triple.scanOperation, taskPackage,
				subTransaction->indexForTT);
		taskEnQueue(chunkTask1, xyChunkQueue[soType][chunkID]);

		soType = 1;
		xyType = 1;
		chunkID = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(objectID, subjectID);
		ChunkTask *chunkTask2 = new ChunkTask(subTransaction->operationType, subjectID, objectID, subTransaction->triple.scanOperation, taskPackage,
				subTransaction->indexForTT);
		taskEnQueue(chunkTask2, xChunkQueue[soType][chunkID]);
	}
}

void PartitionMaster::executeDeleteData(SubTrans* subTransaction) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << endl;
#endif
	executeInsertData(subTransaction);
}

void PartitionMaster::executeDeleteClause(SubTrans* subTransaction) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << endl;
#endif
	ID subjectID = subTransaction->triple.subject;
	ID objectID = subTransaction->triple.object;
	int soType, xyType;
	size_t xChunkIDMin, xChunkIDMax, xyChunkIDMin, xyChunkIDMax;
	size_t chunkCount, xChunkCount, xyChunkCount;
	xChunkIDMin = xChunkIDMax = xyChunkIDMin = xyChunkIDMax = 0;
	chunkCount = xChunkCount = xyChunkCount = 0;

	if (subTransaction->triple.constSubject) {
		soType = 0;
		xyType = 1;
		if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(subjectID, subjectID + 1, xChunkIDMin)) {
			xChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(subjectID, UINT_MAX);
			assert(xChunkIDMax >= xChunkIDMin);
			xChunkCount = xChunkIDMax - xChunkIDMin + 1;
		} else
			xChunkCount = 0;

		xyType = 2;
		if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(subjectID, 0, xyChunkIDMin)) {
			xyChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(subjectID, subjectID - 1);
			assert(xyChunkIDMax >= xyChunkIDMin);
			xyChunkCount = xyChunkIDMax - xyChunkIDMin + 1;
		} else {
			xyChunkCount = 0;
		}

		if (xChunkCount + xyChunkCount == 0)
			return;

		chunkCount = xChunkCount + xyChunkCount;
		boost::shared_ptr<subTaskPackage> taskPackage(
				new subTaskPackage(chunkCount, subTransaction->operationType, 0, 0, 0, subjectID, 0, partitionBufferManager));
		if (xChunkCount != 0) {
			for (size_t offsetID = xChunkIDMin; offsetID <= xChunkIDMax; offsetID++) {
				ChunkTask *chunkTask = new ChunkTask(subTransaction->operationType, subjectID, objectID, subTransaction->triple.scanOperation,
						taskPackage, subTransaction->indexForTT);
				taskEnQueue(chunkTask, xChunkQueue[soType][offsetID]);
			}
		}
		if (xyChunkCount != 0) {
			for (size_t offsetID = xyChunkIDMin; offsetID <= xyChunkIDMax; offsetID++) {
				ChunkTask *chunkTask = new ChunkTask(subTransaction->operationType, subjectID, objectID, subTransaction->triple.scanOperation,
						taskPackage, subTransaction->indexForTT);
				taskEnQueue(chunkTask, xyChunkQueue[soType][offsetID]);
			}
		}
	} else {
		soType = 1;
		xyType = 1;
		if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(objectID, objectID + 1, xChunkIDMin)) {
			xChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(objectID, UINT_MAX);
			assert(xChunkIDMax >= xChunkIDMin);
			xChunkCount = xChunkIDMax - xChunkIDMin + 1;
		} else
			xChunkCount = 0;

		xyType = 2;
		if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(objectID, 0, xyChunkIDMin)) {
			xyChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(objectID, objectID - 1);
			assert(xyChunkIDMax >= xyChunkIDMin);
			xyChunkCount = xyChunkIDMax - xyChunkIDMin + 1;
		} else
			xyChunkCount = 0;

		if (xChunkCount + xyChunkCount == 0)
			return;
		chunkCount = xChunkCount + xyChunkCount;
		boost::shared_ptr<subTaskPackage> taskPackage(
				new subTaskPackage(chunkCount, subTransaction->operationType, 0, 0, 0, objectID, 0, partitionBufferManager));
		if (xChunkCount != 0) {
			for (size_t offsetID = xChunkIDMin; offsetID <= xChunkIDMax; offsetID++) {
				ChunkTask *chunkTask = new ChunkTask(subTransaction->operationType, subjectID, objectID, subTransaction->triple.scanOperation,
						taskPackage, subTransaction->indexForTT);
				taskEnQueue(chunkTask, xChunkQueue[soType][offsetID]);
			}
		}
		if (xyChunkCount != 0) {
			for (size_t offsetID = xyChunkIDMin; offsetID <= xyChunkIDMax; offsetID++) {
				ChunkTask *chunkTask = new ChunkTask(subTransaction->operationType, subjectID, objectID, subTransaction->triple.scanOperation,
						taskPackage, subTransaction->indexForTT);
				taskEnQueue(chunkTask, xyChunkQueue[soType][offsetID]);
			}
		}
	}
}

void PartitionMaster::executeUpdate(SubTrans *subTransfirst, SubTrans *subTranssecond) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << endl;
#endif
	ID subjectID = subTransfirst->triple.subject;
	ID objectID = subTransfirst->triple.object;
	ID subUpdate = subTranssecond->triple.subject;
	ID obUpdate = subTranssecond->triple.object;
	int soType, xyType;
	size_t xChunkIDMin, xChunkIDMax, xyChunkIDMin, xyChunkIDMax;
	size_t chunkCount, xChunkCount, xyChunkCount;
	xChunkIDMin = xChunkIDMax = xyChunkIDMin = xyChunkIDMax = 0;
	chunkCount = xChunkCount = xyChunkCount = 0;

	if (subTransfirst->triple.constSubject) {
		soType = 0;
		xyType = 1;
		if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(subjectID, subjectID + 1, xChunkIDMin)) {
			xChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(subjectID, UINT_MAX);
			assert(xChunkIDMax >= xChunkIDMin);
			xChunkCount = xChunkIDMax - xChunkIDMin + 1;
		} else
			xChunkCount = 0;

		xyType = 2;
		if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(subjectID, 0, xyChunkIDMin)) {
			xyChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(subjectID, subjectID - 1);
			assert(xyChunkIDMax >= xyChunkIDMin);
			xyChunkCount = xyChunkCount - xyChunkCount + 1;
		} else{
			xyChunkCount = 0;
		}

		if (xChunkCount + xyChunkCount == 0)
			return;
		chunkCount = xChunkCount + xyChunkCount;
		boost::shared_ptr<subTaskPackage> taskPackage(
				new subTaskPackage(chunkCount, subTransfirst->operationType, 0, 0, 0, subjectID, subUpdate, partitionBufferManager));
		if (xChunkCount != 0) {
			for (size_t offsetID = xChunkIDMin; offsetID <= xChunkIDMax; offsetID++) {
				ChunkTask *chunkTask = new ChunkTask(subTransfirst->operationType, subjectID, objectID, subTransfirst->triple.scanOperation,
						taskPackage, subTransfirst->indexForTT);
				taskEnQueue(chunkTask, xChunkQueue[soType][offsetID]);
			}
		}
		if (xyChunkCount != 0) {
			for (size_t offsetID = xyChunkIDMin; offsetID <= xyChunkIDMax; offsetID++) {
				ChunkTask *chunkTask = new ChunkTask(subTransfirst->operationType, subjectID, objectID, subTransfirst->triple.scanOperation,
						taskPackage, subTransfirst->indexForTT);
				taskEnQueue(chunkTask, xyChunkQueue[soType][offsetID]);
			}
		}
	} else {
		soType = 1;
		xyType = 1;
		if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(objectID, objectID + 1, xChunkIDMin)) {
			xChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(objectID, UINT_MAX);
			assert(xChunkIDMax >= xChunkIDMin);
			xChunkCount = xChunkIDMax - xChunkIDMin + 1;
		} else
			xChunkCount = 0;

		xyType = 2;
		if (partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(objectID, 0, xyChunkIDMin)) {
			xyChunkIDMax = partitionChunkManager[soType]->getChunkIndex(xyType)->searchChunk(objectID, objectID - 1);
			assert(xyChunkIDMax >= xyChunkIDMin);
			xyChunkCount = xyChunkCount - xyChunkCount + 1;
		} else
			xyChunkCount = 0;

		if (xChunkCount + xyChunkCount == 0)
			return;
		chunkCount = xChunkCount + xyChunkCount;
		boost::shared_ptr<subTaskPackage> taskPackage(
				new subTaskPackage(chunkCount, subTransfirst->operationType, 0, 0, 0, objectID, obUpdate, partitionBufferManager));
		if (xChunkCount != 0) {
			for (size_t offsetID = xChunkIDMin; offsetID <= xChunkIDMax; offsetID++) {
				ChunkTask *chunkTask = new ChunkTask(subTransfirst->operationType, subjectID, objectID, subTransfirst->triple.scanOperation,
						taskPackage, subTransfirst->indexForTT);
				taskEnQueue(chunkTask, xChunkQueue[soType][offsetID]);
			}
		}
		if (xyChunkCount != 0) {
			for (size_t offsetID = xyChunkIDMin; offsetID <= xyChunkIDMax; offsetID++) {
				ChunkTask *chunkTask = new ChunkTask(subTransfirst->operationType, subjectID, objectID, subTransfirst->triple.scanOperation,
						taskPackage, subTransfirst->indexForTT);
				taskEnQueue(chunkTask, xyChunkQueue[soType][offsetID]);
			}
		}
	}
}

void PrintChunkTaskPart(ChunkTask* chunkTask) {
	cout << "opType:" << chunkTask->operationType << " subject:" << chunkTask->Triple.subject << " object:" << chunkTask->Triple.object
			<< " operation:" << chunkTask->Triple.operation << endl;
}

void PartitionMaster::handleTasksQueueChunk(TasksQueueChunk* tasksQueue) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << " partitionID: " << partitionID<< endl;
#endif

	ChunkTask* chunkTask = NULL;
	ID chunkID = tasksQueue->getChunkID();
	int xyType = tasksQueue->getXYType();
	int soType = tasksQueue->getSOType();
	const uchar* chunkBegin = tasksQueue->getChunkBegin();

	while ((chunkTask = tasksQueue->Dequeue()) != NULL) {
		switch (chunkTask->operationType) {
		case TripleBitQueryGraph::QUERY:
			executeChunkTaskQuery(chunkTask, chunkID, chunkBegin, xyType);
			break;
		case TripleBitQueryGraph::INSERT_DATA:
			executeChunkTaskInsertData(chunkTask, chunkID, chunkBegin, xyType, soType);
			break;
		case TripleBitQueryGraph::DELETE_DATA:
			executeChunkTaskDeleteData(chunkTask, chunkID, chunkBegin, xyType, soType);
			break;
		case TripleBitQueryGraph::DELETE_CLAUSE:
			executeChunkTaskDeleteClause(chunkTask, chunkID, chunkBegin, xyType, soType);
			break;
		case TripleBitQueryGraph::UPDATE:
			executeChunkTaskUpdate(chunkTask, chunkID, chunkBegin, xyType, soType);
			break;
		}
	}
}

void PartitionMaster::executeChunkTaskInsertData(ChunkTask *chunkTask, const ID chunkID, const uchar *startPtr, const int xyType, const int soType) {
	if (soType == 0) {
		if (xyType == 1) {
			xChunkTempBuffer[soType][chunkID]->insertID(chunkTask->Triple.subject, chunkTask->Triple.object);
			if (xChunkTempBuffer[soType][chunkID]->isFull()) {
				//combine the data in tempbuffer into the source data
				combineTempBufferToSource(xChunkTempBuffer[soType][chunkID], startPtr, chunkID, xyType, soType);
			}
		} else if (xyType == 2) {
			xyChunkTempBuffer[soType][chunkID]->insertID(chunkTask->Triple.subject, chunkTask->Triple.object);
			if (xyChunkTempBuffer[soType][chunkID]->isFull()) {
				//combine the data in tempbuffer into the source data
				combineTempBufferToSource(xyChunkTempBuffer[soType][chunkID], startPtr, chunkID, xyType, soType);
			}
		}
	} else if (soType == 1) {
		if (xyType == 1) {
			xChunkTempBuffer[soType][chunkID]->insertID(chunkTask->Triple.object, chunkTask->Triple.subject);
			if (xChunkTempBuffer[soType][chunkID]->isFull()) {
				//combine the data in tempbuffer into the source data
				combineTempBufferToSource(xChunkTempBuffer[soType][chunkID], startPtr, chunkID, xyType, soType);
			}
		} else if (xyType == 2) {
			xyChunkTempBuffer[soType][chunkID]->insertID(chunkTask->Triple.object, chunkTask->Triple.subject);
			if (xyChunkTempBuffer[soType][chunkID]->isFull()) {
				//combine the data in tempbuffer into the source data
				combineTempBufferToSource(xyChunkTempBuffer[soType][chunkID], startPtr, chunkID, xyType, soType);
			}
		}
	}

	chunkTask->indexForTT->completeOneTriple();
}

static void getInsertChars(char *temp, unsigned x, unsigned y) {
	char *ptr = temp;
	while (x >= 128) {
		unsigned char c = static_cast<unsigned char>(x & 127);
		*ptr = c;
		ptr++;
		x >>= 7;
	}
	*ptr = static_cast<unsigned char>(x & 127);
	ptr++;

	while (y >= 128) {
		unsigned char c = static_cast<unsigned char>(y | 128);
		*ptr = c;
		ptr++;
		y >>= 7;
	}
	*ptr = static_cast<unsigned char>(y | 128);
	ptr++;
}

void PartitionMaster::readIDInTempPage(const uchar *&currentPtrTemp, const uchar *&endPtrTemp, const uchar *&startPtrTemp, char *&tempPage,
		char *&tempPage2, bool &theOtherPageEmpty, bool &isInTempPage) {
	if (currentPtrTemp >= endPtrTemp) {
		//the Ptr has reach the end of the page
		if (theOtherPageEmpty) {
			//TempPage is ahead of Chunk
			MetaData *metaData = (MetaData*) startPtrTemp;
			if (metaData->haveNextPage) {
				//TempPage still have followed Page
				size_t pageNo = metaData->NextPageNo;
				memcpy(tempPage, TempMMapBuffer::getInstance().getAddress() + pageNo * MemoryBuffer::pagesize, MemoryBuffer::pagesize);
				isInTempPage = true;

				startPtrTemp = reinterpret_cast<uchar*>(tempPage);
				currentPtrTemp = startPtrTemp + sizeof(MetaData);
				endPtrTemp = startPtrTemp + ((MetaData*) startPtrTemp)->usedSpace;
			}
		} else {
			//Chunk ahead of TempPage, but Chunk is ahead one Page most,that is Chunk is on the next page of the TempPage
			if (isInTempPage) {
				startPtrTemp = reinterpret_cast<uchar*>(tempPage2);
				isInTempPage = false;
			} else {
				startPtrTemp = reinterpret_cast<uchar*>(tempPage);
				isInTempPage = true;
			}
			currentPtrTemp = startPtrTemp + sizeof(MetaData);
			endPtrTemp = startPtrTemp + ((MetaData*) startPtrTemp)->usedSpace;
			theOtherPageEmpty = true;
		}
	}
}

void PartitionMaster::handleEndofChunk(const uchar *startPtr, uchar *&chunkBegin, uchar *&currentPtrChunk, uchar *&endPtrChunk,
		const uchar *&startPtrTemp, char *&tempPage, char *&tempPage2, bool &isInTempPage, bool &theOtherPageEmpty, ID minID, const int xyType,
		const ID chunkID) {
	assert(currentPtrChunk <= endPtrChunk);
	MetaData *metaData = NULL;
	if (xyType == 1 && chunkBegin == startPtr - sizeof(ChunkManagerMeta)) {
		metaData = (MetaData*) startPtr;
		const uchar *reader = startPtr + sizeof(MetaData);
		ID tempx = 0;
		reader = Chunk::readXId(reader, tempx);
		metaData->minID = tempx;
		metaData->usedSpace = currentPtrChunk - startPtr;
	} else if (xyType == 2 && chunkID == 0 && chunkBegin == startPtr) {
		metaData = (MetaData*) chunkBegin;
		const uchar *reader = chunkBegin + sizeof(MetaData);
		ID tempx = 0, tempy = 0;
		reader = Chunk::readYId(Chunk::readXId(reader, tempx), tempy);
		metaData->minID = tempx + tempy;
		metaData->usedSpace = currentPtrChunk - chunkBegin;
	} else {
		metaData = (MetaData*) chunkBegin;
		metaData->usedSpace = currentPtrChunk - chunkBegin;
	}
	if (metaData->haveNextPage) {
		MetaData *metaDataTemp = (MetaData*) startPtrTemp;
		size_t pageNo = metaData->NextPageNo;
		if (metaDataTemp->NextPageNo <= pageNo) {
			if (isInTempPage)
				memcpy(tempPage2, TempMMapBuffer::getInstance().getAddress() + pageNo * MemoryBuffer::pagesize, MemoryBuffer::pagesize);
			else
				memcpy(tempPage, TempMMapBuffer::getInstance().getAddress() + pageNo * MemoryBuffer::pagesize, MemoryBuffer::pagesize);
			theOtherPageEmpty = false;
		}
		chunkBegin = reinterpret_cast<uchar*>(TempMMapBuffer::getInstance().getAddress()) + pageNo * MemoryBuffer::pagesize;
		metaData = (MetaData*) chunkBegin;
	} else {
		//get a new Page from TempMMapBuffer
		size_t pageNo = 0;
		chunkBegin = reinterpret_cast<uchar*>(TempMMapBuffer::getInstance().getPage(pageNo));

		metaData->haveNextPage = true;
		metaData->NextPageNo = pageNo;

		metaData = (MetaData*) chunkBegin;
		metaData->haveNextPage = false;
		metaData->NextPageNo = 0;
	}
	currentPtrChunk = chunkBegin + sizeof(MetaData);
	endPtrChunk = chunkBegin + MemoryBuffer::pagesize;
	metaData->minID = minID;
	assert(currentPtrChunk <= endPtrChunk);
}

void PartitionMaster::combineTempBufferToSource(TempBuffer *buffer, const uchar *startPtr, const ID chunkID, const int xyType, const int soType) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << " partitionID: " << partitionID<< endl;
#endif

	assert(buffer != NULL);

	buffer->sort();

	buffer->uniqe();

	if (buffer->isEmpty())
		return;

	char *tempPage = (char*) malloc(MemoryBuffer::pagesize);
	char *tempPage2 = (char*) malloc(MemoryBuffer::pagesize);

	if (tempPage == NULL || tempPage2 == NULL) {
		cout << "malloc a tempPage error" << endl;
		free(tempPage);
		free(tempPage2);
		return;
	}

	uchar *currentPtrChunk, *endPtrChunk, *chunkBegin;
	const uchar *lastPtrTemp, *currentPtrTemp, *endPtrTemp, *startPtrTemp;
	bool isInTempPage = true, theOtherPageEmpty = true;

	if (xyType == 1 && chunkID == 0) {
		chunkBegin = const_cast<uchar*>(startPtr) - sizeof(ChunkManagerMeta);
		memcpy(tempPage, chunkBegin, MemoryBuffer::pagesize);
		currentPtrChunk = chunkBegin + sizeof(ChunkManagerMeta) + sizeof(MetaData);
		lastPtrTemp = currentPtrTemp = reinterpret_cast<uchar*>(tempPage) + sizeof(ChunkManagerMeta) + sizeof(MetaData);
	} else {
		chunkBegin = const_cast<uchar*>(startPtr);
		memcpy(tempPage, chunkBegin, MemoryBuffer::pagesize);
		currentPtrChunk = chunkBegin + sizeof(MetaData);
		lastPtrTemp = currentPtrTemp = reinterpret_cast<uchar*>(tempPage) + sizeof(MetaData);
	}
	endPtrChunk = chunkBegin + MemoryBuffer::pagesize;
	startPtrTemp = lastPtrTemp - sizeof(MetaData);
	endPtrTemp = startPtrTemp + ((MetaData*) startPtrTemp)->usedSpace;

	register ID xChunk = 0, yChunk = 0, xTemp = 0, yTemp = 0;
	ID *lastTempBuffer, *currentTempBuffer, *endTempBuffer;
	ID *start = buffer->getBuffer(), *end = buffer->getEnd();
	lastTempBuffer = currentTempBuffer = start;
	endTempBuffer = end;

	xTemp = *(currentTempBuffer++);
	yTemp = *(currentTempBuffer++);
	if (currentPtrTemp >= endPtrTemp) {
		xChunk = 0;
		yChunk = 0;
	} else {
		currentPtrTemp = Chunk::readYId(Chunk::readXId(currentPtrTemp, xChunk), yChunk);
	}

	if (xyType == 1) {
		while (lastPtrTemp < endPtrTemp && lastTempBuffer < endTempBuffer) {
			//the Ptr not reach the end
			if (xChunk == 0 || (xChunk == xTemp && (xChunk + yChunk) == yTemp)) {
				//the data is 0 or the data in chunk and tempbuffer are same,so must dismiss it
				readIDInTempPage(currentPtrTemp, endPtrTemp, startPtrTemp, tempPage, tempPage2, theOtherPageEmpty, isInTempPage);
				lastPtrTemp = currentPtrTemp;
				if (currentPtrTemp >= endPtrTemp) {
					xChunk = 0;
					yChunk = 0;
				} else {
					currentPtrTemp = Chunk::readYId(Chunk::readXId(currentPtrTemp, xChunk), yChunk);
				}
			} else {
				if (xChunk < xTemp || (xChunk == xTemp && (xChunk + yChunk) < yTemp)) {
					unsigned len = currentPtrTemp - lastPtrTemp;
					if (currentPtrChunk + len > endPtrChunk) {
						handleEndofChunk(startPtr, chunkBegin, currentPtrChunk, endPtrChunk, startPtrTemp, tempPage, tempPage2, isInTempPage,
								theOtherPageEmpty, xChunk, xyType, chunkID);
					}
					memcpy(currentPtrChunk, lastPtrTemp, len);
					currentPtrChunk += len;
//					assert(currentPtrChunk <= endPtrChunk);

					//continue read data from tempPage
					readIDInTempPage(currentPtrTemp, endPtrTemp, startPtrTemp, tempPage, tempPage2, theOtherPageEmpty, isInTempPage);
					lastPtrTemp = currentPtrTemp;
					if (currentPtrTemp >= endPtrTemp) {
						xChunk = 0;
						yChunk = 0;
					} else {
						currentPtrTemp = Chunk::readYId(Chunk::readXId(currentPtrTemp, xChunk), yChunk);
					}
				} else {
					//insert data read from tempbuffer
					yTemp = yTemp - xTemp;
					unsigned len = getLen(xTemp) + getLen(yTemp);
					char temp[10];
					getInsertChars(temp, xTemp, yTemp);
					if (currentPtrChunk + len > endPtrChunk) {
						handleEndofChunk(startPtr, chunkBegin, currentPtrChunk, endPtrChunk, startPtrTemp, tempPage, tempPage2, isInTempPage,
								theOtherPageEmpty, xTemp, xyType, chunkID);
					}
					memcpy(currentPtrChunk, lastPtrTemp, len);
					currentPtrChunk += len;
//					assert(currentPtrChunk <= endPtrChunk);

					lastTempBuffer = currentTempBuffer;
					if (currentTempBuffer < endTempBuffer) {
						xTemp = *(currentTempBuffer++);
						yTemp = *(currentTempBuffer++);
					}
				}
			}
		}

		while (lastPtrTemp < endPtrTemp) {
			if (xChunk == 0) {
				readIDInTempPage(currentPtrTemp, endPtrTemp, startPtrTemp, tempPage, tempPage2, theOtherPageEmpty, isInTempPage);
				lastPtrTemp = currentPtrTemp;
				if (currentPtrTemp >= endPtrTemp) {
					xChunk = 0;
					yChunk = 0;
				} else {
					currentPtrTemp = Chunk::readYId(Chunk::readXId(currentPtrTemp, xChunk), yChunk);
				}
			} else {
				unsigned len = currentPtrTemp - lastPtrTemp;
				if (currentPtrChunk + len > endPtrChunk) {
					handleEndofChunk(startPtr, chunkBegin, currentPtrChunk, endPtrChunk, startPtrTemp, tempPage, tempPage2, isInTempPage,
							theOtherPageEmpty, xChunk, xyType, chunkID);
				}
				memcpy(currentPtrChunk, lastPtrTemp, len);
				currentPtrChunk += len;
//				assert(currentPtrChunk <= endPtrChunk);

				//continue read data from tempPage
				readIDInTempPage(currentPtrTemp, endPtrTemp, startPtrTemp, tempPage, tempPage2, theOtherPageEmpty, isInTempPage);
				lastPtrTemp = currentPtrTemp;
				if (currentPtrTemp >= endPtrTemp) {
					xChunk = 0;
					yChunk = 0;
				} else {
					currentPtrTemp = Chunk::readYId(Chunk::readXId(currentPtrTemp, xChunk), yChunk);
				}
			}
		}

		while (lastTempBuffer < endTempBuffer) {
			yTemp = yTemp - xTemp;
			unsigned len = getLen(xTemp) + getLen(yTemp);
			char temp[10];
			getInsertChars(temp, xTemp, yTemp);

			if (currentPtrChunk + len > endPtrChunk) {
				handleEndofChunk(startPtr, chunkBegin, currentPtrChunk, endPtrChunk, startPtrTemp, tempPage, tempPage2, isInTempPage,
						theOtherPageEmpty, xTemp, xyType, chunkID);
			}
			memcpy(currentPtrChunk, lastPtrTemp, len);
			currentPtrChunk += len;
//			assert(currentPtrChunk <= endPtrChunk);

			lastTempBuffer = currentTempBuffer;
			if (currentTempBuffer < endTempBuffer) {
				xTemp = *(currentTempBuffer++);
				yTemp = *(currentTempBuffer++);
			}
		}

		if (chunkBegin == startPtr - sizeof(ChunkManagerMeta)) {
			MetaData *metaData = (MetaData*) startPtr;
			const uchar* reader = startPtr + sizeof(MetaData);
			ID tempx = 0;
			reader = Chunk::readXId(reader, tempx);
			metaData->minID = tempx;
			metaData->usedSpace = currentPtrChunk - startPtr;
		} else {
			MetaData *metaData = (MetaData*) chunkBegin;
			metaData->usedSpace = currentPtrChunk - chunkBegin;
		}
	} else {
		//xyType == 2
		while (lastPtrTemp < endPtrTemp && lastTempBuffer < endTempBuffer) {
			if (xChunk == 0 || ((xChunk + yChunk) == xTemp && xChunk == yTemp)) {
				readIDInTempPage(currentPtrTemp, endPtrTemp, startPtrTemp, tempPage, tempPage2, theOtherPageEmpty, isInTempPage);
				lastPtrTemp = currentPtrTemp;
				if (currentPtrTemp >= endPtrTemp) {
					xChunk = 0;
					yChunk = 0;
				} else {
					currentPtrTemp = Chunk::readYId(Chunk::readXId(currentPtrTemp, xChunk), yChunk);
				}
			} else {
				if ((xChunk + yChunk) < xTemp || ((xChunk + yChunk) == xTemp || xChunk < yTemp)) {
					unsigned len = currentPtrTemp - lastPtrTemp;
					if (currentPtrChunk + len > endPtrChunk) {
						handleEndofChunk(startPtr, chunkBegin, currentPtrChunk, endPtrChunk, startPtrTemp, tempPage, tempPage2, isInTempPage,
								theOtherPageEmpty, xChunk + yChunk, xyType, chunkID);
					}
					memcpy(currentPtrChunk, lastPtrTemp, len);
					currentPtrChunk += len;
//					assert(currentPtrChunk <= endPtrChunk);

					readIDInTempPage(currentPtrTemp, endPtrTemp, startPtrTemp, tempPage, tempPage2, theOtherPageEmpty, isInTempPage);
					lastPtrTemp = currentPtrTemp;
					if (currentPtrTemp >= endPtrTemp) {
						xChunk = 0;
						yChunk = 0;
					} else {
						currentPtrTemp = Chunk::readYId(Chunk::readXId(currentPtrTemp, xChunk), yChunk);
					}
				} else {
					xTemp ^= yTemp ^= xTemp ^= yTemp;
					yTemp = yTemp - xTemp;
					unsigned len = getLen(xTemp) + getLen(yTemp);
					char temp[10];
					getInsertChars(temp, xTemp, yTemp);

					if (currentPtrChunk + len > endPtrChunk) {
						handleEndofChunk(startPtr, chunkBegin, currentPtrChunk, endPtrChunk, startPtrTemp, tempPage, tempPage2, isInTempPage,
								theOtherPageEmpty, xTemp + yTemp, xyType, chunkID);
					}
					memcpy(currentPtrChunk, lastPtrTemp, len);
					currentPtrChunk += len;
//					assert(currentPtrChunk <= endPtrChunk);

					lastTempBuffer = currentTempBuffer;
					if (currentTempBuffer < endTempBuffer) {
						xTemp = *(currentTempBuffer++);
						yTemp = *(currentTempBuffer++);
					}
				}
			}
		}

		while (lastPtrTemp < endPtrTemp) {
			if (xChunk == 0) {
				readIDInTempPage(currentPtrTemp, endPtrTemp, startPtrTemp, tempPage, tempPage2, theOtherPageEmpty, isInTempPage);
				lastPtrTemp = currentPtrTemp;
				if (currentPtrTemp >= endPtrTemp) {
					xChunk = 0;
					yChunk = 0;
				} else {
					currentPtrTemp = Chunk::readYId(Chunk::readXId(currentPtrTemp, xChunk), yChunk);
				}
			} else {
				unsigned len = currentPtrTemp - lastPtrTemp;
				if (currentPtrChunk + len > endPtrChunk) {
					handleEndofChunk(startPtr, chunkBegin, currentPtrChunk, endPtrChunk, startPtrTemp, tempPage, tempPage2, isInTempPage,
							theOtherPageEmpty, xChunk + yChunk, xyType, chunkID);
				}
				memcpy(currentPtrChunk, lastPtrTemp, len);
				currentPtrChunk += len;
//				assert(currentPtrChunk <= endPtrChunk);

				readIDInTempPage(currentPtrTemp, endPtrTemp, startPtrTemp, tempPage, tempPage2, theOtherPageEmpty, isInTempPage);
				lastPtrTemp = currentPtrTemp;
				if (currentPtrTemp >= endPtrTemp) {
					xChunk = 0;
					yChunk = 0;
				} else {
					currentPtrTemp = Chunk::readYId(Chunk::readXId(currentPtrTemp, xChunk), yChunk);
				}
			}
		}

		while (lastTempBuffer < endTempBuffer) {
			xTemp ^= yTemp ^= xTemp ^= yTemp;
			yTemp = yTemp - xTemp;
			unsigned len = getLen(xTemp) + getLen(yTemp);
			char temp[10];
			getInsertChars(temp, xTemp, yTemp);

			if (currentPtrChunk + len > endPtrChunk) {
				handleEndofChunk(startPtr, chunkBegin, currentPtrChunk, endPtrChunk, startPtrTemp, tempPage, tempPage2, isInTempPage,
						theOtherPageEmpty, xTemp + yTemp, xyType, chunkID);
			}
			memcpy(currentPtrChunk, lastPtrTemp, len);
			currentPtrChunk += len;
//			assert(currentPtrChunk <= endPtrChunk);

			lastTempBuffer = currentTempBuffer;
			if (currentTempBuffer < endTempBuffer) {
				xTemp = *(currentTempBuffer++);
				yTemp = *(currentTempBuffer++);
			}
		}

		if (chunkBegin == startPtr && chunkID == 0) {
			MetaData *metaData = (MetaData*) chunkBegin;
			const uchar *reader = chunkBegin + sizeof(MetaData);
			ID tempx = 0, tempy = 0;
			reader = Chunk::readYId(Chunk::readXId(reader, tempx), tempy);
			metaData->minID = tempx + tempy;
			metaData->usedSpace = currentPtrChunk - chunkBegin;
		} else {
			MetaData *metaData = (MetaData*) chunkBegin;
			metaData->usedSpace = currentPtrChunk - chunkBegin;
		}
	}

	partitionChunkManager[soType]->getChunkIndex(xyType)->updateChunkMetaData(chunkID);
	free(tempPage);
	free(tempPage2);

	buffer->clear();
}

void PartitionMaster::executeChunkTaskDeleteData(ChunkTask *chunkTask, const ID chunkID, const uchar* startPtr, const int xyType, const int soType) {
	if (soType == 0) {
		if (xyType == 1) {
			//sort by S, x < y
			ID subjectID = chunkTask->Triple.subject, objectID = chunkTask->Triple.object;
			register ID x, y;
			const uchar *reader, *limit, *chunkBegin = startPtr;
			uchar *temp;

			MetaData *metaData = (MetaData*) chunkBegin;
			reader = chunkBegin + sizeof(MetaData);
			limit = chunkBegin + metaData->usedSpace;

			while (reader < limit) {
				temp = const_cast<uchar*>(reader);
				reader = Chunk::readYId(Chunk::readXId(reader, x), y);
				if (x < subjectID)
					continue;
				else if (x == subjectID) {
					if (x + y < objectID)
						continue;
					else if (x + y == objectID) {
						temp = Chunk::deleteYId(Chunk::deleteXId(temp));
						return;
					} else
						return;
				} else
					return;
			}
			while (metaData->haveNextPage) {
				chunkBegin = reinterpret_cast<uchar*>(TempMMapBuffer::getInstance().getAddress()) + MemoryBuffer::pagesize * metaData->NextPageNo;
				metaData = (MetaData*) chunkBegin;
				reader = chunkBegin + sizeof(MetaData);
				limit = chunkBegin + metaData->usedSpace;
				while (reader < limit) {
					temp = const_cast<uchar*>(reader);
					reader = Chunk::readYId(Chunk::readXId(reader, x), y);
					if (x < subjectID)
						continue;
					else if (x == subjectID) {
						if (x + y < objectID)
							continue;
						else if (x + y == objectID) {
							temp = Chunk::deleteYId(Chunk::deleteXId(temp));
							return;
						} else
							return;
					} else
						return;
				}
			}
		} else if (xyType == 2) {
			//sort by S x>y
			ID subjectID = chunkTask->Triple.subject, objectID = chunkTask->Triple.object;
			register ID x, y;
			const uchar *reader, *limit, *chunkBegin = startPtr;
			uchar *temp;

			MetaData *metaData = (MetaData*) chunkBegin;
			reader = chunkBegin + sizeof(MetaData);
			limit = chunkBegin + metaData->usedSpace;
			while (reader < limit) {
				temp = const_cast<uchar*>(reader);
				reader = Chunk::readYId(Chunk::readXId(reader, x), y);
				if (x + y < subjectID)
					continue;
				else if (x + y == subjectID) {
					if (x < objectID)
						continue;
					else if (x == objectID) {
						temp = Chunk::deleteYId(Chunk::deleteXId(temp));
						return;
					} else
						return;
				} else
					return;
			}
			while (metaData->haveNextPage) {
				chunkBegin = reinterpret_cast<uchar*>(TempMMapBuffer::getInstance().getAddress()) + MemoryBuffer::pagesize * metaData->NextPageNo;
				metaData = (MetaData*) chunkBegin;
				reader = chunkBegin + sizeof(MetaData);
				limit = chunkBegin + metaData->usedSpace;
				while (reader < limit) {
					temp = const_cast<uchar*>(reader);
					reader = Chunk::readYId(Chunk::readXId(reader, x), y);
					if (x + y < subjectID)
						continue;
					else if (x + y == subjectID) {
						if (x < objectID)
							continue;
						else if (x == objectID) {
							temp = Chunk::deleteYId(Chunk::deleteXId(temp));
							return;
						} else
							return;
					} else
						return;
				}
			}
		}
	} else if (soType == 1) {
		if (xyType == 1) {
			//sort by O, x<y
			ID subjectID = chunkTask->Triple.subject, objectID = chunkTask->Triple.object;
			register ID x, y;
			const uchar *reader, *limit, *chunkBegin = startPtr;
			uchar* temp;

			MetaData *metaData = (MetaData*) chunkBegin;
			reader = chunkBegin + sizeof(MetaData);
			limit = chunkBegin + metaData->usedSpace;
			while (reader < limit) {
				temp = const_cast<uchar*>(reader);
				reader = Chunk::readYId(Chunk::readXId(reader, x), y);
				if (x < objectID)
					continue;
				else if (x == objectID) {
					if (x + y < subjectID)
						continue;
					else if (x + y == subjectID) {
						temp = Chunk::deleteYId(Chunk::deleteXId(temp));
						return;
					} else
						return;
				} else
					return;
			}
			while (metaData->haveNextPage) {
				chunkBegin = reinterpret_cast<uchar*>(TempMMapBuffer::getInstance().getAddress()) + MemoryBuffer::pagesize * metaData->NextPageNo;
				metaData = (MetaData*) chunkBegin;
				reader = chunkBegin + sizeof(MetaData);
				limit = chunkBegin + metaData->usedSpace;
				while (reader < limit) {
					temp = const_cast<uchar*>(reader);
					reader = Chunk::readYId(Chunk::readXId(reader, x), y);
					if (x < objectID)
						continue;
					else if (x == objectID) {
						if (x + y < subjectID)
							continue;
						else if (x + y == subjectID) {
							temp = Chunk::deleteYId(Chunk::deleteXId(temp));
							return;
						} else
							return;
					} else
						return;
				}
			}
		} else if (xyType == 2) {
			//sort by O, x>y
			ID subjectID = chunkTask->Triple.subject, objectID = chunkTask->Triple.object;
			register ID x, y;
			const uchar *reader, *limit, *chunkBegin = startPtr;
			uchar *temp;
			MetaData *metaData = (MetaData*) chunkBegin;
			reader = chunkBegin + sizeof(MetaData);
			limit = chunkBegin + metaData->usedSpace;
			while (reader < limit) {
				temp = const_cast<uchar*>(reader);
				reader = Chunk::readYId(Chunk::readXId(reader, x), y);
				if (x + y < objectID)
					continue;
				else if (x + y == objectID) {
					if (x < subjectID)
						continue;
					else if (x == subjectID) {
						temp = Chunk::deleteYId(Chunk::deleteXId(temp));
						return;
					} else
						return;
				} else
					return;
			}
			while (metaData->haveNextPage) {
				chunkBegin = reinterpret_cast<uchar*>(TempMMapBuffer::getInstance().getAddress()) + MemoryBuffer::pagesize * metaData->NextPageNo;
				metaData = (MetaData*) chunkBegin;
				reader = chunkBegin + sizeof(MetaData);
				limit = chunkBegin + metaData->usedSpace;
				while (reader < limit) {
					temp = const_cast<uchar*>(reader);
					reader = Chunk::readYId(Chunk::readXId(reader, x), y);
					if (x + y < objectID)
						continue;
					else if (x + y == objectID) {
						if (x < subjectID)
							continue;
						else if (x == subjectID) {
							temp = Chunk::deleteYId(Chunk::deleteXId(temp));
							return;
						} else
							return;
					} else
						return;
				}
			}
		}
	}
}

void PartitionMaster::deleteDataForDeleteClause(EntityIDBuffer *buffer, const ID deleteID, const int soType) {
	size_t size = buffer->getSize();
	ID *retBuffer = buffer->getBuffer();
	size_t index;
	int chunkID;
	boost::shared_ptr<subTaskPackage> taskPackage(new subTaskPackage);
	boost::shared_ptr<IndexForTT> indexForTT(new IndexForTT);
	TripleBitQueryGraph::OpType operationType = TripleBitQueryGraph::DELETE_DATA;
	TripleNode::Op scanType = TripleNode::NOOP;

	for (index = 0; index < size; ++index)
		if (retBuffer[index] > deleteID)
			break;
	if (soType == 0) {
		//deleteID -->subject
		int deleteSOType = 1;
		for (size_t i = 0; i < index; ++i) {
			//object < subject == x<y
			chunkID = partitionChunkManager[deleteSOType]->getChunkIndex(1)->searchChunk(retBuffer[i], deleteID);
			ChunkTask *chunkTask = new ChunkTask(operationType, deleteID, retBuffer[i], scanType, taskPackage, indexForTT);
			xChunkQueue[deleteSOType][chunkID]->EnQueue(chunkTask);
		}
		for (size_t i = index; i < size; ++i) {
			//object >subject == x>y
			chunkID = partitionChunkManager[deleteSOType]->getChunkIndex(2)->searchChunk(retBuffer[i], deleteID);
			ChunkTask *chunkTask = new ChunkTask(operationType, deleteID, retBuffer[i], scanType, taskPackage, indexForTT);
			xyChunkQueue[deleteSOType][chunkID]->EnQueue(chunkTask);
		}
	} else if (soType == 1) {
		//deleteID -->object
		int deleteSOType = 0;
		for (size_t i = 0; i < index; ++i) {
			//subject <object== x<y
			chunkID = partitionChunkManager[deleteSOType]->getChunkIndex(1)->searchChunk(retBuffer[i], deleteID);
			ChunkTask *chunkTask = new ChunkTask(operationType, retBuffer[i], deleteID, scanType, taskPackage, indexForTT);
			xChunkQueue[deleteSOType][chunkID]->EnQueue(chunkTask);
		}
		for (size_t i = index; i < size; ++i) {
			//subject > object == x >y
			chunkID = partitionChunkManager[deleteSOType]->getChunkIndex(2)->searchChunk(retBuffer[i], deleteID);
			ChunkTask *chunkTask = new ChunkTask(operationType, retBuffer[i], deleteID, scanType, taskPackage, indexForTT);
			xyChunkQueue[deleteSOType][chunkID]->EnQueue(chunkTask);
		}
	}
}

void PartitionMaster::executeChunkTaskDeleteClause(ChunkTask *chunkTask, const ID chunkID, const uchar *startPtr, const int xyType,
		const int soType) {
	ID deleteXID = 0, deleteXYID = 0;
	if (soType == 0) {
		if (xyType == 1) {
			//sort by S,x < y
			deleteXID = chunkTask->Triple.subject;
		} else if (xyType == 2) {
			//sort by S,x > y
			deleteXYID = chunkTask->Triple.subject;
		}
	} else if (soType == 1) {
		if (xyType == 1) {
			//sort by O, x<y
			deleteXID = chunkTask->Triple.object;
		} else if (xyType == 2) {
			//sort by O, x>y
			deleteXYID = chunkTask->Triple.object;
		}
	}
	EntityIDBuffer *retBuffer = new EntityIDBuffer;
	retBuffer->empty();
	retBuffer->setIDCount(1);
	retBuffer->setSortKey(0);
	register ID x, y;
	const uchar *reader, *limit, *chunkBegin = startPtr;
	uchar *temp;
	if (xyType == 1) {
		//x<y
		MetaData *metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = chunkBegin + metaData->usedSpace;
		while (reader < limit) {
			temp = const_cast<uchar*>(reader);
			reader = Chunk::readYId(Chunk::readXId(reader, x), y);
			if (x < deleteXID)
				continue;
			else if (x == deleteXID) {
				retBuffer->insertID(x + y);
				temp = Chunk::deleteYId(Chunk::deleteXId(temp));
			} else
				goto END;
		}
		while (metaData->haveNextPage) {
			chunkBegin = reinterpret_cast<uchar*>(TempMMapBuffer::getInstance().getAddress()) + metaData->NextPageNo * MemoryBuffer::pagesize;
			metaData = (MetaData*) chunkBegin;
			reader = chunkBegin + sizeof(MetaData);
			limit = chunkBegin + metaData->usedSpace;
			while (reader < limit) {
				temp = const_cast<uchar*>(reader);
				reader = Chunk::readYId(Chunk::readXId(reader, x), y);
				if (x < deleteXID)
					continue;
				else if (x == deleteXID) {
					retBuffer->insertID(x + y);
					temp = Chunk::deleteYId(Chunk::deleteXId(temp));
				} else
					goto END;
			}
		}
	} else if (xyType == 2) {
		//x>y
		MetaData *metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = chunkBegin + metaData->usedSpace;
		while (reader < limit) {
			temp = const_cast<uchar*>(reader);
			reader = Chunk::readYId(Chunk::readXId(reader, x), y);
			if (x + y < deleteXYID)
				continue;
			else if (x + y == deleteXYID) {
				retBuffer->insertID(x);
				temp = Chunk::deleteYId(Chunk::deleteXId(temp));
			} else
				goto END;
		}
		while (metaData->haveNextPage) {
			chunkBegin = reinterpret_cast<uchar*>(TempMMapBuffer::getInstance().getAddress()) + metaData->NextPageNo * MemoryBuffer::pagesize;
			metaData = (MetaData*) chunkBegin;
			reader = chunkBegin + sizeof(MetaData);
			limit = chunkBegin + metaData->usedSpace;
			while (reader < limit) {
				temp = const_cast<uchar*>(reader);
				reader = Chunk::readYId(Chunk::readXId(reader, x), y);
				if (x + y < deleteXYID)
					continue;
				else if (x + y == deleteXYID) {
					retBuffer->insertID(x);
					temp = Chunk::deleteYId(Chunk::deleteXId(temp));
				} else
					goto END;
			}
		}
	}

	END:
//	chunkTask->taskPackage->completeSubTask(chunkID, retBuffer, xyType);
	if (chunkTask->taskPackage->completeSubTask(chunkID, retBuffer, xyType)) {
		EntityIDBuffer *buffer = chunkTask->taskPackage->getTaskResult();
		ID deleteID = chunkTask->taskPackage->deleteID;
		deleteDataForDeleteClause(buffer, deleteID, soType);

		partitionBufferManager->freeBuffer(buffer);
	}
	retBuffer = NULL;
}

void PartitionMaster::updateDataForUpdate(EntityIDBuffer *buffer, const ID deleteID, const ID updateID, const int soType) {
	size_t size = buffer->getSize();
	ID *retBuffer = buffer->getBuffer();
	size_t indexDelete, indexUpdate;
	int chunkID;
	boost::shared_ptr<subTaskPackage> taskPackage(new subTaskPackage);
	boost::shared_ptr<IndexForTT> indexForTT(new IndexForTT);
	TripleBitQueryGraph::OpType opDelete = TripleBitQueryGraph::DELETE_DATA;
	TripleBitQueryGraph::OpType opInsert = TripleBitQueryGraph::INSERT_DATA;
	TripleNode::Op scanType = TripleNode::NOOP;

	for (indexDelete = 0; indexDelete < size; ++indexDelete)
		if (retBuffer[indexDelete] > deleteID)
			break;
	for (indexUpdate = 0; indexUpdate < size; ++indexUpdate)
		if (retBuffer[indexUpdate] > updateID)
			break;

	int deleteSOType, insertSOType, xyType;
	if (soType == 0) {
		//deleteID -->subject
		deleteSOType = 1;
		xyType = 1;
		for (size_t i = 0; i < indexDelete; ++i) {
			//object < subject == x<y
			chunkID = partitionChunkManager[deleteSOType]->getChunkIndex(xyType)->searchChunk(retBuffer[i], deleteID);
			ChunkTask *chunkTask = new ChunkTask(opDelete, deleteID, retBuffer[i], scanType, taskPackage, indexForTT);
			xChunkQueue[deleteSOType][chunkID]->EnQueue(chunkTask);
		}
		xyType = 2;
		for (size_t i = indexDelete; i < size; ++i) {
			//object > subject == x>y
			chunkID = partitionChunkManager[deleteSOType]->getChunkIndex(xyType)->searchChunk(retBuffer[i], deleteID);
			ChunkTask *chunkTask = new ChunkTask(opDelete, deleteID, retBuffer[i], scanType, taskPackage, indexForTT);
			xyChunkQueue[deleteSOType][chunkID]->EnQueue(chunkTask);
		}

		for (size_t i = 0; i < indexUpdate; ++i) {
			//subject > object
			insertSOType = 0;
			xyType = 2;
			chunkID = partitionChunkManager[insertSOType]->getChunkIndex(xyType)->searchChunk(updateID, retBuffer[i]);
			ChunkTask *chunkTask1 = new ChunkTask(opInsert, updateID, retBuffer[i], scanType, taskPackage, indexForTT);
			xyChunkQueue[insertSOType][chunkID]->EnQueue(chunkTask1);
			insertSOType = 1;
			xyType = 1;
			chunkID = partitionChunkManager[insertSOType]->getChunkIndex(xyType)->searchChunk(retBuffer[i], updateID);
			ChunkTask *chunkTask2 = new ChunkTask(opInsert, updateID, retBuffer[i], scanType, taskPackage, indexForTT);
			xChunkQueue[insertSOType][chunkID]->EnQueue(chunkTask2);
		}
		for (size_t i = indexUpdate; i < size; ++i) {
			//subject < object
			insertSOType = 0;
			xyType = 1;
			chunkID = partitionChunkManager[insertSOType]->getChunkIndex(xyType)->searchChunk(updateID, retBuffer[i]);
			ChunkTask *chunkTask1 = new ChunkTask(opInsert, updateID, retBuffer[i], scanType, taskPackage, indexForTT);
			xChunkQueue[insertSOType][chunkID]->EnQueue(chunkTask1);
			insertSOType = 1;
			xyType = 2;
			chunkID = partitionChunkManager[insertSOType]->getChunkIndex(xyType)->searchChunk(retBuffer[i], updateID);
			ChunkTask *chunkTask2 = new ChunkTask(opInsert, updateID, retBuffer[i], scanType, taskPackage, indexForTT);
			xyChunkQueue[insertSOType][chunkID]->EnQueue(chunkTask2);
		}
	} else if (soType == 1) {
		//deleteID-->object
		deleteSOType = 0;
		xyType = 1;
		for (size_t i = 0; i < indexDelete; ++i) {
			chunkID = partitionChunkManager[deleteSOType]->getChunkIndex(xyType)->searchChunk(retBuffer[i], deleteID);
			ChunkTask *chunkTask = new ChunkTask(opDelete, deleteID, retBuffer[i], scanType, taskPackage, indexForTT);
			xChunkQueue[deleteSOType][chunkID]->EnQueue(chunkTask);
		}
		xyType = 2;
		for (size_t i = indexDelete; i < size; ++i) {
			chunkID = partitionChunkManager[deleteSOType]->getChunkIndex(xyType)->searchChunk(retBuffer[i], deleteID);
			ChunkTask *chunkTask = new ChunkTask(opDelete, deleteID, retBuffer[i], scanType, taskPackage, indexForTT);
			xyChunkQueue[deleteSOType][chunkID]->EnQueue(chunkTask);
		}

		for (size_t i = 0; i < indexUpdate; ++i) {
			//subject < object
			insertSOType = 0, xyType = 1;
			chunkID = partitionChunkManager[insertSOType]->getChunkIndex(xyType)->searchChunk(retBuffer[i], updateID);
			ChunkTask *chunkTask1 = new ChunkTask(opInsert, retBuffer[i], updateID, scanType, taskPackage, indexForTT);
			xChunkQueue[insertSOType][chunkID]->EnQueue(chunkTask1);
			insertSOType = 1, xyType = 2;
			chunkID = partitionChunkManager[insertSOType]->getChunkIndex(xyType)->searchChunk(updateID, retBuffer[i]);
			ChunkTask *chunkTask2 = new ChunkTask(opInsert, retBuffer[i], updateID, scanType, taskPackage, indexForTT);
			xyChunkQueue[insertSOType][chunkID]->EnQueue(chunkTask2);
		}
		for (size_t i = indexUpdate; i < size; ++i) {
			//subject > object
			insertSOType = 0;
			xyType = 2;
			chunkID = partitionChunkManager[insertSOType]->getChunkIndex(xyType)->searchChunk(retBuffer[i], updateID);
			ChunkTask *chunkTask1 = new ChunkTask(opInsert, retBuffer[i], updateID, scanType, taskPackage, indexForTT);
			xyChunkQueue[insertSOType][chunkID]->EnQueue(chunkTask1);
			insertSOType = 1;
			xyType = 1;
			chunkID = partitionChunkManager[insertSOType]->getChunkIndex(xyType)->searchChunk(updateID, retBuffer[i]);
			ChunkTask *chunkTask2 = new ChunkTask(opInsert, retBuffer[i], updateID, scanType, taskPackage, indexForTT);
			xyChunkQueue[insertSOType][chunkID]->EnQueue(chunkTask2);
		}
	}
}

void PartitionMaster::executeChunkTaskUpdate(ChunkTask *chunkTask, const ID chunkID, const uchar* startPtr, const int xyType, const int soType) {
	ID deleteXID = 0, deleteXYID = 0;
	if (soType == 0) {
		if (xyType == 1)
			deleteXID = chunkTask->Triple.subject;
		else if (xyType == 2)
			deleteXYID = chunkTask->Triple.subject;
	} else if (soType == 1) {
		if (xyType == 1)
			deleteXID = chunkTask->Triple.object;
		else if (xyType == 2)
			deleteXYID = chunkTask->Triple.object;
	}
	EntityIDBuffer *retBuffer = new EntityIDBuffer;
	retBuffer->empty();
	retBuffer->setIDCount(1);
	retBuffer->setSortKey(0);
	register ID x, y;
	const uchar *reader, *limit, *chunkBegin = startPtr;
	uchar *temp;
	if (xyType == 1) {
		//x < y
		MetaData *metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = chunkBegin + metaData->usedSpace;
		while (reader < limit) {
			temp = const_cast<uchar*>(reader);
			reader = Chunk::readYId(Chunk::readXId(reader, x), y);
			if (x < deleteXID)
				continue;
			else if (x == deleteXID) {
				retBuffer->insertID(x + y);
				temp = Chunk::deleteYId(Chunk::deleteXId(temp));
			} else
				goto END;
		}
		while (metaData->haveNextPage) {
			chunkBegin = reinterpret_cast<uchar*>(TempMMapBuffer::getInstance().getAddress()) + metaData->NextPageNo * MemoryBuffer::pagesize;
			metaData = (MetaData*) chunkBegin;
			reader = chunkBegin + sizeof(MetaData);
			limit = chunkBegin + metaData->usedSpace;
			while (reader < limit) {
				temp = const_cast<uchar*>(reader);
				reader = Chunk::readYId(Chunk::readXId(reader, x), y);
				if (x < deleteXID)
					continue;
				else if (x == deleteXID) {
					retBuffer->insertID(x + y);
					temp = Chunk::deleteYId(Chunk::deleteXId(temp));
				} else
					goto END;
			}
		}
	} else if (xyType == 2) {
		//x >y
		MetaData *metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = chunkBegin + metaData->usedSpace;
		while (reader < limit) {
			temp = const_cast<uchar*>(reader);
			reader = Chunk::readYId(Chunk::readXId(reader, x), y);
			if (x + y < deleteXYID)
				continue;
			else if (x + y == deleteXYID) {
				retBuffer->insertID(x);
				temp = Chunk::deleteYId(Chunk::deleteXId(temp));
			} else
				goto END;
		}
		while (metaData->haveNextPage) {
			chunkBegin = reinterpret_cast<uchar*>(TempMMapBuffer::getInstance().getAddress()) + metaData->NextPageNo * MemoryBuffer::pagesize;
			metaData = (MetaData*) chunkBegin;
			reader = chunkBegin + sizeof(MetaData);
			limit = chunkBegin + metaData->usedSpace;
			while (reader < limit) {
				temp = const_cast<uchar*>(reader);
				reader = Chunk::readYId(Chunk::readXId(reader, x), y);
				if (x + y < deleteXYID)
					continue;
				else if (x + y == deleteXYID) {
					retBuffer->insertID(x);
					temp = Chunk::deleteYId(Chunk::deleteXId(temp));
				} else
					goto END;
			}
		}
	}
	END: if (chunkTask->taskPackage->completeSubTask(chunkID, retBuffer, xyType)) {
#ifdef MYTESTDEBUG
		cout << "complete all task update" << endl;
#endif
		EntityIDBuffer *buffer = chunkTask->taskPackage->getTaskResult();
		ID deleteID = chunkTask->taskPackage->deleteID;
		ID updateID = chunkTask->taskPackage->updateID;
		updateDataForUpdate(buffer, deleteID, updateID, soType);

		partitionBufferManager->freeBuffer(buffer);
	}
	retBuffer = NULL;
}

void PartitionMaster::executeChunkTaskQuery(ChunkTask *chunkTask, const ID chunkID, const uchar* chunkBegin, const int xyType) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << " partitionID: " << partitionID << endl;
#endif

//	EntityIDBuffer* retBuffer = partitionBufferManager->getNewBuffer();
	EntityIDBuffer *retBuffer = new EntityIDBuffer;
	retBuffer->empty();

	switch (chunkTask->Triple.operation) {
	case TripleNode::FINDSBYPO:
		findSubjectIDByPredicateAndObject(chunkTask->Triple.object, retBuffer, chunkTask->taskPackage->minID, chunkTask->taskPackage->maxID,
				chunkBegin, xyType);
		break;
	case TripleNode::FINDOBYSP:
		findObjectIDByPredicateAndSubject(chunkTask->Triple.subject, retBuffer, chunkTask->taskPackage->minID, chunkTask->taskPackage->maxID,
				chunkBegin, xyType);
		break;
	case TripleNode::FINDSBYP:
		findSubjectIDByPredicate(retBuffer, chunkTask->taskPackage->minID, chunkTask->taskPackage->maxID, chunkBegin, xyType);
		break;
	case TripleNode::FINDOBYP:
		findObjectIDByPredicate(retBuffer, chunkTask->taskPackage->minID, chunkTask->taskPackage->maxID, chunkBegin, xyType);
		break;
	case TripleNode::FINDOSBYP:
		findObjectIDAndSubjectIDByPredicate(retBuffer, chunkTask->taskPackage->minID, chunkTask->taskPackage->maxID, chunkBegin, xyType);
		break;
	case TripleNode::FINDSOBYP:
		findSubjectIDAndObjectIDByPredicate(retBuffer, chunkTask->taskPackage->minID, chunkTask->taskPackage->maxID, chunkBegin, xyType);
		break;
	default:
		cout << "unsupport now! executeChunkTaskQuery" << endl;
		break;
	}

	if (chunkTask->taskPackage->completeSubTask(chunkID, retBuffer, xyType)) {
//		EntityIDBuffer* buffer = chunkTask->taskPackage->getTaskResult();
		ResultIDBuffer* buffer = new ResultIDBuffer(chunkTask->taskPackage);

		resultBuffer[chunkTask->taskPackage->sourceWorkerID]->EnQueue(buffer);
	}
	retBuffer = NULL;
}

void PartitionMaster::findObjectIDByPredicateAndSubject(const ID subjectID, EntityIDBuffer *retBuffer, const ID minID, const ID maxID,
		const uchar* startPtr, const int xyType) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << " partitionID: " << partitionID<< endl;
#endif

	if (minID == 0 && maxID == UINT_MAX) {
		findObjectIDByPredicateAndSubject(subjectID, retBuffer, startPtr, xyType);
		return;
	}

	register ID x, y;
	const uchar* limit, *reader, *chunkBegin = startPtr;

	retBuffer->setIDCount(1);
	retBuffer->setSortKey(0);

	ID & a = retBuffer->range[0].first;
	ID & b = retBuffer->range[0].second;
	ID & c = retBuffer->range[1].first;
	ID & d = retBuffer->range[1].second;

	if (xyType == 1) {
		MetaData *metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = chunkBegin + metaData->usedSpace;

		while (reader < limit) {
			reader = Chunk::readXYId(reader,x,y);
			if (x < subjectID)
				continue;
			else if (x == subjectID) {
				if (x + y < minID)
					continue;
				else if (x + y <= maxID){
					if(preLevel==0){
						patternBitmap[preVar][preLevel]->insert(x+y);
						a = min(a,x+y);
						b = max(b,x+y);
						if(isSingle) retBuffer->insertID(x+y);
					}
					else{
						if(isPreUnknown)
						{
							patternBitmap[preVar][preLevel]->insert(x+y);
							a = min(a,x+y);
							b = max(b,x+y);
							if(isSingle) 
								retBuffer->insertID(x+y);
						}
						else
						{
							if(patternBitmap[preVar][preLevel-1]->contains(x+y)){
								patternBitmap[preVar][preLevel]->insert(x+y);
								a = min(a,x+y);
								b = max(b,x+y);
								if(isSingle) 
									retBuffer->insertID(x+y);
							}
						}
					}
				}
				else
					return;
			} 
			else
				return;
		}
	
	} else if (xyType == 2) {
		MetaData* metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = chunkBegin + metaData->usedSpace;
		while (reader < limit) {
			reader = Chunk::readYId(Chunk::readXId(reader, x), y);
			if (x + y < subjectID)
				continue;
			else if (x + y == subjectID) {
				if (x < minID)
					continue;
				else if (x <= maxID){
					if(preLevel==0){
						patternBitmap[preVar][preLevel]->insert(x);
						a = min(a,x);
						b = max(b,x);
						if(isSingle) retBuffer->insertID(x); // untest
					}else{
						if(isPreUnknown)
						{
							patternBitmap[preVar][preLevel]->insert(x);
							a = min(a,x);
							b = max(b,x);
							if(isSingle) retBuffer->insertID(x);
						}
						else
						{
							if(patternBitmap[preVar][preLevel-1]->contains(x)){
								patternBitmap[preVar][preLevel]->insert(x);
								a = min(a,x);
								b = max(b,x);
								if(isSingle) retBuffer->insertID(x);
							}
						}
					}
				}
				else
					return;
			} 
			else
				return;
		}
	}
}

void PartitionMaster::findObjectIDByPredicateAndSubject(const ID subjectID, EntityIDBuffer *retBuffer, const uchar *startPtr, const int xyType) {
	register ID x, y;
	const uchar *reader, *limit, *chunkBegin = startPtr;

	retBuffer->setIDCount(1);
	retBuffer->setSortKey(0);
	
	ID & a = retBuffer->range[0].first;
	ID & b = retBuffer->range[0].second;
	ID & c = retBuffer->range[1].first;
	ID & d = retBuffer->range[1].second;

	if (xyType == 1) {
		MetaData* metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = chunkBegin + metaData->usedSpace;

		while (reader < limit) {
			reader = Chunk::readXYId(reader,x,y);
			if (x < subjectID)
				continue;
			else if (x == subjectID){
				if(preLevel==0){
					patternBitmap[preVar][preLevel]->insert(x+y);
					a = min(a,x+y);
					b = max(b,x+y);
					if(isSingle) retBuffer->insertID(x+y);//2019年12月10日 09:13:50
				}else{
					if(isPreUnknown)
					{
						patternBitmap[preVar][preLevel]->insert(x+y);
						a = min(a,x+y);
						b = max(b,x+y);
						if(isSingle) retBuffer->insertID(x+y);
					}
					else{
						if(patternBitmap[preVar][preLevel-1]->contains(x+y)){
							patternBitmap[preVar][preLevel]->insert(x+y);
							a = min(a,x+y);
							b = max(b,x+y);
							if(isSingle) retBuffer->insertID(x+y);
						}
					}
				}
			}
			else
				return;
		}
	} else if (xyType == 2) {
		MetaData* metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = chunkBegin + metaData->usedSpace;

		while (reader < limit) {
			reader = Chunk::readXYId(reader,x,y);
			int count = 0;
			if (x+y < subjectID)
				continue;
			else if (x+y == subjectID){
				if(preLevel==0){
					patternBitmap[preVar][preLevel]->insert(x);
					a = min(a,x);
					b = max(b,x);
					if(isSingle) retBuffer->insertID(x); //2019年12月10日 09:13:40
				}else{
					if(isPreUnknown)
					{
						patternBitmap[preVar][preLevel]->insert(x);
						a = min(a,x);
						b = max(b,x);
						if(isSingle) retBuffer->insertID(x);
					}
					else{
						if(patternBitmap[preVar][preLevel-1]->contains(x)){
							patternBitmap[preVar][preLevel]->insert(x);
							a = min(a,x);
							b = max(b,x);
							if(isSingle) retBuffer->insertID(x);
						}
					}
				}
			}
			else
				return;
		}
	}
}

void PartitionMaster::findSubjectIDByPredicateAndObject(const ID objectID, EntityIDBuffer *retBuffer, const ID minID, const ID maxID,
		const uchar* startPtr, const int xyType) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << " partitionID: " << partitionID<< endl;
#endif
	findObjectIDByPredicateAndSubject(objectID, retBuffer, minID, maxID, startPtr, xyType);
}

void PartitionMaster::findSubjectIDByPredicateAndObject(const ID objectID, EntityIDBuffer *retBuffer, const uchar *startPtr, const int xyType) {
}

void PartitionMaster::findObjectIDAndSubjectIDByPredicate(EntityIDBuffer *retBuffer, const ID minID, const ID maxID, const uchar *startPtr,
		const int xyType) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << " partitionID: " << partitionID<< endl;
#endif

	if (minID == 0 && maxID == UINT_MAX) {
		findObjectIDAndSubjectIDByPredicate(retBuffer, startPtr, xyType);
		return;
	}

	register ID x, y;
	const uchar *limit, *reader, *chunkBegin = startPtr;

	retBuffer->setIDCount(2);
	retBuffer->setSortKey(0);

	ID & a = retBuffer->range[0].first;
	ID & b = retBuffer->range[0].second;
	ID & c = retBuffer->range[1].first;
	ID & d = retBuffer->range[1].second;
	
	if (xyType == 1) {
		MetaData *metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = chunkBegin + metaData->usedSpace;

		while (reader < limit) {
			reader = Chunk::readXYId(reader,x,y);
			if (x < minID)
				continue;
			else if (x <= maxID) {
				if(preLevel == 0 || patternBitmap[preVar][preLevel-1]->contains(x)){
					if(postLevel == 0||patternBitmap[postVar][postLevel-1]->contains(x+y)){
						patternBitmap[preVar][preLevel]->insert(x);
						patternBitmap[postVar][postLevel]->insert(x+y);
						retBuffer->insertID(x);
						retBuffer->insertID(x + y);
						a = min(a,x);
						b = max(b,x);
						c = min(c,x+y);
						d = max(d,x+y);
					}
				}
			} else
				return;
		}
		
	} else if (xyType == 2) {
		MetaData *metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = chunkBegin + metaData->usedSpace;

		while (reader < limit) {
			reader = Chunk::readXYId(reader,x,y);
			if (x + y < minID)
				continue;
			else if (x + y <= maxID) {
				if(preLevel == 0 || patternBitmap[preVar][preLevel-1]->contains(x+y)){
					if(postLevel == 0||patternBitmap[postVar][postLevel-1]->contains(x)){
						patternBitmap[preVar][preLevel]->insert(x+y);
						patternBitmap[postVar][postLevel]->insert(x);
						retBuffer->insertID(x+y);
						retBuffer->insertID(x);
						a = min(a,x+y);
						b = max(b,x+y);
						c = min(c,x);
						d = max(d,x);
					}
				}
			} else
				return;
		}
	}
}

void PartitionMaster::findObjectIDAndSubjectIDByPredicate(EntityIDBuffer *retBuffer, const uchar *startPtr, const int xyType) {
	register ID x, y;
	const uchar *reader, *limit, *chunkBegin = startPtr;

	retBuffer->setIDCount(2);
	retBuffer->setSortKey(0);

	ID & a = retBuffer->range[0].first;
	ID & b = retBuffer->range[0].second;
	ID & c = retBuffer->range[1].first;
	ID & d = retBuffer->range[1].second;

	if (xyType == 1) {
		MetaData *metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = reader + metaData->usedSpace;
		while (reader < limit) {
			reader = Chunk::readXYId(reader,x,y);
			if(preLevel == 0 || patternBitmap[preVar][preLevel-1]->contains(x)){
				if(postLevel == 0||patternBitmap[postVar][postLevel-1]->contains(x+y)){
					patternBitmap[preVar][preLevel]->insert(x);
					patternBitmap[postVar][postLevel]->insert(x+y);
					retBuffer->insertID(x);
					retBuffer->insertID(x + y);
					a = min(a,x);
					b = max(b,x);
					c = min(c,x+y);
					d = max(d,x+y);
				}
			}
		}
		
	} else if (xyType == 2) {
		MetaData *metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = reader + metaData->usedSpace;
		while (reader < limit) {
			reader = Chunk::readXYId(reader,x,y);
			if(preLevel == 0 || patternBitmap[preVar][preLevel-1]->contains(x+y)){
				if(postLevel == 0||patternBitmap[postVar][postLevel-1]->contains(x)){
					patternBitmap[preVar][preLevel]->insert(x+y);
					patternBitmap[postVar][postLevel]->insert(x);
					retBuffer->insertID(x+y);
					retBuffer->insertID(x);
					a = min(a,x+y);
					b = max(b,x+y);
					c = min(c,x);
					d = max(d,x);
				}
			}
		}
	}
}

void PartitionMaster::findSubjectIDAndObjectIDByPredicate(EntityIDBuffer *retBuffer, const ID minID, const ID maxID, const uchar *startPtr,
		const int xyType) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << " partitionID: " << partitionID<< endl;
#endif
	findObjectIDAndSubjectIDByPredicate(retBuffer, minID, maxID, startPtr, xyType);
}

void PartitionMaster::findSubjectIDAndObjectIDByPredicate(EntityIDBuffer *retBuffer, const uchar *startPtr, const int xyType) {
}

void PartitionMaster::findObjectIDByPredicate(EntityIDBuffer *retBuffer, const ID minID, const ID maxID, const uchar* startPtr, const int xyType) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << " partitionID: " << partitionID<< endl;
#endif
	if (minID == 0 && maxID == UINT_MAX) {
		findObjectIDByPredicate(retBuffer, startPtr, xyType);
		return;
	}
	retBuffer->setIDCount(1);
	retBuffer->setSortKey(0);

	register ID x, y;
	const uchar *reader, *limit, *chunkBegin = startPtr;

	ID & a = retBuffer->range[0].first;
	ID & b = retBuffer->range[0].second;
	ID & c = retBuffer->range[1].first;
	ID & d = retBuffer->range[1].second;
	
	if (xyType == 1) {
		MetaData* metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = chunkBegin + metaData->usedSpace;
		while (reader < limit) {
			//reader = Chunk::readXYId(reader,x,y);
			reader = Chunk::readYId(Chunk::readXId(reader, x), y);
			if (x < minID)
				continue;
			else if (x <= maxID){
				if(preLevel==0){
				    patternBitmap[preVar][preLevel]->insert(x);
					a = min(a,x);
					b = max(b,x);
				}else{
					if(patternBitmap[preVar][preLevel-1]->contains(x)){
						patternBitmap[preVar][preLevel]->insert(x);
						a = min(a,x);
						b = max(b,x);
						if(isSingle) 
							retBuffer->insertID(x);
					}
				}	
			}
			else
				return;
		}

	} else if (xyType == 2) {
		MetaData *metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = chunkBegin + metaData->usedSpace;
		while (reader < limit) {
			//reader = Chunk::readXYId(reader,x,y);
			reader = Chunk::readYId(Chunk::readXId(reader, x), y);
			if (x + y < minID)
				continue;
			else if (x + y <= maxID){
				if(preLevel==0){
				    patternBitmap[preVar][preLevel]->insert(x+y);
					a = min(a,x+y);
					b = max(b,x+y);
					// retBuffer->insertID(x+y);
				}else{
					if(patternBitmap[preVar][preLevel-1]->contains(x + y)){
						patternBitmap[preVar][preLevel]->insert(x + y);
						if(isSingle) retBuffer->insertID(x + y);
						a = min(a,x+y);
						b = max(b,x+y);
					}
				}
			}
			else
				return;
		}
	}
}

void PartitionMaster::findObjectIDByPredicate(EntityIDBuffer *retBuffer, const uchar *startPtr, const int xyType) {
	retBuffer->setIDCount(1);
	retBuffer->setSortKey(0);

	register ID x, y;
	const uchar *reader, *limit, *chunkBegin = startPtr;

	ID & a = retBuffer->range[0].first;
	ID & b = retBuffer->range[0].second;
	ID & c = retBuffer->range[1].first;
	ID & d = retBuffer->range[1].second;

	if (xyType == 1) {
		MetaData *metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData);
		limit = reader + metaData->usedSpace;
		while (reader < limit) {
			//reader = Chunk::readXYId(reader,x,y);
			reader = Chunk::readYId(Chunk::readXId(reader, x), y);
			if(preLevel==0){
			    patternBitmap[preVar][preLevel]->insert(x);
				a = min(a,x);
				b = max(b,x);
			}else{
				if(patternBitmap[preVar][preLevel-1]->contains(x)){
					patternBitmap[preVar][preLevel]->insert(x);
					a = min(a,x);
					b = max(b,x);
					if(isSingle) retBuffer->insertID(x);
				}
			}
		}
	
	} else if (xyType == 2) {
		MetaData *metaData = (MetaData*) chunkBegin;
		reader = chunkBegin + sizeof(MetaData*);
		limit = reader + metaData->usedSpace;
		while (reader < limit) {
			//reader = Chunk::readXYId(reader,x,y);
			reader = Chunk::readYId(Chunk::readXId(reader, x), y);
			if(preLevel==0){
				patternBitmap[preVar][preLevel]->insert(x+y);
				a = min(a,x+y);
				b = max(b,x+y);
			}else{
				if(patternBitmap[preVar][preLevel-1]->contains(x + y)){
					patternBitmap[preVar][preLevel]->insert(x + y);
					if(isSingle) retBuffer->insertID(x + y);
					a = min(a,x+y);
					b = max(b,x+y);
				}
			}
		}
	}
}

void PartitionMaster::findSubjectIDByPredicate(EntityIDBuffer *retBuffer, const ID minID, const ID maxID, const uchar *startPtr, const int xyType) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << " partitionID: " << partitionID<< endl;
#endif
	findObjectIDByPredicate(retBuffer, minID, maxID, startPtr, xyType);
}

void PartitionMaster::findSubjectIDByPredicate(EntityIDBuffer *retBuffer, const uchar *startPtr, const int xyType) {
#ifdef MYDEBUG
	cout << __FUNCTION__ << " partitionID: " << partitionID<< endl;
#endif
	findObjectIDByPredicate(retBuffer, startPtr, xyType);
}

