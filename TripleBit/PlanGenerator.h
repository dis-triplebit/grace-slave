/*
 * PlanGenerator.h
 *
 *  Created on: 2019-09-04
 *      Author: favoniankong
 */

#ifndef PLANGENERATOR_H_
#define PLANGENERATOR_H_

class IRepository;
class TripleBitQueryGraph;

#include "IRepository.h"
#include "TripleBitQueryGraph.h"
#include "TripleBit.h"


class PlanGenerator {
private:
	IRepository& repo;
	TripleBitQueryGraph::SubQuery* query;
	TripleBitQueryGraph* graph;
public:
	PlanGenerator(IRepository& _repo);
	Status generatePlan(TripleBitQueryGraph& _graph);
	virtual ~PlanGenerator();
	int	getSelectivity(TripleBitQueryGraph::TripleNodeID& tripleID);
	void sortJoinVariableNode(TripleBitQueryGraph::JoinVariableNode& node);
private:
	/// Generate the scan operator for the query pattern.
	Status generateScanOperator(TripleNode& node, TripleBitQueryGraph::JoinVariableNodeID varID, map<TripleBitQueryGraph::JoinVariableNodeID,int> & varRecord);
	Status generateSelectivity(TripleBitQueryGraph::JoinVariableNode& node, map<TripleBitQueryGraph::JoinVariableNodeID,int>& selectivityMap);
	TripleBitQueryGraph::JoinVariableNode::JoinType getJoinType(TripleBitQueryGraph::JoinVariableNode& node);
	Status bfsTraverseVariableNode();
	Status getAdjVariableByID(TripleBitQueryGraph::JoinVariableNodeID id, vector<TripleBitQueryGraph::JoinVariableNodeID>& nodes);
};

#endif /* PLANGENERATOR_H_ */
