/*
 * PlanGenerator.cpp
 *
 *  Created on: 2010-5-12
 *      Author: liupu
 */

#include "PlanGenerator.h"
#include "TripleBitRepository.h"
#include "ThreadPool.h"

#include <vector>
#include <map>
#include <queue>
using namespace std;

#define DEBUGPLAN

bool isUnused(const TripleBitQueryGraph::SubQuery& query,const TripleNode& node,unsigned val)
// Check if a variable is unused outside its primary pattern
{
	for (vector<TripleBitQueryGraph::Filter>::const_iterator iter=query.filters.begin(),limit=query.filters.end();iter!=limit;++iter)
		if ((*iter).id==val)
			return false;
	for (vector<TripleNode>::const_iterator iter=query.tripleNodes.begin(),limit=query.tripleNodes.end();iter!=limit;++iter) {
		const TripleNode& n=*iter;
		if ((&n)==(&node))
			continue;
		if ((!n.constSubject)&&(val==n.subject)) return false;
		if ((!n.constPredicate)&&(val==n.predicate)) return false;
		if ((!n.constObject)&&(val==n.object)) return false;
	}
	for (vector<TripleBitQueryGraph::SubQuery>::const_iterator iter=query.optional.begin(),limit=query.optional.end();iter!=limit;++iter)
		if (!isUnused(*iter,node,val))
			return false;
	for (vector<vector<TripleBitQueryGraph::SubQuery> >::const_iterator iter=query.unions.begin(),limit=query.unions.end();iter!=limit;++iter)
		for (vector<TripleBitQueryGraph::SubQuery>::const_iterator iter2=(*iter).begin(),limit2=(*iter).end();iter2!=limit2;++iter2)
			if (!isUnused(*iter2,node,val))
				return false;
	return true;
}

static bool isUnused(const TripleBitQueryGraph& graph, const TripleNode& node, unsigned val)
{
	// check if a variable is unused outside its primary pattern.
	for ( TripleBitQueryGraph::projection_iterator begin = graph.projectionBegin(), limit = graph.projectionEnd(); begin != limit; begin++ )
	{
		if ( *begin == val)
			return false;
	}

	return isUnused(graph.getQuery(), node, val);
}

PlanGenerator::PlanGenerator(IRepository& _repo):repo(_repo) {
	// TODO Auto-generated constructor stub
}

PlanGenerator::~PlanGenerator() {
	// TODO Auto-generated destructor stub
}

Status PlanGenerator::generatePlan(TripleBitQueryGraph& _graph)
{
	TripleBitQueryGraph::SubQuery& _query = _graph.getQuery();
	graph = &_graph;
	query = &_query;
	bool setted = false;
	int selectivity = INT_MAX;
	int minSeleID = 0;

	//for predicate unknown
	// generate the selectivity for patterns and join variables;

	//cout<<"xxxxxxxxxxxxxxxxxxx begin of fliter!"<<endl;
	for(vector<TripleNode>::iterator iter = query->tripleNodes.begin(); iter != query->tripleNodes.end(); ) {
		int selectivity=getSelectivity(iter->tripleNodeID);
		//cout<< "Pattern size: " << query->tripleNodes.size()<<endl;
		//if(getSelectivity(iter->tripleNodeID) == 0)
		//	iter = query->tripleNodes.erase(iter);			
		
		
#ifdef DEBUGPLAN
		cout << "TripleNodeID: " << iter->tripleNodeID << " Selectivity: " << selectivity << endl;
#endif	
		iter++;
		if(selectivity==0){
			return NULL_RESULT;
		}
	}

	//cout<<"xxxxxxxxxxxxxxxxxxx end of fliter!"<<endl;
	
	vector<TripleBitQueryGraph::JoinVariableNode>::iterator joinVariableIter = query->joinVariableNodes.begin();
	map<TripleBitQueryGraph::JoinVariableNodeID,int> selectivityMap;
	for(; joinVariableIter != query->joinVariableNodes.end(); joinVariableIter++)
	{
		generateSelectivity(*joinVariableIter,selectivityMap);
	}

	TripleBitQueryGraph::JoinVariableNode::JoinType joinType;
	joinVariableIter = query->joinVariableNodes.begin();
	for(; joinVariableIter != query->joinVariableNodes.end(); joinVariableIter++)
	{
		joinType = getJoinType(*joinVariableIter);

		if(joinType == TripleBitQueryGraph::JoinVariableNode::SP || joinType == TripleBitQueryGraph::JoinVariableNode::OP){
			query->rootID = joinVariableIter->value;
			setted = true;
			break;
		}
#ifdef DEBUGPLAN
		cout << "join var:" << joinVariableIter->value << " select:" << selectivityMap[joinVariableIter->value] << endl;
#endif
		if(selectivityMap[joinVariableIter->value] < selectivity){
			selectivity = selectivityMap[joinVariableIter->value];
			minSeleID = joinVariableIter->value;
		}
	}

	// if the root id has not been set, the set the root node min selectivity node.
	if(setted == false){
		query->rootID = minSeleID;
	}

	bfsTraverseVariableNode();

	//pre plan
	/*
	/// generate the patterns' scan operator.
	vector<bool> tnodes;
	tnodes.resize(query->tripleNodes.size() + 1, false);
	for(size_t i = 0; i != query->joinVariables.size(); i++) {
		size_t j, k;
		for(j = 0; j != query->joinVariableNodes.size(); j++) {
			if(query->joinVariableNodes[j].value == query->joinVariables[i])
				break;
		}
		TripleBitQueryGraph::JoinVariableNodeID varid = query->joinVariables[i];
		TripleBitQueryGraph::JoinVariableNode& jnode = query->joinVariableNodes[j];
		for(size_t j = 0; j != jnode.appear_tpnodes.size(); j++) {
			TripleBitQueryGraph::TripleNodeID tid = jnode.appear_tpnodes[j].first;
			if(tnodes[tid] == true)
				continue;
			tnodes[tid] = true;
			for(k = 0; k != query->tripleNodes.size(); k++) {
				if(query->tripleNodes[k].tripleNodeID == tid)
					break;
			}
			TripleNode& tnode = query->tripleNodes[k];
			generateScanOperator(tnode, varid);
		}
	}

	query->selectivityMap.clear();
	map<TripleBitQueryGraph::JoinVariableNodeID,int>::iterator iter = selectivityMap.begin();
	for(; iter != selectivityMap.end(); iter++)
	{
		query->selectivityMap[iter->first] = iter->second;
	}
	return OK;
	*/
	
	// level plan
	for(vector<TripleNode>::iterator iter = query->tripleNodes.begin(), limit = query->tripleNodes.end(); iter != limit; iter++) {
		cout << "TripleNodeID: " << iter->tripleNodeID << " Selectivity: " << getSelectivity(iter->tripleNodeID) << endl;
	}

	joinVariableIter = query->joinVariableNodes.begin();
	map<TripleBitQueryGraph::JoinVariableNodeID,int> varRecord;
	for(; joinVariableIter != query->joinVariableNodes.end(); joinVariableIter++)
	{
		varRecord[joinVariableIter->value] = 0;
	}

	// generate the patterns' scan operator.
	vector<bool> tnodes;
	tnodes.resize(query->tripleNodes.size() + 1, false);
	std::vector<TripleNode> tripleNodes_shuffle;
	for(size_t i = 0; i != query->joinVariables.size(); i++) {
		size_t j, k;
		for(j = 0; j != query->joinVariableNodes.size(); j++) {
			if(query->joinVariableNodes[j].value == query->joinVariables[i])
				break;
		}
		TripleBitQueryGraph::JoinVariableNodeID varid = query->joinVariables[i];
		TripleBitQueryGraph::JoinVariableNode& jnode = query->joinVariableNodes[j];
		for(size_t j = 0; j != jnode.appear_tpnodes.size(); j++) {
			TripleBitQueryGraph::TripleNodeID tid = jnode.appear_tpnodes[j].first;
			if(tnodes[tid] == true)
				continue;
			tnodes[tid] = true;
			for(k = 0; k != query->tripleNodes.size(); k++) {
				if(query->tripleNodes[k].tripleNodeID == tid)
					break;
			}
			TripleNode& tnode = query->tripleNodes[k];
			generateScanOperator(tnode, varid,varRecord);
			tripleNodes_shuffle.push_back(query->tripleNodes[k]);
			cout << "TripleNodeID: " << tnode.tripleNodeID << endl;
			cout << "scanOp: "<< tnode.scanOperation <<endl;
			cout << "varHeight "<<endl;
			for(int i = 0;i < tnode.varLevel.size() ;i++){
				pair<int,int> &x = tnode.varLevel[i];
				cout<<x.first<<" "<<x.second<<endl;
			}
			cout<<endl;
		}
	}

	query->selectivityMap.clear();
	map<TripleBitQueryGraph::JoinVariableNodeID,int>::iterator iter = selectivityMap.begin();
	for(; iter != selectivityMap.end(); iter++)
	{
		query->selectivityMap[iter->first] = iter->second;
	}

	//tripleNodes根据选择度初次排序，不在是analyse之后的tripleNode顺序了。
	query->tripleNodes = tripleNodes_shuffle;
	return OK;
	
}

Status PlanGenerator::generateScanOperator(TripleNode& node, TripleBitQueryGraph::JoinVariableNodeID varID, map<TripleBitQueryGraph::JoinVariableNodeID,int> & varRecord)
{
	bool subjectUnused, predicateUnused, objectUnused;

	subjectUnused = (!node.constSubject) && isUnused(*graph, node, node.subject);
	predicateUnused = (!node.constPredicate) && isUnused(*graph, node, node.predicate);
	objectUnused = (!node.constObject) && isUnused(*graph, node, node.object);

	unsigned unusedSum = subjectUnused + predicateUnused + objectUnused;
	unsigned variableCnt = (!node.constObject) + (!node.constPredicate) + (!node.constSubject);

	if (variableCnt == unusedSum) {
		node.scanOperation = TripleNode::NOOP;
		return OK;
	}

	if (unusedSum == 3) {
		// the query pattern content is all variables; but they are not used in other patterns
	} else if (unusedSum == 2) {
		if (!subjectUnused)
			node.scanOperation = TripleNode::FINDS;
		if (!predicateUnused)
			node.scanOperation = TripleNode::FINDP;
		if (!objectUnused)
			node.scanOperation = TripleNode::FINDO;
	} else if (unusedSum == 1) {
		if (subjectUnused) {
			if (node.constObject)
				node.scanOperation = TripleNode::FINDPBYO;
			else{
				node.scanOperation = TripleNode::FINDOBYP;
				node.varLevel.push_back(make_pair(node.object,varRecord[node.object]++));
			}
		} else if (predicateUnused) {
			if (node.constObject)
				node.scanOperation = TripleNode::FINDSBYO;
			else
				node.scanOperation = TripleNode::FINDOBYS;
		} else {
			if (node.constPredicate){
				node.scanOperation = TripleNode::FINDSBYP;
				node.varLevel.push_back(make_pair(node.subject,varRecord[node.subject]++));
			}
			else
				node.scanOperation = TripleNode::FINDPBYS;
		}
	} else {
		if (variableCnt == 2) {
			if (node.constSubject) {
				if(node.predicate == varID)
					node.scanOperation = TripleNode::FINDPOBYS;
				else
					node.scanOperation = TripleNode::FINDOPBYS;
			} else if (node.constPredicate) {
				if(node.subject == varID){
					node.scanOperation = TripleNode::FINDSOBYP;
					node.varLevel.push_back(make_pair(node.subject,varRecord[node.subject]++));
					node.varLevel.push_back(make_pair(node.object,varRecord[node.object]++));	
				}
				else{
					node.scanOperation = TripleNode::FINDOSBYP;
					node.varLevel.push_back(make_pair(node.object,varRecord[node.object]++));
					node.varLevel.push_back(make_pair(node.subject,varRecord[node.subject]++));	
				}
			} else if (node.constObject) {
				if(node.subject == varID)
					node.scanOperation = TripleNode::FINDSPBYO;
				else
					node.scanOperation = TripleNode::FINDPSBYO;
			}
		} else if (variableCnt == 1) {
			if (!node.constSubject){
				node.scanOperation = TripleNode::FINDSBYPO;
				node.varLevel.push_back(make_pair(node.subject,varRecord[node.subject]++));
			}
			if (!node.constPredicate)
				node.scanOperation = TripleNode::FINDPBYSO;
			if (!node.constObject){
				node.scanOperation = TripleNode::FINDOBYSP;
				node.varLevel.push_back(make_pair(node.object,varRecord[node.object]++));
			}
		} else {
			/// all are variable,
			node.scanOperation = TripleNode::NOOP;
		}
	}
	return OK;
}

TripleBitQueryGraph::JoinVariableNode::JoinType PlanGenerator::getJoinType(TripleBitQueryGraph::JoinVariableNode& node)
{
	vector<std::pair<TripleBitQueryGraph::TripleNodeID, TripleBitQueryGraph::JoinVariableNode::DimType> >::iterator iter = node.appear_tpnodes.begin();

	char dim = 0;
	for(; iter != node.appear_tpnodes.end(); iter++)
	{
		dim = dim | iter->second;
	}

	switch(dim)
	{
	case 1:
		return TripleBitQueryGraph::JoinVariableNode::SS;
		break;
	case 4:
		return TripleBitQueryGraph::JoinVariableNode::OO;
		break;
	case 2:
		return TripleBitQueryGraph::JoinVariableNode::PP;
		break;
	case 5:
		return TripleBitQueryGraph::JoinVariableNode::SO;
		break;
	case 3:
		return TripleBitQueryGraph::JoinVariableNode::SP;
		break;
	case 6:
		return TripleBitQueryGraph::JoinVariableNode::OP;
		break;
	}

	return TripleBitQueryGraph::JoinVariableNode::UNKNOWN;
}

void PlanGenerator::sortJoinVariableNode(TripleBitQueryGraph::JoinVariableNode& node)
{
	size_t sizeNode = node.appear_tpnodes.size();
	size_t i, j;
	pair<TripleBitQueryGraph::TripleNodeID, TripleBitQueryGraph::JoinVariableNode::DimType> temp;
	for(i = 0; i < sizeNode - 1; ++i)
	{
		for(j = 0; j < sizeNode - 1 - i; ++j)
		{
			if(getSelectivity(node.appear_tpnodes[j + 1].first) < getSelectivity(node.appear_tpnodes[j].first))
			{
				temp = node.appear_tpnodes[j + 1];
				node.appear_tpnodes[j + 1] = node.appear_tpnodes[j];
				node.appear_tpnodes[j] = temp;
			}
		}
	}
}

Status PlanGenerator::generateSelectivity(TripleBitQueryGraph::JoinVariableNode& node, map<TripleBitQueryGraph::JoinVariableNodeID,int>& selectivityMap)
{
	TripleBitQueryGraph::TripleNodeID selNodeID;
	sortJoinVariableNode(node);

#ifdef DEBUGPLAN
	size_t sizeNode = node.appear_tpnodes.size();
	cout << "node value:" << node.value << endl;
	for(size_t i = 0; i < sizeNode; ++i){
		cout << "TripleNodeID: " << node.appear_tpnodes[i].first << " Selectivity: " << getSelectivity(node.appear_tpnodes[i].first);
	}
#endif

	selNodeID = node.appear_tpnodes[0].first;
	vector<TripleNode>::iterator iter = query->tripleNodes.begin(), limit = query->tripleNodes.end();
	for(; iter != limit; iter++) {
		if(iter->tripleNodeID == selNodeID)
			break;
	}

	selectivityMap[node.value] = iter->selectivity;
#ifdef DEBUGPLAN
	cout << "node value:" << node.value << " select:" << iter->selectivity << endl;
#endif

	return OK;
}

int PlanGenerator::getSelectivity(TripleBitQueryGraph::TripleNodeID& tripleID)
{
	vector<TripleNode>::iterator iter = query->tripleNodes.begin();
	for(; iter != query->tripleNodes.end(); iter++)
	{
		if(iter->tripleNodeID == tripleID)
			break;
	}

	if(iter->selectivity != -1) {
		return iter->selectivity;
	}

	int selectivity = INT_MAX;

	if(iter->constPredicate){
		if ( iter->constObject) {
			if (iter->constSubject)
				selectivity = 1;
			else selectivity = repo.get_object_predicate_count(iter->object, iter->predicate);
		} else {
			if (iter->constSubject)
				selectivity = repo.get_subject_predicate_count(iter->subject, iter->predicate);
			else selectivity = repo.get_predicate_count(iter->predicate);
		}
	} else {
		if ( iter->constObject) {
			if (iter->constSubject)
				selectivity = repo.get_subject_object_count(iter->subject, iter->object);
			else selectivity = repo.get_object_count(iter->object);
		} else {
			if (iter->constSubject)
				selectivity = repo.get_subject_count(iter->subject);
			else selectivity = TripleBitRepository::colNo;
		}
	}
	iter->selectivity = selectivity;
	return selectivity;
}


Status PlanGenerator::bfsTraverseVariableNode()
{
	vector<bool> visited;
	vector<TripleBitQueryGraph::JoinVariableNodeID> temp;
	vector<TripleBitQueryGraph::JoinVariableNodeID> leafNode;
	queue<TripleBitQueryGraph::JoinVariableNodeID> idQueue;
	queue<TripleBitQueryGraph::JoinVariableNodeID> parentQueue;
	vector<TripleBitQueryGraph::JoinVariableNodeID>::iterator iter;
	vector<TripleBitQueryGraph::JoinVariableNodeID> adjNode;
	TripleBitQueryGraph::JoinVariableNodeID tempID, parentID;
	Status s;

	visited.resize(query->joinVariableNodes.size());
	unsigned int i,size = query->joinVariables.size(),j;
	int pos;
	bool flag;

	// find the root node's position.
	iter = find(query->joinVariables.begin(),query->joinVariables.end(),query->rootID);
	// exchange the root node's id with the first node's id.
	tempID = *iter;
	*iter = query->joinVariables[0];
	query->joinVariables[0] = tempID;

	query->joinGraph = TripleBitQueryGraph::ACYCLIC;

	// breadth first traverse the join variable.
	for( i = 0; i != size; i++)
		visited[i] = false;

	for( i = 0; i != size; i++)
	{
		if(!visited[i]){
			visited[i] = true;
			temp.push_back(query->joinVariables[i]);
			idQueue.push(query->joinVariables[i]);
			parentQueue.push(0);
			while(!idQueue.empty()){
				flag = false;
				tempID = idQueue.front();
				idQueue.pop();

				parentID = parentQueue.front();
				parentQueue.pop();

				// get the adj. variable nodes
				s = getAdjVariableByID(tempID,adjNode);
				if( s != OK)
					continue;

				for( j = 0; j < adjNode.size(); j++)
				{
					// get the variable position in the vector.
					iter = find(query->joinVariables.begin(),query->joinVariables.end(),adjNode[j]);
					pos = iter - query->joinVariables.begin();
					if( !visited[pos]){
						visited[pos] = true;
						temp.push_back(*iter);
						idQueue.push(*iter);
						flag = true;
						parentQueue.push(tempID);
					}else{
						if(adjNode[j] != parentID){
							query->joinGraph = TripleBitQueryGraph::CYCLIC;
						}
					}
				}

				//
				if(flag == false)
				{
					leafNode.push_back(tempID);
				}
			}
		}
	}

	query->joinVariables.assign(temp.begin(),temp.end());
	query->leafNodes.assign(leafNode.begin(),leafNode.end());
	return OK;
}

Status PlanGenerator::getAdjVariableByID(TripleBitQueryGraph::JoinVariableNodeID id, vector<TripleBitQueryGraph::JoinVariableNodeID>& nodes)
{
	nodes.clear();
	vector<TripleBitQueryGraph::JoinVariableNodesEdge>::iterator iter = query->joinVariableEdges.begin();
	for (; iter != query->joinVariableEdges.end(); iter++) {
		if (iter->to == id) {
			nodes.push_back(iter->from);
		}
		if(iter->from == id){
			nodes.push_back(iter->to);
		}
	}

	if (nodes.size() == 0)
		return ERR;

	return OK;
}
