/*
 * QuerySemanticAnalysis.h
 *
 *  Created on: 2010-4-8
 *      Author: wdl
 */

#ifndef QUERYSEMANTICANALYSIS_H_
#define QUERYSEMANTICANALYSIS_H_

#include "IRepository.h"
#include "BitmapBuffer.h"

class IRepository;
class SPARQLParser;
class TripleBitQueryGraph;

///Semantic Analysis for SPARQL query. Transform the parse result into a query Graph.
class QuerySemanticAnalysis {

private:
	///Repository use for String and URI lookup.
	IRepository& repo;
//	URITable* uriTable;
//	PredicateTable* preTable;
public:
	QuerySemanticAnalysis(IRepository &repo);
	virtual ~QuerySemanticAnalysis();

	/// Perform the transformation
	bool transform(const SPARQLParser& input, TripleBitQueryGraph& output);
//	bool transformQuery(const SPARQLParser& input, TripleBitQueryGraph& output);
//	bool transformInsertData(const SPARQLParser::PatternGroup& group, TripleBitQueryGraph::SubQuery& output);
//	bool transformDeleteData(const SPARQLParser::PatternGroup& group, TripleBitQueryGraph::SubQuery& output);
//	bool transformDeleteClause(const SPARQLParser::PatternGroup& group, TripleBitQueryGraph::SubQuery& output);
//	bool transformUpdate(const SPARQLParser::PatternGroup& group, TripleBitQueryGraph::SubQuery& output);
//
//	bool encodeTripleNodeUpdate(const SPARQLParser::Pattern& triplePattern, TripleNode& tripleNode);
};

#endif /* QUERYSEMANTICANALYSIS_H_ */
