/*
 * PartitionMaster.h
 *
 *  Created on: 2013-8-19
 *      Author: root
 */

#ifndef PARTITIONMASTER_H_
#define PARTITIONMASTER_H_

class BitmapBuffer;
class TripleBitRepository;
class EntityIDBuffer;
class Chunk;
class TasksQueueWP;
class SubTrans;
class ChunkManager;
class ResultBuffer;
class TempBuffer;
class PartitionBufferManager;
class TasksQueueChunk;
class subTaskPackage;
class IndexForTT;
class ChunkTask;

#include "TripleBit.h"
#include "TripleBitQueryGraph.h"
#include <boost/thread/thread.hpp>
using namespace boost;

class PartitionMaster {
private:
	TripleBitRepository *tripleBitRepo;
	BitmapBuffer *bitmap;

	int workerNum;
	int partitionNum;

	TasksQueueWP *tasksQueue;
	map<ID, ResultBuffer*> resultBuffer;

	ID partitionID;
	ChunkManager *partitionChunkManager[2];

	//something used to do update
//	TempBuffer *tempBuffer[2];
//	MMapBuffer *tempMMapBuffer;
//	size_t usedPage;

	map<ID, TasksQueueChunk*> xChunkQueue[2];
	map<ID, TasksQueueChunk*> xyChunkQueue[2];
	map<ID, TempBuffer*> xChunkTempBuffer[2];
	map<ID, TempBuffer*> xyChunkTempBuffer[2];
	unsigned xChunkNumber[2];
	unsigned xyChunkNumber[2];
	PartitionBufferManager *partitionBufferManager;

public:
	PartitionMaster(TripleBitRepository *&repo, const ID parID);
	virtual ~PartitionMaster();
	void Work();
	void endupdate();
	unsigned char getLen(ID id) {
		unsigned char len = 0;
		while (id >= 128) {
			++len;
			id >>= 7;
		}
		return len + 1;
	}

private:
	void doChunkQuery(pair<ChunkTask*,TasksQueueChunk*> &task);
	void doChunkGroupQuery(vector<pair<ChunkTask*,TasksQueueChunk*> > & tasks,int begin,int end);

	void taskEnQueue(ChunkTask *chunkTask, TasksQueueChunk *tasksQueue);

	void combineTempBufferToSource(TempBuffer *buffer, const uchar* startPtr, const ID chunkID, const int xyType, const int soType);
	void readIDInTempPage(const uchar *&currentPtrTemp, const uchar *&endPtrTemp, const uchar *&startPtrTemp, char *&tempPage, char *&tempPage2,
			bool &theOtherPageEmpty, bool &isInTempPage);
	void handleEndofChunk(const uchar *startPtr, uchar *&chunkBegin, uchar *&currentPtrChunk, uchar *&endPtrChunk, const uchar *&startPtrTemp,
			char *&tempPage, char *&tempPage2, bool &isInTempPage, bool &theOtherPageEmpty, ID minID, const int xyType, const ID chunkID);

	void executeQuery(SubTrans *subTransaction);
	void executeInsertData(SubTrans *subTransaction);
	void executeDeleteData(SubTrans *subTransaction);
	void executeDeleteClause(SubTrans *subTransaction);
	void executeUpdate(SubTrans *subTransfirst, SubTrans *subTranssecond);

	void deleteDataForDeleteClause(EntityIDBuffer *buffer, const ID deleteID, const int soType);
	void updateDataForUpdate(EntityIDBuffer *buffer, const ID deleteID, const ID updateID, const int soType);

	void handleTasksQueueChunk(TasksQueueChunk *tasksQueue);
	void executeChunkTaskQuery(ChunkTask *chunkTask, const ID chunkID, const uchar* chunkBegin, const int xyType);
	void executeChunkTaskInsertData(ChunkTask *chunkTask, const ID chunkID, const uchar *startPtr, const int xyType, const int soType);
	void executeChunkTaskDeleteData(ChunkTask *chunkTask, const ID chunkID, const uchar *startPtr, const int xyType, const int soType);
	void executeChunkTaskDeleteClause(ChunkTask *chunkTask, const ID chunkID, const uchar *startPtr, const int xyType, const int soType);
	void executeChunkTaskUpdate(ChunkTask *chunkTask, const ID chunkID, const uchar *startPtr, const int xyType, const int soType);

	void findSubjectIDByPredicate(EntityIDBuffer *retBuffer, const ID minID, const ID maxID, const uchar *startPtr, const int xyType);
	void findSubjectIDByPredicate(EntityIDBuffer *retBuffer, const uchar *startPtr, const int xyType);
	void findObjectIDByPredicate(EntityIDBuffer *retBuffer, const ID minID, const ID maxID, const uchar *startPtr, const int xyType);
	void findObjectIDByPredicate(EntityIDBuffer *retBuffer, const uchar *startPtr, const int xyType);
	void findSubjectIDAndObjectIDByPredicate(EntityIDBuffer *retBuffer, const ID minID, const ID maxID, const uchar *startPtr, const int xyType);
	void findSubjectIDAndObjectIDByPredicate(EntityIDBuffer *retBuffer, const uchar *startPtr, const int xyType);
	void findObjectIDAndSubjectIDByPredicate(EntityIDBuffer *retBuffer, const ID minID, const ID maxID, const uchar *startPtr, const int xyType);
	void findObjectIDAndSubjectIDByPredicate(EntityIDBuffer *retBuffer, const uchar *startPtr, const int xyType);
	void findObjectIDByPredicateAndSubject(const ID subject, EntityIDBuffer *retBuffer, const ID minID, const ID maxID, const uchar* startPtr,
			const int xyType);
	void findSubjectIDByPredicateAndObject(const ID object, EntityIDBuffer *retBuffer, const ID minID, const ID maxID, const uchar* startPtr,
			const int xyType);
	void findObjectIDByPredicateAndSubject(const ID subject, EntityIDBuffer *retBuffer, const uchar* startPtr, const int xyType);
	void findSubjectIDByPredicateAndObject(const ID object, EntityIDBuffer *retBuffer, const uchar* startPtr, const int xyType);

};

#endif /* PARTITIONMASTER_H_ */
