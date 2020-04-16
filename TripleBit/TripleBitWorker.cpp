/*
 * TripleBitWorker.cpp
 *
 *  Created on: 2013-6-28
 *      Author: root
 */

#include "SPARQLLexer.h"
#include "SPARQLParser.h"
#include "QuerySemanticAnalysis.h"
#include "PlanGenerator.h"
#include "TripleBitQueryGraph.h"
#include "MemoryBuffer.h"
#include "BitmapBuffer.h"
#include "TripleBitRepository.h"
#include "URITable.h"
#include "PredicateTable.h"
#include "EntityIDBuffer.h"
#include "util/BufferManager.h"
#include "TripleBitWorker.h"
#include "comm/TransQueueSW.h"
#include "TripleBitWorkerQuery.h"

#define MYDEBUG
#define TEST_QUERYTYPE
TripleBitWorker::TripleBitWorker(TripleBitRepository* repo, ID workID) {
	tripleBitRepo = repo;
	preTable = repo->getPredicateTable();
	uriTable = repo->getURITable();
	bitmapBuffer = repo->getBitmapBuffer();
	transQueSW = repo->getTransQueueSW();
	uriMutex = repo->getUriMutex();

	workerID = workID;

	queryGraph = new TripleBitQueryGraph();
	planGen = new PlanGenerator(*repo);
	semAnalysis = new QuerySemanticAnalysis(*repo);
	workerQuery = new TripleBitWorkerQuery(repo, workerID);
}

void TripleBitWorker::Work() {
	while (1) {
		trans = transQueSW->DeQueue();
#ifdef MYDEBUG
		cout << "transTime(sec): " << trans->transTime.tv_sec << endl;
		cout << "transTime(usec): " << trans->transTime.tv_usec << endl;
		cout << "transInfo: " << trans->transInfo << endl;
#endif
		string queryString = trans->transInfo;
		cout << "TripleBitWorker::Work test1" << endl;
		if(queryString == "exit"){
			delete trans;
			tripleBitRepo->workerComplete();
			break;
		}
		
		Execute(queryString);
		cout << "Back from TripleBitWorker::Work execute(quertString)" << endl;
		/*
		tripleBitRepo->ttForResult->completeOneTriple();
		cout<<"After ttForResult mark a query"<<endl;

		tripleBitRepo->ttForResult->wait();
		cout<<"After ttForResult wait"<<endl;

		cout<< "res.szie:"<< resultSet.size()<<endl;
		for(int i=0;i<resultSet.size();i++)
			cout<<resultSet[i]<<endl;
		cout<<"resultSet trace finished!"<<endl;

		tripleBitRepo->ttForResult->resumeReference();
		*/
		tripleBitRepo->ttForResult->completeOneTriple();
		delete trans;
		cout << "TripleBitWorker::Work test4" << endl;
	}
}

Status TripleBitWorker::Execute(string& queryString) {
#ifdef TOTAL_TIME
	struct timeval start, end;
	gettimeofday(&start, NULL);
#endif
	resultSet.resize(0);
	SPARQLLexer *lexer = new SPARQLLexer(queryString);
	SPARQLParser *parser = new SPARQLParser(*lexer);
	try {
		parser->parse();
	} catch (const SPARQLParser::ParserException& e) {
		cout << "Parser error: " << e.message << endl;
		return ERR;
	}

	if (parser->getOperationType() == SPARQLParser::QUERY) {
		queryGraph->Clear();
		uriMutex->lock();
		if (!this->semAnalysis->transform(*parser, *queryGraph)) {
			return NOT_FOUND;
		}
		uriMutex->unlock();

		if (queryGraph->knownEmpty() == true) {
			cout << "Empty result" << endl;
			resultSet.push_back(0);
			return OK;
		}

//		if (queryGraph->isPredicateConst() == false) {
//			resultSet.push_back("-1");
//			resultSet.push_back("predicate should be constant");
//			return ERR;
//		}

		cout << "---------------After transform----------------" << endl;
		Print();
		Status s = planGen->generatePlan(*queryGraph);
		cout << "---------------After GeneratePlan-------------" << endl;
		Print();
		
		if(s != NULL_RESULT){
			workerQuery->query(queryGraph, resultSet, trans->transTime);
		}

#ifdef TOTAL_TIME
		gettimeofday(&end, NULL);
		cout << " time elapsed: " << ((end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec) / 1000000.0 << " s" << endl;
#endif

		workerQuery->releaseBuffer();
	} else {
		queryGraph->Clear();

		uriMutex->lock();
		if (!this->semAnalysis->transform(*parser, *queryGraph)) {
			return ERR;
		}
		uriMutex->unlock();

#ifdef MYDEBUG
		Print();
#endif

		workerQuery->query(queryGraph, resultSet, trans->transTime);

#ifdef TOTAL_TIME
		gettimeofday(&end, NULL);
		cout << " time elapsed: " << ((end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec) / 1000000.0 << " s" << endl;
#endif

		workerQuery->releaseBuffer();
	}
	delete lexer;
	delete parser;
	return OK;
}

void TripleBitWorker::Print() {
	TripleBitQueryGraph::SubQuery& query = queryGraph->getQuery();
	unsigned int i, size, j;
	size = query.tripleNodes.size();
	cout << "join triple size: " << size << endl;

	vector<TripleNode>& triples = query.tripleNodes;
	for (i = 0; i < size; i++) {
		cout << i << " triple: " << endl;
		cout << triples[i].constSubject << " " << triples[i].subject << endl;
		cout << triples[i].constPredicate << " " << triples[i].predicate << endl;
		cout << triples[i].constObject << " " << triples[i].object << endl;
		cout << endl;
	}

	size = query.joinVariables.size();
	cout << "join variables size: " << size << endl;
	vector<TripleBitQueryGraph::JoinVariableNodeID>& variables = query.joinVariables;
	for (i = 0; i < size; i++) {
		cout << variables[i] << endl;
	}

	vector<TripleBitQueryGraph::JoinVariableNode>& nodes = query.joinVariableNodes;
	size = nodes.size();
	cout << "join variable nodes size: " << size << endl;
	for (i = 0; i < size; i++) {
		cout << i << "variable nodes" << endl;
		cout << nodes[i].value << endl;
		for (j = 0; j < nodes[i].appear_tpnodes.size(); j++) {
			cout << nodes[i].appear_tpnodes[j].first << " " << nodes[i].appear_tpnodes[j].second << endl;
		}
		cout << endl;
	}

	size = query.joinVariableEdges.size();
	cout << "join variable edges size: " << size << endl;
	vector<TripleBitQueryGraph::JoinVariableNodesEdge>& edge = query.joinVariableEdges;
	for (i = 0; i < size; i++) {
		cout << i << " edge" << endl;
		cout << "From: " << edge[i].from << "To: " << edge[i].to << endl;
	}

	cout << " query type: " << query.joinGraph << endl;
	cout << " root ID: " << query.rootID << endl;
	cout << " query leafs: ";
	size = query.leafNodes.size();
	for (i = 0; i < size; i++) {
		cout << query.leafNodes[i] << " ";
	}
	cout << endl;

	vector<ID>& projection = queryGraph->getProjection();
	cout << "variables need to project: " << endl;
	cout << "variable count: " << projection.size() << endl;
	for (i = 0; i < projection.size(); i++) {
		cout << projection[i] << endl;
	}
	cout << endl;
}
