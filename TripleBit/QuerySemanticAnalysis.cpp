/*
 * QuerySemanticAnalysis.cpp
 * modify from RDF-3x
 *
 *  Created on: 2010-4-8
 *      Author: wdl
 *
 */

#include "QuerySemanticAnalysis.h"
#include <algorithm>
#include <iostream>
#include "SPARQLParser.h"
#include "TripleBit.h"
#include "TripleBitQueryGraph.h"
#include "TripleBitRepository.h"
#include <set>

using namespace std;

QuerySemanticAnalysis::QuerySemanticAnalysis(IRepository &repo) : repo(repo){
}

QuerySemanticAnalysis::~QuerySemanticAnalysis() {
}

extern bool isUnused(const TripleBitQueryGraph::SubQuery& query,const TripleNode& node,unsigned val);
//---------------------------------------------------------------------------
static bool binds(const SPARQLParser::PatternGroup& group, ID id)
   // Check if a variable is bound in a pattern group
{
   for (std::vector<SPARQLParser::Pattern>::const_iterator iter=group.patterns.begin(),limit=group.patterns.end();iter!=limit;++iter)
      if ((((*iter).subject.type==SPARQLParser::Element::Variable)&&((*iter).subject.id==id))||
          (((*iter).predicate.type==SPARQLParser::Element::Variable)&&((*iter).predicate.id==id))||
          (((*iter).object.type==SPARQLParser::Element::Variable)&&((*iter).object.id==id)))
         return true;
   for (std::vector<SPARQLParser::PatternGroup>::const_iterator iter=group.optional.begin(),limit=group.optional.end();iter!=limit;++iter)
      if (binds(*iter,id))
         return true;
   for (std::vector<std::vector<SPARQLParser::PatternGroup> >::const_iterator iter=group.unions.begin(),limit=group.unions.end();iter!=limit;++iter)
      for (std::vector<SPARQLParser::PatternGroup>::const_iterator iter2=(*iter).begin(),limit2=(*iter).end();iter2!=limit2;++iter2)
         if (binds(*iter2,id))
            return true;
   return false;
}

//---------------------------------------------------------------------------
static bool encodeFilter(IRepository &repo,const SPARQLParser::PatternGroup& group,const SPARQLParser::Filter& input,TripleBitQueryGraph::SubQuery& output)
   // Encode an element for the query graph
{
	// Check if the id is bound somewhere
	if (!binds(group,input.id))
		return false;

	// A complex filter? XXX handles only primitive filters
	if ((input.values.size()==1)&&(input.values[0].type==SPARQLParser::Element::Variable)) {
		if (!binds(group,input.id))
			return input.type==SPARQLParser::Filter::Exclude;
		TripleBitQueryGraph::ComplexFilter filter;
		filter.id1=input.id;
		filter.id2=input.values[0].id;
		filter.equal=(input.type==SPARQLParser::Filter::Normal);
		output.complexFilters.push_back(filter);
		return true;
	}

	// Resolve all values
	std::set<unsigned> values;
	for (std::vector<SPARQLParser::Element>::const_iterator iter=input.values.begin(),limit=input.values.end();iter!=limit;++iter) {
		unsigned id;
		if (repo.lookup(iter->value, id))
			values.insert(id);
	}

	// Construct the filter
	TripleBitQueryGraph::Filter filter;
	filter.id=input.id;
	filter.values.clear();
	if (input.type!=SPARQLParser::Filter::Path) {
		for (std::set<unsigned>::const_iterator iter=values.begin(),limit=values.end();iter!=limit;++iter)
			filter.values.push_back(*iter);
		filter.exclude=(input.type==SPARQLParser::Filter::Exclude);
	} else if (values.size()==2) {
		unsigned target,via;
		repo.lookup(input.values[0].value,target);
		repo.lookup(input.values[1].value,via);

		// Explore the path
		std::set<unsigned> explored;
		std::vector<unsigned> toDo;
		toDo.push_back(target);
		while (!toDo.empty()) {
			// Examine the next reachable node
			unsigned current=toDo.front();
			toDo.erase(toDo.begin());
			if (explored.count(current))
				continue;
			explored.insert(current);
			filter.values.push_back(current);

			// Request all other reachable nodes
			ID id;
			if( repo.getSubjectByObjectPredicate(current, via ) == OK ) {
				while (id == repo.next())
					toDo.push_back(id);
			}
			/*
			FactsSegment::Scan scan;
			if (scan.first(db.getFacts(Database::Order_Predicate_Object_Subject),via,current,0)) {
				while ((scan.getValue1()==via)&&(scan.getValue2()==current)) {
					toDo.push_back(scan.getValue3());
					if (!scan.next())
						break;
				}
			}
			*/
		}
	}

	output.filters.push_back(filter);
	return true;
}

//--------------------------------------------------------------------------
bool static encodeTripleNode(IRepository& repo, const SPARQLParser::Pattern& triplePattern, TripleNode &tripleNode)
// Encode a triple pattern for graph. encode subject,predicate,object to ids, also store their types.
{
	cout<<"enter encodetripleNode"<<endl;
	
	//encode subject node
	switch(triplePattern.subject.type) {
	 case SPARQLParser::Element::Variable:
		 tripleNode.subject = triplePattern.subject.id;
		 tripleNode.constSubject = false;
		 break;
	 case SPARQLParser::Element::String:
	 case SPARQLParser::Element::IRI:
	 	cout<<tripleNode.subject<<" ||| "<<triplePattern.subject.value<<endl;
		 if(repo.find_soid_by_string(tripleNode.subject,triplePattern.subject.value)){
			 tripleNode.constSubject = true;
			 break;
		 }else return false;
	 default: return false; // Error, this should not happen!
	}

	//encode object node.
	switch(triplePattern.object.type) {
	 case SPARQLParser::Element::Variable:
		 tripleNode.object = triplePattern.object.id;
		 tripleNode.constObject = false;
		 break;
	 case SPARQLParser::Element::String:
	 case SPARQLParser::Element::IRI:
	 	 cout<<tripleNode.object<<" ||| "<<triplePattern.object.value<<endl;
		 if(repo.find_soid_by_string(tripleNode.object,triplePattern.object.value)){
			 tripleNode.constObject = true;
			 break;
		 }else{
			 cout<<"object not found: "<<triplePattern.object.value<<endl;
			 return false;
		 }
	 default: return false; // Error, this should not happen!
	}

	//encode predicate node.
	switch(triplePattern.predicate.type) {
	 case SPARQLParser::Element::Variable:
		 tripleNode.predicate = triplePattern.predicate.id;
		 tripleNode.constPredicate = false;
		 break;
	 case SPARQLParser::Element::String:
	 case SPARQLParser::Element::IRI:
		 if(repo.find_pid_by_string(tripleNode.predicate,triplePattern.predicate.value)){
			 tripleNode.constPredicate = true;
			 break;
		 }else {
			 cout<<"predicate id not found: "<<triplePattern.predicate.value<<endl;
			 return false;
		 }
	 default: return false; // Error, this should not happen!
	}

	return true;
}
//-------------------------------------------------------------
static bool encodeTripleNodeUpdate(IRepository& repo,const SPARQLParser::Pattern& triplePattern, TripleNode &tripleNode)
// Encode a triple pattern for graph, encode subject, predicate, object to ids, also store their types
{
	//encode subject node
	switch(triplePattern.subject.type)
	{
	case SPARQLParser::Element::Variable:
		tripleNode.subject = triplePattern.subject.id;
		tripleNode.constSubject = false;
		tripleNode.scanOperation = TripleNode::FINDSBYPO;
		break;
	case SPARQLParser::Element::String:
	case SPARQLParser::Element::IRI:
		if(repo.find_soid_by_string_update(tripleNode.subject, triplePattern.subject.value))
		{
			tripleNode.constSubject = true;
			break;
		}else return false;
	default: return false; //Error, this should not happen!
	}

	//encode object node
	switch(triplePattern.object.type)
	{
	case SPARQLParser::Element::Variable:
		tripleNode.object = triplePattern.object.id;
		tripleNode.constObject = false;
		tripleNode.scanOperation = TripleNode::FINDOBYSP;
		break;
	case SPARQLParser::Element::String:
	case SPARQLParser::Element::IRI:
		if(repo.find_soid_by_string_update(tripleNode.object, triplePattern.object.value))
		{
			tripleNode.constObject = true;
			break;
		}else return false;
	default: return false; //Error, this should not happen!
	}

	//encode predicate node
	switch(triplePattern.predicate.type)
	{
	case SPARQLParser::Element::Variable:
		tripleNode.predicate = triplePattern.predicate.id;
		tripleNode.constPredicate = false;
		break;
	case SPARQLParser::Element::String:
	case SPARQLParser::Element::IRI:
		if(repo.find_pid_by_string_update(tripleNode.predicate, triplePattern.predicate.value))
		{
			tripleNode.constPredicate = true;
			break;
		}else return false;
	default: return false; // Error, this should not happen!
	}
	return true;
}

bool static collectJoinVariables(TripleBitQueryGraph::SubQuery& query)
   //collect all variables id from triple nodes.
{
	vector<ID>::iterator iter;

	for(unsigned int  index = 0, limit = query.tripleNodes.size(); index < limit ; index ++) {
	      const TripleNode& tpn=query.tripleNodes[index];
		  if (!tpn.constSubject) {
			  if(isUnused(query, tpn, tpn.subject) == false || query.tripleNodes.size() == 1) {
				  iter = find(query.joinVariables.begin(),query.joinVariables.end(),tpn.subject);
				  if(iter == query.joinVariables.end())
					  query.joinVariables.push_back(tpn.subject);
			  }
	      }
	      if (!tpn.constPredicate) {
			  if(isUnused(query, tpn, tpn.predicate) == false || query.tripleNodes.size() == 1) {
				  iter = find(query.joinVariables.begin(),query.joinVariables.end(),tpn.predicate);
				  if(iter == query.joinVariables.end())
					  query.joinVariables.push_back(tpn.predicate);
			  }
	      }
	      if (!tpn.constObject) {
			  if(isUnused(query, tpn, tpn.object) == false || query.tripleNodes.size() == 1) {
				  iter = find(query.joinVariables.begin(),query.joinVariables.end(),tpn.object);
				  if(iter == query.joinVariables.end())
					  query.joinVariables.push_back(tpn.object);
			  }
	      }
	}

	return true;
}

//bool static
//TODO
bool static encodeJoinVariableNodes(TripleBitQueryGraph::SubQuery& query)
	//encode VariablesNodes,the variable nodes include information about the variable node and triple nodes edges
{
	//construct variable nodes ,fill their members.
	std::vector<TripleBitQueryGraph::JoinVariableNodeID>::size_type limit = query.joinVariables.size();
	query.joinVariableNodes.resize(limit);
	//iterate to check a join variable node's edges
	for(unsigned int i = 0;i < limit;i ++) {
		query.joinVariableNodes[i].value = query.joinVariables[i];

		//cout<<"asdfasdf"<<endl;

		//check if has an edge with  triple node  j.
		std::vector< TripleNode>::size_type triplenodes_size = query.tripleNodes.size();
		for(unsigned int j = 0; j < triplenodes_size; ++ j) {
			if(!query.tripleNodes[j].constSubject) {
				if(query.joinVariableNodes[i].value == query.tripleNodes[j].subject){
					//add an edge
					std::pair<TripleBitQueryGraph::TripleNodeID,TripleBitQueryGraph::JoinVariableNode::DimType> edge;
					edge.first = query.tripleNodes[j].tripleNodeID;
					edge.second = TripleBitQueryGraph::JoinVariableNode::SUBJECT;
					query.joinVariableNodes[i].appear_tpnodes.push_back(edge);
				}
			}
			if(!query.tripleNodes[j].constPredicate){
				if(query.joinVariableNodes[i].value == query.tripleNodes[j].predicate){
					//add an edge
					std::pair<TripleBitQueryGraph::TripleNodeID,TripleBitQueryGraph::JoinVariableNode::DimType> edge;
					edge.first = query.tripleNodes[j].tripleNodeID;
					edge.second = TripleBitQueryGraph::JoinVariableNode::PREDICATE;
					query.joinVariableNodes[i].appear_tpnodes.push_back(edge);
				}
			}
			if(!query.tripleNodes[j].constObject){
				if(query.joinVariableNodes[i].value == query.tripleNodes[j].object){
					//add an edge
					std::pair<TripleBitQueryGraph::TripleNodeID,TripleBitQueryGraph::JoinVariableNode::DimType> edge;
					edge.first = query.tripleNodes[j].tripleNodeID;
					edge.second = TripleBitQueryGraph::JoinVariableNode::OBJCET;
					query.joinVariableNodes[i].appear_tpnodes.push_back(edge);
				}
			}

		}//end of for
	}
	return true;
}

static bool encodeJoinVariableEdges(TripleBitQueryGraph::SubQuery& output)
{
	vector<TripleBitQueryGraph::JoinVariableNode>::size_type size = output.joinVariableNodes.size(), i, j;
	vector< pair < TripleBitQueryGraph::TripleNodeID , TripleBitQueryGraph::JoinVariableNode::DimType> >::iterator iterI,iterJ;

	TripleBitQueryGraph::JoinVariableNodesEdge edge;

	for( i= 0; i < size - 1; i++)
	{
		//iterI = output.joinVariableNodes[i].appear_tpnodes.begin();
		for(j = i+1; j < size; j++)
		{
			iterI = output.joinVariableNodes[i].appear_tpnodes.begin();

			for(; iterI != output.joinVariableNodes[i].appear_tpnodes.end();iterI++){
				iterJ = output.joinVariableNodes[j].appear_tpnodes.begin();
				for( ; iterJ != output.joinVariableNodes[j].appear_tpnodes.end(); iterJ++){
					if(iterI->first == iterJ->first){
						edge.from = output.joinVariableNodes[i].value;
						edge.to = output.joinVariableNodes[j].value;
						output.joinVariableEdges.push_back(edge);
					}
				}
			}
		}
	}
	return true;
}

//---------------------------------------------------------------------------
//测试用
void static printTripleNode(IRepository& repo, TripleNode &tripleNode){
    //打印主语
    if(tripleNode.constSubject== false){//变量
        std::cout<<"subject is variable, id= "<<tripleNode.subject<<std::endl;
    }else{//常量
        std::string s;
        repo.find_string_by_soid(s,tripleNode.subject);
        std::cout<<"subject is constant, id="<<tripleNode.subject<<", find_string_by_soid="<<s<<std::endl;
    }

    //打印谓词
    if(tripleNode.constPredicate==false){//变量
        std::cout<<"predicate is variable, id= "<<tripleNode.predicate<<std::endl;
    }else{
        std::string s;
        repo.find_string_by_pid(s, tripleNode.predicate);
        std::cout << "predicate is constant, id=" << tripleNode.predicate << ", find_string_by_pid=" << s
                  << std::endl;
    }

    //打印宾语
    if(tripleNode.constObject==false){//变量
        std::cout<<"object is variable, id= "<<tripleNode.object<<std::endl;
    } else{//常量
        std::string s;
        repo.find_string_by_soid(s,tripleNode.object);
        std::cout<<"object is constant, id="<<tripleNode.object<<", find_string_by_soid="<<s<<endl;
    }
    cout<<endl;
}
/*
static bool transformSubquery(IRepository& repo,const SPARQLParser::PatternGroup& group,TripleBitQueryGraph::SubQuery& output)
	// Transform a subquery, encoding PatternGroup, fill the subquery,build the JoinVariable Nodes and their edges with triple nodes.
{
	// Encode all triple patterns
	TripleNode tripleNode;
	unsigned int tr_id = 0;
	for (std::vector<SPARQLParser::Pattern>::const_iterator iter = group.patterns.begin(),limit = group.patterns.end();iter != limit;++ iter, ++ tr_id) {
		// Encode the a triple pattern
		//TripleBitQueryGraph::TripleNode tripleNode;
		if(!encodeTripleNode(repo,(*iter),tripleNode)) return false;

        if(tripleNode.constPredicate == SPARQLParser::Element::Variable){
            //如果谓词未知
            output.joinType = TripleBitQueryGraph::UNION;//并集

            //重新生成模式
            std::vector<ID> allPredicateID;
            BitmapBuffer* tempBitmapBuffer=((TripleBitRepository*)&repo)->getBitmapBuffer();
            tempBitmapBuffer->getAllPredicateID(allPredicateID);
            std::vector<ID>::iterator begin, limit;
            for(begin=allPredicateID.begin(),limit=allPredicateID.end();begin!=limit;++begin){

                tripleNode.predicate = *begin;
                tripleNode.constPredicate = true;
                tripleNode.tripleNodeID = tr_id ;
                output.tripleNodes.push_back(tripleNode);
                ++tr_id;

#define UnknownedPredicate_Debug
#ifdef UnknownedPredicate_Debug
                printTripleNode(repo,tripleNode);
#endif
            }
        }else{
            //谓词已知
            tripleNode.tripleNodeID = tr_id ;
            output.tripleNodes.push_back(tripleNode);
            ++tr_id;

#define UnknownedPredicate_Debug
#ifdef UnknownedPredicate_Debug
            printTripleNode(repo,tripleNode);
#endif

        }
	}

	//TODO
	// Encode the filter conditions
	for (vector<SPARQLParser::Filter>::const_iterator iter = group.filters.begin(), limit = group.filters.end(); iter != limit; iter++) {
		if ( !encodeFilter(repo, group, *iter, output) ) {
			return false;
		}
	}

	//Collect join variables
	collectJoinVariables(output);

	//Encode Join Variable Nodes.
	TripleBitQueryGraph::JoinVariableNode joinVariableNode;
	if(!encodeJoinVariableNodes(output)) return false;

	//TODO Encode Join Variables Edges
	encodeJoinVariableEdges(output);
	//not TODO Encode Join Variable and Triple Node Edge,because such edge already exists in JoinVariableNode.
	//also it can be construct ,all depends on your choice.

	//TODO
	// Encode all optional parts

	//TODO
	// Encode all union parts

	return true;
}
*/

static bool transformSubquery(IRepository& repo,const SPARQLParser::PatternGroup& group,TripleBitQueryGraph::SubQuery& output)
// Transform a subquery, encoding PatternGroup, fill the subquery,build the JoinVariable Nodes and their edges with triple nodes.
{
    // Encode all triple patterns
    TripleNode tripleNode;
    unsigned int tr_id = 0;
    for (std::vector<SPARQLParser::Pattern>::const_iterator iter = group.patterns.begin(),limit = group.patterns.end();iter != limit;++ iter) {
        // Encode the a triple pattern
        //TripleBitQueryGraph::TripleNode tripleNode;
        if(!encodeTripleNode(repo,(*iter),tripleNode)) return false;
        if(tripleNode.constPredicate == SPARQLParser::Element::Variable){
            //如果谓词未知
            output.joinType = TripleBitQueryGraph::UNION;//并集

            //重新生成模式，遍历所有的谓词
            std::vector<ID> allPredicateID;
            BitmapBuffer* tempBitmapBuffer=((TripleBitRepository*)&repo)->getBitmapBuffer();
            tempBitmapBuffer->getAllPredicateID(allPredicateID);
            std::vector<ID>::iterator iter, limit;
            for(iter=allPredicateID.begin(),limit=allPredicateID.end();iter!=limit;++iter){
                //过滤掉选择度为0的模式
                int selectivity = INT_MAX;
                if (tripleNode.constObject) {
                    if (tripleNode.constSubject)
                        selectivity = 1;
                    else selectivity = repo.get_object_predicate_count(tripleNode.object, *iter);
                } else {
                    if (tripleNode.constSubject)
                        selectivity = repo.get_subject_predicate_count(tripleNode.subject, *iter);
                    else selectivity = repo.get_predicate_count(*iter);
                }

                //选择度不为0
                if(selectivity>0){
                    tripleNode.predicate = *iter;
                    tripleNode.constPredicate = true;
                    tripleNode.tripleNodeID = tr_id ;
                    output.tripleNodes.push_back(tripleNode);
                    ++tr_id;

#define UnknownedPredicate_Debug
#ifdef UnknownedPredicate_Debug
                    printTripleNode(repo,tripleNode);
#endif
                }
            }
        }else{
            //谓词已知
            tripleNode.tripleNodeID = tr_id ;
            output.tripleNodes.push_back(tripleNode);
            ++tr_id;

#define UnknownedPredicate_Debug
#ifdef UnknownedPredicate_Debug
            printTripleNode(repo,tripleNode);
#endif

        }
    }

    //TODO
    // Encode the filter conditions
    for (vector<SPARQLParser::Filter>::const_iterator iter = group.filters.begin(), limit = group.filters.end(); iter != limit; iter++) {
        if ( !encodeFilter(repo, group, *iter, output) ) {
            return false;
        }
    }

    //Collect join variables
    collectJoinVariables(output);

    //Encode Join Variable Nodes.
    TripleBitQueryGraph::JoinVariableNode joinVariableNode;
    if(!encodeJoinVariableNodes(output)) return false;

    //TODO Encode Join Variables Edges
    encodeJoinVariableEdges(output);
    //not TODO Encode Join Variable and Triple Node Edge,because such edge already exists in JoinVariableNode.
    //also it can be construct ,all depends on your choice.

    //TODO
    // Encode all optional parts

    //TODO
    // Encode all union parts

    return true;
}


//------------------------------------------------------------------------
static bool transformInsertData(IRepository& repo, const SPARQLParser::PatternGroup& group, TripleBitQueryGraph::SubQuery& output)
{
	TripleNode tripleNode;
	unsigned int tr_id = 0;
	for(std::vector<SPARQLParser::Pattern>::const_iterator iter = group.patterns.begin(), limit = group.patterns.end(); iter != limit; ++iter, ++tr_id)
	{
		//Encode a triple pattern
		if(!encodeTripleNodeUpdate(repo, (*iter), tripleNode)) return false;
		tripleNode.tripleNodeID = tr_id;
		output.tripleNodes.push_back(tripleNode);
	}
	return true;
}
//---------------------------------------------------------------------------
static bool transformDeleteData(IRepository& repo, const SPARQLParser::PatternGroup& group, TripleBitQueryGraph::SubQuery& output)
{
	TripleNode tripleNode;
	unsigned int tr_id = 0;
	for(std::vector<SPARQLParser::Pattern>::const_iterator iter = group.patterns.begin(), limit = group.patterns.end(); iter != limit; ++iter, ++tr_id)
	{
		//Encode a triple pattern
		if(!encodeTripleNode(repo, (*iter), tripleNode)) continue;  //Ignore the not exited triple, just delete the exited triple
		tripleNode.tripleNodeID = tr_id;
		output.tripleNodes.push_back(tripleNode);
	}
	return true;
}
//--------------------------------------------------------------------------
static bool transformDeleteClause(IRepository& repo, const SPARQLParser::PatternGroup& group, TripleBitQueryGraph::SubQuery& output)
{
	TripleNode tripleNode;
	std::vector<SPARQLParser::Pattern>::const_iterator iter = group.patterns.begin();
	if(!encodeTripleNode(repo, (*iter), tripleNode)) return false;
	tripleNode.tripleNodeID = 0;
	output.tripleNodes.push_back(tripleNode);
	return true;
}
//-------------------------------------------------------------------------
static bool transformUpdate(IRepository& repo, const SPARQLParser::PatternGroup& group, TripleBitQueryGraph::SubQuery& output)
{
	//Encode all triple patterns
	TripleNode tripleNode;
	unsigned int tr_id = 0;

	std::vector<SPARQLParser::Pattern>::const_iterator iter = group.patterns.begin();
	if(!encodeTripleNode(repo, (*iter), tripleNode)) return false;
	tripleNode.tripleNodeID = tr_id;
	output.tripleNodes.push_back(tripleNode);

	++iter;
	++tr_id;

	if(!encodeTripleNodeUpdate(repo, (*iter), tripleNode)) return false;
	tripleNode.tripleNodeID = tr_id;
	output.tripleNodes.push_back(tripleNode);

	return true;
}

static bool transformQuery(IRepository& repo,const SPARQLParser& input, TripleBitQueryGraph& output)
{
	output.variableCount = input.getVarCount();
	if (!transformSubquery(repo, input.getPatterns(), output.getQuery())) {
		  // A constant could not be resolved. This will produce an empty result
		 output.markAsKnownEmpty();
		 return true;
	}
	// Compute the subquery edges(always the graph pattern ,option pattern,filter pattern join edges)
	output.constructSubqueryEdges();

	// Add the projection entry
	for (SPARQLParser::projection_iterator iter=input.projectionBegin(),limit=input.projectionEnd();iter!=limit;++iter)
		output.addProjection(*iter);

	// Set the duplicate handling

	switch (input.getProjectionModifier()) {
	case SPARQLParser::Modifier_None: output.setDuplicateHandling(TripleBitQueryGraph::AllDuplicates); break;
	case SPARQLParser::Modifier_Distinct: output.setDuplicateHandling(TripleBitQueryGraph::NoDuplicates); break;
	case SPARQLParser::Modifier_Reduced: output.setDuplicateHandling(TripleBitQueryGraph::ReducedDuplicates); break;
	case SPARQLParser::Modifier_Count: output.setDuplicateHandling(TripleBitQueryGraph::CountDuplicates); break;
	case SPARQLParser::Modifier_Duplicates: output.setDuplicateHandling(TripleBitQueryGraph::ShowDuplicates); break;
   }

	// Set the limit
	output.setLimit(input.getLimit());
	return true;
}

//---------------------------------------------------------------------------
bool QuerySemanticAnalysis::transform(const SPARQLParser& input,TripleBitQueryGraph& output)
// Perform the transformation
{
	output.clear();
	TripleBitQueryGraph::OpType token;
	switch(input.getOperationType())
	{
	case SPARQLParser::QUERY:
		token = TripleBitQueryGraph::QUERY;
		output.setOpType(token);
		transformQuery(repo, input, output);
		break;
	case SPARQLParser::INSERT_DATA:
		token = TripleBitQueryGraph::INSERT_DATA;
		output.setOpType(token);
		transformInsertData(repo, input.getPatterns(), output.getQuery());
		break;
	case SPARQLParser::DELETE_DATA:
		token = TripleBitQueryGraph::DELETE_DATA;
		output.setOpType(token);
		transformDeleteData(repo, input.getPatterns(), output.getQuery());
		break;
	case SPARQLParser::DELETE_CLAUSE:
		token = TripleBitQueryGraph::DELETE_CLAUSE;
		output.setOpType(token);
		transformDeleteClause(repo, input.getPatterns(), output.getQuery());
		break;
	case SPARQLParser::UPDATE:
		token = TripleBitQueryGraph::UPDATE;
		output.setOpType(token);
		transformUpdate(repo, input.getPatterns(), output.getQuery());
		break;
	}
	return true;
}
