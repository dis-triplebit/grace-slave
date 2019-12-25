/*
 * URITable.h
 *
 *  Created on: Apr 17, 2010
 *      Author: root
 */

#ifndef URITABLE_H_
#define URITABLE_H_

#include "TripleBit.h"
#include "StringIDSegment.h"

using namespace std;
class URITable {
	ID prefixStartID;
	StringIDSegment* prefix_segment;
	StringIDSegment* suffix_segment;
	LengthString prefix, suffix;
	LengthString searchLen;

	string SINGLE;
	string searchStr;

private:
	Status getPrefix(const char*  URI);
public:
	URITable();
	URITable(const string dir);
	virtual ~URITable();
	Status insertTable(const char* URI,ID& id);
	Status getIdByURI(const char* URI,ID& id);
	Status getURIById(string& URI,ID id);

	size_t getSize() {
		cout<<"max id: "<<suffix_segment->getMaxID()<<endl;
		return prefix_segment->getSize() + suffix_segment->getSize();
	}

	ID getMaxID(){
		return suffix_segment->getMaxID();
	}

	void dump();
public:
	static URITable* load(const string dir);
};

#endif /* URITABLE_H_ */
