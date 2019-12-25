/*
 * TripleBitWorker.h
 *
 *  Created on: 2013-6-28
 *      Author: root
 */

#ifndef TRIPLEBITWORKER_H_
#define TRIPLEBITWORKER_H_

class BitmapBuffer;
class URITable;
class PredicateTable;
class TripleBitRepository;
class TripleBitQueryGraph;
class EntityIDBuffer;
class HashJoin;
class SPARQLLexer;
class SPARQLParser;
class QuerySemanticAnalysis;
class PlanGenerator;
class transQueueSW;
class transaction;
class TasksQueueWP;
class TripleBitWorkerQuery;

#include "TripleBit.h"

class TripleBitWorker
{
public:
	vector<unsigned int> resultSet;
private:
	TripleBitRepository* tripleBitRepo;
	PredicateTable* preTable;
	URITable* uriTable;
	BitmapBuffer* bitmapBuffer;
	transQueueSW* transQueSW;
	TripleBitWorkerQuery* workerQuery;

	QuerySemanticAnalysis* semAnalysis;
	PlanGenerator* planGen;
	TripleBitQueryGraph* queryGraph;
	
	vector<string>::iterator resBegin;
	vector<string>::iterator resEnd;

	ID workerID;

	boost::mutex* uriMutex;

	
	transaction *trans;

public:
	TripleBitWorker(TripleBitRepository* repo, ID workID);
	Status Execute(string& queryString);
	~TripleBitWorker(){}
	void Work();
	void Print();
};

#endif /* TRIPLEBITWORKER_H_ */
