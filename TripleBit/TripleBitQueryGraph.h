/*
 * BitmatQueryGraph.h
 *
 * modify from rdf-3x source code and bitmat paper.
 *
 *  Created on: 2019-05-10
 *      Author: favoniankong
 */

#ifndef TRIPLEBITQUERYGRAPH_H_
#define TRIPLEBITQUERYGRAPH_H_

#include "TripleBit.h"
#include "IRepository.h"
#include "SPARQLLexer.h"
#include "SPARQLParser.h"
#include <vector>
#include <utility>

class SPARQLParser;

class TripleBitQueryGraph {

public:
    /// Possible duplicate handling modes
    enum DuplicateHandling { AllDuplicates, CountDuplicates, ReducedDuplicates, NoDuplicates, ShowDuplicates };
    //join variable's style
    enum JoinGraph{ CYCLIC,	ACYCLIC };

    //used to identify the query operation type
    enum OpType {QUERY, INSERT_DATA , DELETE_DATA, DELETE_CLAUSE, UPDATE};

    typedef unsigned int ID;
    typedef unsigned int TripleNodeID;
    typedef unsigned int JoinVariableNodeID;
    typedef unsigned int ConstElementNodeID;

    int variableCount;
    /// The variable node of triple node.
    typedef struct JoinVariableNode {
        //used to identify the join operation type
        enum JoinType{
            SS,
            OO,
            SO,
            PP,
            OP,
            SP,
            PS,
            PO,
            OS,
            UNKNOWN
        };

        //not use!
        std::string text;
        //the value in the triple node
        ID value;
        //the variable appear in which triple node and the triples dimension.
        enum DimType{SUBJECT = 1,PREDICATE = 2,OBJCET = 4};
        std::vector<std::pair<TripleNodeID,DimType> > appear_tpnodes;
        // Is there an variable edge to another JoinVariableNode
        bool hasEdge(const JoinVariableNode& other)const;
    }jvar_node;

    /// A value filter
    struct Filter {
        /// The id
        unsigned id;
        /// The valid values. Sorted by id.
        std::vector<unsigned> values;
        /// Negative filter?
        bool exclude;
    };

    /// A (potentially) complex filter. Currently very limited.
    struct ComplexFilter {
        /// The ids
        unsigned id1,id2;
        /// Test for  equal?
        bool equal;
    };

    /// The same Element(s,p,o) between TripleNodes.
    //TODO:Not use now
    typedef struct ConstElementNode {
        enum Type { SUBJECT, PREDICATE, OBJECT };
        ID value;
        Type type;
    }const_e_node;


    /// Join Edge between two TripleNodes.
    typedef struct TripleNodesEdge {
        /// The endpoints
        TripleNodeID from,to;
        /// Common variables
        std::vector<ID> common;
        /// Constructor
        TripleNodesEdge(TripleNodeID from, TripleNodeID to, const std::vector<ID>& common);
        /// Destructor
        ~TripleNodesEdge();
    }tpn_edge;


    /// an edge between two variable nodes
    typedef struct JoinVariableNodesEdge {
        JoinVariableNodeID from;
        JoinVariableNodeID to;
    }j_var_edge;

    //谓词是否已知，如果谓词未知，重新解释查询语句，执行并操作UNION，只支持一个模式
    enum JoinType{INTERSECTION,UNION};

    /// Description of a subquery
    struct SubQuery {
        SubQuery();//构造函数
        JoinType joinType;//谓词已知或者未知，默认为已知，只要出现一个谓词未知的模式就会改变状态

        // The TripleNodes
        std::vector<TripleNode> tripleNodes;
        // The triple node's edges
        std::vector<TripleNodesEdge> tripleEdges;

        //the join Variable Node
        std::vector<JoinVariableNodeID > joinVariables;
        std::vector<JoinVariableNode> joinVariableNodes;
        //the join Variable Edge.
        std::vector<JoinVariableNodesEdge> joinVariableEdges;

        //not use!! the join variable and the triple node edge.
//	  std::vector<JoinVariableNodeTripleNodeEdge> joinVriableNodeTripleNodeEdge;

        //TODO not implement!!!
        /// The filter conditions
        std::vector<Filter> filters;
        /// The complex filter conditions
        std::vector<ComplexFilter> complexFilters;
        /// Optional subqueries
        std::vector<SubQuery> optional;
        /// Union subqueries
        std::vector<std::vector<SubQuery> > unions;
        /// tree root node
        JoinVariableNodeID rootID;
        /// leaf nodes
        std::vector<JoinVariableNodeID> leafNodes;
        /// is cyclic or acyclic
        JoinGraph joinGraph;
        /// selectivity;
        std::map<JoinVariableNodeID,int> selectivityMap;
    };

private:
    /// The query itself,also is the Graph triple
    SubQuery query;
    /// The projection
    std::vector<ID> projection;
    /// The duplicate handling
    DuplicateHandling duplicateHandling;
    /// Maximum result size
    unsigned int limit;
    /// Is the query known to produce an empty result?
    bool knownEmptyResult;

    OpType QueryOperation;

public:
    ///constructor
    TripleBitQueryGraph();
    virtual ~TripleBitQueryGraph();

    /// Clear the graph
    void clear();
    /// Construct the edges for a specific subquery(always the graph pattern ,option pattern,filter pattern join edges)
    void constructSubqueryEdges();

    //set query operation type
    void setOpType(OpType token) { QueryOperation = token;}
    //get query operation type
    OpType getOpType() { return QueryOperation;}
    /// Set the duplicate handling mode
    void setDuplicateHandling(DuplicateHandling d) { duplicateHandling=d; }
    /// Get the duplicate handling mode
    DuplicateHandling getDuplicateHandling() const { return duplicateHandling; }
    /// Set the result limit
    void setLimit(unsigned int l) { limit=l; }
    /// Get the result limit
    unsigned getLimit() const { return limit; }
    /// Known empty result
    void markAsKnownEmpty() { knownEmptyResult=true; }
    /// Known empty result?
    bool knownEmpty() const { return knownEmptyResult; }

    void Clear();
    /// Get the query
    SubQuery& getQuery() { return query; }
    /// Get the query
    const SubQuery& getQuery() const { return query; }

    /// Add an entry to the output projection
    void addProjection(unsigned int id) { projection.push_back(id); }
    /// Iterator over the projection
    typedef std::vector<unsigned int>::const_iterator projection_iterator;
    /// Iterator over the projection
    projection_iterator projectionBegin() const { return projection.begin(); }
    /// Iterator over the projection
    projection_iterator projectionEnd() const { return projection.end(); }
    /// project IDs
    vector<ID>& getProjection() { return projection; }

    bool isPredicateConst();
};

#endif /* TRIPLEBITQUERYGRAPH_H_ */
