/*
 * modify from rdf-3x source code.
 * TripleBitQueryGraph.cpp
 *
 *  Created on: 2010-5-10
 *      Author: dlwu
 */

#include "TripleBitQueryGraph.h"
#include <set>
using namespace std;

//构造函数，默认为谓词已知
TripleBitQueryGraph::SubQuery::SubQuery()
{
    joinType=TripleBitQueryGraph::INTERSECTION;
}

//
TripleBitQueryGraph::TripleNodesEdge::TripleNodesEdge(TripleNodeID from, TripleNodeID to, const std::vector<ID>& common)
        : from(from),to(to),common(common)
// Constructor
{
}

//
TripleBitQueryGraph::TripleNodesEdge::~TripleNodesEdge()
//Deconstructor
{

}

//TODO
bool TripleBitQueryGraph::JoinVariableNode::hasEdge(const TripleBitQueryGraph::JoinVariableNode& other)const
{
    return false;
}

//
TripleBitQueryGraph::TripleBitQueryGraph(): duplicateHandling(AllDuplicates),limit(~0u),knownEmptyResult(false)
//
{
}

//
TripleBitQueryGraph::~TripleBitQueryGraph()
{
}

void TripleBitQueryGraph::clear()
// clear the QueryGraph
{
    query=SubQuery();
    duplicateHandling=AllDuplicates;
    knownEmptyResult=false;
}

void TripleBitQueryGraph::Clear()
{
    query = SubQuery();
    projection.clear();
}

//---------------------------------------------------------------------------
static bool intersects(const set<unsigned int>& a,const set<unsigned int>& b,vector<ID>& common)
// Check if two sets overlap
{
    common.clear();
    set<unsigned int>::const_iterator ia,la,ib,lb;
    if (a.size()<b.size()) {
        if (a.empty())
            return false;
        ia=a.begin(); la=a.end();
        ib=b.lower_bound(*ia); lb=b.end();
    } else {
        if (b.empty())
            return false;
        ib=b.begin(); lb=b.end();
        ia=a.lower_bound(*ib); la=a.end();
    }
    bool result=false;
    while ((ia!=la)&&(ib!=lb)) {
        unsigned va=*ia,vb=*ib;
        if (va<vb) {
            ++ia;
        } else if (va>vb) {
            ++ib;
        } else {
            result=true;
            common.push_back(*ia);
            ++ia; ++ib;
        }
    }
    return result;
}

//---------------------------------------------------------------------------
void TripleBitQueryGraph::constructSubqueryEdges()
// Construct the edges for a specific subquery
{
    set<unsigned int> bindings;
    //#############################################################
    // Collect all variable bindings
    vector<set<ID> > patternBindings;//
    //TODO optionalBindings,unionBindings

    set<JoinVariableNodeID> TripleNodesVariables;
    //Collect Patterns variable
    patternBindings.resize(query.tripleNodes.size());
    for (unsigned int index=0,limit=patternBindings.size();index<limit;++index) {
//      const TripleBitQueryGraph::TripleNode& tpn=query.tripleNodes[index];
        const TripleNode& tpn = query.tripleNodes[index];
        if (!tpn.constSubject) {
            patternBindings[index].insert(tpn.subject);
            bindings.insert(tpn.subject);
            TripleNodesVariables.insert(tpn.subject);
        }
        if (!tpn.constPredicate) {
            patternBindings[index].insert(tpn.predicate);
            bindings.insert(tpn.predicate);
            TripleNodesVariables.insert(tpn.predicate);
        }
        if (!tpn.constObject) {
            patternBindings[index].insert(tpn.object);
            bindings.insert(tpn.object);
            TripleNodesVariables.insert(tpn.object);
        }
    }
    //TODO Collect OptionBindings variable
    //TODO Collect UnionBindings  variable

    // Derive all edges
    query.tripleEdges.clear();
    //query.joinVariableNodes.clear();

    ///triple,TODO option, unions node edges
    vector<ID> common; //common variable
    for (unsigned int index=0,limit=patternBindings.size();index<limit;++index) {
        //triple patterns edges
        for (unsigned int index2=index+1;index2<limit;index2++)
            if (intersects(patternBindings[index],patternBindings[index2],common)){
                query.tripleEdges.push_back(TripleBitQueryGraph::TripleNodesEdge(index,index2,common));
            }
        //TODO option unions edges
    }
}

bool TripleBitQueryGraph::isPredicateConst()
{
    size_t size = query.tripleNodes.size();

    vector<TripleNode>& node = query.tripleNodes;
    for(size_t i = 0; i < size; i++) {
        if(node[i].constPredicate == false)
            return false;
    }

    return true;
}
