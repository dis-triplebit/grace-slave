#ifndef TRIPLEBITBUILDER_H_
#define TRIPLEBITBUILDER_H_

#define TRIPLEBITBUILDER_DEBUG 1

class PredicateTable;
class URITable;
class URIStatisticsBuffer;
class StatementReificationTable;
class FindColumns;
class BitmapBuffer;
class Sorter;
class TempFile;
class StatisticsBuffer;

#include "TripleBit.h"
#include "StatisticsBuffer.h"

#include <fstream>
#include <pthread.h>
#include <cassert>
#include <cstring>
#include <set>
#include <unordered_set>

#include "TurtleParser.hpp"
#include "ThreadPool.h"
#include "TempFile.h"
#include "../TripleBit/tools/Systeminfo.hpp"

using namespace std;

class TripleBitBuilder {
private:
	//MySQL* mysql;
	BitmapBuffer* bitmap;
	PredicateTable* preTable;
	URITable* uriTable;
	vector<string> predicates;
	string dir;
	/// statistics buffer;
	StatisticsBuffer* statBuffer[4];
	StatementReificationTable* staReifTable;
	FindColumns* columnFinder;
public:
	TripleBitBuilder();
	TripleBitBuilder(const string dir);
	Status initBuild();
	Status startBuild();
	static const char* skipIdIdId(const char* reader);
	static int compareValue(const char* left, const char* right);
	static int compare213(const char* left, const char* right);
	static int compare231(const char* left, const char* right);
	static int compare123(const char* left, const char* right);
	static int compare321(const char* left, const char* right);
	static inline void loadTriple(const char* data, ID& v1, ID& v2, ID& v3) {
		TempFile::readId(TempFile::readId(TempFile::readId(data, v1), v2), v3);
	}

	static inline int cmpValue(ID l ,ID r) {
		return (l < r) ? -1 : ((l > r) ? 1 : 0);
	}
	static inline int cmpTriples(ID l1, ID l2, ID l3, ID r1, ID r2, ID r3) {
		int c = cmpValue(l1, r1);
		if(c)
			return c;
		c = cmpValue(l2, r2);
		if(c)
			return c;
		return cmpValue(l3, r3);

	}
	StatisticsBuffer* getStatBuffer(StatisticsBuffer::StatisticsType type) {
		return statBuffer[static_cast<int>(type)];
	}

	Status resolveTriples(string rawFactsFilename, string sosetFile,string psetFile);
	Status startBuildN3(string fileName);
	bool N3Parse(istream& in, const char* name, TempFile&);
	Status importFromMySQL(string db, string server, string username, string password);
	void NTriplesParse(const char* subject, const char* predicate, const char* object, TempFile&);
	bool generateXY(ID& subjectID, ID& objectID);
	Status buildIndex();
	Status endBuild();
	
	static bool isStatementReification(const char* object);
	virtual ~TripleBitBuilder();
};
#endif /* TRIPLEBITBUILDER_H_ */
