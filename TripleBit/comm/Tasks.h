/*
 * Tasks.h
 *
 *  Created on: 2019-09-15
 *      Author: favoniankong
 */

#ifndef TASKS_H_
#define TASKS_H_

#include "Tools.h"
#include "../TripleBit.h"
#include "../TripleBitQueryGraph.h"
#include "subTaskPackage.h"
#include "IndexForTT.h"

class SubTrans : private Uncopyable{
public:
	struct timeval transTime;
	ID sourceWorkerID;
	ID minID;
	ID maxID;
	TripleBitQueryGraph::OpType operationType;
	size_t tripleNumber;
	TripleNode triple;
	boost::shared_ptr<IndexForTT> indexForTT;

	SubTrans(timeval &transtime, ID sWorkerID, ID miID, ID maID,
			TripleBitQueryGraph::OpType &opType, size_t triNumber,
			TripleNode &trip, boost::shared_ptr<IndexForTT> index_forTT) :
			transTime(transtime), sourceWorkerID(sWorkerID), minID(miID), maxID(
					maID), operationType(opType), tripleNumber(triNumber), triple(
					trip) , indexForTT(index_forTT){
	}
};

class ChunkTask{
public:
	struct ChunkTriple
	{
		ID subject, object;
		TripleNode::Op operation;
	};
	TripleBitQueryGraph::OpType operationType;
	ChunkTriple Triple;
	boost::shared_ptr<subTaskPackage> taskPackage;
	boost::shared_ptr<IndexForTT> indexForTT;

	ID minID, maxID;
	ChunkTask(TripleBitQueryGraph::OpType opType, ID subject, ID object, TripleNode::Op operation, boost::shared_ptr<subTaskPackage> task_Package, boost::shared_ptr<IndexForTT> index_ForTT):
		operationType(opType), Triple({subject, object, operation}), taskPackage(task_Package), indexForTT(index_ForTT){
	}
		
	ChunkTask(TripleBitQueryGraph::OpType opType, ID subject, ID object, TripleNode::Op operation, ID _minID,ID _maxID):
		operationType(opType), Triple({subject, object, operation}), minID(_minID),maxID(_maxID){
	}

	~ChunkTask(){}
};

#endif /* TASKS_H_ */
