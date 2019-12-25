/*
 * TripleBitWorkerQuery.h
 *
 *  Created on: 2019-10-10
 *      Author: favoniankong
 */

#ifndef TRIPLEBITWORKERQUERY_H_
#define TRIPLEBITWORKERQUERY_H_

class BitmapBuffer;
class URITable;
class PredicateTable;
class TripleBitRepository;
class TripleBitQueryGraph;
class EntityIDBuffer;
class HashJoin;
class TasksQueueWP;
class ResultBuffer;
class PartitionThreadPool;
class IndexForTT;
class SubTrans;

#include "TripleBitQueryGraph.h"
#include "TripleBit.h"
#include "util/HashJoin.h"
#include "util/SortMergeJoin.h"
#include <boost/thread/mutex.hpp>

typedef map<ID, EntityIDBuffer*> EntityIDListType;
typedef map<ID, EntityIDBuffer*>::iterator EntityIDListIterType;

class TripleBitWorkerQuery
{
private:
	TripleBitRepository* tripleBitRepo;
	BitmapBuffer* bitmap;
	URITable* uriTable;
	PredicateTable* preTable;

	TripleBitQueryGraph* _queryGraph;
	TripleBitQueryGraph::SubQuery* _query;

	EntityIDListType EntityIDList;
	vector<TripleBitQueryGraph::JoinVariableNodeID> idTreeBFS;
	vector<TripleBitQueryGraph::JoinVariableNodeID> leafNode;
	vector<TripleBitQueryGraph::JoinVariableNodeID> varVec;

	ID workerID;
	ID max_id;

	//something about shared memory
	int partitionNum;
	vector<TasksQueueWP*> tasksQueueWP;
	vector<ResultBuffer*> resultWP;
	vector<boost::mutex*> tasksQueueWPMutex;

	map<ID, TasksQueueWP*> tasksQueue;
	map<ID, ResultBuffer*> resultBuffer;

	//used to classity the InsertData or DeleteData
	map<ID, set<TripleNode*> > tripleNodeMap;

	//used to get the results
	vector<int> varPos;
	vector<int> keyPos;
	vector<int> resultPos;
	vector<int> verifyPos;
	vector<ID> resultVec;
	vector<size_t> bufPreIndexs;
	bool needselect;

	HashJoin hashJoin;
	SortMergeJoin mergeJoin;

	timeval *transactionTime;
	
public:
	vector<unsigned int>* resultPtr;

public:
	TripleBitWorkerQuery(TripleBitRepository*& repo, ID workID);
	~TripleBitWorkerQuery();

	void releaseBuffer();
	Status query(TripleBitQueryGraph* queryGraph, vector<unsigned int>& resultSet, timeval& transTime);

private:
	void tasksEnQueue(ID partitionID, SubTrans *subTrans);
	Status excuteQuery();
	Status excuteInsertData();
	Status excuteDeleteData();
	Status excuteDeleteClause();
	Status excuteUpdate();
	Status predicateJoin();
	Status centerJoin();
	Status linerJoin();
	Status treeShapeJoin();
	Status findEntitiesAndJoin(TripleBitQueryGraph::JoinVariableNodeID id,
			vector<pair<TripleBitQueryGraph::TripleNodeID, TripleBitQueryGraph::JoinVariableNode::DimType> >& tpnodes,
			bool firstTime);
	Status findEntitiesAndJoinFirstTime(
			vector<pair<TripleBitQueryGraph::TripleNodeID, TripleBitQueryGraph::JoinVariableNode::DimType> >& tpnodes,
			ID tripleID, map<ID, bool>& firstInsertFlag, TripleBitQueryGraph::JoinVariableNodeID id);
	Status modifyEntitiesAndJoin(
			vector<pair<TripleBitQueryGraph::TripleNodeID, TripleBitQueryGraph::JoinVariableNode::DimType> >& tpnodes,
			ID tripleID, TripleBitQueryGraph::JoinVariableNodeID id);
	Status getTripleNodeByID(TripleNode*& triple, TripleBitQueryGraph::TripleNodeID nodeID);
	Status getVariableNodeByID(TripleBitQueryGraph::JoinVariableNode*& node,
			TripleBitQueryGraph::JoinVariableNodeID id);
	int getVariableCount(TripleBitQueryGraph::TripleNodeID id);
	int getVariableCount(TripleNode* triple);
	bool nodeIsLeaf(TripleBitQueryGraph::JoinVariableNodeID varID);
	Status sortEntityBuffer(vector<TripleBitQueryGraph::JoinVariableNode::JoinType>& joinVec,
			vector<TripleBitQueryGraph::TripleNodeID>& tripleVec);
	int getVariablePos(EntityType type, TripleNode* triple);
	ResultIDBuffer* findEntityIDByTriple(TripleNode* triple, ID minID, ID maxID);
	int getVariablePos(TripleBitQueryGraph::JoinVariableNodeID id, TripleBitQueryGraph::TripleNodeID tripleID);
	int getVariablePos(TripleBitQueryGraph::JoinVariableNodeID id, TripleNode* triple);

	bool getResult(ID key, std::vector<EntityIDBuffer*>& bufferlist, size_t buf_index);
	void getResult_join(ID key, std::vector<EntityIDBuffer*>& bufferlist, size_t buf_index);

	void classifyTripleNode();
};

#endif /* TRIPLEBITWORKERQUERY_H_ */
